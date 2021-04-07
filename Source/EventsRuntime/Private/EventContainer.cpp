// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventContainer.h"
#include "HAL/IConsoleManager.h"
#include "UObject/CoreNet.h"
#include "UObject/UnrealType.h"
#include "Engine/PackageMapClient.h"
#include "UObject/Package.h"
#include "Engine/NetConnection.h"
#include "EventsManager.h"
#include "EventsRuntimeModule.h"
#include "Misc/OutputDeviceNull.h"

const FEventInfo FEventInfo::EmptyTag;
const FEventContainer FEventContainer::EmptyContainer;
const FEventQuery FEventQuery::EmptyQuery;

DEFINE_STAT(STAT_FEventContainer_HasTag);
DEFINE_STAT(STAT_FEventContainer_DoesTagContainerMatch);
DEFINE_STAT(STAT_UEventsManager_EventsMatch);

/**
 *	Replicates a tag in a packed format:
 *	-A segment of NetIndexFirstBitSegment bits are always replicated.
 *	-Another bit is replicated to indicate "more"
 *	-If "more", then another segment of (MaxBits - NetIndexFirstBitSegment) length is replicated.
 *	
 *	This format is basically the same as SerializeIntPacked, except that there are only 2 segments and they are not the same size.
 *	The gameplay tag system is able to exploit knoweledge in what tags are frequently replicated to ensure they appear in the first segment.
 *	Making frequently replicated tags as cheap as possible. 
 *	
 *	
 *	Setting up your project to take advantage of the packed format.
 *	-Run a normal networked game on non shipping build. 
 *	-After some time, run console command "Events.PrintReport" or set "Events.PrintReportOnShutdown 1" cvar.
 *	-This will generate information on the server log about what tags replicate most frequently.
 *	-Take this list and put it in DefaultEvents.ini.
 *	-CommonlyReplicatedTags is the ordered list of tags.
 *	-NetIndexFirstBitSegment is the number of bits (not including the "more" bit) for the first segment.
 *
 */
void SerializeTagNetIndexPacked(FArchive& Ar, FEventNetIndex& Value, const int32 NetIndexFirstBitSegment, const int32 MaxBits)
{
	// Case where we have no segment or the segment is larger than max bits
	if (NetIndexFirstBitSegment <= 0 || NetIndexFirstBitSegment >= MaxBits)
	{
		if (Ar.IsLoading())
		{
			Value = 0;
		}
		Ar.SerializeBits(&Value, MaxBits);
		return;
	}


	const uint32 BitMasks[] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff, 0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff};
	const uint32 MoreBits[] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000};

	const int32 FirstSegment = NetIndexFirstBitSegment;
	const int32 SecondSegment = MaxBits - NetIndexFirstBitSegment;

	if (Ar.IsSaving())
	{
		uint32 Mask = BitMasks[FirstSegment];
		if (Value > Mask)
		{
			uint32 FirstDataSegment = ((Value & Mask) | MoreBits[FirstSegment+1]);
			uint32 SecondDataSegment = (Value >> FirstSegment);

			uint32 SerializedValue = FirstDataSegment | (SecondDataSegment << (FirstSegment+1));				

			Ar.SerializeBits(&SerializedValue, MaxBits + 1);
		}
		else
		{
			uint32 SerializedValue = Value;
			Ar.SerializeBits(&SerializedValue, NetIndexFirstBitSegment + 1);
		}

	}
	else
	{
		uint32 FirstData = 0;
		Ar.SerializeBits(&FirstData, FirstSegment + 1);
		uint32 More = FirstData & MoreBits[FirstSegment+1];
		if (More)
		{
			uint32 SecondData = 0;
			Ar.SerializeBits(&SecondData, SecondSegment);
			Value = (SecondData << FirstSegment);
			Value |= (FirstData & BitMasks[FirstSegment]);
		}
		else
		{
			Value = FirstData;
		}

	}
}


/** Helper class to parse/eval query token streams. */
class FQueryEvaluator
{
public:
	FQueryEvaluator(FEventQuery const& Q)
		: Query(Q), 
		CurStreamIdx(0), 
		Version(EEventQueryStreamVersion::LatestVersion), 
		bReadError(false)
	{}

	/** Evaluates the query against the given tag container and returns the result (true if matching, false otherwise). */
	bool Eval(FEventContainer const& Tags);

	/** Parses the token stream into an FExpr. */
	void Read(struct FEventQueryExpression& E);

private:
	FEventQuery const& Query;
	int32 CurStreamIdx;
	int32 Version;
	bool bReadError;

	bool EvalAnyTagsMatch(FEventContainer const& Tags, bool bSkip);
	bool EvalAllTagsMatch(FEventContainer const& Tags, bool bSkip);
	bool EvalNoTagsMatch(FEventContainer const& Tags, bool bSkip);

	bool EvalAnyExprMatch(FEventContainer const& Tags, bool bSkip);
	bool EvalAllExprMatch(FEventContainer const& Tags, bool bSkip);
	bool EvalNoExprMatch(FEventContainer const& Tags, bool bSkip);

	bool EvalExpr(FEventContainer const& Tags, bool bSkip = false);
	void ReadExpr(struct FEventQueryExpression& E);

#if WITH_EDITOR
public:
	UEditableEventQuery* CreateEditableQuery();

private:
	UEditableEventQueryExpression* ReadEditableQueryExpr(UObject* ExprOuter);
	void ReadEditableQueryTags(UEditableEventQueryExpression* EditableQueryExpr);
	void ReadEditableQueryExprList(UEditableEventQueryExpression* EditableQueryExpr);
#endif // WITH_EDITOR

	/** Returns the next token in the stream. If there's a read error, sets bReadError and returns zero, so be sure to check that. */
	uint8 GetToken()
	{
		if (Query.QueryTokenStream.IsValidIndex(CurStreamIdx))
		{
			return Query.QueryTokenStream[CurStreamIdx++];
		}
		
		UE_LOG(LogEvents, Warning, TEXT("Error parsing FEventQuery!"));
		bReadError = true;
		return 0;
	}
};

bool FQueryEvaluator::Eval(FEventContainer const& Tags)
{
	CurStreamIdx = 0;

	// start parsing the set
	Version = GetToken();
	if (bReadError)
	{
		return false;
	}
	
	bool bRet = false;

	uint8 const bHasRootExpression = GetToken();
	if (!bReadError && bHasRootExpression)
	{
		bRet = EvalExpr(Tags);

	}

	ensure(CurStreamIdx == Query.QueryTokenStream.Num());
	return bRet;
}

void FQueryEvaluator::Read(FEventQueryExpression& E)
{
	E = FEventQueryExpression();
	CurStreamIdx = 0;

	if (Query.QueryTokenStream.Num() > 0)
	{
		// start parsing the set
		Version = GetToken();
		if (!bReadError)
		{
			uint8 const bHasRootExpression = GetToken();
			if (!bReadError && bHasRootExpression)
			{
				ReadExpr(E);
			}
		}

		ensure(CurStreamIdx == Query.QueryTokenStream.Num());
	}
}

