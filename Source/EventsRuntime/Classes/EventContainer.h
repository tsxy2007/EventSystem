// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "EventContainer.generated.h"

class UEditableEventQuery;
struct FEventContainer;
struct FPropertyTag;

EVENTSRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogEvents, Log, All);

DECLARE_STATS_GROUP_VERBOSE(TEXT("Gameplay Tags"), STATGROUP_Events, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("FEventContainer::HasTag"), STAT_FEventContainer_HasTag, STATGROUP_Events, EVENTSRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("FEventContainer::DoesTagContainerMatch"), STAT_FEventContainer_DoesTagContainerMatch, STATGROUP_Events, EVENTSRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("UEventsManager::EventsMatch"), STAT_UEventsManager_EventsMatch, STATGROUP_Events, EVENTSRUNTIME_API);

struct FEventContainer;

// DEPRECATED ENUMS
UENUM(BlueprintType)
namespace EEventMatchType
{
	enum Type
	{
		Explicit,			// This will check for a match against just this tag
		IncludeParentTags,	// This will also check for matches against all parent tags
	};
}

UENUM(BlueprintType)
enum class EEventContainerMatchType : uint8
{
	Any,	//	Means the filter is populated by any tag matches in this container.
	All		//	Means the filter is only populated if all of the tags in this container match.
};

typedef uint16 FEventNetIndex;
#define INVALID_TAGNETINDEX MAX_uint16

/**
 * A single gameplay tag, which represents a hierarchical name of the form x.y that is registered in the EventsManager
 * You can filter the gameplay tags displayed in the editor using, meta = (Categories = "Tag1.Tag2.Tag3"))
 */
USTRUCT(BlueprintType, meta = (HasNativeMake = "Events.BlueprintEventLibrary.MakeLiteralEvent", HasNativeBreak = "Events.BlueprintEventLibrary.GetTagName"))
struct EVENTSRUNTIME_API FEventInfo
{
	GENERATED_USTRUCT_BODY()

	/** Constructors */
	FEventInfo()
	{
	}

	/**
	 * Gets the FEventInfo that corresponds to the TagName
	 *
	 * @param TagName The Name of the tag to search for
	 * @param ErrorIfNotfound: ensure() that tag exists.
	 * @return Will return the corresponding FEventInfo or an empty one if not found.
	 */
	static FEventInfo RequestEvent(const FName& TagName, bool ErrorIfNotFound=true);

	/** 
	 * Returns true if this is a valid gameplay tag string (foo.bar.baz). If false, it will fill 
	 * @param TagString String to check for validity
	 * @param OutError If non-null and string invalid, will fill in with an error message
	 * @param OutFixedString If non-null and string invalid, will attempt to fix. Will be empty if no fix is possible
	 * @return True if this can be added to the tag dictionary, false if there's a syntax error
	 */
	static bool IsValidEventString(const FString& TagString, FText* OutError = nullptr, FString* OutFixedString = nullptr);

	/** Operators */
	FORCEINLINE bool operator==(FEventInfo const& Other) const
	{
		return TagName == Other.TagName;
	}

	FORCEINLINE bool operator!=(FEventInfo const& Other) const
	{
		return TagName != Other.TagName;
	}

	FORCEINLINE bool operator<(FEventInfo const& Other) const
	{
		return TagName.LexicalLess(Other.TagName);
	}

	/**
	 * Determine if this tag matches TagToCheck, expanding our parent tags
	 * "A.1".MatchesTag("A") will return True, "A".MatchesTag("A.1") will return False
	 * If TagToCheck is not Valid it will always return False
	 * 
	 * @return True if this tag matches TagToCheck
	 */
	bool MatchesTag(const FEventInfo& TagToCheck) const;

	/**
	 * Determine if TagToCheck is valid and exactly matches this tag
	 * "A.1".MatchesTagExact("A") will return False
	 * If TagToCheck is not Valid it will always return False
	 * 
	 * @return True if TagToCheck is Valid and is exactly this tag
	 */
	FORCEINLINE bool MatchesTagExact(const FEventInfo& TagToCheck) const
	{
		if (!TagToCheck.IsValid())
		{
			return false;
		}
		// Only check check explicit tag list
		return TagName == TagToCheck.TagName;
	}

	/**
	 * Check to see how closely two FEvents match. Higher values indicate more matching terms in the tags.
	 *
	 * @param TagToCheck	Tag to match against
	 *
	 * @return The depth of the match, higher means they are closer to an exact match
	 */
	int32 MatchesTagDepth(const FEventInfo& TagToCheck) const;

	/**
	 * Checks if this tag matches ANY of the tags in the specified container, also checks against our parent tags
	 * "A.1".MatchesAny({"A","B"}) will return True, "A".MatchesAny({"A.1","B"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return False
	 *
	 * @return True if this tag matches ANY of the tags of in ContainerToCheck
	 */
	bool MatchesAny(const FEventContainer& ContainerToCheck) const;

	/**
	 * Checks if this tag matches ANY of the tags in the specified container, only allowing exact matches
	 * "A.1".MatchesAny({"A","B"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return False
	 *
	 * @return True if this tag matches ANY of the tags of in ContainerToCheck exactly
	 */
	bool MatchesAnyExact(const FEventContainer& ContainerToCheck) const;

	/** Returns whether the tag is valid or not; Invalid tags are set to NAME_None and do not exist in the game-specific global dictionary */
	FORCEINLINE bool IsValid() const
	{
		return (TagName != NAME_None);
	}

