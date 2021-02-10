// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventsManager.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "UObject/UObjectThreadContext.h"
#include "EventsSettings.h"
#include "EventsRuntimeModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/CoreDelegates.h"

#if WITH_EDITOR
#include "SourceControlHelpers.h"
#include "ISourceControlModule.h"
#include "Editor.h"
#include "PropertyHandle.h"
FSimpleMulticastDelegate UEventsManager::OnEditorRefreshEventTree;
#endif
#include "HAL/IConsoleManager.h"

const FName UEventsManager::NAME_Categories("Categories");
const FName UEventsManager::NAME_EventFilter("EventFilter");

#define LOCTEXT_NAMESPACE "EventManager"

UEventsManager* UEventsManager::SingletonManager = nullptr;

UEventsManager::UEventsManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseFastReplication = false;
	bShouldWarnOnInvalidTags = true;
	bDoneAddingNativeTags = false;
	NetIndexFirstBitSegment = 16;
	NetIndexTrueBitNum = 16;
	NumBitsForContainerSize = 6;
	NetworkEventNodeIndexHash = 0;
}

// Enable to turn on detailed startup logging
#define EventS_VERBOSE 0

#if STATS && EventS_VERBOSE
#define SCOPE_LOG_EventS(Name) SCOPE_LOG_TIME_IN_SECONDS(Name, nullptr)
#else
#define SCOPE_LOG_EventS(Name)
#endif

void UEventsManager::LoadEventTables(bool bAllowAsyncLoad)
{
	UEventsSettings* MutableDefault = GetMutableDefault<UEventsSettings>();
	EventTables.Empty();

#if !WITH_EDITOR
	// If we're a cooked build and in a safe spot, start an async load so we can pipeline it
	if (bAllowAsyncLoad && !IsLoading() && MutableDefault->EventTableList.Num() > 0)
	{
		for (FSoftObjectPath DataTablePath : MutableDefault->EventTableList)
		{
			LoadPackageAsync(DataTablePath.GetLongPackageName());
		}

		return;
	}
#endif // !WITH_EDITOR

	SCOPE_LOG_EventS(TEXT("UEventsManager::LoadEventTables"));
	for (FSoftObjectPath DataTablePath : MutableDefault->EventTableList)
	{
		UDataTable* TagTable = LoadObject<UDataTable>(nullptr, *DataTablePath.ToString(), nullptr, LOAD_None, nullptr);

		// Handle case where the module is dynamically-loaded within a LoadPackage stack, which would otherwise
		// result in the tag table not having its RowStruct serialized in time. Without the RowStruct, the tags manager
		// will not be initialized correctly.
		if (TagTable)
		{
			FLinkerLoad* TagLinker = TagTable->GetLinker();
			if (TagLinker)
			{
				TagTable->GetLinker()->Preload(TagTable);
			}
		}
		EventTables.Add(TagTable);
	}
}

struct FCompareFEventNodeByTag
{
	FORCEINLINE bool operator()( const TSharedPtr<FEventNode>& A, const TSharedPtr<FEventNode>& B ) const
	{
		// Note: GetSimpleTagName() is not good enough here. The individual tag nodes are share frequently (E.g, Dog.Tail, Cat.Tail have sub nodes with the same simple tag name)
		// Compare with equal FNames will look at the backing number/indice to the FName. For FNames used elsewhere, like "A" for example, this can cause non determinism in platforms
		// (For example if static order initialization differs on two platforms, the "version" of the "A" FName that two places get could be different, causing this comparison to also be)
		return (A->GetCompleteTagName().Compare(B->GetCompleteTagName())) < 0;
	}
};

void UEventsManager::AddTagIniSearchPath(const FString& RootDir)
{
	// Read all tags from the ini
	TArray<FString> FilesInDirectory;
	IFileManager::Get().FindFilesRecursive(FilesInDirectory, *RootDir, TEXT("*.ini"), true, false);
	if (FilesInDirectory.Num() > 0)
	{
		FilesInDirectory.Sort();
		for (const FString& IniFilePath : FilesInDirectory)
		{
			ExtraTagIniList.AddUnique(IniFilePath);
		}

		if (!bIsConstructingEventTree)
		{
#if WITH_EDITOR
			EditorRefreshEventTree();
#else
			AddTagsFromAdditionalLooseIniFiles(FilesInDirectory);

			ConstructNetIndex();

			IEventsModule::OnEventTreeChanged.Broadcast();
#endif
		}
	}
}

void UEventsManager::AddTagsFromAdditionalLooseIniFiles(const TArray<FString>& IniFileList)
{
	// Read all tags from the ini
	for (const FString& IniFilePath : IniFileList)
	{
		const FName TagSource = FName(*FPaths::GetCleanFilename(IniFilePath));

		// skip the restricted tag files
		if (RestrictedEventSourceNames.Contains(TagSource))
		{
			continue;
		}

		FEventSource* FoundSource = FindOrAddTagSource(TagSource, EEventSourceType::TagList);

		UE_CLOG(EventS_VERBOSE, LogEvents, Display, TEXT("Loading Tag File: %s"), *IniFilePath);

		if (FoundSource && FoundSource->SourceTagList)
		{
			FoundSource->SourceTagList->ConfigFileName = IniFilePath;

			// Check deprecated locations
			TArray<FString> Tags;
			if (GConfig->GetArray(TEXT("UserTags"), TEXT("Events"), Tags, IniFilePath))
			{
				for (const FString& Tag : Tags)
				{
					FoundSource->SourceTagList->EventList.AddUnique(FEventTableRow(FName(*Tag)));
				}
			}
			else
			{
				// Load from new ini
				FoundSource->SourceTagList->LoadConfig(UEventsList::StaticClass(), *IniFilePath);
			}

#if WITH_EDITOR
			if (GIsEditor || IsRunningCommandlet()) // Sort tags for UI Purposes but don't sort in -game scenario since this would break compat with noneditor cooked builds
			{
				FoundSource->SourceTagList->SortTags();
			}
#endif

			for (const FEventTableRow& TableRow : FoundSource->SourceTagList->EventList)
			{
				AddTagTableRow(TableRow, TagSource);
			}
		}
	}
}