void FQueryEvaluator::ReadExpr(FEventQueryExpression& E)
{
	E.ExprType = (EEventQueryExprType::Type) GetToken();
	if (bReadError)
	{
		return;
	}
	
	if (E.UsesTagSet())
	{
		// parse tag set
		int32 NumTags = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumTags; ++Idx)
		{
			int32 const TagIdx = GetToken();
			if (bReadError)
			{
				return;
			}

			FEventInfo Tag = Query.GetTagFromIndex(TagIdx);
			E.AddTag(Tag);
		}
	}
	else
	{
		// parse expr set
		int32 NumExprs = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumExprs; ++Idx)
		{
			FEventQueryExpression Exp;
			ReadExpr(Exp);
			E.AddExpr(Exp);
		}
	}
}


bool FQueryEvaluator::EvalAnyTagsMatch(FEventContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;
	bool Result = false;

	// parse tagset
	int32 const NumTags = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumTags; ++Idx)
	{
		int32 const TagIdx = GetToken();
		if (bReadError)
		{
			return false;
		}

		if (bShortCircuit == false)
		{
			FEventInfo Tag = Query.GetTagFromIndex(TagIdx);

			bool bHasTag = Tags.HasTag(Tag);

			if (bHasTag)
			{
				// one match is sufficient for a true result!
				bShortCircuit = true;
				Result = true;
			}
		}
	}

	return Result;
}

bool FQueryEvaluator::EvalAllTagsMatch(FEventContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse tagset
	int32 const NumTags = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumTags; ++Idx)
	{
		int32 const TagIdx = GetToken();
		if (bReadError)
		{
			return false;
		}

		if (bShortCircuit == false)
		{
			FEventInfo const Tag = Query.GetTagFromIndex(TagIdx);
			bool const bHasTag = Tags.HasTag(Tag);

			if (bHasTag == false)
			{
				// one failed match is sufficient for a false result
				bShortCircuit = true;
				Result = false;
			}
		}
	}

	return Result;
}

bool FQueryEvaluator::EvalNoTagsMatch(FEventContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse tagset
	int32 const NumTags = GetToken();
	if (bReadError)
	{
		return false;
	}
	
	for (int32 Idx = 0; Idx < NumTags; ++Idx)
	{
		int32 const TagIdx = GetToken();
		if (bReadError)
		{
			return false;
		}

		if (bShortCircuit == false)
		{
			FEventInfo const Tag = Query.GetTagFromIndex(TagIdx);
			bool const bHasTag = Tags.HasTag(Tag);

			if (bHasTag == true)
			{
				// one match is sufficient for a false result
				bShortCircuit = true;
				Result = false;
			}
		}
	}

	return Result;
}

bool FQueryEvaluator::EvalAnyExprMatch(FEventContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume false until proven otherwise
	bool Result = false;

	// parse exprset
	int32 const NumExprs = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumExprs; ++Idx)
	{
		bool const bExprResult = EvalExpr(Tags, bShortCircuit);
		if (bShortCircuit == false)
		{
			if (bExprResult == true)
			{
				// one match is sufficient for true result
				Result = true;
				bShortCircuit = true;
			}
		}
	}

	return Result;
}
bool FQueryEvaluator::EvalAllExprMatch(FEventContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse exprset
	int32 const NumExprs = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumExprs; ++Idx)
	{
		bool const bExprResult = EvalExpr(Tags, bShortCircuit);
		if (bShortCircuit == false)
		{
			if (bExprResult == false)
			{
				// one fail is sufficient for false result
				Result = false;
				bShortCircuit = true;
			}
		}
	}

	return Result;
}
bool FQueryEvaluator::EvalNoExprMatch(FEventContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse exprset
	int32 const NumExprs = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumExprs; ++Idx)
	{
		bool const bExprResult = EvalExpr(Tags, bShortCircuit);
		if (bShortCircuit == false)
		{
			if (bExprResult == true)
			{
				// one match is sufficient for fail result
				Result = false;
				bShortCircuit = true;
			}
		}
	}

	return Result;
}


bool FQueryEvaluator::EvalExpr(FEventContainer const& Tags, bool bSkip)
{
	EEventQueryExprType::Type const ExprType = (EEventQueryExprType::Type) GetToken();
	if (bReadError)
	{
		return false;
	}

	// emit exprdata
	switch (ExprType)
	{
	case EEventQueryExprType::AnyTagsMatch:
		return EvalAnyTagsMatch(Tags, bSkip);
	case EEventQueryExprType::AllTagsMatch:
		return EvalAllTagsMatch(Tags, bSkip);
	case EEventQueryExprType::NoTagsMatch:
		return EvalNoTagsMatch(Tags, bSkip);

	case EEventQueryExprType::AnyExprMatch:
		return EvalAnyExprMatch(Tags, bSkip);
	case EEventQueryExprType::AllExprMatch:
		return EvalAllExprMatch(Tags, bSkip);
	case EEventQueryExprType::NoExprMatch:
		return EvalNoExprMatch(Tags, bSkip);
	}

	check(false);
	return false;
}


FEventContainer& FEventContainer::operator=(FEventContainer const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}
	Events.Empty(Other.Events.Num());
	Events.Append(Other.Events);

	ParentTags.Empty(Other.ParentTags.Num());
	ParentTags.Append(Other.ParentTags);

	return *this;
}

FEventContainer& FEventContainer::operator=(FEventContainer&& Other)
{
	Events = MoveTemp(Other.Events);
	ParentTags = MoveTemp(Other.ParentTags);
	return *this;
}

bool FEventContainer::operator==(FEventContainer const& Other) const
{
	// This is to handle the case where the two containers are in different orders
	if (Events.Num() != Other.Events.Num())
	{
		return false;
	}

	for (const FEventInfo& Tag : Events)
	{
		if (!Tag.MatchesAnyExact(Other))
		{
			return false;
		}
	}

	return true;
}

bool FEventContainer::operator!=(FEventContainer const& Other) const
{
	return !operator==(Other);
}

bool FEventContainer::ComplexHasTag(FEventInfo const& TagToCheck, TEnumAsByte<EEventMatchType::Type> TagMatchType, TEnumAsByte<EEventMatchType::Type> TagToCheckMatchType) const
{
	check(TagMatchType != EEventMatchType::Explicit || TagToCheckMatchType != EEventMatchType::Explicit);

	if (TagMatchType == EEventMatchType::IncludeParentTags)
	{
		FEventContainer ExpandedConatiner = GetEventParents();
		return ExpandedConatiner.HasTagFast(TagToCheck, EEventMatchType::Explicit, TagToCheckMatchType);
	}
	else
	{
		const FEventContainer* SingleContainer = UEventsManager::Get().GetSingleTagContainer(TagToCheck);
		if (SingleContainer && SingleContainer->DoesTagContainerMatch(*this, EEventMatchType::IncludeParentTags, EEventMatchType::Explicit, EEventContainerMatchType::Any))
		{
			return true;
		}

	}
	return false;
}

DECLARE_CYCLE_STAT(TEXT("FEventContainer::RemoveTagByExplicitName"), STAT_FEventContainer_RemoveTagByExplicitName, STATGROUP_Events);