	/** Returns reference to a EventContainer containing only this tag */
	const FEventContainer& GetSingleTagContainer() const;

	/** Returns direct parent Event of this Event, calling on x.y will return x */
	FEventInfo RequestDirectParent() const;

	/** Returns a new container explicitly containing the tags of this tag */
	FEventContainer GetEventParents() const;

	/** Used so we can have a TMap of this struct */
	FORCEINLINE friend uint32 GetTypeHash(const FEventInfo& Tag)
	{
		return ::GetTypeHash(Tag.TagName);
	}

	/** Displays gameplay tag as a string for blueprint graph usage */
	FORCEINLINE FString ToString() const
	{
		return TagName.ToString();
	}

	/** Get the tag represented as a name */
	FORCEINLINE FName GetTagName() const
	{
		return TagName;
	}

	friend void operator<<(FStructuredArchive::FSlot Slot, FEventInfo& Event)
	{
		Slot << Event.TagName;
	}

	/** Overridden for fast serialize */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Handles fixup and errors. This is only called when not serializing a full FEventContainer */
	void PostSerialize(const FArchive& Ar);
	bool NetSerialize_Packed(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Used to upgrade a Name property to a Event struct property */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

	/** Sets from a ImportText string, used in asset registry */
	void FromExportString(const FString& ExportString);

	/** Handles importing tag strings without (TagName=) in it */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	/** An empty Gameplay Tag */
	static const FEventInfo EmptyTag;

	// DEPRECATED

PRAGMA_DISABLE_DEPRECATION_WARNINGS

	/**
	 * Check to see if two FEvents match with explicit match types
	 *
	 * @param MatchTypeOne	How we compare this tag, Explicitly or a match with any parents as well
	 * @param Other			The second tag to compare against
	 * @param MatchTypeTwo	How we compare Other tag, Explicitly or a match with any parents as well
	 * 
	 * @return True if there is a match according to the specified match types; false if not
	 */
	UE_DEPRECATED(4.15, "Deprecated in favor of MatchesTag")
	FORCEINLINE_DEBUGGABLE bool Matches(TEnumAsByte<EEventMatchType::Type> MatchTypeOne, const FEventInfo& Other, TEnumAsByte<EEventMatchType::Type> MatchTypeTwo) const
	{
		bool bResult = false;
		if (MatchTypeOne == EEventMatchType::Explicit && MatchTypeTwo == EEventMatchType::Explicit)
		{
			bResult = TagName == Other.TagName;
		}
		else
		{
			bResult = ComplexMatches(MatchTypeOne, Other, MatchTypeTwo);
		}
		return bResult;
	}
	/**
	 * Check to see if two FEvents match
	 *
	 * @param MatchTypeOne	How we compare this tag, Explicitly or a match with any parents as well
	 * @param Other			The second tag to compare against
	 * @param MatchTypeTwo	How we compare Other tag, Explicitly or a match with any parents as well
	 * 
	 * @return True if there is a match according to the specified match types; false if not
	 */
	UE_DEPRECATED(4.15, "Deprecated in favor of MatchesTag")
	bool ComplexMatches(TEnumAsByte<EEventMatchType::Type> MatchTypeOne, const FEventInfo& Other, TEnumAsByte<EEventMatchType::Type> MatchTypeTwo) const;

PRAGMA_ENABLE_DEPRECATION_WARNINGS

protected:

	/** Intentionally private so only the tag manager can use */
	explicit FEventInfo(const FName& InTagName);

	/** This Tags Name */
	UPROPERTY(VisibleAnywhere, Category = Events, SaveGame)
	FName TagName;

	friend class UEventsManager;
	friend struct FEventContainer;
	friend struct FEventNode;
};

template<>
struct TStructOpsTypeTraits< FEventInfo > : public TStructOpsTypeTraitsBase2< FEventInfo >
{
	enum
	{
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithPostSerialize = true,
		WithStructuredSerializeFromMismatchedTag = true,
		WithImportTextItem = true,
	};
};

/** A Tag Container holds a collection of FEvents, tags are included explicitly by adding them, and implicitly from adding child tags */
USTRUCT(BlueprintType, meta = (HasNativeMake = "Events.BlueprintEventLibrary.MakeEventContainerFromArray", HasNativeBreak = "Events.BlueprintEventLibrary.BreakEventContainer"))
struct EVENTSRUNTIME_API FEventContainer
{
	GENERATED_USTRUCT_BODY()

	/** Constructors */
	FEventContainer()
	{
	}

	FEventContainer(FEventContainer const& Other)
	{
		*this = Other;
	}

	/** Explicit to prevent people from accidentally using the wrong type of operation */
	explicit FEventContainer(const FEventInfo& Tag)
	{
		AddTag(Tag);
	}

	FEventContainer(FEventContainer&& Other)
		: Events(MoveTemp(Other.Events))
		, ParentTags(MoveTemp(Other.ParentTags))
	{

	}

	~FEventContainer()
	{
	}

	/** Creates a container from an array of tags, this is more efficient than adding them all individually */
	template<class AllocatorType>
	static FEventContainer CreateFromArray(const TArray<FEventInfo, AllocatorType>& SourceTags)
	{
		FEventContainer Container;
		Container.Events.Append(SourceTags);
		Container.FillParentTags();
		return Container;
	}

	/** Assignment/Equality operators */
	FEventContainer& operator=(FEventContainer const& Other);
	FEventContainer& operator=(FEventContainer&& Other);
	bool operator==(FEventContainer const& Other) const;
	bool operator!=(FEventContainer const& Other) const;