void UEventsManager::ConstructEventTree()
{
	SCOPE_LOG_EventS(TEXT("UEventsManager::ConstructEventTree"));
	TGuardValue<bool> GuardRebuilding(bIsConstructingEventTree, true);
	if (!GameplayRootTag.IsValid())
	{
		GameplayRootTag = MakeShareable(new FEventNode());

		UEventsSettings* MutableDefault = GetMutableDefault<UEventsSettings>();

		// Copy invalid characters, then add internal ones
		InvalidTagCharacters = MutableDefault->InvalidTagCharacters;
		InvalidTagCharacters.Append(TEXT("\r\n\t"));

		// Add prefixes first
		if (ShouldImportTagsFromINI())
		{
			SCOPE_LOG_EventS(TEXT("UEventsManager::ConstructEventTree: ImportINI prefixes"));

			TArray<FString> RestrictedEventFiles;
			GetRestrictedTagConfigFiles(RestrictedEventFiles);
			RestrictedEventFiles.Sort();

			for (const FString& FileName : RestrictedEventFiles)
			{
				FName TagSource = FName(*FPaths::GetCleanFilename(FileName));
				if (TagSource == NAME_None)
				{
					continue;
				}
				RestrictedEventSourceNames.Add(TagSource);
				FEventSource* FoundSource = FindOrAddTagSource(TagSource, EEventSourceType::RestrictedTagList);

				// Make sure we have regular tag sources to match the restricted tag sources but don't try to read any tags from them yet.
				FindOrAddTagSource(TagSource, EEventSourceType::TagList);

				if (FoundSource && FoundSource->SourceRestrictedTagList)
				{
					FoundSource->SourceRestrictedTagList->LoadConfig(URestrictedEventsList::StaticClass(), *FileName);

#if WITH_EDITOR
					if (GIsEditor || IsRunningCommandlet()) // Sort tags for UI Purposes but don't sort in -game scenario since this would break compat with noneditor cooked builds
					{
						FoundSource->SourceRestrictedTagList->SortTags();
					}
#endif
					for (const FRestrictedEventTableRow& TableRow : FoundSource->SourceRestrictedTagList->RestrictedEventList)
					{
						AddTagTableRow(TableRow, TagSource, true);
					}
				}
			}
		}

		{
			SCOPE_LOG_EventS(TEXT("UEventsManager::ConstructEventTree: Add native tags"));
			// Add native tags before other tags
		for (FName TagToAdd : NativeTagsToAdd)
		{
			AddTagTableRow(FEventTableRow(TagToAdd), FEventSource::GetNativeName());
		}
		}

		// If we didn't load any tables it might be async loading, so load again with a flush
		if (EventTables.Num() == 0)
		{
			LoadEventTables(false);
		}
		
			{
			SCOPE_LOG_EventS(TEXT("UEventsManager::ConstructEventTree: Construct from data asset"));
			for (UDataTable* DataTable : EventTables)
				{
				if (DataTable)
				{
					PopulateTreeFromDataTable(DataTable);
				}
			}
		}

		// Create native source
		FindOrAddTagSource(FEventSource::GetNativeName(), EEventSourceType::Native);

		if (ShouldImportTagsFromINI())
		{
			SCOPE_LOG_EventS(TEXT("UEventsManager::ConstructEventTree: ImportINI tags"));

			// Copy from deprecated list in DefaultEngine.ini
			TArray<FString> EngineConfigTags;
			GConfig->GetArray(TEXT("/Script/Events.EventsSettings"), TEXT("+Events"), EngineConfigTags, GEngineIni);
			
			for (const FString& EngineConfigTag : EngineConfigTags)
			{
				MutableDefault->EventList.AddUnique(FEventTableRow(FName(*EngineConfigTag)));
			}

			// Copy from deprecated list in DefaultGamplayTags.ini
			EngineConfigTags.Empty();
			GConfig->GetArray(TEXT("/Script/Events.EventsSettings"), TEXT("+Events"), EngineConfigTags, MutableDefault->GetDefaultConfigFilename());

			for (const FString& EngineConfigTag : EngineConfigTags)
			{
				MutableDefault->EventList.AddUnique(FEventTableRow(FName(*EngineConfigTag)));
			}

#if WITH_EDITOR
			MutableDefault->SortTags();
#endif

			FName TagSource = FEventSource::GetDefaultName();
			FEventSource* DefaultSource = FindOrAddTagSource(TagSource, EEventSourceType::DefaultTagList);

			for (const FEventTableRow& TableRow : MutableDefault->EventList)
			{
				AddTagTableRow(TableRow, TagSource);
			}

			// Extra tags
			AddTagIniSearchPath(FPaths::ProjectConfigDir() / TEXT("Tags"));
			AddTagsFromAdditionalLooseIniFiles(ExtraTagIniList);
		}

#if WITH_EDITOR
		// Add any transient editor-only tags
		for (FName TransientTag : TransientEditorTags)
		{
			AddTagTableRow(FEventTableRow(TransientTag), FEventSource::GetTransientEditorName());
		}
#endif
		{
			SCOPE_LOG_EventS(TEXT("UEventsManager::ConstructEventTree: Request common tags"));
		// Grab the commonly replicated tags
		CommonlyReplicatedTags.Empty();
		for (FName TagName : MutableDefault->CommonlyReplicatedTags)
		{
			FEventInfo Tag = RequestEvent(TagName);
			if (Tag.IsValid())
			{
				CommonlyReplicatedTags.Add(Tag);
			}
			else
			{
				UE_LOG(LogEvents, Warning, TEXT("%s was found in the CommonlyReplicatedTags list but doesn't appear to be a valid tag!"), *TagName.ToString());
			}
		}

		bUseFastReplication = MutableDefault->FastReplication;
		bShouldWarnOnInvalidTags = MutableDefault->WarnOnInvalidTags;
		NumBitsForContainerSize = MutableDefault->NumBitsForContainerSize;
		NetIndexFirstBitSegment = MutableDefault->NetIndexFirstBitSegment;
		}

		if (ShouldUseFastReplication())
		{
			SCOPE_LOG_EventS(TEXT("UEventsManager::ConstructEventTree: Reconstruct NetIndex"));
			ConstructNetIndex();
		}

		{
			SCOPE_LOG_EventS(TEXT("UEventsManager::ConstructEventTree: EventTreeChangedEvent.Broadcast"));
			IEventsModule::OnEventTreeChanged.Broadcast();
		}

		{
			SCOPE_LOG_EventS(TEXT("UEventsManager::ConstructEventTree: Load redirects"));
		// Update the TagRedirects map
		TagRedirects.Empty();

		// Check the deprecated location
		bool bFoundDeprecated = false;
			FConfigSection* PackageRedirects = GConfig->GetSectionPrivate(TEXT("/Script/Engine.Engine"), false, true, GEngineIni);

		if (PackageRedirects)
		{
			for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
			{
				if (It.Key() == TEXT("+EventRedirects"))
				{
					FName OldTagName = NAME_None;
					FName NewTagName;

					if (FParse::Value(*It.Value().GetValue(), TEXT("OldTagName="), OldTagName))
					{
						if (FParse::Value(*It.Value().GetValue(), TEXT("NewTagName="), NewTagName))
						{
							FEventRedirect Redirect;
							Redirect.OldTagName = OldTagName;
							Redirect.NewTagName = NewTagName;

							MutableDefault->EventRedirects.AddUnique(Redirect);

							bFoundDeprecated = true;
						}
					}
				}
			}
		}

		if (bFoundDeprecated)
		{
			UE_LOG(LogEvents, Log, TEXT("EventRedirects is in a deprecated location, after editing Events developer settings you must remove these manually"));
		}

		// Check settings object
		for (const FEventRedirect& Redirect : MutableDefault->EventRedirects)
		{
			FName OldTagName = Redirect.OldTagName;
			FName NewTagName = Redirect.NewTagName;

			if (ensureMsgf(!TagRedirects.Contains(OldTagName), TEXT("Old tag %s is being redirected to more than one tag. Please remove all the redirections except for one."), *OldTagName.ToString()))
			{
				FEventInfo OldTag = RequestEvent(OldTagName, false); //< This only succeeds if OldTag is in the Table!
				if (OldTag.IsValid())
				{
					FEventContainer MatchingChildren = RequestEventChildren(OldTag);

					FString Msg = FString::Printf(TEXT("Old tag (%s) which is being redirected still exists in the table!  Generally you should "
						TEXT("remove the old tags from the table when you are redirecting to new tags, or else users will ")
						TEXT("still be able to add the old tags to containers.")), *OldTagName.ToString());

					if (MatchingChildren.Num() == 0)
					{
						UE_LOG(LogEvents, Warning, TEXT("%s"), *Msg);
					}
					else
					{
						Msg += TEXT("\nSuppressed warning due to redirected tag being a single component that matched other hierarchy elements.");
						UE_LOG(LogEvents, Log, TEXT("%s"), *Msg);
					}
				}

				FEventInfo NewTag = (NewTagName != NAME_None) ? RequestEvent(NewTagName, false) : FEventInfo();

				// Basic infinite recursion guard
				int32 IterationsLeft = 10;
				while (!NewTag.IsValid() && NewTagName != NAME_None)
				{
					bool bFoundRedirect = false;

					// See if it got redirected again
					for (const FEventRedirect& SecondRedirect : MutableDefault->EventRedirects)
					{
						if (SecondRedirect.OldTagName == NewTagName)
						{
							NewTagName = SecondRedirect.NewTagName;
							NewTag = RequestEvent(NewTagName, false);
							bFoundRedirect = true;
							break;
						}
					}
					IterationsLeft--;

					if (!bFoundRedirect || IterationsLeft <= 0)
					{
						UE_LOG(LogEvents, Warning, TEXT("Invalid new tag %s!  Cannot replace old tag %s."),
							*Redirect.NewTagName.ToString(), *Redirect.OldTagName.ToString());
						break;
					}
				}

				if (NewTag.IsValid())
				{
					// Populate the map
					TagRedirects.Add(OldTagName, NewTag);
				}
			}
		}
	}
	}
}