bool FEventContainer::RemoveTagByExplicitName(const FName& TagName)
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_RemoveTagByExplicitName);

	for (auto Event : this->Events)
	{
		if (Event.GetTagName() == TagName)
		{
			RemoveTag(Event);
			return true;
		}
	}

	return false;
}

FORCEINLINE_DEBUGGABLE void FEventContainer::AddParentsForTag(const FEventInfo& Tag)
{
	const FEventContainer* SingleContainer = UEventsManager::Get().GetSingleTagContainer(Tag);

	if (SingleContainer)
	{
		// Add Parent tags from this tag to our own
		for (const FEventInfo& ParentTag : SingleContainer->ParentTags)
		{
			ParentTags.AddUnique(ParentTag);
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("FEventContainer::FillParentTags"), STAT_FEventContainer_FillParentTags, STATGROUP_Events);

void FEventContainer::FillParentTags()
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_FillParentTags);

	ParentTags.Reset();

	for (const FEventInfo& Tag : Events)
	{
		AddParentsForTag(Tag);
	}
}

DECLARE_CYCLE_STAT(TEXT("FEventContainer::GetEventParents"), STAT_FEventContainer_GetEventParents, STATGROUP_Events);

FEventContainer FEventContainer::GetEventParents() const
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_GetEventParents);

	FEventContainer ResultContainer;
	ResultContainer.Events = Events;

	// Add parent tags to explicit tags, the rest got copied over already
	for (const FEventInfo& Tag : ParentTags)
	{
		ResultContainer.Events.AddUnique(Tag);
	}

	return ResultContainer;
}

DECLARE_CYCLE_STAT(TEXT("FEventContainer::Filter"), STAT_FEventContainer_Filter, STATGROUP_Events);

FEventContainer FEventContainer::Filter(const FEventContainer& OtherContainer, TEnumAsByte<EEventMatchType::Type> TagMatchType, TEnumAsByte<EEventMatchType::Type> OtherTagMatchType) const
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_Filter);

	FEventContainer ResultContainer;

	for (const FEventInfo& Tag : Events)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// Check to see if all of these tags match other container, with types swapped
		if (OtherContainer.HasTag(Tag, OtherTagMatchType, TagMatchType))
		{
			ResultContainer.AddTagFast(Tag);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	return ResultContainer;
}

FEventContainer FEventContainer::Filter(const FEventContainer& OtherContainer) const
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_Filter);

	FEventContainer ResultContainer;

	for (const FEventInfo& Tag : Events)
	{
		if (Tag.MatchesAny(OtherContainer))
		{
			ResultContainer.AddTagFast(Tag);
		}
	}

	return ResultContainer;
}

FEventContainer FEventContainer::FilterExact(const FEventContainer& OtherContainer) const
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_Filter);

	FEventContainer ResultContainer;

	for (const FEventInfo& Tag : Events)
	{
		if (Tag.MatchesAnyExact(OtherContainer))
		{
			ResultContainer.AddTagFast(Tag);
		}
	}

	return ResultContainer;
}

bool FEventContainer::DoesTagContainerMatchComplex(const FEventContainer& OtherContainer, TEnumAsByte<EEventMatchType::Type> TagMatchType, TEnumAsByte<EEventMatchType::Type> OtherTagMatchType, EEventContainerMatchType ContainerMatchType) const
{
	UEventsManager& TagManager = UEventsManager::Get();

	for (TArray<FEventInfo>::TConstIterator OtherIt(OtherContainer.Events); OtherIt; ++OtherIt)
	{
		bool bTagFound = false;

		for (TArray<FEventInfo>::TConstIterator It(this->Events); It; ++It)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			if (TagManager.EventsMatch(*It, TagMatchType, *OtherIt, OtherTagMatchType) == true)
			{
				if (ContainerMatchType == EEventContainerMatchType::Any)
				{
					return true;
				}

				bTagFound = true;

				// we only need one match per tag in OtherContainer, so don't bother looking for more
				break;
			}
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		if (ContainerMatchType == EEventContainerMatchType::All && bTagFound == false)
		{
			return false;
		}
	}

	// if we've reached this far then either we are looking for any match and didn't find one (return false) or we're looking for all matches and didn't miss one (return true).
	check(ContainerMatchType == EEventContainerMatchType::All || ContainerMatchType == EEventContainerMatchType::Any);
	return ContainerMatchType == EEventContainerMatchType::All;
}

bool FEventContainer::MatchesQuery(const FEventQuery& Query) const
{
	return Query.Matches(*this);
}

DECLARE_CYCLE_STAT(TEXT("FEventContainer::AppendTags"), STAT_FEventContainer_AppendTags, STATGROUP_Events);

void FEventContainer::AppendTags(FEventContainer const& Other)
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_AppendTags);

	Events.Reserve(Events.Num() + Other.Events.Num());
	ParentTags.Reserve(ParentTags.Num() + Other.ParentTags.Num());

	// Add other container's tags to our own
	for(const FEventInfo& OtherTag : Other.Events)
	{
		Events.AddUnique(OtherTag);
	}

	for (const FEventInfo& OtherTag : Other.ParentTags)
	{
		ParentTags.AddUnique(OtherTag);
	}
}

DECLARE_CYCLE_STAT(TEXT("FEventContainer::AppendMatchingTags"), STAT_FEventContainer_AppendMatchingTags, STATGROUP_Events);


void FEventContainer::AppendMatchingTags(FEventContainer const& OtherA, FEventContainer const& OtherB)
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_AppendMatchingTags);

	for(const FEventInfo& OtherATag : OtherA.Events)
	{
		if (OtherATag.MatchesAny(OtherB))
		{
			AddTag(OtherATag);
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("FEventContainer::AddTag"), STAT_FEventContainer_AddTag, STATGROUP_Events);

static UEventsManager* CachedTagManager = nullptr;


void FEventContainer::AddTag(const FEventInfo& TagToAdd)
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_AddTag);

	if (TagToAdd.IsValid())
	{
		// Don't want duplicate tags
		Events.AddUnique(TagToAdd);

		AddParentsForTag(TagToAdd);
	}
}

void FEventContainer::AddTagFast(const FEventInfo& TagToAdd)
{
	Events.Add(TagToAdd);
	AddParentsForTag(TagToAdd);
}

bool FEventContainer::AddLeafTag(const FEventInfo& TagToAdd)
{
	UEventsManager& TagManager = UEventsManager::Get();

	// Check tag is not already explicitly in container
	if (HasTagExact(TagToAdd))
	{
		return true;
	}

	// If this tag is parent of explicitly added tag, fail
	if (HasTag(TagToAdd))
	{
		return false;
	}

	const FEventContainer* TagToAddContainer = UEventsManager::Get().GetSingleTagContainer(TagToAdd);

	// This should always succeed
	if (!ensure(TagToAddContainer))
	{
		return false;
	}

	// Remove any tags in the container that are a parent to TagToAdd
	for (const FEventInfo& ParentTag : TagToAddContainer->ParentTags)
	{
		if (HasTagExact(ParentTag))
		{
			RemoveTag(ParentTag);
		}
	}

	// Add the tag
	AddTag(TagToAdd);
	return true;
}