	/**
	 * Determine if TagToCheck is present in this container, also checking against parent tags
	 * {"A.1"}.HasTag("A") will return True, {"A"}.HasTag("A.1") will return False
	 * If TagToCheck is not Valid it will always return False
	 * 
	 * @return True if TagToCheck is in this container, false if it is not
	 */
	FORCEINLINE_DEBUGGABLE bool HasTag(const FEventInfo& TagToCheck) const
	{
		if (!TagToCheck.IsValid())
		{
			return false;
		}
		// Check explicit and parent tag list 
		return Events.Contains(TagToCheck) || ParentTags.Contains(TagToCheck);
	}

	/**
	 * Determine if TagToCheck is explicitly present in this container, only allowing exact matches
	 * {"A.1"}.HasTagExact("A") will return False
	 * If TagToCheck is not Valid it will always return False
	 * 
	 * @return True if TagToCheck is in this container, false if it is not
	 */
	FORCEINLINE_DEBUGGABLE bool HasTagExact(const FEventInfo& TagToCheck) const
	{
		if (!TagToCheck.IsValid())
		{
			return false;
		}
		// Only check check explicit tag list
		return Events.Contains(TagToCheck);
	}

	/**
	 * Checks if this container contains ANY of the tags in the specified container, also checks against parent tags
	 * {"A.1"}.HasAny({"A","B"}) will return True, {"A"}.HasAny({"A.1","B"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return False
	 *
	 * @return True if this container has ANY of the tags of in ContainerToCheck
	 */
	FORCEINLINE_DEBUGGABLE bool HasAny(const FEventContainer& ContainerToCheck) const
	{
		if (ContainerToCheck.IsEmpty())
		{
			return false;
		}
		for (const FEventInfo& OtherTag : ContainerToCheck.Events)
		{
			if (Events.Contains(OtherTag) || ParentTags.Contains(OtherTag))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Checks if this container contains ANY of the tags in the specified container, only allowing exact matches
	 * {"A.1"}.HasAny({"A","B"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return False
	 *
	 * @return True if this container has ANY of the tags of in ContainerToCheck
	 */
	FORCEINLINE_DEBUGGABLE bool HasAnyExact(const FEventContainer& ContainerToCheck) const
	{
		if (ContainerToCheck.IsEmpty())
		{
			return false;
		}
		for (const FEventInfo& OtherTag : ContainerToCheck.Events)
		{
			if (Events.Contains(OtherTag))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Checks if this container contains ALL of the tags in the specified container, also checks against parent tags
	 * {"A.1","B.1"}.HasAll({"A","B"}) will return True, {"A","B"}.HasAll({"A.1","B.1"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return True, because there were no failed checks
	 *
	 * @return True if this container has ALL of the tags of in ContainerToCheck, including if ContainerToCheck is empty
	 */
	FORCEINLINE_DEBUGGABLE bool HasAll(const FEventContainer& ContainerToCheck) const
	{
		if (ContainerToCheck.IsEmpty())
		{
			return true;
		}
		for (const FEventInfo& OtherTag : ContainerToCheck.Events)
		{
			if (!Events.Contains(OtherTag) && !ParentTags.Contains(OtherTag))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * Checks if this container contains ALL of the tags in the specified container, only allowing exact matches
	 * {"A.1","B.1"}.HasAll({"A","B"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return True, because there were no failed checks
	 *
	 * @return True if this container has ALL of the tags of in ContainerToCheck, including if ContainerToCheck is empty
	 */
	FORCEINLINE_DEBUGGABLE bool HasAllExact(const FEventContainer& ContainerToCheck) const
	{
		if (ContainerToCheck.IsEmpty())
		{
			return true;
		}
		for (const FEventInfo& OtherTag : ContainerToCheck.Events)
		{
			if (!Events.Contains(OtherTag))
			{
				return false;
			}
		}
		return true;
	}

	/** Returns the number of explicitly added tags */
	FORCEINLINE int32 Num() const
	{
		return Events.Num();
	}

	/** Returns whether the container has any valid tags */
	FORCEINLINE bool IsValid() const
	{
		return Events.Num() > 0;
	}

	/** Returns true if container is empty */
	FORCEINLINE bool IsEmpty() const
	{
		return Events.Num() == 0;
	}

	/** Returns a new container explicitly containing the tags of this container and all of their parent tags */
	FEventContainer GetEventParents() const;

	/**
	 * Returns a filtered version of this container, returns all tags that match against any of the tags in OtherContainer, expanding parents
	 *
	 * @param OtherContainer		The Container to filter against
	 *
	 * @return A FEventContainer containing the filtered tags
	 */
	FEventContainer Filter(const FEventContainer& OtherContainer) const;

	/**
	 * Returns a filtered version of this container, returns all tags that match exactly one in OtherContainer
	 *
	 * @param OtherContainer		The Container to filter against
	 *
	 * @return A FEventContainer containing the filtered tags
	 */
	FEventContainer FilterExact(const FEventContainer& OtherContainer) const;

	/** 
	 * Checks if this container matches the given query.
	 *
	 * @param Query		Query we are checking against
	 *
	 * @return True if this container matches the query, false otherwise.
	 */
	bool MatchesQuery(const struct FEventQuery& Query) const;

	/** 
	 * Adds all the tags from one container to this container 
	 * NOTE: From set theory, this effectively is the union of the container this is called on with Other.
	 *
	 * @param Other TagContainer that has the tags you want to add to this container 
	 */
	void AppendTags(FEventContainer const& Other);

	/** 
	 * Adds all the tags that match between the two specified containers to this container.  WARNING: This matches any
	 * parent tag in A, not just exact matches!  So while this should be the union of the container this is called on with
	 * the intersection of OtherA and OtherB, it's not exactly that.  Since OtherB matches against its parents, any tag
	 * in OtherA which has a parent match with a parent of OtherB will count.  For example, if OtherA has Color.Green
	 * and OtherB has Color.Red, that will count as a match due to the Color parent match!
	 * If you want an exact match, you need to call A.FilterExact(B) (above) to get the intersection of A with B.
	 * If you need the disjunctive union (the union of two sets minus their intersection), use AppendTags to create
	 * Union, FilterExact to create Intersection, and then call Union.RemoveTags(Intersection).
	 *
	 * @param OtherA TagContainer that has the matching tags you want to add to this container, these tags have their parents expanded
	 * @param OtherB TagContainer used to check for matching tags.  If the tag matches on any parent, it counts as a match.
	 */
	void AppendMatchingTags(FEventContainer const& OtherA, FEventContainer const& OtherB);

	/**
	 * Add the specified tag to the container
	 *
	 * @param TagToAdd Tag to add to the container
	 */
	void AddTag(const FEventInfo& TagToAdd);

	/**
	 * Add the specified tag to the container without checking for uniqueness
	 *
	 * @param TagToAdd Tag to add to the container
	 * 
	 * Useful when building container from another data struct (TMap for example)
	 */
	void AddTagFast(const FEventInfo& TagToAdd);

	/**
	 * Adds a tag to the container and removes any direct parents, wont add if child already exists
	 *
	 * @param Tag			The tag to try and add to this container
	 * 
	 * @return True if tag was added
	 */
	bool AddLeafTag(const FEventInfo& TagToAdd);

	/**
	 * Tag to remove from the container
	 * 
	 * @param TagToRemove		Tag to remove from the container
	 * @param bDeferParentTags	Skip calling FillParentTags for performance (must be handled by calling code)
	 */
	bool RemoveTag(const FEventInfo& TagToRemove, bool bDeferParentTags=false);

	/**
	 * Removes all tags in TagsToRemove from this container
	 *
	 * @param TagsToRemove	Tags to remove from the container
	 */
	void RemoveTags(const FEventContainer& TagsToRemove);

	/** Remove all tags from the container. Will maintain slack by default */
	void Reset(int32 Slack = 0);
	
	/** Serialize the tag container */
	bool Serialize(FStructuredArchive::FSlot Slot);

	/** Efficient network serialize, takes advantage of the dictionary */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Handles fixup after importing from text */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	/** Fill in the ParentTags array and any other transient parameters */
	void PostScriptConstruct();

	/** Returns string version of container in ImportText format */
	FString ToString() const;

	/** Sets from a ImportText string, used in asset registry */
	void FromExportString(const FString& ExportString);

	/** Returns abbreviated human readable Tag list without parens or property names. If bQuoted is true it will quote each tag */
	FString ToStringSimple(bool bQuoted = false) const;

	/** Returns human readable description of what match is being looked for on the readable tag list. */
	FText ToMatchingText(EEventContainerMatchType MatchType, bool bInvertCondition) const;

	/** Gets the explicit list of gameplay tags */
	void GetEventArray(TArray<FEventInfo>& InOutEvents) const
	{
		InOutEvents = Events;
	}

	/** Creates a const iterator for the contents of this array */
	TArray<FEventInfo>::TConstIterator CreateConstIterator() const
	{
		return Events.CreateConstIterator();
	}

	bool IsValidIndex(int32 Index) const
	{
		return Events.IsValidIndex(Index);
	}

	FEventInfo GetByIndex(int32 Index) const
	{
		if (IsValidIndex(Index))
		{
			return Events[Index];
		}
		return FEventInfo();
	}	

	FEventInfo First() const
	{
		return Events.Num() > 0 ? Events[0] : FEventInfo();
	}

	FEventInfo Last() const
	{
		return Events.Num() > 0 ? Events.Last() : FEventInfo();
	}

	/** Fills in ParentTags from Events */
	void FillParentTags();

	/** An empty Gameplay Tag Container */
	static const FEventContainer EmptyContainer;

	// DEPRECATED FUNCTIONALITY

PRAGMA_DISABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(4.15, "Deprecated in favor of Reset")
		void RemoveAllTags(int32 Slack = 0)
	{
		Reset(Slack);
	}

	UE_DEPRECATED(4.15, "Deprecated in favor of Reset")
		void RemoveAllTagsKeepSlack()
	{
		Reset();
	}

	/**
	 * Determine if the container has the specified tag. This forces an explicit match. 
	 * This function exists for convenience and brevity. We do not wish to use default values for ::HasTag match type parameters, to avoid confusion on what the default behavior is. (E.g., we want people to think and use the right match type).
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	UE_DEPRECATED(4.15, "Deprecated in favor of HasTagExact")
	FORCEINLINE_DEBUGGABLE bool HasTagExplicit(FEventInfo const& TagToCheck) const
	{
		return HasTag(TagToCheck, EEventMatchType::Explicit, EEventMatchType::Explicit);
	}

	/**
	 * Determine if the container has the specified tag
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param TagToCheckMatchType	Type of match to use for the TagToCheck Param
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	UE_DEPRECATED(4.15, "Deprecated in favor of HasTag with no parameters")
	FORCEINLINE_DEBUGGABLE bool HasTag(FEventInfo const& TagToCheck, TEnumAsByte<EEventMatchType::Type> TagMatchType, TEnumAsByte<EEventMatchType::Type> TagToCheckMatchType) const
	{
		SCOPE_CYCLE_COUNTER(STAT_FEventContainer_HasTag);
		if (!TagToCheck.IsValid())
		{
			return false;
		}

		return HasTagFast(TagToCheck, TagMatchType, TagToCheckMatchType);
	}

	/** Version of above that is called from conditions where you know tag is valid */
	FORCEINLINE_DEBUGGABLE bool HasTagFast(FEventInfo const& TagToCheck, TEnumAsByte<EEventMatchType::Type> TagMatchType, TEnumAsByte<EEventMatchType::Type> TagToCheckMatchType) const
	{
		bool bResult;
		if (TagToCheckMatchType == EEventMatchType::Explicit)
		{
			// Always check explicit
			bResult = Events.Contains(TagToCheck);

			if (!bResult && TagMatchType == EEventMatchType::IncludeParentTags)
			{
				// Check parent tags as well
				bResult = ParentTags.Contains(TagToCheck);
			}
		}
		else
		{
			bResult = ComplexHasTag(TagToCheck, TagMatchType, TagToCheckMatchType);
		}
		return bResult;
	}

	/**
	 * Determine if the container has the specified tag
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param TagToCheckMatchType	Type of match to use for the TagToCheck Param
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	bool ComplexHasTag(FEventInfo const& TagToCheck, TEnumAsByte<EEventMatchType::Type> TagMatchType, TEnumAsByte<EEventMatchType::Type> TagToCheckMatchType) const;

	/**
	 * Checks if this container matches ANY of the tags in the specified container. Performs matching by expanding this container out
	 * to include its parent tags.
	 *
	 * @param Other					Container we are checking against
	 * @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching even if it's empty
	 *
	 * @return True if this container has ANY the tags of the passed in container
	 */
	UE_DEPRECATED(4.15, "Deprecated in favor of HasAny")
	FORCEINLINE_DEBUGGABLE bool MatchesAny(const FEventContainer& Other, bool bCountEmptyAsMatch) const
	{
		if (Other.IsEmpty())
		{
			return bCountEmptyAsMatch;
		}
		return DoesTagContainerMatch(Other, EEventMatchType::IncludeParentTags, EEventMatchType::Explicit, EEventContainerMatchType::Any);
	}

	/**
	 * Checks if this container matches ALL of the tags in the specified container. Performs matching by expanding this container out to
	 * include its parent tags.
	 *
	 * @param Other				Container we are checking against
	 * @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching even if it's empty
	 * 
	 * @return True if this container has ALL the tags of the passed in container
	 */
	UE_DEPRECATED(4.15, "Deprecated in favor of HasAll")
	FORCEINLINE_DEBUGGABLE bool MatchesAll(const FEventContainer& Other, bool bCountEmptyAsMatch) const
	{
		if (Other.IsEmpty())
		{
			return bCountEmptyAsMatch;
		}
		return DoesTagContainerMatch(Other, EEventMatchType::IncludeParentTags, EEventMatchType::Explicit, EEventContainerMatchType::All);
	}

	/**
	 * Returns true if the tags in this container match the tags in OtherContainer for the specified matching types.
	 *
	 * @param OtherContainer		The Container to filter against
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param OtherTagMatchType		Type of match to use for the tags in the OtherContainer param
	 * @param ContainerMatchType	Type of match to use for filtering
	 *
	 * @return Returns true if ContainerMatchType is Any and any of the tags in OtherContainer match the tags in this or ContainerMatchType is All and all of the tags in OtherContainer match at least one tag in this. Returns false otherwise.
	 */
	FORCEINLINE_DEBUGGABLE bool DoesTagContainerMatch(const FEventContainer& OtherContainer, TEnumAsByte<EEventMatchType::Type> TagMatchType, TEnumAsByte<EEventMatchType::Type> OtherTagMatchType, EEventContainerMatchType ContainerMatchType) const
	{
		SCOPE_CYCLE_COUNTER(STAT_FEventContainer_DoesTagContainerMatch);
		bool bResult;
		if (OtherTagMatchType == EEventMatchType::Explicit)
		{
			// Start true for all, start false for any
			bResult = (ContainerMatchType == EEventContainerMatchType::All);
			for (const FEventInfo& OtherTag : OtherContainer.Events)
			{
				if (HasTagFast(OtherTag, TagMatchType, OtherTagMatchType))
				{
					if (ContainerMatchType == EEventContainerMatchType::Any)
					{
						bResult = true;
						break;
					}
				}
				else if (ContainerMatchType == EEventContainerMatchType::All)
				{
					bResult = false;
					break;
				}
			}			
		}
		else
		{
			FEventContainer OtherExpanded = OtherContainer.GetEventParents();
			return DoesTagContainerMatch(OtherExpanded, TagMatchType, EEventMatchType::Explicit, ContainerMatchType);
		}
		return bResult;
	}

	/**
	 * Returns a filtered version of this container, as if the container were filtered by matches from the parameter container
	 *
	 * @param OtherContainer		The Container to filter against
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param OtherTagMatchType		Type of match to use for the tags in the OtherContainer param
	 *
	 * @return A FEventContainer containing the filtered tags
	 */
	UE_DEPRECATED(4.15, "Deprecated in favor of HasAll")
	FEventContainer Filter(const FEventContainer& OtherContainer, TEnumAsByte<EEventMatchType::Type> TagMatchType, TEnumAsByte<EEventMatchType::Type> OtherTagMatchType) const;

PRAGMA_ENABLE_DEPRECATION_WARNINGS

protected:

	/**
	 * Returns true if the tags in this container match the tags in OtherContainer for the specified matching types.
	 *
	 * @param OtherContainer		The Container to filter against
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param OtherTagMatchType		Type of match to use for the tags in the OtherContainer param
	 * @param ContainerMatchType	Type of match to use for filtering
	 *
	 * @return Returns true if ContainerMatchType is Any and any of the tags in OtherContainer match the tags in this or ContainerMatchType is All and all of the tags in OtherContainer match at least one tag in this. Returns false otherwise.
	 */
	bool DoesTagContainerMatchComplex(const FEventContainer& OtherContainer, TEnumAsByte<EEventMatchType::Type> TagMatchType, TEnumAsByte<EEventMatchType::Type> OtherTagMatchType, EEventContainerMatchType ContainerMatchType) const;

	/**
	 * If a Tag with the specified tag name explicitly exists, it will remove that tag and return true.  Otherwise, it 
	   returns false.  It does NOT check the TagName for validity (i.e. the tag could be obsolete and so not exist in
	   the table). It also does NOT check parents (because it cannot do so for a tag that isn't in the table).
	   NOTE: This function should ONLY ever be used by EventsManager when redirecting tags.  Do NOT make this
	   function public!
	 */
	bool RemoveTagByExplicitName(const FName& TagName);

	/** Adds parent tags for a single tag */
	void AddParentsForTag(const FEventInfo& Tag);

	/** Array of gameplay tags */
	UPROPERTY(BlueprintReadWrite, Category=Events, SaveGame) // Change to VisibleAnywhere after fixing up games
	TArray<FEventInfo> Events;

	/** Array of expanded parent tags, in addition to Events. Used to accelerate parent searches. May contain duplicates in some cases */
	UPROPERTY(Transient)
	TArray<FEventInfo> ParentTags;

	friend class UEventsManager;
	friend struct FEventQuery;
	friend struct FEventQueryExpression;
	friend struct FEventNode;
	friend struct FEventInfo;
	
private:

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	
	FORCEINLINE friend TArray<FEventInfo>::TConstIterator begin(const FEventContainer& Array) { return Array.CreateConstIterator(); }
	FORCEINLINE friend TArray<FEventInfo>::TConstIterator end(const FEventContainer& Array) { return TArray<FEventInfo>::TConstIterator(Array.Events, Array.Events.Num()); }
};

FORCEINLINE bool FEventInfo::MatchesAnyExact(const FEventContainer& ContainerToCheck) const
{
	if (ContainerToCheck.IsEmpty())
	{
		return false;
	}
	return ContainerToCheck.Events.Contains(*this);
}

template<>
struct TStructOpsTypeTraits<FEventContainer> : public TStructOpsTypeTraitsBase2<FEventContainer>
{
	enum
	{
		WithStructuredSerializer = true,
		WithIdenticalViaEquality = true,
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
		WithImportTextItem = true,
		WithCopy = true,
		WithPostScriptConstruct = true,
	};
};

struct EVENTSRUNTIME_API FEventNativeAdder
{
	FEventNativeAdder();

	virtual void AddTags() = 0;
};

/**
 *	Helper struct for viewing tag references (assets that reference a tag). Drop this into a struct and set the OnGetgameplayStatName. A details customization
 *	will display a tree view of assets referencing the tag
 */
USTRUCT()
struct FEventReferenceHelper
{
	GENERATED_USTRUCT_BODY()

	FEventReferenceHelper()
	{
	}

	/** 
	 *	Delegate to be called to get the tag we want to inspect. The void* is a raw pointer to the outer struct's data. You can do a static cast to access it. Eg:
	 *	
	 *	ReferenceHelper.OnGetEventName.BindLambda([this](void* RawData) {
	 *		FMyStructType* ThisData = static_cast<FMyStructType*>(RawData);
	 *		return ThisData->MyTag.GetTagName();
	 *	});
	 *
	*/
	DECLARE_DELEGATE_RetVal_OneParam(FName, FOnGetEventName, void* /**RawOuterStructData*/);
	FOnGetEventName OnGetEventName;
};

/** Helper struct: drop this in another struct to get an embedded create new tag widget. */
USTRUCT()
struct FEventCreationWidgetHelper
{
	GENERATED_USTRUCT_BODY()
};

/** Enumerates the list of supported query expression types. */
UENUM()
namespace EEventQueryExprType
{
	enum Type
	{
		Undefined = 0,
		AnyTagsMatch,
		AllTagsMatch,
		NoTagsMatch,
		AnyExprMatch,
		AllExprMatch,
		NoExprMatch,
	};
}

namespace EEventQueryStreamVersion
{
	enum Type
	{
		InitialVersion = 0,

		// -----<new versions can be added before this line>-------------------------------------------------
		// - this needs to be the last line (see note below)
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
}

/**
 * An FEventQuery is a logical query that can be run against an FEventContainer.  A query that succeeds is said to "match".
 * Queries are logical expressions that can test the intersection properties of another tag container (all, any, or none), or the matching state of a set of sub-expressions
 * (all, any, or none). This allows queries to be arbitrarily recursive and very expressive.  For instance, if you wanted to test if a given tag container contained tags 
 * ((A && B) || (C)) && (!D), you would construct your query in the form ALL( ANY( ALL(A,B), ALL(C) ), NONE(D) )
 * 
 * You can expose the query structs to Blueprints and edit them with a custom editor, or you can construct them natively in code. 
 * 
 * Example of how to build a query via code:
 *	FEventQuery Q;
 *	Q.BuildQuery(
 *		FEventQueryExpression()
 * 		.AllTagsMatch()
 *		.AddTag(FEventBase::RequestEvent(FName(TEXT("Animal.Mammal.Dog.Corgi"))))
 *		.AddTag(FEventBase::RequestEvent(FName(TEXT("Plant.Tree.Spruce"))))
 *		);
 * 
 * Queries are internally represented as a byte stream that is memory-efficient and can be evaluated quickly at runtime.
 * Note: these have an extensive details and graph pin customization for editing, so there is no need to expose the internals to Blueprints.
 */
USTRUCT(BlueprintType, meta=(HasNativeMake="Events.BlueprintEventLibrary.MakeEventQuery"))
struct EVENTSRUNTIME_API FEventQuery
{
	GENERATED_BODY();

public:
	FEventQuery();

	FEventQuery(FEventQuery const& Other);
	FEventQuery(FEventQuery&& Other);

	/** Assignment/Equality operators */
	FEventQuery& operator=(FEventQuery const& Other);
	FEventQuery& operator=(FEventQuery&& Other);

private:
	// Note: Properties need to be editable to allow FComponentPropertyWriter to serialize them, but are hidden in the editor by the customizations mentioned above.

	/** Versioning for future token stream protocol changes. See EEventQueryStreamVersion. */
	UPROPERTY(EditAnywhere, Category = Hidden)
	int32 TokenStreamVersion;

	/** List of tags referenced by this entire query. Token stream stored indices into this list. */
	UPROPERTY(EditAnywhere, Category = Hidden)
	TArray<FEventInfo> TagDictionary;

	/** Stream representation of the actual hierarchical query */
	UPROPERTY(EditAnywhere, Category = Hidden)
	TArray<uint8> QueryTokenStream;

	/** User-provided string describing the query */
	UPROPERTY(EditAnywhere, Category = Hidden)
	FString UserDescription;

	/** Auto-generated string describing the query */
	UPROPERTY(EditAnywhere, Category = Hidden)
	FString AutoDescription;

	/** Returns a gameplay tag from the tag dictionary */
	FEventInfo GetTagFromIndex(int32 TagIdx) const
	{
		ensure(TagDictionary.IsValidIndex(TagIdx));
		return TagDictionary[TagIdx];
	}

public:

	/** Replaces existing tags with passed in tags. Does not modify the tag query expression logic. Useful when you need to cache off and update often used query. Must use same sized tag container! */
	void ReplaceTagsFast(FEventContainer const& Tags)
	{
		ensure(Tags.Num() == TagDictionary.Num());
		TagDictionary.Reset();
		TagDictionary.Append(Tags.Events);
	}

	/** Replaces existing tags with passed in tag. Does not modify the tag query expression logic. Useful when you need to cache off and update often used query. */		 
	void ReplaceTagFast(FEventInfo const& Tag)
	{
		ensure(1 == TagDictionary.Num());
		TagDictionary.Reset();
		TagDictionary.Add(Tag);
	}

	/** Returns true if the given tags match this query, or false otherwise. */
	bool Matches(FEventContainer const& Tags) const;

	/** Returns true if this query is empty, false otherwise. */
	bool IsEmpty() const;

	/** Resets this query to its default empty state. */
	void Clear();

	/** Creates this query with the given root expression. */
	void Build(struct FEventQueryExpression& RootQueryExpr, FString InUserDescription = FString());

	/** Static function to assemble and return a query. */
	static FEventQuery BuildQuery(struct FEventQueryExpression& RootQueryExpr, FString InDescription = FString());

	/** Builds a FEventQueryExpression from this query. */
	void GetQueryExpr(struct FEventQueryExpression& OutExpr) const;

	/** Returns description string. */
	const FString& GetDescription() const { return UserDescription.IsEmpty() ? AutoDescription : UserDescription; };

#if WITH_EDITOR
	/** Creates this query based on the given EditableQuery object */
	void BuildFromEditableQuery(class UEditableEventQuery& EditableQuery); 

	/** Creates editable query object tree based on this query */
	UEditableEventQuery* CreateEditableQuery();
#endif // WITH_EDITOR

	static const FEventQuery EmptyQuery;

	/**
	* Shortcuts for easily creating common query types
	* @todo: add more as dictated by use cases
	*/

	/** Creates a tag query that will match if there are any common tags between the given tags and the tags being queries against. */
	static FEventQuery MakeQuery_MatchAnyTags(FEventContainer const& InTags);
	static FEventQuery MakeQuery_MatchAllTags(FEventContainer const& InTags);
	static FEventQuery MakeQuery_MatchNoTags(FEventContainer const& InTags);

	static FEventQuery MakeQuery_MatchTag(FEventInfo const& InTag);

	friend class FQueryEvaluator;
};

struct EVENTSRUNTIME_API FEventQueryExpression
{
	/** 
	 * Fluid syntax approach for setting the type of this expression. 
	 */

	FEventQueryExpression& AnyTagsMatch()
	{
		ExprType = EEventQueryExprType::AnyTagsMatch;
		return *this;
	}

	FEventQueryExpression& AllTagsMatch()
	{
		ExprType = EEventQueryExprType::AllTagsMatch;
		return *this;
	}

	FEventQueryExpression& NoTagsMatch()
	{
		ExprType = EEventQueryExprType::NoTagsMatch;
		return *this;
	}

	FEventQueryExpression& AnyExprMatch()
	{
		ExprType = EEventQueryExprType::AnyExprMatch;
		return *this;
	}

	FEventQueryExpression& AllExprMatch()
	{
		ExprType = EEventQueryExprType::AllExprMatch;
		return *this;
	}

	FEventQueryExpression& NoExprMatch()
	{
		ExprType = EEventQueryExprType::NoExprMatch;
		return *this;
	}

	FEventQueryExpression& AddTag(TCHAR const* TagString)
	{
		return AddTag(FName(TagString));
	}
	FEventQueryExpression& AddTag(FName TagName);
	FEventQueryExpression& AddTag(FEventInfo Tag)
	{
		ensure(UsesTagSet());
		TagSet.Add(Tag);
		return *this;
	}
	
	FEventQueryExpression& AddTags(FEventContainer const& Tags)
	{
		ensure(UsesTagSet());
		TagSet.Append(Tags.Events);
		return *this;
	}

	FEventQueryExpression& AddExpr(FEventQueryExpression& Expr)
	{
		ensure(UsesExprSet());
		ExprSet.Add(Expr);
		return *this;
	}
	
	/** Writes this expression to the given token stream. */
	void EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary) const;

	/** Which type of expression this is. */
	EEventQueryExprType::Type ExprType;
	/** Expression list, for expression types that need it */
	TArray<struct FEventQueryExpression> ExprSet;
	/** Tag list, for expression types that need it */
	TArray<FEventInfo> TagSet;

	/** Returns true if this expression uses the tag data. */
	FORCEINLINE bool UsesTagSet() const
	{
		return (ExprType == EEventQueryExprType::AllTagsMatch) || (ExprType == EEventQueryExprType::AnyTagsMatch) || (ExprType == EEventQueryExprType::NoTagsMatch);
	}
	/** Returns true if this expression uses the expression list data. */
	FORCEINLINE bool UsesExprSet() const
	{
		return (ExprType == EEventQueryExprType::AllExprMatch) || (ExprType == EEventQueryExprType::AnyExprMatch) || (ExprType == EEventQueryExprType::NoExprMatch);
	}
};

template<>
struct TStructOpsTypeTraits<FEventQuery> : public TStructOpsTypeTraitsBase2<FEventQuery>
{
	enum
	{
		WithCopy = true
	};
};



/** 
 * This is an editor-only representation of a query, designed to be editable with a typical property window. 
 * To edit a query in the editor, an FEventQuery is converted to a set of UObjects and edited,  When finished,
 * the query struct is rewritten and these UObjects are discarded.
 * This query representation is not intended for runtime use.
 */
UCLASS(editinlinenew, collapseCategories, Transient) 
class EVENTSRUNTIME_API UEditableEventQuery : public UObject
{
	GENERATED_BODY()

public:
	/** User-supplied description, shown in property details. Auto-generated description is shown if not supplied. */
	UPROPERTY(EditDefaultsOnly, Category = Query)
	FString UserDescription;

	/** Automatically-generated description */
	FString AutoDescription;

	/** The base expression of this query. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = Query)
	class UEditableEventQueryExpression* RootExpression;

#if WITH_EDITOR
	/** Converts this editor query construct into the runtime-usable token stream. */
	void EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString=nullptr) const;

	/** Generates and returns the export text for this query. */
	FString GetTagQueryExportText(FEventQuery const& TagQuery);
#endif  // WITH_EDITOR

private:
	/** Property to hold a gameplay tag query so we can use the exporttext path to get a string representation. */
	UPROPERTY()
	FEventQuery TagQueryExportText_Helper;
};

UCLASS(abstract, editinlinenew, collapseCategories, Transient)
class EVENTSRUNTIME_API UEditableEventQueryExpression : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
public:
	/** Converts this editor query construct into the runtime-usable token stream. */
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString=nullptr) const {};

protected:
	void EmitTagTokens(FEventContainer const& TagsToEmit, TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const;
	void EmitExprListTokens(TArray<UEditableEventQueryExpression*> const& ExprList, TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString) const;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta=(DisplayName="Any Tags Match"))
class UEditableEventQueryExpression_AnyTagsMatch : public UEditableEventQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = Expr)
	FEventContainer Tags;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "All Tags Match"))
class UEditableEventQueryExpression_AllTagsMatch : public UEditableEventQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = Expr)
	FEventContainer Tags;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "No Tags Match"))
class UEditableEventQueryExpression_NoTagsMatch : public UEditableEventQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = Expr)
	FEventContainer Tags;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "Any Expressions Match"))
class UEditableEventQueryExpression_AnyExprMatch : public UEditableEventQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = Expr)
	TArray<UEditableEventQueryExpression*> Expressions;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "All Expressions Match"))
class UEditableEventQueryExpression_AllExprMatch : public UEditableEventQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = Expr)
	TArray<UEditableEventQueryExpression*> Expressions;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "No Expressions Match"))
class UEditableEventQueryExpression_NoExprMatch : public UEditableEventQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = Expr)
	TArray<UEditableEventQueryExpression*> Expressions;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FEventInfo>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