int32 PrintNetIndiceAssignment = 0;
static FAutoConsoleVariableRef CVarPrintNetIndiceAssignment(TEXT("Events.PrintNetIndiceAssignment"), PrintNetIndiceAssignment, TEXT("Logs Event NetIndice assignment"), ECVF_Default );
void UEventsManager::ConstructNetIndex()
{
	NetworkEventNodeIndex.Empty();

	EventNodeMap.GenerateValueArray(NetworkEventNodeIndex);

	NetworkEventNodeIndex.Sort(FCompareFEventNodeByTag());

	check(CommonlyReplicatedTags.Num() <= NetworkEventNodeIndex.Num());

	// Put the common indices up front
	for (int32 CommonIdx=0; CommonIdx < CommonlyReplicatedTags.Num(); ++CommonIdx)
	{
		int32 BaseIdx=0;
		FEventInfo& Tag = CommonlyReplicatedTags[CommonIdx];

		bool Found = false;
		for (int32 findidx=0; findidx < NetworkEventNodeIndex.Num(); ++findidx)
		{
			if (NetworkEventNodeIndex[findidx]->GetCompleteTag() == Tag)
			{
				NetworkEventNodeIndex.Swap(findidx, CommonIdx);
				Found = true;
				break;
			}
		}

		// A non fatal error should have been thrown when parsing the CommonlyReplicatedTags list. If we make it here, something is seriously wrong.
		checkf( Found, TEXT("Tag %s not found in NetworkEventNodeIndex"), *Tag.ToString() );
	}

	InvalidTagNetIndex = NetworkEventNodeIndex.Num()+1;
	NetIndexTrueBitNum = FMath::CeilToInt(FMath::Log2(InvalidTagNetIndex));
	
	// This should never be smaller than NetIndexTrueBitNum
	NetIndexFirstBitSegment = FMath::Min<int64>(NetIndexFirstBitSegment, NetIndexTrueBitNum);

	// This is now sorted and it should be the same on both client and server
	if (NetworkEventNodeIndex.Num() >= INVALID_TAGNETINDEX)
	{
		ensureMsgf(false, TEXT("Too many tags in dictionary for networking! Remove tags or increase tag net index size"));

		NetworkEventNodeIndex.SetNum(INVALID_TAGNETINDEX - 1);
	}

	UE_CLOG(PrintNetIndiceAssignment, LogEvents, Display, TEXT("Assigning NetIndices to %d tags."), NetworkEventNodeIndex.Num() );

	NetworkEventNodeIndexHash = 0;

	for (FEventNetIndex i = 0; i < NetworkEventNodeIndex.Num(); i++)
	{
		if (NetworkEventNodeIndex[i].IsValid())
		{
			NetworkEventNodeIndex[i]->NetIndex = i;

			NetworkEventNodeIndexHash = FCrc::StrCrc32(*NetworkEventNodeIndex[i]->GetCompleteTagString().ToLower(), NetworkEventNodeIndexHash);

			UE_CLOG(PrintNetIndiceAssignment, LogEvents, Display, TEXT("Assigning NetIndex (%d) to Tag (%s)"), i, *NetworkEventNodeIndex[i]->GetCompleteTag().ToString());
		}
		else
		{
			UE_LOG(LogEvents, Warning, TEXT("TagNode Indice %d is invalid!"), i);
		}
	}

	UE_LOG(LogEvents, Log, TEXT("NetworkEventNodeIndexHash is %x"), NetworkEventNodeIndexHash);
}

FName UEventsManager::GetTagNameFromNetIndex(FEventNetIndex Index) const
{
	if (Index >= NetworkEventNodeIndex.Num())
	{
		// Ensure Index is the invalid index. If its higher than that, then something is wrong.
		ensureMsgf(Index == InvalidTagNetIndex, TEXT("Received invalid tag net index %d! Tag index is out of sync on client!"), Index);
		return NAME_None;
	}
	return NetworkEventNodeIndex[Index]->GetCompleteTagName();
}

FEventNetIndex UEventsManager::GetNetIndexFromTag(const FEventInfo &InTag) const
{
	TSharedPtr<FEventNode> EventNode = FindTagNode(InTag);

	if (EventNode.IsValid())
	{
		return EventNode->GetNetIndex();
	}

	return InvalidTagNetIndex;
}

bool UEventsManager::ShouldImportTagsFromINI() const
{
	UEventsSettings* MutableDefault = GetMutableDefault<UEventsSettings>();

	// Deprecated path
	bool ImportFromINI = false;
	if (GConfig->GetBool(TEXT("Events"), TEXT("ImportTagsFromConfig"), ImportFromINI, GEngineIni))
	{
		if (ImportFromINI)
		{
			MutableDefault->ImportTagsFromConfig = ImportFromINI;
			UE_LOG(LogEvents, Log, TEXT("ImportTagsFromConfig is in a deprecated location, open and save Event settings to fix"));
		}
		return ImportFromINI;
	}

	return MutableDefault->ImportTagsFromConfig;
}

void UEventsManager::GetRestrictedTagConfigFiles(TArray<FString>& RestrictedConfigFiles) const
{
	UEventsSettings* MutableDefault = GetMutableDefault<UEventsSettings>();

	if (MutableDefault)
	{
		for (const FEventRestrictedConfigInfo& Config : MutableDefault->RestrictedConfigFiles)
		{
			RestrictedConfigFiles.Add(FString::Printf(TEXT("%sTags/%s"), *FPaths::SourceConfigDir(), *Config.RestrictedConfigName));
		}
	}
}

void UEventsManager::GetRestrictedTagSources(TArray<const FEventSource*>& Sources) const
{
	UEventsSettings* MutableDefault = GetMutableDefault<UEventsSettings>();

	if (MutableDefault)
	{
		for (const FEventRestrictedConfigInfo& Config : MutableDefault->RestrictedConfigFiles)
		{
			const FEventSource* Source = FindTagSource(*Config.RestrictedConfigName);
			if (Source)
			{
				Sources.Add(Source);
			}
		}
	}
}

void UEventsManager::GetOwnersForTagSource(const FString& SourceName, TArray<FString>& OutOwners) const
{
	UEventsSettings* MutableDefault = GetMutableDefault<UEventsSettings>();

	if (MutableDefault)
	{
		for (const FEventRestrictedConfigInfo& Config : MutableDefault->RestrictedConfigFiles)
		{
			if (Config.RestrictedConfigName.Equals(SourceName))
			{
				OutOwners = Config.Owners;
				return;
			}
		}
	}
}

void UEventsManager::EventContainerLoaded(FEventContainer& Container, FProperty* SerializingProperty) const
{
	RedirectTagsForContainer(Container, SerializingProperty);

	if (OnEventLoadedDelegate.IsBound())
	{
		for (const FEventInfo& Tag : Container)
		{
			OnEventLoadedDelegate.Broadcast(Tag);
		}
	}
}

void UEventsManager::SingleEventLoaded(FEventInfo& Tag, FProperty* SerializingProperty) const
{
	RedirectSingleEvent(Tag, SerializingProperty);

	OnEventLoadedDelegate.Broadcast(Tag);
}

void UEventsManager::RedirectTagsForContainer(FEventContainer& Container, FProperty* SerializingProperty) const
{
	TSet<FName> NamesToRemove;
	TSet<const FEventInfo*> TagsToAdd;

	// First populate the NamesToRemove and TagsToAdd sets by finding tags in the container that have redirects
	for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
	{
		const FName TagName = TagIt->GetTagName();
		const FEventInfo* NewTag = TagRedirects.Find(TagName);
		if (NewTag)
		{
			NamesToRemove.Add(TagName);
			if (NewTag->IsValid())
			{
				TagsToAdd.Add(NewTag);
			}
		}
#if WITH_EDITOR
		else if (SerializingProperty)
		{
			// Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds
			FEventInfo OldTag = RequestEvent(TagName, false);
			if (!OldTag.IsValid() && ShouldWarnOnInvalidTags())
			{
				FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
				UObject* LoadingObject = LoadContext ? LoadContext->SerializedObject : nullptr;
				UE_LOG(LogEvents, Warning, TEXT("Invalid Event %s found while loading %s in property %s."), *TagName.ToString(), *GetPathNameSafe(LoadingObject), *GetPathNameSafe(SerializingProperty));
			}
		}
#endif
	}

	// Remove all tags from the NamesToRemove set
	for (FName RemoveName : NamesToRemove)
	{
		FEventInfo OldTag = RequestEvent(RemoveName, false);
		if (OldTag.IsValid())
		{
			Container.RemoveTag(OldTag);
		}
		else
		{
			Container.RemoveTagByExplicitName(RemoveName);
		}
	}

	// Add all tags from the TagsToAdd set
	for (const FEventInfo* AddTag : TagsToAdd)
	{
		check(AddTag);
		Container.AddTag(*AddTag);
	}
}

void UEventsManager::RedirectSingleEvent(FEventInfo& Tag, FProperty* SerializingProperty) const
{
	const FName TagName = Tag.GetTagName();
	const FEventInfo* NewTag = TagRedirects.Find(TagName);
	if (NewTag)
	{
		if (NewTag->IsValid())
		{
			Tag = *NewTag;
		}
	}
#if WITH_EDITOR
	else if (TagName != NAME_None && SerializingProperty)
	{
		// Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds
		FEventInfo OldTag = RequestEvent(TagName, false);
		if (!OldTag.IsValid() && ShouldWarnOnInvalidTags())
		{
			FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
			UObject* LoadingObject = LoadContext ? LoadContext->SerializedObject : nullptr;
			UE_LOG(LogEvents, Warning, TEXT("Invalid Event %s found while loading %s in property %s."), *TagName.ToString(), *GetPathNameSafe(LoadingObject), *GetPathNameSafe(SerializingProperty));
		}
	}
#endif
}

bool UEventsManager::ImportSingleEvent(FEventInfo& Tag, FName ImportedTagName) const
{
	bool bRetVal = false;
	if (const FEventInfo* RedirectedTag = TagRedirects.Find(ImportedTagName))
	{
		Tag = *RedirectedTag;
		bRetVal = true;
	}
	else if (ValidateTagCreation(ImportedTagName))
	{
		// The tag name is valid
		Tag.TagName = ImportedTagName;
		bRetVal = true;
	}

	if (bRetVal)
	{
		OnEventLoadedDelegate.Broadcast(Tag);
	}
	else
	{
	// No valid tag established in this attempt
	Tag.TagName = NAME_None;
	}

	return bRetVal;
}