DECLARE_CYCLE_STAT(TEXT("FEventContainer::RemoveTag"), STAT_FEventContainer_RemoveTag, STATGROUP_Events);

bool FEventContainer::RemoveTag(const FEventInfo& TagToRemove, bool bDeferParentTags)
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_RemoveTag);

	int32 NumChanged = Events.RemoveSingle(TagToRemove);

	if (NumChanged > 0)
	{
		if (!bDeferParentTags)
		{
			// Have to recompute parent table from scratch because there could be duplicates providing the same parent tag
			FillParentTags();
		}
		return true;
	}
	return false;
}

DECLARE_CYCLE_STAT(TEXT("FEventContainer::RemoveTags"), STAT_FEventContainer_RemoveTags, STATGROUP_Events);

void FEventContainer::RemoveTags(const FEventContainer& TagsToRemove)
{
	SCOPE_CYCLE_COUNTER(STAT_FEventContainer_RemoveTags);

	int32 NumChanged = 0;

	for (auto Tag : TagsToRemove)
	{
		NumChanged += Events.RemoveSingle(Tag);
	}

	if (NumChanged > 0)
	{
		// Recompute once at the end
		FillParentTags();
	}
}

void FEventContainer::Reset(int32 Slack)
{
	Events.Reset(Slack);

	// ParentTags is usually around size of Events on average
	ParentTags.Reset(Slack);
}

bool FEventContainer::Serialize(FStructuredArchive::FSlot Slot)
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();

	const bool bOldTagVer = UnderlyingArchive.UE4Ver() < VER_UE4_GAMEPLAY_TAG_CONTAINER_TAG_TYPE_CHANGE;
	
	if (bOldTagVer)
	{
		TArray<FName> Tags_DEPRECATED;
		Slot << Tags_DEPRECATED;
		// Too old to deal with
		UE_LOG(LogEvents, Error, TEXT("Failed to load old Event container, too old to migrate correctly"));
	}
	else
	{
		Slot << Events;
	}
	
	// Only do redirects for real loads, not for duplicates or recompiles
	if (UnderlyingArchive.IsLoading() )
	{
		if (UnderlyingArchive.IsPersistent() && !(UnderlyingArchive.GetPortFlags() & PPF_Duplicate) && !(UnderlyingArchive.GetPortFlags() & PPF_DuplicateForPIE))
		{
			// Rename any tags that may have changed by the ini file.  Redirects can happen regardless of version.
			// Regardless of version, want loading to have a chance to handle redirects
			UEventsManager::Get().EventContainerLoaded(*this, UnderlyingArchive.GetSerializedProperty());
		}

		FillParentTags();
	}

	if (UnderlyingArchive.IsSaving())
	{
		// This marks the saved name for later searching
		for (const FEventInfo& Tag : Events)
		{
			UnderlyingArchive.MarkSearchableName(FEventInfo::StaticStruct(), Tag.TagName);
		}
	}

	return true;
}

FString FEventContainer::ToString() const
{
	FString ExportString;
	FEventContainer::StaticStruct()->ExportText(ExportString, this, this, nullptr, 0, nullptr);

	return ExportString;
}

void FEventContainer::FromExportString(const FString& ExportString)
{
	Reset();

	FOutputDeviceNull NullOut;
	FEventContainer::StaticStruct()->ImportText(*ExportString, this, nullptr, 0, &NullOut, TEXT("FEventContainer"), true);
}

bool FEventContainer::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	// Call default import, but skip the native callback to avoid recursion
	Buffer = FEventContainer::StaticStruct()->ImportText(Buffer, this, Parent, PortFlags, ErrorText, TEXT("FEventContainer"), false);

	if (Buffer)
	{
		// Compute parent tags
		FillParentTags();	
	}
	return true;
}

void FEventContainer::PostScriptConstruct()
{
	FillParentTags();
}

FString FEventContainer::ToStringSimple(bool bQuoted) const
{
	FString RetString;
	for (int i = 0; i < Events.Num(); ++i)
	{
		if (bQuoted)
		{
			RetString += TEXT("\"");
		}
		RetString += Events[i].ToString();
		if (bQuoted)
		{
			RetString += TEXT("\"");
		}
		
		if (i < Events.Num() - 1)
		{
			RetString += TEXT(", ");
		}
	}
	return RetString;
}

bool FEventContainer::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// 1st bit to indicate empty tag container or not (empty tag containers are frequently replicated). Early out if empty.
	uint8 IsEmpty = Events.Num() == 0;
	Ar.SerializeBits(&IsEmpty, 1);
	if (IsEmpty)
	{
		if (Events.Num() > 0)
		{
			Reset();
		}
		bOutSuccess = true;
		return true;
	}

	// -------------------------------------------------------

	int32 NumBitsForContainerSize = UEventsManager::Get().NumBitsForContainerSize;

	if (Ar.IsSaving())
	{
		uint8 NumTags = Events.Num();
		uint8 MaxSize = (1 << NumBitsForContainerSize) - 1;
		if (!ensureMsgf(NumTags <= MaxSize, TEXT("TagContainer has %d elements when max is %d! Tags: %s"), NumTags, MaxSize, *ToStringSimple()))
		{
			NumTags = MaxSize;
		}
		
		Ar.SerializeBits(&NumTags, NumBitsForContainerSize);
		for (int32 idx=0; idx < NumTags;++idx)
		{
			FEventInfo& Tag = Events[idx];
			Tag.NetSerialize_Packed(Ar, Map, bOutSuccess);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			UEventsManager::Get().NotifyTagReplicated(Tag, true);
#endif
		}
	}
	else
	{
		// No Common Container tags, just replicate this like normal
		uint8 NumTags = 0;
		Ar.SerializeBits(&NumTags, NumBitsForContainerSize);

		Events.Empty(NumTags);
		Events.AddDefaulted(NumTags);
		for (uint8 idx = 0; idx < NumTags; ++idx)
		{
			Events[idx].NetSerialize_Packed(Ar, Map, bOutSuccess);
		}
		FillParentTags();
	}


	bOutSuccess  = true;
	return true;
}

FText FEventContainer::ToMatchingText(EEventContainerMatchType MatchType, bool bInvertCondition) const
{
	enum class EMatchingTypes : int8
	{
		Inverted	= 0x01,
		All			= 0x02
	};

#define LOCTEXT_NAMESPACE "FEventContainer"
	const FText MatchingDescription[] =
	{
		LOCTEXT("MatchesAnyEvents", "Has any tags in set: {EventSet}"),
		LOCTEXT("NotMatchesAnyEvents", "Does not have any tags in set: {EventSet}"),
		LOCTEXT("MatchesAllEvents", "Has all tags in set: {EventSet}"),
		LOCTEXT("NotMatchesAllEvents", "Does not have all tags in set: {EventSet}")
	};
#undef LOCTEXT_NAMESPACE

	int32 DescriptionIndex = bInvertCondition ? static_cast<int32>(EMatchingTypes::Inverted) : 0;
	switch (MatchType)
	{
		case EEventContainerMatchType::All:
			DescriptionIndex |= static_cast<int32>(EMatchingTypes::All);
			break;

		case EEventContainerMatchType::Any:
			break;

		default:
			UE_LOG(LogEvents, Warning, TEXT("Invalid value for TagsToMatch (EEventContainerMatchType) %d.  Should only be Any or All."), static_cast<int32>(MatchType));
			break;
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("EventSet"), FText::FromString(*ToString()));
	return FText::Format(MatchingDescription[DescriptionIndex], Arguments);
}

