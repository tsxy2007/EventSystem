// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventsEditorModule.h"
#include "Misc/Paths.h"
#include "PropertyEditorModule.h"
#include "Factories/Factory.h"
#include "EdGraphUtilities.h"
#include "EventContainer.h"
#include "Engine/DataTable.h"
#include "EventsManager.h"
#include "AssetData.h"
#include "Misc/ConfigCacheIni.h"
#include "EventsGraphPanelPinFactory.h"
#include "EventsGraphPanelNodeFactory.h"
#include "EventContainerCustomization.h"
#include "EventQueryCustomization.h"
#include "EventCustomization.h"
#include "EventsSettings.h"
#include "EventsSettingsCustomization.h"
#include "EventsRuntimeModule.h"
#include "ISettingsModule.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "AssetRegistryModule.h"
#include "Editor.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "Stats/StatsMisc.h"
#include "EventReferenceHelperDetails.h"
#include "UObject/UObjectHash.h"
#include "EventReferenceHelperDetails.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "EventEditor"


class FEventsEditorModule
	: public IEventsEditorModule
{
public:

	// IModuleInterface

	virtual void StartupModule() override
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FEventsEditorModule::OnPostEngineInit);
	}
	
	void OnPostEngineInit()
	{
		// Register the details customizer
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.RegisterCustomPropertyTypeLayout("EventContainer", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEventContainerCustomization::MakeInstance));
			PropertyModule.RegisterCustomPropertyTypeLayout("Event", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEventCustomizationPublic::MakeInstance));
			PropertyModule.RegisterCustomPropertyTypeLayout("EventQuery", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEventQueryCustomization::MakeInstance));

			PropertyModule.RegisterCustomClassLayout(UEventsSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FEventsSettingsCustomization::MakeInstance));

			PropertyModule.RegisterCustomPropertyTypeLayout("EventReferenceHelper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEventReferenceHelperDetails::MakeInstance));
			PropertyModule.RegisterCustomPropertyTypeLayout("EventCreationWidgetHelper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEventCreationWidgetHelperDetails::MakeInstance));

			PropertyModule.NotifyCustomizationModuleChanged();
		}

		TSharedPtr<FEventsGraphPanelPinFactory> EventsGraphPanelPinFactory = MakeShareable(new FEventsGraphPanelPinFactory());
		FEdGraphUtilities::RegisterVisualPinFactory(EventsGraphPanelPinFactory);

		TSharedPtr<FEventsGraphPanelNodeFactory> EventsGraphPanelNodeFactory = MakeShareable(new FEventsGraphPanelNodeFactory());
		FEdGraphUtilities::RegisterVisualNodeFactory(EventsGraphPanelNodeFactory);

		// These objects are not UDeveloperSettings because we only want them to register if the editor plugin is enabled

		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Project", "Events",
				LOCTEXT("EventSettingsName", "Events"),
				LOCTEXT("EventSettingsNameDesc", "Event Settings"),
				GetMutableDefault<UEventsSettings>()
			);
		}

		EventPackageName = FEventInfo::StaticStruct()->GetOutermost()->GetFName();
		EventStructName = FEventInfo::StaticStruct()->GetFName();

		// Hook into notifications for object re-imports so that the gameplay tag tree can be reconstructed if the table changes
		if (GIsEditor)
		{
			GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.AddRaw(this, &FEventsEditorModule::OnObjectReimported);
			FEditorDelegates::OnEditAssetIdentifiers.AddRaw(this, &FEventsEditorModule::OnEditEvent);
			IEventsModule::OnTagSettingsChanged.AddRaw(this, &FEventsEditorModule::OnEditorSettingsChanged);
			UPackage::PackageSavedEvent.AddRaw(this, &FEventsEditorModule::OnPackageSaved);
		}
	}

	void OnObjectReimported(UFactory* ImportFactory, UObject* InObject)
	{
		UEventsManager& Manager = UEventsManager::Get();

		// Re-construct the gameplay tag tree if the base table is re-imported
		if (GIsEditor && !IsRunningCommandlet() && InObject && Manager.EventTables.Contains(Cast<UDataTable>(InObject)))
		{
			Manager.EditorRefreshEventTree();
		}
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::OnPostEngineInit.RemoveAll(this);

		// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
		// we call this function before unloading the module.

		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Project", "Events");
			SettingsModule->UnregisterSettings("Project", "Project", "Events Developer");
		}

		if (GEditor)
		{
			GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.RemoveAll(this);
		}
		FEditorDelegates::OnEditAssetIdentifiers.RemoveAll(this);
		IEventsModule::OnTagSettingsChanged.RemoveAll(this);
		UPackage::PackageSavedEvent.RemoveAll(this);
	}

	void OnEditorSettingsChanged()
	{
		// This is needed to make networking changes as well, so let's always refresh
		UEventsManager::Get().EditorRefreshEventTree();

		// Attempt to migrate the settings if needed
		MigrateSettings();
	}

	void OnPackageSaved(const FString& PackageFileName, UObject* PackageObj)
	{
		if (GIsEditor && !IsRunningCommandlet())
		{
			UEventsManager& Manager = UEventsManager::Get();

			bool bRefreshEventTree = false;

			TArray<UObject*> Objects;
			const bool bIncludeNestedObjects = false;
			GetObjectsWithOuter(PackageObj, Objects, bIncludeNestedObjects);
			for (UObject* Entry : Objects)
			{
				if (UDataTable* DataTable = Cast<UDataTable>(Entry))
				{
					if (Manager.EventTables.Contains(DataTable))
					{
						bRefreshEventTree = true;
						break;
					}
				}
			}

			// Re-construct the gameplay tag tree if a data table is saved (presumably with modifications).
			if (bRefreshEventTree)
			{
				Manager.EditorRefreshEventTree();
			}
		}
	}

	void OnEditEvent(TArray<FAssetIdentifier> AssetIdentifierList)
	{
		// If any of these are gameplay tags, open up tag viewer
		for (FAssetIdentifier Identifier : AssetIdentifierList)
		{
			if (Identifier.IsValue() && Identifier.PackageName == EventPackageName && Identifier.ObjectName == EventStructName)
			{
				if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
				{
					// TODO: Select tag maybe?
					SettingsModule->ShowViewer("Project", "Project", "Events");
				}
				return;
			}
		}
	}

	void ShowNotification(const FText& TextToDisplay, float TimeToDisplay, bool bLogError = false)
	{
		FNotificationInfo Info(TextToDisplay);
		Info.ExpireDuration = TimeToDisplay;

		FSlateNotificationManager::Get().AddNotification(Info);

		// Also log if error
		if (bLogError)
		{
			UE_LOG(LogEvents, Error, TEXT("%s"), *TextToDisplay.ToString())
		}
	}

	void MigrateSettings()
	{
		UEventsManager& Manager = UEventsManager::Get();

		FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());

		UEventsSettings* Settings = GetMutableDefault<UEventsSettings>();
		
		// The refresh has already set the in memory version of this to be correct, just need to save it out now
		if (!GConfig->GetSectionPrivate(TEXT("Events"), false, true, DefaultEnginePath))
		{
			// Already migrated or no data
			return;
		}

		// Check out defaultengine
		EventsUpdateSourceControl(DefaultEnginePath);

		// Delete gameplay tags section entirely. This modifies the disk version
		GConfig->EmptySection(TEXT("Events"), DefaultEnginePath);

		FConfigSection* PackageRedirects = GConfig->GetSectionPrivate(TEXT("/Script/Engine.Engine"), false, false, DefaultEnginePath);

		if (PackageRedirects)
		{
			for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
			{
				if (It.Key() == TEXT("+EventRedirects"))
				{
					It.RemoveCurrent();
				}
			}
		}

		// This will remove comments, etc. It is expected for someone to diff this before checking in to manually fix it
		GConfig->Flush(false, DefaultEnginePath);

		// Write out Events.ini
		EventsUpdateSourceControl(Settings->GetDefaultConfigFilename());
		Settings->UpdateDefaultConfigFile();

		GConfig->LoadFile(Settings->GetDefaultConfigFilename());
		
		// Write out all other tag lists
		TArray<const FEventSource*> Sources;

		Manager.FindTagSourcesWithType(EEventSourceType::TagList, Sources);
		Manager.FindTagSourcesWithType(EEventSourceType::RestrictedTagList, Sources);

		for (const FEventSource* Source : Sources)
		{
			UEventsList* TagList = Source->SourceTagList;
			if (TagList)
			{
				EventsUpdateSourceControl(TagList->ConfigFileName);
				TagList->UpdateDefaultConfigFile(TagList->ConfigFileName);

				// Reload off disk
				GConfig->LoadFile(TagList->ConfigFileName);
				//FString DestFileName;
				//FConfigCacheIni::LoadGlobalIniFile(DestFileName, *FString::Printf(TEXT("Tags/%s"), *Source->SourceName.ToString()), nullptr, true);

				// Explicitly remove user tags section
				GConfig->EmptySection(TEXT("UserTags"), TagList->ConfigFileName);
			}
		}

		ShowNotification(LOCTEXT("MigrationText", "Migrated Tag Settings, check DefaultEngine.ini before checking in!"), 10.0f);
	}

	void EventsUpdateSourceControl(const FString& RelativeConfigFilePath)
	{
		FString ConfigPath = FPaths::ConvertRelativePathToFull(RelativeConfigFilePath);

		if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ConfigPath))
		{
			return;
		}

		if (ISourceControlModule::Get().IsEnabled())
		{
			FText ErrorMessage;

			if (!SourceControlHelpers::CheckoutOrMarkForAdd(ConfigPath, FText::FromString(ConfigPath), NULL, ErrorMessage))
			{
				ShowNotification(ErrorMessage, 3.0f);
			}
		}
		else
		{
			if (!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ConfigPath, false))
			{
				ShowNotification(FText::Format(LOCTEXT("FailedToMakeWritable", "Could not make {0} writable."), FText::FromString(ConfigPath)), 3.0f);
			}
		}
	}

	bool DeleteTagRedirector(const FName& TagToDelete)
	{
		UEventsSettings* Settings = GetMutableDefault<UEventsSettings>();
		UEventsManager& Manager = UEventsManager::Get();

		for (int32 i = 0; i < Settings->EventRedirects.Num(); i++)
		{
			if (Settings->EventRedirects[i].OldTagName == TagToDelete)
			{
				Settings->EventRedirects.RemoveAt(i);

				EventsUpdateSourceControl(Settings->GetDefaultConfigFilename());
				Settings->UpdateDefaultConfigFile();
				GConfig->LoadFile(Settings->GetDefaultConfigFilename());

				Manager.EditorRefreshEventTree();

				ShowNotification(FText::Format(LOCTEXT("RemoveTagRedirect", "Deleted tag redirect {0}"), FText::FromName(TagToDelete)), 5.0f);

				return true;
			}
		}

		return false;
	}

	virtual bool AddNewEventToINI(const FString& NewTag, const FString& Comment, FName TagSourceName, bool bIsRestrictedTag, bool bAllowNonRestrictedChildren) override
	{
		UEventsManager& Manager = UEventsManager::Get();

		if (NewTag.IsEmpty())
		{
			return false;
		}

		if (Manager.ShouldImportTagsFromINI() == false)
		{
			return false;
		}

		UEventsSettings*			Settings = GetMutableDefault<UEventsSettings>();
		UEventsDeveloperSettings* DevSettings = GetMutableDefault<UEventsDeveloperSettings>();

		FText ErrorText;
		FString FixedString;
		if (!Manager.IsValidEventString(NewTag, &ErrorText, &FixedString))
		{
			ShowNotification(FText::Format(LOCTEXT("AddTagFailure_BadString", "Failed to add gameplay tag {0}: {1}, try {2} instead!"), FText::FromString(NewTag), ErrorText, FText::FromString(FixedString)), 10.0f, true);
			return false;
		}

		FName NewTagName = FName(*NewTag);

		// Delete existing redirector
		DeleteTagRedirector(NewTagName);

		// Already in the list as an explicit tag, ignore. Note we want to add if it is in implicit tag. (E.g, someone added A.B.C then someone tries to add A.B)
		if (Manager.IsDictionaryTag(NewTagName))
		{
			ShowNotification(FText::Format(LOCTEXT("AddTagFailure_AlreadyExists", "Failed to add gameplay tag {0}, already exists!"), FText::FromString(NewTag)), 10.0f, true);

			return false;
		}

		if (bIsRestrictedTag)
		{
			// restricted tags can't be children of non-restricted tags
			FString AncestorTag = NewTag;
			bool bWasSplit = NewTag.Split(TEXT("."), &AncestorTag, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			while (bWasSplit)
			{
				if (Manager.IsDictionaryTag(FName(*AncestorTag)))
				{
					FString TagComment;
					FName Source;
					bool bIsExplicit;
					bool bIsRestricted;
					bool bAllowsNonRestrictedChildren;

					Manager.GetTagEditorData(*AncestorTag, TagComment, Source, bIsExplicit, bIsRestricted, bAllowsNonRestrictedChildren);
					if (bIsRestricted)
					{
						break;
					}
					ShowNotification(FText::Format(LOCTEXT("AddRestrictedTagFailure", "Failed to add restricted gameplay tag {0}, {1} is not a restricted tag"), FText::FromString(NewTag), FText::FromString(AncestorTag)), 10.0f, true);

					return false;
				}

				bWasSplit = AncestorTag.Split(TEXT("."), &AncestorTag, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			}
		}
		else
		{
			// non-restricted tags can only be children of restricted tags if the restricted tag allows it
			FString AncestorTag = NewTag;
			bool bWasSplit = NewTag.Split(TEXT("."), &AncestorTag, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			while (bWasSplit)
			{
				if (Manager.IsDictionaryTag(FName(*AncestorTag)))
				{
					FString TagComment;
					FName Source;
					bool bIsExplicit;
					bool bIsRestricted;
					bool bAllowsNonRestrictedChildren;

					Manager.GetTagEditorData(*AncestorTag, TagComment, Source, bIsExplicit, bIsRestricted, bAllowsNonRestrictedChildren);
					if (bIsRestricted)
					{
						if (bAllowsNonRestrictedChildren)
						{
							break;
						}

						ShowNotification(FText::Format(LOCTEXT("AddTagFailure_RestrictedTag", "Failed to add gameplay tag {0}, {1} is a restricted tag and does not allow non-restricted children"), FText::FromString(NewTag), FText::FromString(AncestorTag)), 10.0f, true);

						return false;
					}
				}

				bWasSplit = AncestorTag.Split(TEXT("."), &AncestorTag, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			}
		}

		if ((TagSourceName == NAME_None || TagSourceName == FEventSource::GetDefaultName()) && DevSettings && !DevSettings->DeveloperConfigName.IsEmpty())
		{
			// Try to use developer config file
			TagSourceName = FName(*FString::Printf(TEXT("%s.ini"), *DevSettings->DeveloperConfigName));
		}

		if (TagSourceName == NAME_None)
		{
			// If not set yet, set to default
			TagSourceName = FEventSource::GetDefaultName();
		}

		const FEventSource* TagSource = Manager.FindTagSource(TagSourceName);

		if (!TagSource)
		{
			// Create a new one
			TagSource = Manager.FindOrAddTagSource(TagSourceName, EEventSourceType::TagList);
		}

		bool bSuccess = false;
		if (TagSource)
		{
			UObject* TagListObj = nullptr;
			FString ConfigFileName;

			if (bIsRestrictedTag && TagSource->SourceRestrictedTagList)
			{
				URestrictedEventsList* RestrictedTagList = TagSource->SourceRestrictedTagList;
				TagListObj = RestrictedTagList;
				RestrictedTagList->RestrictedEventList.AddUnique(FRestrictedEventTableRow(FName(*NewTag), Comment, bAllowNonRestrictedChildren));
				RestrictedTagList->SortTags();
				ConfigFileName = RestrictedTagList->ConfigFileName;
				bSuccess = true;
			}
			else if (TagSource->SourceTagList)
			{
				UEventsList* TagList = TagSource->SourceTagList;
				TagListObj = TagList;
				TagList->EventList.AddUnique(FEventTableRow(FName(*NewTag), Comment));
				TagList->SortTags();
				ConfigFileName = TagList->ConfigFileName;
				bSuccess = true;
			}

			EventsUpdateSourceControl(ConfigFileName);

			// Check source control before and after writing, to make sure it gets created or checked out

			TagListObj->UpdateDefaultConfigFile(ConfigFileName);
			EventsUpdateSourceControl(ConfigFileName);
			GConfig->LoadFile(ConfigFileName);
		}
		
		if (!bSuccess)
		{
			ShowNotification(FText::Format(LOCTEXT("AddTagFailure", "Failed to add gameplay tag {0} to dictionary {1}!"), FText::FromString(NewTag), FText::FromName(TagSourceName)), 10.0f, true);

			return false;
		}

		{
			FString PerfMessage = FString::Printf(TEXT("ConstructEventTree Event tables after adding new tag"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)

			Manager.EditorRefreshEventTree();
		}

		return true;
	}

	virtual bool DeleteTagFromINI(TSharedPtr<FEventNode> TagNodeToDelete) override
	{
		FName TagName = TagNodeToDelete->GetCompleteTagName();

		UEventsManager& Manager = UEventsManager::Get();
		UEventsSettings* Settings = GetMutableDefault<UEventsSettings>();

		FString Comment;
		FName TagSourceName;
		bool bTagIsExplicit;
		bool bTagIsRestricted;
		bool bTagAllowsNonRestrictedChildren;

		if (DeleteTagRedirector(TagName))
		{
			return true;
		}
		
		if (!Manager.GetTagEditorData(TagName, Comment, TagSourceName, bTagIsExplicit, bTagIsRestricted, bTagAllowsNonRestrictedChildren))
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureNoTag", "Cannot delete tag {0}, does not exist!"), FText::FromName(TagName)), 10.0f, true);
			return false;
		}

		ensure(bTagIsRestricted == TagNodeToDelete->IsRestrictedEvent());

		const FEventSource* TagSource = Manager.FindTagSource(TagSourceName);

		// Check if the tag is implicitly defined
		if (!bTagIsExplicit || !TagSource)
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureNoSource", "Cannot delete tag {0} as it is implicit, remove children manually"), FText::FromName(TagName)), 10.0f, true);
			return false;
		}
		
		if (bTagIsRestricted && !TagSource->SourceRestrictedTagList)
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureBadSource", "Cannot delete tag {0} from source {1}, remove manually"), FText::FromName(TagName), FText::FromName(TagSourceName)), 10.0f, true);
			return false;
		}

		if (!bTagIsRestricted && !TagSource->SourceTagList)
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureBadSource", "Cannot delete tag {0} from source {1}, remove manually"), FText::FromName(TagName), FText::FromName(TagSourceName)), 10.0f, true);
			return false;
		}

		FEventInfo ActualTag = Manager.RequestEvent(TagName);
		FEventContainer ChildTags = Manager.RequestEventChildrenInDictionary(ActualTag);

		TArray<FName> TagsThatWillBeDeleted;

		TagsThatWillBeDeleted.Add(TagName);

		FEventInfo ParentTag = ActualTag.RequestDirectParent();
		while (ParentTag.IsValid() && !Manager.FindTagNode(ParentTag)->IsExplicitTag())
		{
			// See if there are more children than the one we are about to delete
			FEventContainer ParentChildTags = Manager.RequestEventChildrenInDictionary(ParentTag);

			ensure(ParentChildTags.HasTagExact(ActualTag));
			if (ParentChildTags.Num() == 1)
			{
				// This is the only tag, add to deleted list
				TagsThatWillBeDeleted.Add(ParentTag.GetTagName());
				ParentTag = ParentTag.RequestDirectParent();
			}
			else
			{
				break;
			}
		}

		for (FName TagNameToDelete : TagsThatWillBeDeleted)
		{
			// Verify references
			FAssetIdentifier TagId = FAssetIdentifier(FEventInfo::StaticStruct(), TagName);
			TArray<FAssetIdentifier> Referencers;

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().GetReferencers(TagId, Referencers, EAssetRegistryDependencyType::SearchableName);

			if (Referencers.Num() > 0)
			{
				ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureBadSource_Referenced", "Cannot delete tag {0}, still referenced by {1} and possibly others"), FText::FromName(TagNameToDelete), FText::FromString(Referencers[0].ToString())), 10.0f, true);

				return false;
			}
		}

		// Passed, delete and save
		const FString& ConfigFileName = bTagIsRestricted ? TagSource->SourceRestrictedTagList->ConfigFileName : TagSource->SourceTagList->ConfigFileName;
		int32 TagListSize = bTagIsRestricted ? TagSource->SourceRestrictedTagList->RestrictedEventList.Num() : TagSource->SourceTagList->EventList.Num();

		for (int32 i = 0; i < TagListSize; i++)
		{
			bool bRemoved = false;
			if (bTagIsRestricted)
			{
				if (TagSource->SourceRestrictedTagList->RestrictedEventList[i].Tag == TagName)
				{
					TagSource->SourceRestrictedTagList->RestrictedEventList.RemoveAt(i);
					TagSource->SourceRestrictedTagList->UpdateDefaultConfigFile(ConfigFileName);
					bRemoved = true;
				}
			}
			else
			{
				if (TagSource->SourceTagList->EventList[i].Tag == TagName)
				{
					TagSource->SourceTagList->EventList.RemoveAt(i);
					TagSource->SourceTagList->UpdateDefaultConfigFile(ConfigFileName);
					bRemoved = true;
				}
			}

			if (bRemoved)
			{
				EventsUpdateSourceControl(ConfigFileName);
				GConfig->LoadFile(ConfigFileName);

				// See if we still live due to child tags

				if (ChildTags.Num() > 0)
				{
					ShowNotification(FText::Format(LOCTEXT("RemoveTagChildrenExist", "Deleted explicit tag {0}, still exists implicitly due to children"), FText::FromName(TagName)), 5.0f);
				}
				else
				{
					ShowNotification(FText::Format(LOCTEXT("RemoveTag", "Deleted tag {0}"), FText::FromName(TagName)), 5.0f);
				}

				// This invalidates all local variables, need to return right away
				Manager.EditorRefreshEventTree();

				return true;
			}
		}

		ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureNoTag", "Cannot delete tag {0}, does not exist!"), FText::FromName(TagName)), 10.0f, true);
		
		return false;
	}

	virtual bool UpdateTagInINI(const FString& TagToUpdate, const FString& Comment, bool bIsRestrictedTag, bool bAllowNonRestrictedChildren) override
	{
		FName TagName = FName(*TagToUpdate);

		UEventsManager& Manager = UEventsManager::Get();
		UEventsSettings* Settings = GetMutableDefault<UEventsSettings>();

		FString OldComment;
		FName TagSourceName;
		bool bTagIsExplicit;
		bool bTagWasRestricted;
		bool bTagDidAllowNonRestrictedChildren;

		bool bSuccess = false;

		if (Manager.GetTagEditorData(TagName, OldComment, TagSourceName, bTagIsExplicit, bTagWasRestricted, bTagDidAllowNonRestrictedChildren))
		{
			if (const FEventSource* TagSource = Manager.FindTagSource(TagSourceName))
			{
				// if we're disallowing non-restricted children make sure we don't already have some
				if (bTagDidAllowNonRestrictedChildren && !bAllowNonRestrictedChildren)
				{
					FEventInfo ActualTag = Manager.RequestEvent(TagName);
					FEventContainer ChildTags = Manager.RequestEventDirectDescendantsInDictionary(ActualTag, EEventSelectionType::NonRestrictedOnly);
					if (!ChildTags.IsEmpty())
					{
						ShowNotification(LOCTEXT("ToggleAllowNonRestrictedChildrenFailure", "Cannot prevent non-restricted children since some already exist! Delete them first."), 10.0f, true);
						return false;
					}
				}

				UObject* TagListObj = nullptr;
				FString ConfigFileName;

				if (bIsRestrictedTag && TagSource->SourceRestrictedTagList)
				{
					URestrictedEventsList* RestrictedTagList = TagSource->SourceRestrictedTagList;
					TagListObj = RestrictedTagList;
					ConfigFileName = RestrictedTagList->ConfigFileName;

					for (int32 i = 0; i < RestrictedTagList->RestrictedEventList.Num(); i++)
					{
						if (RestrictedTagList->RestrictedEventList[i].Tag == TagName)
						{
							RestrictedTagList->RestrictedEventList[i].bAllowNonRestrictedChildren = bAllowNonRestrictedChildren;
							bSuccess = true;
							break;
						}
					}
				}

				if (bSuccess)
				{
					// Check source control before and after writing, to make sure it gets created or checked out
					EventsUpdateSourceControl(ConfigFileName);
					TagListObj->UpdateDefaultConfigFile(ConfigFileName);
					EventsUpdateSourceControl(ConfigFileName);

					GConfig->LoadFile(ConfigFileName);
				}

			}
		}

		return bSuccess;
	}

	virtual bool RenameTagInINI(const FString& TagToRename, const FString& TagToRenameTo) override
	{
		FName OldTagName = FName(*TagToRename);
		FName NewTagName = FName(*TagToRenameTo);

		UEventsManager& Manager = UEventsManager::Get();
		UEventsSettings* Settings = GetMutableDefault<UEventsSettings>();

		FString OldComment, NewComment;
		FName OldTagSourceName, NewTagSourceName;
		bool bTagIsExplicit;
		bool bTagIsRestricted;
		bool bTagAllowsNonRestrictedChildren;

		// Delete existing redirector
		DeleteTagRedirector(NewTagName);
		DeleteTagRedirector(OldTagName);

		if (Manager.GetTagEditorData(OldTagName, OldComment, OldTagSourceName, bTagIsExplicit, bTagIsRestricted, bTagAllowsNonRestrictedChildren))
		{
			// Add new tag if needed
			if (!Manager.GetTagEditorData(NewTagName, NewComment, NewTagSourceName, bTagIsExplicit, bTagIsRestricted, bTagAllowsNonRestrictedChildren))
			{
				if (!AddNewEventToINI(TagToRenameTo, OldComment, OldTagSourceName, bTagIsRestricted, bTagAllowsNonRestrictedChildren))
				{
					// Failed to add new tag, so fail
					return false;
				}
			}

			// Delete old tag if possible, still make redirector if this fails
			const FEventSource* OldTagSource = Manager.FindTagSource(OldTagSourceName);

			if (OldTagSource && OldTagSource->SourceTagList)
			{
				UEventsList* TagList = OldTagSource->SourceTagList;

				for (int32 i = 0; i < TagList->EventList.Num(); i++)
				{
					if (TagList->EventList[i].Tag == OldTagName)
					{
						TagList->EventList.RemoveAt(i);

						TagList->UpdateDefaultConfigFile(TagList->ConfigFileName);
						EventsUpdateSourceControl(TagList->ConfigFileName);
						GConfig->LoadFile(TagList->ConfigFileName);

						break;
					}
				}
			}
			else
			{
				ShowNotification(FText::Format(LOCTEXT("RenameFailure", "Tag {0} redirector was created but original tag was not destroyed as it has children"), FText::FromString(TagToRename)), 10.0f, true);
			}
		}

		// Add redirector no matter what
		FEventRedirect Redirect;
		Redirect.OldTagName = OldTagName;
		Redirect.NewTagName = NewTagName;

		Settings->EventRedirects.AddUnique(Redirect);

		EventsUpdateSourceControl(Settings->GetDefaultConfigFilename());
		Settings->UpdateDefaultConfigFile();
		GConfig->LoadFile(Settings->GetDefaultConfigFilename());

		ShowNotification(FText::Format(LOCTEXT("AddTagRedirect", "Renamed tag {0} to {1}"), FText::FromString(TagToRename), FText::FromString(TagToRenameTo)), 3.0f);

		Manager.EditorRefreshEventTree();

		return true;
	}

	virtual bool AddTransientEditorEvent(const FString& NewTransientTag) override
	{
		UEventsManager& Manager = UEventsManager::Get();

		if (NewTransientTag.IsEmpty())
		{
			return false;
		}

		Manager.TransientEditorTags.Add(*NewTransientTag);

		{
			FString PerfMessage = FString::Printf(TEXT("ConstructEventTree Event tables after adding new transient tag"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)

			Manager.EditorRefreshEventTree();
		}

		return true;
	}

	static bool WriteCustomReport(FString FileName, TArray<FString>& FileLines)
	{
		// Has a report been generated
		bool ReportGenerated = false;

		// Ensure we have a log to write
		if (FileLines.Num())
		{
			// Create the file name		
			FString FileLocation = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() + TEXT("Reports/"));
			FString FullPath = FString::Printf(TEXT("%s%s"), *FileLocation, *FileName);

			// save file
			FArchive* LogFile = IFileManager::Get().CreateFileWriter(*FullPath);

			if (LogFile != NULL)
			{
				for (int32 Index = 0; Index < FileLines.Num(); ++Index)
				{
					FString LogEntry = FString::Printf(TEXT("%s"), *FileLines[Index]) + LINE_TERMINATOR;
					LogFile->Serialize(TCHAR_TO_ANSI(*LogEntry), LogEntry.Len());
				}

				LogFile->Close();
				delete LogFile;

				// A report has been generated
				ReportGenerated = true;
			}
		}

		return ReportGenerated;
	}

	static void DumpTagList()
	{
		UEventsManager& Manager = UEventsManager::Get();

		TArray<FString> ReportLines;

		ReportLines.Add(TEXT("Tag,Reference Count,Source,Comment"));

		FEventContainer AllTags;
		Manager.RequestAllEvents(AllTags, true);

		TArray<FEventInfo> ExplicitList;
		AllTags.GetEventArray(ExplicitList);

		ExplicitList.Sort();

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		
		for (const FEventInfo& Tag : ExplicitList)
		{
			TArray<FAssetIdentifier> Referencers;
			FAssetIdentifier TagId = FAssetIdentifier(FEventInfo::StaticStruct(), Tag.GetTagName());
			AssetRegistryModule.Get().GetReferencers(TagId, Referencers, EAssetRegistryDependencyType::SearchableName);

			FString Comment;
			FName TagSource;
			bool bExplicit, bRestricted, bAllowNonRestrictedChildren;

			Manager.GetTagEditorData(Tag.GetTagName(), Comment, TagSource, bExplicit, bRestricted, bAllowNonRestrictedChildren);

			ReportLines.Add(FString::Printf(TEXT("%s,%d,%s,%s"), *Tag.ToString(), Referencers.Num(), *TagSource.ToString(), *Comment));
		}

		WriteCustomReport(TEXT("TagList.csv"), ReportLines);
	}

	FDelegateHandle AssetImportHandle;
	FDelegateHandle SettingsChangedHandle;

	FName EventPackageName;
	FName EventStructName;
};

static FAutoConsoleCommand CVarDumpTagList(
	TEXT("Events.DumpTagList"),
	TEXT("Writes out a csv with all tags to Reports/TagList.csv"),
	FConsoleCommandDelegate::CreateStatic(FEventsEditorModule::DumpTagList),
	ECVF_Cheat);

IMPLEMENT_MODULE(FEventsEditorModule, EventsEditor)

#undef LOCTEXT_NAMESPACE