void UEventsManager::InitializeManager()
{
	check(!SingletonManager);
	SCOPED_BOOT_TIMING("UEventsManager::InitializeManager");
	SCOPE_LOG_TIME_IN_SECONDS(TEXT("UEventsManager::InitializeManager"), nullptr);

	SingletonManager = NewObject<UEventsManager>(GetTransientPackage(), NAME_None);
	SingletonManager->AddToRoot();

	UEventsSettings* MutableDefault = nullptr;
	{
		SCOPE_LOG_EventS(TEXT("UEventsManager::InitializeManager: Load settings"));
		MutableDefault = GetMutableDefault<UEventsSettings>();
	}

	{
		SCOPE_LOG_EventS(TEXT("UEventsManager::InitializeManager: Load deprecated"));

		TArray<FString> EventTablePaths;
		GConfig->GetArray(TEXT("Events"), TEXT("+EventTableList"), EventTablePaths, GEngineIni);

	// Report deprecation
		if (EventTablePaths.Num() > 0)
	{
		UE_LOG(LogEvents, Log, TEXT("EventTableList is in a deprecated location, open and save Event settings to fix"));
			for (const FString& DataTable : EventTablePaths)
		{
			MutableDefault->EventTableList.AddUnique(DataTable);
		}
	}
	}

	SingletonManager->LoadEventTables(true);
	SingletonManager->ConstructEventTree();

	// Bind to end of engine init to be done adding native tags
	FCoreDelegates::OnPostEngineInit.AddUObject(SingletonManager, &UEventsManager::DoneAddingNativeTags);
}

void UEventsManager::PopulateTreeFromDataTable(class UDataTable* InTable)
{
	checkf(GameplayRootTag.IsValid(), TEXT("ConstructEventTree() must be called before PopulateTreeFromDataTable()"));
	static const FString ContextString(TEXT("UEventsManager::PopulateTreeFromDataTable"));
	
	TArray<FEventTableRow*> TagTableRows;
	InTable->GetAllRows<FEventTableRow>(ContextString, TagTableRows);

	FName SourceName = InTable->GetOutermost()->GetFName();

	FEventSource* FoundSource = FindOrAddTagSource(SourceName, EEventSourceType::DataTable);

	for (const FEventTableRow* TagRow : TagTableRows)
	{
		if (TagRow)
		{
			AddTagTableRow(*TagRow, SourceName);
		}
	}
}

void UEventsManager::AddTagTableRow(const FEventTableRow& TagRow, FName SourceName, bool bIsRestrictedTag)
{
	TSharedPtr<FEventNode> CurNode = GameplayRootTag;
	TArray<TSharedPtr<FEventNode>> AncestorNodes;
	bool bAllowNonRestrictedChildren = true;

	const FRestrictedEventTableRow* RestrictedTagRow = static_cast<const FRestrictedEventTableRow*>(&TagRow);
	if (bIsRestrictedTag && RestrictedTagRow)
	{
		bAllowNonRestrictedChildren = RestrictedTagRow->bAllowNonRestrictedChildren;
	}

	// Split the tag text on the "." delimiter to establish tag depth and then insert each tag into the gameplay tag tree
	// We try to avoid as many FString->FName conversions as possible as they are slow
	FName OriginalTagName = TagRow.Tag;
	FString FullTagString = OriginalTagName.ToString();

#if WITH_EDITOR
	{
		// In editor builds, validate string
		// These must get fixed up cooking to work properly
		FText ErrorText;
		FString FixedString;

		if (!IsValidEventString(FullTagString, &ErrorText, &FixedString))
		{
			if (FixedString.IsEmpty())
			{
				// No way to fix it
				UE_LOG(LogEvents, Error, TEXT("Invalid tag %s from source %s: %s!"), *FullTagString, *SourceName.ToString(), *ErrorText.ToString());
				return;
			}
			else
			{
				UE_LOG(LogEvents, Error, TEXT("Invalid tag %s from source %s: %s! Replacing with %s, you may need to modify InvalidTagCharacters"), *FullTagString, *SourceName.ToString(), *ErrorText.ToString(), *FixedString);
				FullTagString = FixedString;
				OriginalTagName = FName(*FixedString);
			}
		}
	}
#endif

	TArray<FString> SubTags;
	FullTagString.ParseIntoArray(SubTags, TEXT("."), true);

	// We will build this back up as we go
	FullTagString.Reset();

	int32 NumSubTags = SubTags.Num();
	bool bHasSeenConflict = false;

	for (int32 SubTagIdx = 0; SubTagIdx < NumSubTags; ++SubTagIdx)
	{
		bool bIsExplicitTag = (SubTagIdx == (NumSubTags - 1));
		FName ShortTagName = *SubTags[SubTagIdx];
		FName FullTagName;

		if (bIsExplicitTag)
		{
			// We already know the final name
			FullTagName = OriginalTagName;
		}
		else if (SubTagIdx == 0)
		{
			// Full tag is the same as short tag, and start building full tag string
			FullTagName = ShortTagName;
			FullTagString = SubTags[SubTagIdx];
		}
		else
		{
			// Add .Tag and use that as full tag
			FullTagString += TEXT(".");
			FullTagString += SubTags[SubTagIdx];

			FullTagName = FName(*FullTagString);
		}
			
		TArray< TSharedPtr<FEventNode> >& ChildTags = CurNode.Get()->GetChildTagNodes();
		int32 InsertionIdx = InsertTagIntoNodeArray(ShortTagName, FullTagName, CurNode, ChildTags, SourceName, TagRow.DevComment, bIsExplicitTag, bIsRestrictedTag, bAllowNonRestrictedChildren);

		CurNode = ChildTags[InsertionIdx];

		// Tag conflicts only affect the editor so we don't look for them in the game
#if WITH_EDITORONLY_DATA
		if (bIsRestrictedTag)
		{
			CurNode->bAncestorHasConflict = bHasSeenConflict;

			// If the sources don't match and the tag is explicit and we should've added the tag explicitly here, we have a conflict
			if (CurNode->SourceName != SourceName && (CurNode->bIsExplicitTag && bIsExplicitTag))
			{
				// mark all ancestors as having a bad descendant
				for (TSharedPtr<FEventNode> CurAncestorNode : AncestorNodes)
				{
					CurAncestorNode->bDescendantHasConflict = true;
				}

				// mark the current tag as having a conflict
				CurNode->bNodeHasConflict = true;

				// append source names
				FString CombinedSources = CurNode->SourceName.ToString();
				CombinedSources.Append(TEXT(" and "));
				CombinedSources.Append(SourceName.ToString());
				CurNode->SourceName = FName(*CombinedSources);

				// mark all current descendants as having a bad ancestor
				MarkChildrenOfNodeConflict(CurNode);
			}

			// mark any children we add later in this function as having a bad ancestor
			if (CurNode->bNodeHasConflict)
			{
				bHasSeenConflict = true;
			}

			AncestorNodes.Add(CurNode);
		}
#endif
	}
}

void UEventsManager::MarkChildrenOfNodeConflict(TSharedPtr<FEventNode> CurNode)
{
#if WITH_EDITORONLY_DATA
	TArray< TSharedPtr<FEventNode> >& ChildTags = CurNode.Get()->GetChildTagNodes();
	for (TSharedPtr<FEventNode> ChildNode : ChildTags)
	{
		ChildNode->bAncestorHasConflict = true;
		MarkChildrenOfNodeConflict(ChildNode);
	}
#endif
}

UEventsManager::~UEventsManager()
{
	DestroyEventTree();
	SingletonManager = nullptr;
}

void UEventsManager::DestroyEventTree()
{
	if (GameplayRootTag.IsValid())
	{
		GameplayRootTag->ResetNode();
		GameplayRootTag.Reset();
		EventNodeMap.Reset();
	}
	RestrictedEventSourceNames.Reset();
}

bool UEventsManager::IsNativelyAddedTag(FEventInfo Tag) const
{
	return NativeTagsToAdd.Contains(Tag.GetTagName());
}