bool FEventInfo::ComplexMatches(TEnumAsByte<EEventMatchType::Type> MatchTypeOne, const FEventInfo& Other, TEnumAsByte<EEventMatchType::Type> MatchTypeTwo) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return UEventsManager::Get().EventsMatch(*this, MatchTypeOne, Other, MatchTypeTwo);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

DECLARE_CYCLE_STAT(TEXT("FEventBase::GetSingleTagContainer"), STAT_FEvent_GetSingleTagContainer, STATGROUP_Events);

const FEventContainer& FEventInfo::GetSingleTagContainer() const
{
	SCOPE_CYCLE_COUNTER(STAT_FEvent_GetSingleTagContainer);

	const FEventContainer* TagContainer = UEventsManager::Get().GetSingleTagContainer(*this);

	if (TagContainer)
	{
		return *TagContainer;
	}

	// This should always be invalid if the node is missing
	ensure(!IsValid());

	return FEventContainer::EmptyContainer;
}

FEventInfo FEventInfo::RequestEvent(const FName& TagName, bool ErrorIfNotFound)
{
	return UEventsManager::Get().RequestEvent(TagName, ErrorIfNotFound);
}

bool FEventInfo::IsValidEventString(const FString& TagString, FText* OutError, FString* OutFixedString)
{
	return UEventsManager::Get().IsValidEventString(TagString, OutError, OutFixedString);
}

FEventContainer FEventInfo::GetEventParents() const
{
	return UEventsManager::Get().RequestEventParents(*this);
}

DECLARE_CYCLE_STAT(TEXT("FEventBase::MatchesTag"), STAT_FEvent_MatchesTag, STATGROUP_Events);

bool FEventInfo::MatchesTag(const FEventInfo& TagToCheck) const
{
	SCOPE_CYCLE_COUNTER(STAT_FEvent_MatchesTag);

	const FEventContainer* TagContainer = UEventsManager::Get().GetSingleTagContainer(*this);

	if (TagContainer)
	{
		return TagContainer->HasTag(TagToCheck);
	}

	// This should always be invalid if the node is missing
	ensureMsgf(!IsValid(), TEXT("Valid tag failed to convert to single tag container. %s"), *GetTagName().ToString());

	return false;
}

DECLARE_CYCLE_STAT(TEXT("FEventBase::MatchesAny"), STAT_FEvent_MatchesAny, STATGROUP_Events);

bool FEventInfo::MatchesAny(const FEventContainer& ContainerToCheck) const
{
	SCOPE_CYCLE_COUNTER(STAT_FEvent_MatchesAny);

	const FEventContainer* TagContainer = UEventsManager::Get().GetSingleTagContainer(*this);

	if (TagContainer)
	{
		return TagContainer->HasAny(ContainerToCheck);
	}

	// This should always be invalid if the node is missing
	ensureMsgf(!IsValid(), TEXT("Valid tag failed to conver to single tag container. %s"), *GetTagName().ToString() );
	return false;
}

int32 FEventInfo::MatchesTagDepth(const FEventInfo& TagToCheck) const
{
	return UEventsManager::Get().EventsMatchDepth(*this, TagToCheck);
}

FEventInfo::FEventInfo(const FName& Name)
	: TagName(Name)
{
	// This constructor is used to bypass the table check and is only usable by EventManager
}

bool FEventInfo::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	if (Tag.Type == NAME_NameProperty)
	{
		Slot << TagName;
		return true;
	}
	return false;
}

FEventInfo FEventInfo::RequestDirectParent() const
{
	return UEventsManager::Get().RequestEventDirectParent(*this);
}

DECLARE_CYCLE_STAT(TEXT("FEventBase::NetSerialize"), STAT_FEvent_NetSerialize, STATGROUP_Events);

bool FEventInfo::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Ar.IsSaving())
	{
		UEventsManager::Get().NotifyTagReplicated(*this, false);
	}
#endif

	NetSerialize_Packed(Ar, Map, bOutSuccess);

	bOutSuccess = true;
	return true;
}

static TSharedPtr<FNetFieldExportGroup> CreateNetfieldExportGroupForNetworkEvents(const UEventsManager& TagManager, const TCHAR* NetFieldExportGroupName)
{
	TSharedPtr<FNetFieldExportGroup> NetFieldExportGroup = TSharedPtr<FNetFieldExportGroup>(new FNetFieldExportGroup());

	const TArray<TSharedPtr<FEventNode>>& NetworkEventNodeIndex = TagManager.GetNetworkEventNodeIndex();

	NetFieldExportGroup->PathName = NetFieldExportGroupName;
	NetFieldExportGroup->NetFieldExports.SetNum(NetworkEventNodeIndex.Num());

	for (int32 i = 0; i < NetworkEventNodeIndex.Num(); i++)
	{
		FNetFieldExport NetFieldExport(
			i,
			0,
			NetworkEventNodeIndex[i]->GetCompleteTagName());

		NetFieldExportGroup->NetFieldExports[i] = NetFieldExport;
	}

	return NetFieldExportGroup;
}

bool FEventInfo::NetSerialize_Packed(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	SCOPE_CYCLE_COUNTER(STAT_FEvent_NetSerialize);

	UEventsManager& TagManager = UEventsManager::Get();

	if (TagManager.ShouldUseFastReplication())
	{
		FEventNetIndex NetIndex = INVALID_TAGNETINDEX;

		UPackageMapClient* PackageMapClient = Cast<UPackageMapClient>(Map);
		const bool bIsReplay = PackageMapClient && PackageMapClient->GetConnection() && PackageMapClient->GetConnection()->IsInternalAck();

		TSharedPtr<FNetFieldExportGroup> NetFieldExportGroup;

		if (bIsReplay)
		{
			// For replays, use a net field export group to guarantee we can send the name reliably (without having to rely on the client having a deterministic NetworkEventNodeIndex array)
			const TCHAR* NetFieldExportGroupName = TEXT("NetworkEventNodeIndex");

			// Find this net field export group
			NetFieldExportGroup = PackageMapClient->GetNetFieldExportGroup(NetFieldExportGroupName);

			if (Ar.IsSaving())
			{
				// If we didn't find it, we need to create it (only when saving though, it should be here on load since it was exported at save time)
				if (!NetFieldExportGroup.IsValid())
				{
					NetFieldExportGroup = CreateNetfieldExportGroupForNetworkEvents(TagManager, NetFieldExportGroupName);

					PackageMapClient->AddNetFieldExportGroup(NetFieldExportGroupName, NetFieldExportGroup);
				}

				NetIndex = TagManager.GetNetIndexFromTag(*this);

				if (NetIndex != TagManager.InvalidTagNetIndex && NetIndex != INVALID_TAGNETINDEX)
				{
					PackageMapClient->TrackNetFieldExport(NetFieldExportGroup.Get(), NetIndex);
				}
				else
				{
					NetIndex = INVALID_TAGNETINDEX;		// We can't save InvalidTagNetIndex, since the remote side could have a different value for this
				}
			}

			uint32 NetIndex32 = NetIndex;
			Ar.SerializeIntPacked(NetIndex32);
			NetIndex = NetIndex32;

			if (Ar.IsLoading())
			{
				// Get the tag name from the net field export group entry
				if (NetIndex != INVALID_TAGNETINDEX && ensure(NetFieldExportGroup.IsValid()) && ensure(NetIndex < NetFieldExportGroup->NetFieldExports.Num()))
				{
					TagName = NetFieldExportGroup->NetFieldExports[NetIndex].ExportName;

					// Validate the tag name
					const FEventInfo Tag = UEventsManager::Get().RequestEvent(TagName, false);

					// Warn (once) if the tag isn't found
					if (!Tag.IsValid() && !NetFieldExportGroup->NetFieldExports[NetIndex].bIncompatible)
					{ 
						UE_LOG(LogEvents, Warning, TEXT( "Gameplay tag not found (marking incompatible): %s"), *TagName.ToString());
						NetFieldExportGroup->NetFieldExports[NetIndex].bIncompatible = true;
					}

					TagName = Tag.TagName;
				}
				else
				{
					TagName = NAME_None;
				}
			}

			bOutSuccess = true;
			return true;
		}

		if (Ar.IsSaving())
		{
			NetIndex = TagManager.GetNetIndexFromTag(*this);
			
			SerializeTagNetIndexPacked(Ar, NetIndex, TagManager.NetIndexFirstBitSegment, TagManager.NetIndexTrueBitNum);
		}
		else
		{
			SerializeTagNetIndexPacked(Ar, NetIndex, TagManager.NetIndexFirstBitSegment, TagManager.NetIndexTrueBitNum);
			TagName = TagManager.GetTagNameFromNetIndex(NetIndex);
		}
	}
	else
	{
		Ar << TagName;
	}

	bOutSuccess = true;
	return true;
}

void FEventInfo::PostSerialize(const FArchive& Ar)
{
	// This only happens for tags that are not nested inside a container, containers handle redirectors themselves
	// Only do redirects for real loads, not for duplicates or recompiles
	if (Ar.IsLoading() && Ar.IsPersistent() && !(Ar.GetPortFlags() & PPF_Duplicate) && !(Ar.GetPortFlags() & PPF_DuplicateForPIE))
	{
		// Rename any tags that may have changed by the ini file.
		UEventsManager::Get().SingleEventLoaded(*this, Ar.GetSerializedProperty());
	}

	if (Ar.IsSaving() && IsValid())
	{
		// This marks the saved name for later searching
		Ar.MarkSearchableName(FEventInfo::StaticStruct(), TagName);
	}
}

bool FEventInfo::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	FString ImportedTag = TEXT("");
	const TCHAR* NewBuffer = FPropertyHelpers::ReadToken(Buffer, ImportedTag, true);
	if (!NewBuffer)
	{
		// Failed to read buffer. Maybe normal ImportText will work.
		return false;
	}

	if (ImportedTag == TEXT("None") || ImportedTag.IsEmpty())
	{
		// TagName was none
		TagName = NAME_None;
		return true;
	}

	if (ImportedTag[0] == '(')
	{
		// Let normal ImportText handle this. It appears to be prepared for it.
		UScriptStruct* ScriptStruct = FEventInfo::StaticStruct();
		Buffer = ScriptStruct->ImportText(Buffer, this, Parent, PortFlags, ErrorText, ScriptStruct->GetName(), false);
		UEventsManager::Get().ImportSingleEvent(*this, TagName);
		return true;
	}

	return UEventsManager::Get().ImportSingleEvent(*this, FName(*ImportedTag));
}

void FEventInfo::FromExportString(const FString& ExportString)
{
	TagName = NAME_None;

	FOutputDeviceNull NullOut;
	FEventInfo::StaticStruct()->ImportText(*ExportString, this, nullptr, 0, &NullOut, TEXT("FEventBase"), true);
}

FEventNativeAdder::FEventNativeAdder()
{
	UEventsManager::OnLastChanceToAddNativeTags().AddRaw(this, &FEventNativeAdder::AddTags);
}

FEventQuery::FEventQuery()
	: TokenStreamVersion(EEventQueryStreamVersion::LatestVersion)
{
}

FEventQuery::FEventQuery(FEventQuery const& Other)
{
	*this = Other;
}

FEventQuery::FEventQuery(FEventQuery&& Other)
{
	*this = MoveTemp(Other);
}

/** Assignment/Equality operators */
FEventQuery& FEventQuery::operator=(FEventQuery const& Other)
{
	if (this != &Other)
	{
		TokenStreamVersion = Other.TokenStreamVersion;
		TagDictionary = Other.TagDictionary;
		QueryTokenStream = Other.QueryTokenStream;
		UserDescription = Other.UserDescription;
		AutoDescription = Other.AutoDescription;
	}
	return *this;
}

FEventQuery& FEventQuery::operator=(FEventQuery&& Other)
{
	TokenStreamVersion = Other.TokenStreamVersion;
	TagDictionary = MoveTemp(Other.TagDictionary);
	QueryTokenStream = MoveTemp(Other.QueryTokenStream);
	UserDescription = MoveTemp(Other.UserDescription);
	AutoDescription = MoveTemp(Other.AutoDescription);
	return *this;
}

bool FEventQuery::Matches(FEventContainer const& Tags) const
{
	FQueryEvaluator QE(*this);
	return QE.Eval(Tags);
}

bool FEventQuery::IsEmpty() const
{
	return (QueryTokenStream.Num() == 0);
}

void FEventQuery::Clear()
{
	*this = FEventQuery::EmptyQuery;
}

void FEventQuery::GetQueryExpr(FEventQueryExpression& OutExpr) const
{
	// build the FExpr tree from the token stream and return it
	FQueryEvaluator QE(*this);
	QE.Read(OutExpr);
}

void FEventQuery::Build(FEventQueryExpression& RootQueryExpr, FString InUserDescription)
{
	TokenStreamVersion = EEventQueryStreamVersion::LatestVersion;
	UserDescription = InUserDescription;

	// Reserve size here is arbitrary, goal is to minimizing reallocs while being respectful of mem usage
	QueryTokenStream.Reset(128);
	TagDictionary.Reset();

	// add stream version first
	QueryTokenStream.Add(EEventQueryStreamVersion::LatestVersion);

	// emit the query
	QueryTokenStream.Add(1);		// true to indicate is has a root expression
	RootQueryExpr.EmitTokens(QueryTokenStream, TagDictionary);
}

// static 
FEventQuery FEventQuery::BuildQuery(FEventQueryExpression& RootQueryExpr, FString InDescription)
{
	FEventQuery Q;
	Q.Build(RootQueryExpr, InDescription);
	return Q;
}