int32 UEventsManager::InsertTagIntoNodeArray(FName Tag, FName FullTag, TSharedPtr<FEventNode> ParentNode, TArray< TSharedPtr<FEventNode> >& NodeArray, FName SourceName, const FString& DevComment, bool bIsExplicitTag, bool bIsRestrictedTag, bool bAllowNonRestrictedChildren)
{
	int32 FoundNodeIdx = INDEX_NONE;
	int32 WhereToInsert = INDEX_NONE;

	// See if the tag is already in the array
	for (int32 CurIdx = 0; CurIdx < NodeArray.Num(); ++CurIdx)
	{
		FEventNode* CurrNode = NodeArray[CurIdx].Get();
		if (CurrNode)
		{
			FName SimpleTagName = CurrNode->GetSimpleTagName();
			if (SimpleTagName == Tag)
			{
				FoundNodeIdx = CurIdx;
#if WITH_EDITORONLY_DATA
				// If we are explicitly adding this tag then overwrite the existing children restrictions with whatever is in the ini
				// If we restrict children in the input data, make sure we restrict them in the existing node. This applies to explicit and implicitly defined nodes
				if (bAllowNonRestrictedChildren == false || bIsExplicitTag)
		{
					// check if the tag is explicitly being created in more than one place.
					if (CurrNode->bIsExplicitTag && bIsExplicitTag)
			{
						// restricted tags always get added first
						// 
						// There are two possibilities if we're adding a restricted tag. 
						// If the existing tag is non-restricted the restricted tag should take precedence. This may invalidate some child tags of the existing tag.
						// If the existing tag is restricted we have a conflict. This is explicitly not allowed.
						if (bIsRestrictedTag)
						{
							
						}
					}
					CurrNode->bAllowNonRestrictedChildren = bAllowNonRestrictedChildren;
					CurrNode->bIsExplicitTag = CurrNode->bIsExplicitTag || bIsExplicitTag;
				}
#endif				
				break;
			}
			else if (Tag.LexicalLess(SimpleTagName) && WhereToInsert == INDEX_NONE)
			{
				// Insert new node before this
				WhereToInsert = CurIdx;
			}
		}
	}

	if (FoundNodeIdx == INDEX_NONE)
	{
		if (WhereToInsert == INDEX_NONE)
		{
			// Insert at end
			WhereToInsert = NodeArray.Num();
		}

		// Don't add the root node as parent
		TSharedPtr<FEventNode> TagNode = MakeShareable(new FEventNode(Tag, FullTag, ParentNode != GameplayRootTag ? ParentNode : nullptr, bIsExplicitTag, bIsRestrictedTag, bAllowNonRestrictedChildren));

		// Add at the sorted location
		FoundNodeIdx = NodeArray.Insert(TagNode, WhereToInsert);

		FEventInfo Event = TagNode->GetCompleteTag();

		// These should always match
		ensure(Event.GetTagName() == FullTag);

		{
#if WITH_EDITOR
			// This critical section is to handle an editor-only issue where tag requests come from another thread when async loading from a background thread in FEventContainer::Serialize.
			// This function is not generically threadsafe.
			FScopeLock Lock(&EventMapCritical);
#endif
			EventNodeMap.Add(Event, TagNode);
		}
	}

#if WITH_EDITOR
	static FName NativeSourceName = FEventSource::GetNativeName();

	// Set/update editor only data
	if (NodeArray[FoundNodeIdx]->SourceName.IsNone() && !SourceName.IsNone())
	{
		NodeArray[FoundNodeIdx]->SourceName = SourceName;
	}
	else if (SourceName == NativeSourceName)
	{
		// Native overrides other types
		NodeArray[FoundNodeIdx]->SourceName = SourceName;
	}

	if (NodeArray[FoundNodeIdx]->DevComment.IsEmpty() && !DevComment.IsEmpty())
	{
		NodeArray[FoundNodeIdx]->DevComment = DevComment;
	}
#endif

	return FoundNodeIdx;
}

void UEventsManager::PrintReplicationIndices()
{

	UE_LOG(LogEvents, Display, TEXT("::PrintReplicationIndices (TOTAL %d"), EventNodeMap.Num());

	for (auto It : EventNodeMap)
	{
		FEventInfo Tag = It.Key;
		TSharedPtr<FEventNode> Node = It.Value;

		UE_LOG(LogEvents, Display, TEXT("Tag %s NetIndex: %d"), *Tag.ToString(), Node->GetNetIndex());
	}

}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UEventsManager::PrintReplicationFrequencyReport()
{
	UE_LOG(LogEvents, Warning, TEXT("================================="));
	UE_LOG(LogEvents, Warning, TEXT("Gameplay Tags Replication Report"));

	UE_LOG(LogEvents, Warning, TEXT("\nTags replicated solo:"));
	ReplicationCountMap_SingleTags.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap_SingleTags)
	{
		UE_LOG(LogEvents, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}
	
	// ---------------------------------------

	UE_LOG(LogEvents, Warning, TEXT("\nTags replicated in containers:"));
	ReplicationCountMap_Containers.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap_Containers)
	{
		UE_LOG(LogEvents, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}

	// ---------------------------------------

	UE_LOG(LogEvents, Warning, TEXT("\nAll Tags replicated:"));
	ReplicationCountMap.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap)
	{
		UE_LOG(LogEvents, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}

	TMap<int32, int32> SavingsMap;
	int32 BaselineCost = 0;
	for (int32 Bits=1; Bits < NetIndexTrueBitNum; ++Bits)
	{
		int32 TotalSavings = 0;
		BaselineCost = 0;

		FEventNetIndex ExpectedNetIndex=0;
		for (auto& It : ReplicationCountMap)
		{
			int32 ExpectedCostBits = 0;
			bool FirstSeg = ExpectedNetIndex < FMath::Pow(2, Bits);
			if (FirstSeg)
			{
				// This would fit in the first Bits segment
				ExpectedCostBits = Bits+1;
			}
			else
			{
				// Would go in the second segment, so we pay the +1 cost
				ExpectedCostBits = NetIndexTrueBitNum+1;
			}

			int32 Savings = (NetIndexTrueBitNum - ExpectedCostBits) * It.Value;
			BaselineCost += NetIndexTrueBitNum * It.Value;

			//UE_LOG(LogEvents, Warning, TEXT("[Bits: %d] Tag %s would save %d bits"), Bits, *It.Key.ToString(), Savings);
			ExpectedNetIndex++;
			TotalSavings += Savings;
		}

		SavingsMap.FindOrAdd(Bits) = TotalSavings;
	}

	SavingsMap.ValueSort(TGreater<int32>());
	int32 BestBits = 0;
	for (auto& It : SavingsMap)
	{
		if (BestBits == 0)
		{
			BestBits = It.Key;
		}

		UE_LOG(LogEvents, Warning, TEXT("%d bits would save %d (%.2f)"), It.Key, It.Value, (float)It.Value / (float)BaselineCost);
	}

	UE_LOG(LogEvents, Warning, TEXT("\nSuggested config:"));

	// Write out a nice copy pastable config
	int32 Count=0;
	for (auto& It : ReplicationCountMap)
	{
		UE_LOG(LogEvents, Warning, TEXT("+CommonlyReplicatedTags=%s"), *It.Key.ToString());

		if (Count == FMath::Pow(2, BestBits))
		{
			// Print a blank line out, indicating tags after this are not necessary but still may be useful if the user wants to manually edit the list.
			UE_LOG(LogEvents, Warning, TEXT(""));
		}

		if (Count++ >= FMath::Pow(2, BestBits+1))
		{
			break;
		}
	}

	UE_LOG(LogEvents, Warning, TEXT("NetIndexFirstBitSegment=%d"), BestBits);

	UE_LOG(LogEvents, Warning, TEXT("================================="));
}

void UEventsManager::NotifyTagReplicated(FEventInfo Tag, bool WasInContainer)
{
	ReplicationCountMap.FindOrAdd(Tag)++;

	if (WasInContainer)
	{
		ReplicationCountMap_Containers.FindOrAdd(Tag)++;
	}
	else
	{
		ReplicationCountMap_SingleTags.FindOrAdd(Tag)++;
	}
	
}
#endif

#if WITH_EDITOR

static void RecursiveRootTagSearch(const FString& InFilterString, const TArray<TSharedPtr<FEventNode> >& GameplayRootTags, TArray< TSharedPtr<FEventNode> >& OutTagArray)
{
	FString CurrentFilter, RestOfFilter;
	if (!InFilterString.Split(TEXT("."), &CurrentFilter, &RestOfFilter))
	{
		CurrentFilter = InFilterString;
	}

	for (int32 iTag = 0; iTag < GameplayRootTags.Num(); ++iTag)
	{
		FString RootTagName = GameplayRootTags[iTag].Get()->GetSimpleTagName().ToString();

		if (RootTagName.Equals(CurrentFilter) == true)
		{
			if (RestOfFilter.IsEmpty())
			{
				// We've reached the end of the filter, add tags
				OutTagArray.Add(GameplayRootTags[iTag]);
			}
			else
			{
				// Recurse into our children
				RecursiveRootTagSearch(RestOfFilter, GameplayRootTags[iTag]->GetChildTagNodes(), OutTagArray);
			}
		}		
	}
}