//static 
FEventQuery FEventQuery::MakeQuery_MatchAnyTags(FEventContainer const& InTags)
{
	return FEventQuery::BuildQuery
	(
		FEventQueryExpression()
		.AnyTagsMatch()
		.AddTags(InTags)
	);
}

//static
FEventQuery FEventQuery::MakeQuery_MatchAllTags(FEventContainer const& InTags)
{
	return FEventQuery::BuildQuery
		(
		FEventQueryExpression()
		.AllTagsMatch()
		.AddTags(InTags)
		);
}

// static
FEventQuery FEventQuery::MakeQuery_MatchNoTags(FEventContainer const& InTags)
{
	return FEventQuery::BuildQuery
		(
		FEventQueryExpression()
		.NoTagsMatch()
		.AddTags(InTags)
		);
}

// static
FEventQuery FEventQuery::MakeQuery_MatchTag(FEventInfo const & InTag)
{
	return FEventQuery::BuildQuery
	(
		FEventQueryExpression()
		.AllTagsMatch()
		.AddTag(InTag)
	);
}


#if WITH_EDITOR

UEditableEventQuery* FQueryEvaluator::CreateEditableQuery()
{
	CurStreamIdx = 0;

	UEditableEventQuery* const EditableQuery = NewObject<UEditableEventQuery>(GetTransientPackage(), NAME_None, RF_Transactional);

	// start parsing the set
	Version = GetToken();
	if (!bReadError)
	{
		uint8 const bHasRootExpression = GetToken();
		if (!bReadError && bHasRootExpression)
		{
			EditableQuery->RootExpression = ReadEditableQueryExpr(EditableQuery);
		}
	}
	ensure(CurStreamIdx == Query.QueryTokenStream.Num());

	EditableQuery->UserDescription = Query.UserDescription;

	return EditableQuery;
}

UEditableEventQueryExpression* FQueryEvaluator::ReadEditableQueryExpr(UObject* ExprOuter)
{
	EEventQueryExprType::Type const ExprType = (EEventQueryExprType::Type) GetToken();
	if (bReadError)
	{
		return nullptr;
	}

	UClass* ExprClass = nullptr;
	switch (ExprType)
	{
	case EEventQueryExprType::AnyTagsMatch:
		ExprClass = UEditableEventQueryExpression_AnyTagsMatch::StaticClass();
		break;
	case EEventQueryExprType::AllTagsMatch:
		ExprClass = UEditableEventQueryExpression_AllTagsMatch::StaticClass();
		break;
	case EEventQueryExprType::NoTagsMatch:
		ExprClass = UEditableEventQueryExpression_NoTagsMatch::StaticClass();
		break;
	case EEventQueryExprType::AnyExprMatch:
		ExprClass = UEditableEventQueryExpression_AnyExprMatch::StaticClass();
		break;
	case EEventQueryExprType::AllExprMatch:
		ExprClass = UEditableEventQueryExpression_AllExprMatch::StaticClass();
		break;
	case EEventQueryExprType::NoExprMatch:
		ExprClass = UEditableEventQueryExpression_NoExprMatch::StaticClass();
		break;
	}

	UEditableEventQueryExpression* NewExpr = nullptr;
	if (ExprClass)
	{
		NewExpr = NewObject<UEditableEventQueryExpression>(ExprOuter, ExprClass, NAME_None, RF_Transactional);
		if (NewExpr)
		{
			switch (ExprType)
			{
			case EEventQueryExprType::AnyTagsMatch:
			case EEventQueryExprType::AllTagsMatch:
			case EEventQueryExprType::NoTagsMatch:
				ReadEditableQueryTags(NewExpr);
				break;
			case EEventQueryExprType::AnyExprMatch:
			case EEventQueryExprType::AllExprMatch:
			case EEventQueryExprType::NoExprMatch:
				ReadEditableQueryExprList(NewExpr);
				break;
			}
		}
	}

	return NewExpr;
}

void FQueryEvaluator::ReadEditableQueryTags(UEditableEventQueryExpression* EditableQueryExpr)
{
	// find the tag container to read into
	FEventContainer* Tags = nullptr;
	if (EditableQueryExpr->IsA(UEditableEventQueryExpression_AnyTagsMatch::StaticClass()))
	{
		Tags = &((UEditableEventQueryExpression_AnyTagsMatch*)EditableQueryExpr)->Tags;
	}
	else if (EditableQueryExpr->IsA(UEditableEventQueryExpression_AllTagsMatch::StaticClass()))
	{
		Tags = &((UEditableEventQueryExpression_AllTagsMatch*)EditableQueryExpr)->Tags;
	}
	else if (EditableQueryExpr->IsA(UEditableEventQueryExpression_NoTagsMatch::StaticClass()))
	{
		Tags = &((UEditableEventQueryExpression_NoTagsMatch*)EditableQueryExpr)->Tags;
	}
	ensure(Tags);

	if (Tags)
	{
		// parse tag set
		int32 const NumTags = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumTags; ++Idx)
		{
			int32 const TagIdx = GetToken();
			if (bReadError)
			{
				return;
			}

			FEventInfo const Tag = Query.GetTagFromIndex(TagIdx);
			Tags->AddTag(Tag);
		}
	}
}

void FQueryEvaluator::ReadEditableQueryExprList(UEditableEventQueryExpression* EditableQueryExpr)
{
	// find the tag container to read into
	TArray<UEditableEventQueryExpression*>* ExprList = nullptr;
	if (EditableQueryExpr->IsA(UEditableEventQueryExpression_AnyExprMatch::StaticClass()))
	{
		ExprList = &((UEditableEventQueryExpression_AnyExprMatch*)EditableQueryExpr)->Expressions;
	}
	else if (EditableQueryExpr->IsA(UEditableEventQueryExpression_AllExprMatch::StaticClass()))
	{
		ExprList = &((UEditableEventQueryExpression_AllExprMatch*)EditableQueryExpr)->Expressions;
	}
	else if (EditableQueryExpr->IsA(UEditableEventQueryExpression_NoExprMatch::StaticClass()))
	{
		ExprList = &((UEditableEventQueryExpression_NoExprMatch*)EditableQueryExpr)->Expressions;
	}
	ensure(ExprList);

	if (ExprList)
	{
		// parse expr set
		int32 const NumExprs = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumExprs; ++Idx)
		{
			UEditableEventQueryExpression* const NewExpr = ReadEditableQueryExpr(EditableQueryExpr);
			ExprList->Add(NewExpr);
		}
	}
}

UEditableEventQuery* FEventQuery::CreateEditableQuery()
{
	FQueryEvaluator QE(*this);
	return QE.CreateEditableQuery();
}

void FEventQuery::BuildFromEditableQuery(UEditableEventQuery& EditableQuery)
{
	QueryTokenStream.Reset();
	TagDictionary.Reset();

	UserDescription = EditableQuery.UserDescription;

	// add stream version first
	QueryTokenStream.Add(EEventQueryStreamVersion::LatestVersion);
	EditableQuery.EmitTokens(QueryTokenStream, TagDictionary, &AutoDescription);
}