void UEventsManager::GetFilteredGameplayRootTags(const FString& InFilterString, TArray< TSharedPtr<FEventNode> >& OutTagArray) const
{
	TArray<FString> PreRemappedFilters;
	TArray<FString> Filters;
	TArray<TSharedPtr<FEventNode>>& GameplayRootTags = GameplayRootTag->GetChildTagNodes();

	OutTagArray.Empty();
	if( InFilterString.ParseIntoArray( PreRemappedFilters, TEXT( "," ), true ) > 0 )
	{
		const UEventsSettings* CDO = GetDefault<UEventsSettings>();
		for (FString& Str : PreRemappedFilters)
		{
			bool Remapped = false;
			for (const FEventCategoryRemap& RemapInfo : CDO->CategoryRemapping)
			{
				if (RemapInfo.BaseCategory == Str)
				{
					Remapped = true;
					Filters.Append(RemapInfo.RemapCategories);
				}
			}
			if (Remapped == false)
			{
				Filters.Add(Str);
			}
		}		

		// Check all filters in the list
		for (int32 iFilter = 0; iFilter < Filters.Num(); ++iFilter)
		{
			RecursiveRootTagSearch(Filters[iFilter], GameplayRootTags, OutTagArray);
		}

		if (OutTagArray.Num() == 0)
		{
			// We had filters but nothing matched. Ignore the filters.
			// This makes sense to do with engine level filters that games can optionally specify/override.
			// We never want to impose tag structure on projects, but still give them the ability to do so for their project.
			OutTagArray = GameplayRootTags;
		}

	}
	else
	{
		// No Filters just return them all
		OutTagArray = GameplayRootTags;
	}
}

FString UEventsManager::GetCategoriesMetaFromPropertyHandle(TSharedPtr<IPropertyHandle> PropertyHandle) const
{
	// Global delegate override. Useful for parent structs that want to override tag categories based on their data (e.g. not static property meta data)
	FString DelegateOverrideString;
	OnGetCategoriesMetaFromPropertyHandle.Broadcast(PropertyHandle, DelegateOverrideString);
	if (DelegateOverrideString.IsEmpty() == false)
	{
		return DelegateOverrideString;
	}

	return StaticGetCategoriesMetaFromPropertyHandle(PropertyHandle);
}

FString UEventsManager::StaticGetCategoriesMetaFromPropertyHandle(TSharedPtr<class IPropertyHandle> PropertyHandle)
{
	FString Categories;

	auto GetFieldMetaData = ([&](FField* Field)
	{
		if (Field->HasMetaData(NAME_Categories))
		{
			Categories = Field->GetMetaData(NAME_Categories);
			return true;
		}

		return false;
	});

	auto GetMetaData = ([&](UField* Field)
	{
		if (Field->HasMetaData(NAME_Categories))
		{
			Categories = Field->GetMetaData(NAME_Categories);
			return true;
		}

		return false;
	});
	
	while(PropertyHandle.IsValid())
	{
		if (FProperty* Property = PropertyHandle->GetProperty())
		{
			/**
			 *	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories="GameplayCue"))
			 *	FEventBase GameplayCueTag;
			 */
			if (GetFieldMetaData(Property))
			{
				break;
			}

			/**
			 *	USTRUCT(meta=(Categories="EventKeyword"))
			 *	struct FGameplayEventKeywordTag : public FEventBase
			 */
			if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				if (GetMetaData(StructProperty->Struct))
				{
					break;
				}
			}

			/**	TArray<FGameplayEventKeywordTag> QualifierTagTestList; */
			if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				if (GetFieldMetaData(ArrayProperty->Inner))
				{
					break;
				}
			}
		}
		PropertyHandle = PropertyHandle->GetParentHandle();
	}
	
	return Categories;
}

FString UEventsManager::GetCategoriesMetaFromFunction(const UFunction* ThisFunction, FName ParamName /** = NAME_None */) const
{
	FString FilterString;
	if (ThisFunction)
	{
		// If a param name was specified, check it first for UPARAM metadata
		if (!ParamName.IsNone())
		{
			FProperty* ParamProp = FindFProperty<FProperty>(ThisFunction, ParamName);
			if (ParamProp)
			{
				FilterString = UEventsManager::Get().GetCategoriesMetaFromField(ParamProp);
			}
		}

		// No filter found so far, fall back to UFUNCTION-level
		if (FilterString.IsEmpty() && ThisFunction->HasMetaData(NAME_EventFilter))
		{
			FilterString = ThisFunction->GetMetaData(NAME_EventFilter);
		}
	}

	return FilterString;
}

void UEventsManager::GetAllTagsFromSource(FName TagSource, TArray< TSharedPtr<FEventNode> >& OutTagArray) const
{
	for (const TPair<FEventInfo, TSharedPtr<FEventNode>>& NodePair : EventNodeMap)
	{
		if (NodePair.Value->SourceName == TagSource)
		{
			OutTagArray.Add(NodePair.Value);
		}
	}
}

bool UEventsManager::IsDictionaryTag(FName TagName) const
{
	TSharedPtr<FEventNode> Node = FindTagNode(TagName);
	if (Node.IsValid() && Node->bIsExplicitTag)
	{
		return true;
	}

	return false;
}

bool UEventsManager::GetTagEditorData(FName TagName, FString& OutComment, FName& OutTagSource, bool& bOutIsTagExplicit, bool &bOutIsRestrictedTag, bool &bOutAllowNonRestrictedChildren) const
{
	TSharedPtr<FEventNode> Node = FindTagNode(TagName);
	if (Node.IsValid())
	{
		OutComment = Node->DevComment;
		OutTagSource = Node->SourceName;
		bOutIsTagExplicit = Node->bIsExplicitTag;
		bOutIsRestrictedTag = Node->bIsRestrictedTag;
		bOutAllowNonRestrictedChildren = Node->bAllowNonRestrictedChildren;
		return true;
	}
	return false;
}

void UEventsManager::EditorRefreshEventTree()
{
	DestroyEventTree();
	LoadEventTables(false);
	ConstructEventTree();
	OnEditorRefreshEventTree.Broadcast();
}

FEventContainer UEventsManager::RequestEventChildrenInDictionary(const FEventInfo& Event) const
{
	// Note this purposefully does not include the passed in Event in the container.
	FEventContainer TagContainer;

	TSharedPtr<FEventNode> EventNode = FindTagNode(Event);
	if (EventNode.IsValid())
	{
		AddChildrenTags(TagContainer, EventNode, true, true);
	}
	return TagContainer;
}

#if WITH_EDITORONLY_DATA
FEventContainer UEventsManager::RequestEventDirectDescendantsInDictionary(const FEventInfo& Event, EEventSelectionType SelectionType) const
{
	bool bIncludeRestrictedTags = (SelectionType == EEventSelectionType::RestrictedOnly || SelectionType == EEventSelectionType::All);
	bool bIncludeNonRestrictedTags = (SelectionType == EEventSelectionType::NonRestrictedOnly || SelectionType == EEventSelectionType::All);

	// Note this purposefully does not include the passed in Event in the container.
	FEventContainer TagContainer;

	TSharedPtr<FEventNode> EventNode = FindTagNode(Event);
	if (EventNode.IsValid())
	{
		TArray< TSharedPtr<FEventNode> >& ChildrenNodes = EventNode->GetChildTagNodes();
		int32 CurrArraySize = ChildrenNodes.Num();
		for (int32 Idx = 0; Idx < CurrArraySize; ++Idx)
		{
			TSharedPtr<FEventNode> ChildNode = ChildrenNodes[Idx];
			if (ChildNode.IsValid())
			{
				// if the tag isn't in the dictionary, add its children to the list
				if (ChildNode->SourceName == NAME_None)
				{
					TArray< TSharedPtr<FEventNode> >& GrandChildrenNodes = ChildNode->GetChildTagNodes();
					ChildrenNodes.Append(GrandChildrenNodes);
					CurrArraySize = ChildrenNodes.Num();
				}
				else
				{
					// this tag is in the dictionary so add it to the list
					if ((ChildNode->bIsRestrictedTag && bIncludeRestrictedTags) ||
						(!ChildNode->bIsRestrictedTag && bIncludeNonRestrictedTags))
					{
						TagContainer.AddTag(ChildNode->GetCompleteTag());
					}
				}
			}
		}
	}
	return TagContainer;
}
#endif // WITH_EDITORONLY_DATA

void UEventsManager::NotifyEventDoubleClickedEditor(FString TagName)
{
	FEventInfo Tag = RequestEvent(FName(*TagName), false);
	if(Tag.IsValid())
	{
		FSimpleMulticastDelegate Delegate;
		OnGatherEventDoubleClickedEditor.Broadcast(Tag, Delegate);
		Delegate.Broadcast();
	}
}

bool UEventsManager::ShowEventAsHyperLinkEditor(FString TagName)
{
	FEventInfo Tag = RequestEvent(FName(*TagName), false);
	if(Tag.IsValid())
	{
		FSimpleMulticastDelegate Delegate;
		OnGatherEventDoubleClickedEditor.Broadcast(Tag, Delegate);
		return Delegate.IsBound();
	}
	return false;
}

#endif // WITH_EDITOR

const FEventSource* UEventsManager::FindTagSource(FName TagSourceName) const
{
	for (const FEventSource& TagSource : TagSources)
	{
		if (TagSource.SourceName == TagSourceName)
		{
			return &TagSource;
		}
	}
	return nullptr;
}