FString UEditableEventQuery::GetTagQueryExportText(FEventQuery const& TagQuery)
{
	TagQueryExportText_Helper = TagQuery;
	FProperty* const TQProperty = FindFProperty<FProperty>(GetClass(), TEXT("TagQueryExportText_Helper"));

	FString OutString;
	TQProperty->ExportTextItem(OutString, (void*)&TagQueryExportText_Helper, (void*)&TagQueryExportText_Helper, this, 0);
	return OutString;
}

void UEditableEventQuery::EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const
{
	if (DebugString)
	{
		// start with a fresh string
		DebugString->Empty();
	}

	if (RootExpression)
	{
		TokenStream.Add(1);		// true if has a root expression
		RootExpression->EmitTokens(TokenStream, TagDictionary, DebugString);
	}
	else
	{
		TokenStream.Add(0);		// false if no root expression
		if (DebugString)
		{
			DebugString->Append(TEXT("undefined"));
		}
	}
}

void UEditableEventQueryExpression::EmitTagTokens(FEventContainer const& TagsToEmit, TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const
{
	uint8 const NumTags = (uint8)TagsToEmit.Num();
	TokenStream.Add(NumTags);

	bool bFirstTag = true;

	for (auto T : TagsToEmit)
	{
		int32 TagIdx = TagDictionary.AddUnique(T);
		check(TagIdx <= 255);
		TokenStream.Add((uint8)TagIdx);

		if (DebugString)
		{
			if (bFirstTag == false)
			{
				DebugString->Append(TEXT(","));
			}

			DebugString->Append(TEXT(" "));
			DebugString->Append(T.ToString());
		}

		bFirstTag = false;
	}
}

void UEditableEventQueryExpression::EmitExprListTokens(TArray<UEditableEventQueryExpression*> const& ExprList, TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const
{
	uint8 const NumExprs = (uint8)ExprList.Num();
	TokenStream.Add(NumExprs);

	bool bFirstExpr = true;
	
	for (auto E : ExprList)
	{
		if (DebugString)
		{
			if (bFirstExpr == false)
			{
				DebugString->Append(TEXT(","));
			}

			DebugString->Append(TEXT(" "));
		}

		if (E)
		{
			E->EmitTokens(TokenStream, TagDictionary, DebugString);
		}
		else
		{
			// null expression
			TokenStream.Add(EEventQueryExprType::Undefined);
			if (DebugString)
			{
				DebugString->Append(TEXT("undefined"));
			}
		}

		bFirstExpr = false;
	}
}

void UEditableEventQueryExpression_AnyTagsMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EEventQueryExprType::AnyTagsMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ANY("));
	}

	EmitTagTokens(Tags, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableEventQueryExpression_AllTagsMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EEventQueryExprType::AllTagsMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ALL("));
	}
	
	EmitTagTokens(Tags, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableEventQueryExpression_NoTagsMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EEventQueryExprType::NoTagsMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" NONE("));
	}

	EmitTagTokens(Tags, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableEventQueryExpression_AnyExprMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EEventQueryExprType::AnyExprMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ANY("));
	}

	EmitExprListTokens(Expressions, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableEventQueryExpression_AllExprMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EEventQueryExprType::AllExprMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ALL("));
	}

	EmitExprListTokens(Expressions, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableEventQueryExpression_NoExprMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EEventQueryExprType::NoExprMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" NONE("));
	}

	EmitExprListTokens(Expressions, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}
#endif	// WITH_EDITOR


FEventQueryExpression& FEventQueryExpression::AddTag(FName TagName)
{
	FEventInfo const Tag = UEventsManager::Get().RequestEvent(TagName);
	return AddTag(Tag);
}

void FEventQueryExpression::EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary) const
{
	// emit exprtype
	TokenStream.Add(ExprType);

	// emit exprdata
	switch (ExprType)
	{
	case EEventQueryExprType::AnyTagsMatch:
	case EEventQueryExprType::AllTagsMatch:
	case EEventQueryExprType::NoTagsMatch:
	{
		// emit tagset
		uint8 NumTags = (uint8)TagSet.Num();
		TokenStream.Add(NumTags);

		for (auto Tag : TagSet)
		{
			int32 TagIdx = TagDictionary.AddUnique(Tag);
			check(TagIdx <= 254);		// we reserve token 255 for internal use, so 254 is max unique tags
			TokenStream.Add((uint8)TagIdx);
		}
	}
	break;

	case EEventQueryExprType::AnyExprMatch:
	case EEventQueryExprType::AllExprMatch:
	case EEventQueryExprType::NoExprMatch:
	{
		// emit tagset
		uint8 NumExprs = (uint8)ExprSet.Num();
		TokenStream.Add(NumExprs);

		for (auto& E : ExprSet)
		{
			E.EmitTokens(TokenStream, TagDictionary);
		}
	}
	break;
	default:
		break;
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static void EventPrintReplicationMap()
{
	UEventsManager::Get().PrintReplicationFrequencyReport();
}

FAutoConsoleCommand EventPrintReplicationMapCmd(
	TEXT("Events.PrintReport"), 
	TEXT( "Prints frequency of gameplay tags" ), 
	FConsoleCommandDelegate::CreateStatic(EventPrintReplicationMap)
);

static void EventPrintReplicationIndices()
{
	UEventsManager::Get().PrintReplicationIndices();
}

FAutoConsoleCommand EventPrintReplicationIndicesCmd(
	TEXT("Events.PrintNetIndices"), 
	TEXT( "Prints net indices for all known tags" ), 
	FConsoleCommandDelegate::CreateStatic(EventPrintReplicationIndices)
);

static void TagPackingTest()
{
	for (int32 TotalNetIndexBits=1; TotalNetIndexBits <= 16; ++TotalNetIndexBits)
	{
		for (int32 NetIndexBitsPerComponent=0; NetIndexBitsPerComponent <= TotalNetIndexBits; NetIndexBitsPerComponent++)
		{
			for (int32 NetIndex=0; NetIndex < FMath::Pow(2, TotalNetIndexBits); ++NetIndex)
			{
				FEventNetIndex NI = NetIndex;

				FNetBitWriter	BitWriter(nullptr, 1024 * 8);
				SerializeTagNetIndexPacked(BitWriter, NI, NetIndexBitsPerComponent, TotalNetIndexBits);

				FNetBitReader	Reader(nullptr, BitWriter.GetData(), BitWriter.GetNumBits());

				FEventNetIndex NewIndex;
				SerializeTagNetIndexPacked(Reader, NewIndex, NetIndexBitsPerComponent, TotalNetIndexBits);

				if (ensureAlways((NewIndex == NI)) == false)
				{
					NetIndex--;
					continue;
				}
			}
		}
	}

	UE_LOG(LogEvents, Warning, TEXT("TagPackingTest completed!"));

}

FAutoConsoleCommand TagPackingTestCmd(
	TEXT("Events.PackingTest"), 
	TEXT( "Prints frequency of gameplay tags" ), 
	FConsoleCommandDelegate::CreateStatic(TagPackingTest)
);

#endif