FEventSource* UEventsManager::FindTagSource(FName TagSourceName)
{
	for (FEventSource& TagSource : TagSources)
	{
		if (TagSource.SourceName == TagSourceName)
		{
			return &TagSource;
		}
	}
	return nullptr;
}

void UEventsManager::FindTagSourcesWithType(EEventSourceType TagSourceType, TArray<const FEventSource*>& OutArray) const
{
	for (const FEventSource& TagSource : TagSources)
	{
		if (TagSource.SourceType == TagSourceType)
		{
			OutArray.Add(&TagSource);
		}
	}
}

FEventSource* UEventsManager::FindOrAddTagSource(FName TagSourceName, EEventSourceType SourceType)
{
	FEventSource* FoundSource = FindTagSource(TagSourceName);
	if (FoundSource)
	{
		if (SourceType == FoundSource->SourceType)
		{
			return FoundSource;
		}

		return nullptr;
	}

	// Need to make a new one

	FEventSource* NewSource = new(TagSources) FEventSource(TagSourceName, SourceType);

	if (SourceType == EEventSourceType::DefaultTagList)
	{
		NewSource->SourceTagList = GetMutableDefault<UEventsSettings>();
	}
	else if (SourceType == EEventSourceType::TagList)
	{
		NewSource->SourceTagList = NewObject<UEventsList>(this, TagSourceName, RF_Transient);
		NewSource->SourceTagList->ConfigFileName = FString::Printf(TEXT("%sTags/%s"), *FPaths::SourceConfigDir(), *TagSourceName.ToString());
		if (GUObjectArray.IsDisregardForGC(this))
		{
			NewSource->SourceTagList->AddToRoot();
		}
	}
	else if (SourceType == EEventSourceType::RestrictedTagList)
	{
		NewSource->SourceRestrictedTagList = NewObject<URestrictedEventsList>(this, TagSourceName, RF_Transient);
		NewSource->SourceRestrictedTagList->ConfigFileName = FString::Printf(TEXT("%sTags/%s"), *FPaths::SourceConfigDir(), *TagSourceName.ToString());
		if (GUObjectArray.IsDisregardForGC(this))
		{
			NewSource->SourceRestrictedTagList->AddToRoot();
		}
	}

	return NewSource;
}

DECLARE_CYCLE_STAT(TEXT("UEventsManager::RequestEvent"), STAT_UEventsManager_RequestEvent, STATGROUP_Events);

void UEventsManager::RequestEventContainer(const TArray<FString>& TagStrings, FEventContainer& OutTagsContainer, bool bErrorIfNotFound/*=true*/) const
{
	for (const FString& CurrentTagString : TagStrings)
	{
		FEventInfo RequestedTag = RequestEvent(FName(*(CurrentTagString.TrimStartAndEnd())), bErrorIfNotFound);
		if (RequestedTag.IsValid())
		{
			OutTagsContainer.AddTag(RequestedTag);
		}
	}
}

FEventInfo UEventsManager::RequestEvent(FName TagName, bool ErrorIfNotFound) const
{
	SCOPE_CYCLE_COUNTER(STAT_UEventsManager_RequestEvent);
#if WITH_EDITOR
	// This critical section is to handle and editor-only issue where tag requests come from another thread when async loading from a background thread in FEventContainer::Serialize.
	// This function is not generically threadsafe.
	FScopeLock Lock(&EventMapCritical);
#endif

	FEventInfo PossibleTag(TagName);

	if (EventNodeMap.Contains(PossibleTag))
	{
		return PossibleTag;
	}
	else if (ErrorIfNotFound)
	{
		static TSet<FName> MissingTagName;
		if (!MissingTagName.Contains(TagName))
		{
			ensureAlwaysMsgf(false, TEXT("Requested Tag %s was not found. Check tag data table."), *TagName.ToString());
			MissingTagName.Add(TagName);
		}
	}
	return FEventInfo();
}

bool UEventsManager::IsValidEventString(const FString& TagString, FText* OutError, FString* OutFixedString)
{
	bool bIsValid = true;
	FString FixedString = TagString;
	FText ErrorText;

	if (FixedString.IsEmpty())
	{
		ErrorText = LOCTEXT("EmptyStringError", "Tag is empty");
		bIsValid = false;
	}

	while (FixedString.StartsWith(TEXT("."), ESearchCase::CaseSensitive))
	{
		ErrorText = LOCTEXT("StartWithPeriod", "Tag starts with .");
		FixedString.RemoveAt(0);
		bIsValid = false;
	}

	while (FixedString.EndsWith(TEXT("."), ESearchCase::CaseSensitive))
	{
		ErrorText = LOCTEXT("EndWithPeriod", "Tag ends with .");
		FixedString.RemoveAt(FixedString.Len() - 1);
		bIsValid = false;
	}

	while (FixedString.StartsWith(TEXT(" "), ESearchCase::CaseSensitive))
	{
		ErrorText = LOCTEXT("StartWithSpace", "Tag starts with space");
		FixedString.RemoveAt(0);
		bIsValid = false;
	}

	while (FixedString.EndsWith(TEXT(" "), ESearchCase::CaseSensitive))
	{
		ErrorText = LOCTEXT("EndWithSpace", "Tag ends with space");
		FixedString.RemoveAt(FixedString.Len() - 1);
		bIsValid = false;
	}

	FText TagContext = LOCTEXT("EventContext", "Tag");
	if (!FName::IsValidXName(TagString, InvalidTagCharacters, &ErrorText, &TagContext))
	{
		for (TCHAR& TestChar : FixedString)
		{
			for (TCHAR BadChar : InvalidTagCharacters)
			{
				if (TestChar == BadChar)
				{
					TestChar = TEXT('_');
				}
			}
		}

		bIsValid = false;
	}

	if (OutError)
	{
		*OutError = ErrorText;
	}
	if (OutFixedString)
	{
		*OutFixedString = FixedString;
	}

	return bIsValid;
}

FEventInfo UEventsManager::FindEventFromPartialString_Slow(FString PartialString) const
{
#if WITH_EDITOR
	// This critical section is to handle and editor-only issue where tag requests come from another thread when async loading from a background thread in FEventContainer::Serialize.
	// This function is not generically threadsafe.
	FScopeLock Lock(&EventMapCritical);
#endif

	// Exact match first
	FEventInfo PossibleTag(*PartialString);
	if (EventNodeMap.Contains(PossibleTag))
	{
		return PossibleTag;
	}

	// Find shortest tag name that contains the match string
	FEventInfo FoundTag;
	FEventContainer AllTags;
	RequestAllEvents(AllTags, false);

	int32 BestMatchLength = MAX_int32;
	for (FEventInfo MatchTag : AllTags)
	{
		FString Str = MatchTag.ToString();
		if (Str.Contains(PartialString))
		{
			if (Str.Len() < BestMatchLength)
			{
				FoundTag = MatchTag;
				BestMatchLength = Str.Len();
			}
		}
	}
	
	return FoundTag;
}

FEventInfo UEventsManager::AddNativeEvent(FName TagName, const FString& TagDevComment)
{
	if (TagName.IsNone())
	{
		return FEventInfo();
	}

	// Unsafe to call after done adding
	if (ensure(!bDoneAddingNativeTags))
	{
		FEventInfo NewTag = FEventInfo(TagName);

		if (!NativeTagsToAdd.Contains(TagName))
		{
			NativeTagsToAdd.Add(TagName);
		}

		AddTagTableRow(FEventTableRow(TagName, TagDevComment), FEventSource::GetNativeName());

		return NewTag;
	}

	return FEventInfo();
}
void UEventsManager::CallOrRegister_OnDoneAddingNativeTagsDelegate(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (bDoneAddingNativeTags)
	{
		Delegate.Execute();
	}
	else
	{
		bool bAlreadyBound = Delegate.GetUObject() != nullptr ? OnDoneAddingNativeTagsDelegate().IsBoundToObject(Delegate.GetUObject()) : false;
		if (!bAlreadyBound)
		{
			OnDoneAddingNativeTagsDelegate().Add(Delegate);
		}
	}
}

FSimpleMulticastDelegate& UEventsManager::OnDoneAddingNativeTagsDelegate()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

FSimpleMulticastDelegate& UEventsManager::OnLastChanceToAddNativeTags()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

void UEventsManager::DoneAddingNativeTags()
{
	// Safe to call multiple times, only works the first time, must be called after the engine
	// is initialized (DoneAddingNativeTags is bound to PostEngineInit to cover anything that's skipped).
	if (GEngine && !bDoneAddingNativeTags)
	{
		UE_CLOG(EventS_VERBOSE, LogEvents, Display, TEXT("UEventsManager::DoneAddingNativeTags. DelegateIsBound: %d"), (int32)OnLastChanceToAddNativeTags().IsBound());
		OnLastChanceToAddNativeTags().Broadcast();
		bDoneAddingNativeTags = true;

		// We may add native tags that are needed for redirectors, so reconstruct the Event tree
		DestroyEventTree();
		ConstructEventTree();

		OnDoneAddingNativeTagsDelegate().Broadcast();
	}
}

FEventContainer UEventsManager::RequestEventParents(const FEventInfo& Event) const
{
	const FEventContainer* ParentTags = GetSingleTagContainer(Event);

	if (ParentTags)
	{
		return ParentTags->GetEventParents();
	}
	return FEventContainer();
}

void UEventsManager::RequestAllEvents(FEventContainer& TagContainer, bool OnlyIncludeDictionaryTags) const
{
	TArray<TSharedPtr<FEventNode>> ValueArray;
	EventNodeMap.GenerateValueArray(ValueArray);
	for (const TSharedPtr<FEventNode>& TagNode : ValueArray)
	{
#if WITH_EDITOR
		bool DictTag = IsDictionaryTag(TagNode->GetCompleteTagName());
#else
		bool DictTag = false;
#endif 
		if (!OnlyIncludeDictionaryTags || DictTag)
		{
			const FEventInfo* Tag = EventNodeMap.FindKey(TagNode);
			check(Tag);
			TagContainer.AddTagFast(*Tag);
		}
	}
}

FEventContainer UEventsManager::RequestEventChildren(const FEventInfo& Event) const
{
	FEventContainer TagContainer;
	// Note this purposefully does not include the passed in Event in the container.
	TSharedPtr<FEventNode> EventNode = FindTagNode(Event);
	if (EventNode.IsValid())
	{
		AddChildrenTags(TagContainer, EventNode, true, false);
	}
	return TagContainer;
}

FEventInfo UEventsManager::RequestEventDirectParent(const FEventInfo& Event) const
{
	TSharedPtr<FEventNode> EventNode = FindTagNode(Event);
	if (EventNode.IsValid())
	{
		TSharedPtr<FEventNode> Parent = EventNode->GetParentTagNode();
		if (Parent.IsValid())
		{
			return Parent->GetCompleteTag();
		}
	}
	return FEventInfo();
}

void UEventsManager::AddChildrenTags(FEventContainer& TagContainer, TSharedPtr<FEventNode> EventNode, bool RecurseAll, bool OnlyIncludeDictionaryTags) const
{
	if (EventNode.IsValid())
	{
		TArray< TSharedPtr<FEventNode> >& ChildrenNodes = EventNode->GetChildTagNodes();
		for (TSharedPtr<FEventNode> ChildNode : ChildrenNodes)
		{
			if (ChildNode.IsValid())
			{
				bool bShouldInclude = true;

#if WITH_EDITORONLY_DATA
				if (OnlyIncludeDictionaryTags && ChildNode->SourceName == NAME_None)
				{
					// Only have info to do this in editor builds
					bShouldInclude = false;
				}
#endif	
				if (bShouldInclude)
				{
					TagContainer.AddTag(ChildNode->GetCompleteTag());
				}

				if (RecurseAll)
				{
					AddChildrenTags(TagContainer, ChildNode, true, OnlyIncludeDictionaryTags);
				}
			}

		}
	}
}

void UEventsManager::SplitEventFName(const FEventInfo& Tag, TArray<FName>& OutNames) const
{
	TSharedPtr<FEventNode> CurNode = FindTagNode(Tag);
	while (CurNode.IsValid())
	{
		OutNames.Insert(CurNode->GetSimpleTagName(), 0);
		CurNode = CurNode->GetParentTagNode();
	}
}

int32 UEventsManager::EventsMatchDepth(const FEventInfo& EventOne, const FEventInfo& EventTwo) const
{
	TSet<FName> Tags1;
	TSet<FName> Tags2;

	TSharedPtr<FEventNode> TagNode = FindTagNode(EventOne);
	if (TagNode.IsValid())
	{
		GetAllParentNodeNames(Tags1, TagNode);
	}

	TagNode = FindTagNode(EventTwo);
	if (TagNode.IsValid())
	{
		GetAllParentNodeNames(Tags2, TagNode);
	}

	return Tags1.Intersect(Tags2).Num();
}

DECLARE_CYCLE_STAT(TEXT("UEventsManager::GetAllParentNodeNames"), STAT_UEventsManager_GetAllParentNodeNames, STATGROUP_Events);

void UEventsManager::GetAllParentNodeNames(TSet<FName>& NamesList, TSharedPtr<FEventNode> Event) const
{
	SCOPE_CYCLE_COUNTER(STAT_UEventsManager_GetAllParentNodeNames);

	NamesList.Add(Event->GetCompleteTagName());
	TSharedPtr<FEventNode> Parent = Event->GetParentTagNode();
	if (Parent.IsValid())
	{
		GetAllParentNodeNames(NamesList, Parent);
	}
}

DECLARE_CYCLE_STAT(TEXT("UEventsManager::ValidateTagCreation"), STAT_UEventsManager_ValidateTagCreation, STATGROUP_Events);

bool UEventsManager::ValidateTagCreation(FName TagName) const
{
	SCOPE_CYCLE_COUNTER(STAT_UEventsManager_ValidateTagCreation);

	return FindTagNode(TagName).IsValid();
}

FEventTableRow::FEventTableRow(FEventTableRow const& Other)
{
	*this = Other;
}

FEventTableRow& FEventTableRow::operator=(FEventTableRow const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}

	Tag = Other.Tag;
	DevComment = Other.DevComment;

	return *this;
}

bool FEventTableRow::operator==(FEventTableRow const& Other) const
{
	return (Tag == Other.Tag);
}

bool FEventTableRow::operator!=(FEventTableRow const& Other) const
{
	return (Tag != Other.Tag);
}

bool FEventTableRow::operator<(FEventTableRow const& Other) const
{
	return Tag.LexicalLess(Other.Tag);
}

FRestrictedEventTableRow::FRestrictedEventTableRow(FRestrictedEventTableRow const& Other)
{
	*this = Other;
}

FRestrictedEventTableRow& FRestrictedEventTableRow::operator=(FRestrictedEventTableRow const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}

	Super::operator=(Other);
	bAllowNonRestrictedChildren = Other.bAllowNonRestrictedChildren;

	return *this;
}

bool FRestrictedEventTableRow::operator==(FRestrictedEventTableRow const& Other) const
{
	if (bAllowNonRestrictedChildren != Other.bAllowNonRestrictedChildren)
	{
		return false;
	}

	if (Tag != Other.Tag)
	{
		return false;
	}

	return true;
}

bool FRestrictedEventTableRow::operator!=(FRestrictedEventTableRow const& Other) const
{
	if (bAllowNonRestrictedChildren == Other.bAllowNonRestrictedChildren)
	{
		return false;
	}

	if (Tag == Other.Tag)
	{
		return false;
	}

	return true;
}

FEventNode::FEventNode(FName InTag, FName InFullTag, TSharedPtr<FEventNode> InParentNode, bool InIsExplicitTag, bool InIsRestrictedTag, bool InAllowNonRestrictedChildren)
	: Tag(InTag)
	, ParentNode(InParentNode)
	, NetIndex(INVALID_TAGNETINDEX)
{
	// Manually construct the tag container as we want to bypass the safety checks
	CompleteTagWithParents.Events.Add(FEventInfo(InFullTag));

	FEventNode* RawParentNode = ParentNode.Get();
	if (RawParentNode && RawParentNode->GetSimpleTagName() != NAME_None)
	{
		// Our parent nodes are already constructed, and must have it's tag in Events[0]
		const FEventContainer ParentContainer = RawParentNode->GetSingleTagContainer();

		CompleteTagWithParents.ParentTags.Add(ParentContainer.Events[0]);
		CompleteTagWithParents.ParentTags.Append(ParentContainer.ParentTags);
	}
	
#if WITH_EDITORONLY_DATA
	bIsExplicitTag = InIsExplicitTag;
	bIsRestrictedTag = InIsRestrictedTag;
	bAllowNonRestrictedChildren = InAllowNonRestrictedChildren;

	bDescendantHasConflict = false;
	bNodeHasConflict = false;
	bAncestorHasConflict = false;
#endif 
}

void FEventNode::ResetNode()
{
	Tag = NAME_None;
	CompleteTagWithParents.Reset();
	NetIndex = INVALID_TAGNETINDEX;

	for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
	{
		ChildTags[ChildIdx]->ResetNode();
	}

	ChildTags.Empty();
	ParentNode.Reset();

#if WITH_EDITORONLY_DATA
	SourceName = NAME_None;
	DevComment = "";
	bIsExplicitTag = false;
	bIsRestrictedTag = false;
	bAllowNonRestrictedChildren = false;
	bDescendantHasConflict = false;
	bNodeHasConflict = false;
	bAncestorHasConflict = false;
#endif 
}

#undef LOCTEXT_NAMESPACE
