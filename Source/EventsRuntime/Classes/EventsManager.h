// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "EventContainer.h"
#include "Engine/DataTable.h"

#include "EventsManager.generated.h"

class UEventsList;
struct FStreamableHandle;


USTRUCT(BlueprintInternalUseOnly)
struct FEventParameter
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Event)
		FName Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Event)
		FName Type;
};

/** Simple struct for a table row in the gameplay tag table and element in the ini list */
USTRUCT()
struct FEventTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	/** Tag specified in the table */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Event)
	FName Tag;

	/** Developer comment clarifying the usage of a particular tag, not user facing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Event)
	FString DevComment;

	UPROPERTY(EditAnywhere, Category = Event)
	TArray<FEventParameter> Parameters;

	/** Constructors */
	FEventTableRow() {}
	FEventTableRow(FName InTag, const FString& InDevComment = TEXT(""), const TArray<FEventParameter>& InParameters = {}) :
		Tag(InTag), DevComment(InDevComment),Parameters(InParameters) {}
	EVENTSRUNTIME_API FEventTableRow(FEventTableRow const& Other);

	/** Assignment/Equality operators */
	EVENTSRUNTIME_API FEventTableRow& operator=(FEventTableRow const& Other);
	EVENTSRUNTIME_API bool operator==(FEventTableRow const& Other) const;
	EVENTSRUNTIME_API bool operator!=(FEventTableRow const& Other) const;
	EVENTSRUNTIME_API bool operator<(FEventTableRow const& Other) const;
};

/** Simple struct for a table row in the restricted gameplay tag table and element in the ini list */
USTRUCT()
struct FRestrictedEventTableRow : public FEventTableRow
{
	GENERATED_USTRUCT_BODY()

	/** Tag specified in the table */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Event)
	bool bAllowNonRestrictedChildren;

	/** Constructors */
	FRestrictedEventTableRow() : bAllowNonRestrictedChildren(false) {}
	FRestrictedEventTableRow(FName InTag, 
		const FString& InDevComment = TEXT(""), 
		bool InAllowNonRestrictedChildren = false,
		const TArray<FEventParameter>& InParameters = {}) :
		FEventTableRow(InTag, InDevComment,InParameters), 
		bAllowNonRestrictedChildren(InAllowNonRestrictedChildren) 
	{}
	EVENTSRUNTIME_API FRestrictedEventTableRow(FRestrictedEventTableRow const& Other);

	/** Assignment/Equality operators */
	EVENTSRUNTIME_API FRestrictedEventTableRow& operator=(FRestrictedEventTableRow const& Other);
	EVENTSRUNTIME_API bool operator==(FRestrictedEventTableRow const& Other) const;
	EVENTSRUNTIME_API bool operator!=(FRestrictedEventTableRow const& Other) const;
};

UENUM()
enum class EEventSourceType : uint8
{
	Native,				// Was added from C++ code
	DefaultTagList,		// The default tag list in DefaultEvents.ini
	TagList,			// Another tag list from an ini in Events/*.ini
	RestrictedTagList,	// Restricted tags from an ini
	DataTable,			// From a DataTable
	Invalid,			// Not a real source
};

UENUM()
enum class EEventSelectionType : uint8
{
	None,
	NonRestrictedOnly,
	RestrictedOnly,
	All
};

/** Struct defining where gameplay tags are loaded/saved from. Mostly for the editor */
USTRUCT()
struct FEventSource
{
	GENERATED_USTRUCT_BODY()

	/** Name of this source */
	UPROPERTY()
	FName SourceName;

	/** Type of this source */
	UPROPERTY()
	EEventSourceType SourceType;

	/** If this is bound to an ini object for saving, this is the one */
	UPROPERTY()
	class UEventsList* SourceTagList;

	/** If this has restricted tags and is bound to an ini object for saving, this is the one */
	UPROPERTY()
	class URestrictedEventsList* SourceRestrictedTagList;

	FEventSource() 
		: SourceName(NAME_None), SourceType(EEventSourceType::Invalid), SourceTagList(nullptr), SourceRestrictedTagList(nullptr)
	{
	}

	FEventSource(FName InSourceName, EEventSourceType InSourceType, UEventsList* InSourceTagList = nullptr, URestrictedEventsList* InSourceRestrictedTagList = nullptr) 
		: SourceName(InSourceName), SourceType(InSourceType), SourceTagList(InSourceTagList), SourceRestrictedTagList(InSourceRestrictedTagList)
	{
	}

	static FName GetNativeName()
	{
		static FName NativeName = FName(TEXT("Native"));
		return NativeName;
	}

	static FName GetDefaultName()
	{
		static FName DefaultName = FName(TEXT("DefaultEvents.ini"));
		return DefaultName;
	}

#if WITH_EDITOR
	static FName GetTransientEditorName()
	{
		static FName TransientEditorName = FName(TEXT("TransientEditor"));
		return TransientEditorName;
	}
#endif
};

/** Simple tree node for gameplay tags, this stores metadata about specific tags */
USTRUCT()
struct FEventNode
{
	GENERATED_USTRUCT_BODY()
	FEventNode(){};

	/** Simple constructor, passing redundant data for performance */
	FEventNode(FName InTag, FName InFullTag, TSharedPtr<FEventNode> InParentNode, bool InIsExplicitTag, bool InIsRestrictedTag, bool InAllowNonRestrictedChildren);

	/** Returns a correctly constructed container with only this tag, useful for doing container queries */
	FORCEINLINE const FEventContainer& GetSingleTagContainer() const { return CompleteTagWithParents; }

	/**
	 * Get the complete tag for the node, including all parent tags, delimited by periods
	 * 
	 * @return Complete tag for the node
	 */
	FORCEINLINE const FEventInfo& GetCompleteTag() const { return CompleteTagWithParents.Num() > 0 ? CompleteTagWithParents.Events[0] : FEventInfo::EmptyTag; }
	FORCEINLINE FName GetCompleteTagName() const { return GetCompleteTag().GetTagName(); }
	FORCEINLINE FString GetCompleteTagString() const { return GetCompleteTag().ToString(); }

	/**
	 * Get the simple tag for the node (doesn't include any parent tags)
	 * 
	 * @return Simple tag for the node
	 */
	FORCEINLINE FName GetSimpleTagName() const { return Tag; }

	/**
	 * Get the children nodes of this node
	 * 
	 * @return Reference to the array of the children nodes of this node
	 */
	FORCEINLINE TArray< TSharedPtr<FEventNode> >& GetChildTagNodes() { return ChildTags; }

	/**
	 * Get the children nodes of this node
	 * 
	 * @return Reference to the array of the children nodes of this node
	 */
	FORCEINLINE const TArray< TSharedPtr<FEventNode> >& GetChildTagNodes() const { return ChildTags; }

	/**
	 * Get the parent tag node of this node
	 * 
	 * @return The parent tag node of this node
	 */
	FORCEINLINE TSharedPtr<FEventNode> GetParentTagNode() const { return ParentNode; }

	/**
	* Get the net index of this node
	*
	* @return The net index of this node
	*/
	FORCEINLINE FEventNetIndex GetNetIndex() const { return NetIndex; }

	/** Reset the node of all of its values */
	EVENTSRUNTIME_API void ResetNode();

	/** Returns true if the tag was explicitly specified in code or data */
	FORCEINLINE bool IsExplicitTag() const {
#if WITH_EDITORONLY_DATA
		return bIsExplicitTag;
#endif
		return true;
	}

	/** Returns true if the tag is a restricted tag and allows non-restricted children */
	FORCEINLINE bool GetAllowNonRestrictedChildren() const { 
#if WITH_EDITORONLY_DATA
		return bAllowNonRestrictedChildren;  
#endif
		return true;
	}

	/** Returns true if the tag is a restricted tag */
	FORCEINLINE bool IsRestrictedEvent() const {
#if WITH_EDITORONLY_DATA
		return bIsRestrictedTag;
#endif
		return true;
	}
	TArray<FEventParameter> Parameters;
private:
	/** Raw name for this tag at current rank in the tree */
	FName Tag;

	/** This complete tag is at Events[0], with parents in ParentTags[] */
	FEventContainer CompleteTagWithParents;

	/** Child gameplay tag nodes */
	TArray< TSharedPtr<FEventNode> > ChildTags;

	/** Owner gameplay tag node, if any */
	TSharedPtr<FEventNode> ParentNode;
	
	/** Net Index of this node */
	FEventNetIndex NetIndex;

#if WITH_EDITORONLY_DATA
	/** Package or config file this tag came from. This is the first one added. If None, this is an implicitly added tag */
	FName SourceName;

	/** Comment for this tag */
	FString DevComment;

	/** If this is true then the tag can only have normal tag children if bAllowNonRestrictedChildren is true */
	uint8 bIsRestrictedTag : 1;

	/** If this is true then any children of this tag must come from the restricted tags */
	uint8 bAllowNonRestrictedChildren : 1;

	/** If this is true then the tag was explicitly added and not only implied by its child tags */
	uint8 bIsExplicitTag : 1;

	/** If this is true then at least one tag that inherits from this tag is coming from multiple sources. Used for updating UI in the editor. */
	uint8 bDescendantHasConflict : 1;

	/** If this is true then this tag is coming from multiple sources. No descendants can be changed on this tag until this is resolved. */
	uint8 bNodeHasConflict : 1;

	/** If this is true then at least one tag that this tag descends from is coming from multiple sources. This tag and it's descendants can't be changed in the editor. */
	uint8 bAncestorHasConflict : 1;
#endif 

	friend class UEventsManager;
	friend class SEventWidget;
};

/** Holds data about the tag dictionary, is in a singleton UObject */
UCLASS(config=Engine)
class EVENTSRUNTIME_API UEventsManager : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Destructor */
	~UEventsManager();

	/** Returns the global UEventsManager manager */
	FORCEINLINE static UEventsManager& Get()
	{
		if (SingletonManager == nullptr)
		{
			InitializeManager();
		}

		return *SingletonManager;
	}

	/** Returns possibly nullptr to the manager. Needed for some shutdown cases to avoid reallocating. */
	FORCEINLINE static UEventsManager* GetIfAllocated() { return SingletonManager; }

	/**
	* Adds the gameplay tags corresponding to the strings in the array TagStrings to OutTagsContainer
	*
	* @param TagStrings Array of strings to search for as tags to add to the tag container
	* @param OutTagsContainer Container to add the found tags to.
	* @param ErrorIfNotfound: ensure() that tags exists.
	*
	*/
	void RequestEventContainer(const TArray<FString>& TagStrings, FEventContainer& OutTagsContainer, bool bErrorIfNotFound=true) const;

	/**
	 * Gets the FEventBase that corresponds to the TagName
	 *
	 * @param TagName The Name of the tag to search for
	 * @param ErrorIfNotfound: ensure() that tag exists.
	 * 
	 * @return Will return the corresponding FEventBase or an empty one if not found.
	 */
	FEventInfo RequestEvent(FName TagName, bool ErrorIfNotFound=true) const;

	/** 
	 * Returns true if this is a valid gameplay tag string (foo.bar.baz). If false, it will fill 
	 * @param TagString String to check for validity
	 * @param OutError If non-null and string invalid, will fill in with an error message
	 * @param OutFixedString If non-null and string invalid, will attempt to fix. Will be empty if no fix is possible
	 * @return True if this can be added to the tag dictionary, false if there's a syntax error
	 */
	bool IsValidEventString(const FString& TagString, FText* OutError = nullptr, FString* OutFixedString = nullptr);

	/**
	 *	Searches for a gameplay tag given a partial string. This is slow and intended mainly for console commands/utilities to make
	 *	developer life's easier. This will attempt to match as best as it can. If you pass "A.b" it will match on "A.b." before it matches "a.b.c".
	 */
	FEventInfo FindEventFromPartialString_Slow(FString PartialString) const;

	/**
	 * Registers the given name as a gameplay tag, and tracks that it is being directly referenced from code
	 * This can only be called during engine initialization, the table needs to be locked down before replication
	 *
	 * @param TagName The Name of the tag to add
	 * @param TagDevComment The developer comment clarifying the usage of the tag
	 * 
	 * @return Will return the corresponding FEventBase
	 */
	FEventInfo AddNativeEvent(FName TagName, const FString& TagDevComment = TEXT("(Native)"));

	/** Call to flush the list of native tags, once called it is unsafe to add more */
	void DoneAddingNativeTags();

	static FSimpleMulticastDelegate& OnLastChanceToAddNativeTags();


	void CallOrRegister_OnDoneAddingNativeTagsDelegate(FSimpleMulticastDelegate::FDelegate Delegate);

	/**
	 * Gets a Tag Container containing the supplied tag and all of it's parents as explicit tags
	 *
	 * @param Event The Tag to use at the child most tag for this container
	 * 
	 * @return A Tag Container with the supplied tag and all its parents added explicitly
	 */
	FEventContainer RequestEventParents(const FEventInfo& Event) const;

	/**
	 * Gets a Tag Container containing the all tags in the hierarchy that are children of this tag. Does not return the original tag
	 *
	 * @param Event					The Tag to use at the parent tag
	 * 
	 * @return A Tag Container with the supplied tag and all its parents added explicitly
	 */
	FEventContainer RequestEventChildren(const FEventInfo& Event) const;

	/** Returns direct parent Event of this Event, calling on x.y will return x */
	FEventInfo RequestEventDirectParent(const FEventInfo& Event) const;

	/**
	 * Helper function to get the stored TagContainer containing only this tag, which has searchable ParentTags
	 * @param Event		Tag to get single container of
	 * @return					Pointer to container with this tag
	 */
	FORCEINLINE_DEBUGGABLE const FEventContainer* GetSingleTagContainer(const FEventInfo& Event) const
	{
		TSharedPtr<FEventNode> TagNode = FindTagNode(Event);
		if (TagNode.IsValid())
		{
			return &(TagNode->GetSingleTagContainer());
		}
		return nullptr;
	}

	/**
	 * Checks node tree to see if a FEventNode with the tag exists
	 *
	 * @param TagName	The name of the tag node to search for
	 *
	 * @return A shared pointer to the FEventNode found, or NULL if not found.
	 */
	FORCEINLINE_DEBUGGABLE TSharedPtr<FEventNode> FindTagNode(const FEventInfo& Event) const
	{
		const TSharedPtr<FEventNode>* Node = EventNodeMap.Find(Event);

		if (Node)
		{
			return *Node;
		}
#if WITH_EDITOR
		// Check redirector
		if (GIsEditor && Event.IsValid())
		{
			FEventInfo RedirectedTag = Event;

			RedirectSingleEvent(RedirectedTag, nullptr);

			Node = EventNodeMap.Find(RedirectedTag);

			if (Node)
			{
				return *Node;
			}
		}
#endif
		return nullptr;
	}

	/**
	 * Checks node tree to see if a FEventNode with the name exists
	 *
	 * @param TagName	The name of the tag node to search for
	 *
	 * @return A shared pointer to the FEventNode found, or NULL if not found.
	 */
	FORCEINLINE_DEBUGGABLE TSharedPtr<FEventNode> FindTagNode(FName TagName) const
	{
		FEventInfo PossibleTag(TagName);
		return FindTagNode(PossibleTag);
	}

	/** Loads the tag tables referenced in the EventSettings object */
	void LoadEventTables(bool bAllowAsyncLoad = false);

	/** Loads tag inis contained in the specified path */
	void AddTagIniSearchPath(const FString& RootDir);

	/** Helper function to construct the gameplay tag tree */
	void ConstructEventTree();

	/** Helper function to destroy the gameplay tag tree */
	void DestroyEventTree();

	/** Splits a tag such as x.y.z into an array of names {x,y,z} */
	void SplitEventFName(const FEventInfo& Tag, TArray<FName>& OutNames) const;

	/** Gets the list of all tags in the dictionary */
	void RequestAllEvents(FEventContainer& TagContainer, bool OnlyIncludeDictionaryTags) const;

	/** Returns true if if the passed in name is in the tag dictionary and can be created */
	bool ValidateTagCreation(FName TagName) const;

	/** Returns the tag source for a given tag source name and type, or null if not found */
	const FEventSource* FindTagSource(FName TagSourceName) const;

	/** Returns the tag source for a given tag source name and type, or null if not found */
	FEventSource* FindTagSource(FName TagSourceName);

	/** Fills in an array with all tag sources of a specific type */
	void FindTagSourcesWithType(EEventSourceType TagSourceType, TArray<const FEventSource*>& OutArray) const;

	/**
	 * Check to see how closely two FEvents match. Higher values indicate more matching terms in the tags.
	 *
	 * @param EventOne	The first tag to compare
	 * @param EventTwo	The second tag to compare
	 *
	 * @return the length of the longest matching pair
	 */
	int32 EventsMatchDepth(const FEventInfo& EventOne, const FEventInfo& EventTwo) const;

	/** Returns true if we should import tags from UEventsSettings objects (configured by INI files) */
	bool ShouldImportTagsFromINI() const;

	/** Should we print loading errors when trying to load invalid tags */
	bool ShouldWarnOnInvalidTags() const
	{
		return bShouldWarnOnInvalidTags;
	}

	/** Should use fast replication */
	bool ShouldUseFastReplication() const
	{
		return bUseFastReplication;
	}

	/** Returns the hash of NetworkEventNodeIndex */
	uint32 GetNetworkEventNodeIndexHash() const {	return NetworkEventNodeIndexHash;	}

	/** Returns a list of the ini files that contain restricted tags */
	void GetRestrictedTagConfigFiles(TArray<FString>& RestrictedConfigFiles) const;

	/** Returns a list of the source files that contain restricted tags */
	void GetRestrictedTagSources(TArray<const FEventSource*>& Sources) const;

	/** Returns a list of the owners for a restricted tag config file. May be empty */
	void GetOwnersForTagSource(const FString& SourceName, TArray<FString>& OutOwners) const;

	/** Notification that a tag container has been loaded via serialize */
	void EventContainerLoaded(FEventContainer& Container, FProperty* SerializingProperty) const;

	/** Notification that a gameplay tag has been loaded via serialize */
	void SingleEventLoaded(FEventInfo& Tag, FProperty* SerializingProperty) const;

	/** Handles redirectors for an entire container, will also error on invalid tags */
	void RedirectTagsForContainer(FEventContainer& Container, FProperty* SerializingProperty) const;

	/** Handles redirectors for a single tag, will also error on invalid tag. This is only called for when individual tags are serialized on their own */
	void RedirectSingleEvent(FEventInfo& Tag, FProperty* SerializingProperty) const;

	/** Handles establishing a single tag from an imported tag name (accounts for redirects too). Called when tags are imported via text. */
	bool ImportSingleEvent(FEventInfo& Tag, FName ImportedTagName) const;

	/** Gets a tag name from net index and vice versa, used for replication efficiency */
	FName GetTagNameFromNetIndex(FEventNetIndex Index) const;
	FEventNetIndex GetNetIndexFromTag(const FEventInfo &InTag) const;

	/** Cached number of bits we need to replicate tags. That is, Log2(Number of Tags). Will always be <= 16. */
	int32 NetIndexTrueBitNum;
	
	/** The length in bits of the first segment when net serializing tags. We will serialize NetIndexFirstBitSegment + 1 bit to indicatore "more" (more = second segment that is NetIndexTrueBitNum - NetIndexFirstBitSegment) */
	int32 NetIndexFirstBitSegment;

	/** Numbers of bits to use for replicating container size. This can be set via config. */
	int32 NumBitsForContainerSize;

	/** This is the actual value for an invalid tag "None". This is computed at runtime as (Total number of tags) + 1 */
	FEventNetIndex InvalidTagNetIndex;

	const TArray<TSharedPtr<FEventNode>>& GetNetworkEventNodeIndex() const { return NetworkEventNodeIndex; }

	bool IsNativelyAddedTag(FEventInfo Tag) const;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEventLoaded, const FEventInfo& /*Tag*/)
	FOnEventLoaded OnEventLoadedDelegate;

#if WITH_EDITOR
	/** Gets a Filtered copy of the GameplayRootTags Array based on the comma delimited filter string passed in */
	void GetFilteredGameplayRootTags(const FString& InFilterString, TArray< TSharedPtr<FEventNode> >& OutTagArray) const;

	/** Returns "Categories" meta property from given handle, used for filtering by tag widget */
	FString GetCategoriesMetaFromPropertyHandle(TSharedPtr<class IPropertyHandle> PropertyHandle) const;

	/** Helper function, made to be called by custom OnGetCategoriesMetaFromPropertyHandle handlers  */
	static FString StaticGetCategoriesMetaFromPropertyHandle(TSharedPtr<class IPropertyHandle> PropertyHandle);

	/** Returns "Categories" meta property from given field, used for filtering by tag widget */
	template <typename TFieldType>
	FString GetCategoriesMetaFromField(TFieldType* Field) const
	{
		check(Field);
		if (Field->HasMetaData(NAME_Categories))
		{
			return Field->GetMetaData(NAME_Categories);
		}
		return FString();
	}

	/** Returns "Categories" meta property from given struct, used for filtering by tag widget */
	UE_DEPRECATED(4.22, "Please call GetCategoriesMetaFromField instead.")
	FString GetCategoriesMetaFromStruct(UScriptStruct* Struct) const { return GetCategoriesMetaFromField(Struct); }

	/** Returns "EventFilter" meta property from given function, used for filtering by tag widget for any parameters of the function that end up as BP pins */
	FString GetCategoriesMetaFromFunction(const UFunction* Func, FName ParamName = NAME_None) const;

	/** Gets a list of all gameplay tag nodes added by the specific source */
	void GetAllTagsFromSource(FName TagSource, TArray< TSharedPtr<FEventNode> >& OutTagArray) const;

	/** Returns true if this tag is directly in the dictionary already */
	bool IsDictionaryTag(FName TagName) const;

	/** Returns information about tag. If not found return false */
	bool GetTagEditorData(FName TagName, FString& OutComment, FName &OutTagSource, bool& bOutIsTagExplicit, bool &bOutIsRestrictedTag, bool &bOutAllowNonRestrictedChildren) const;

	/** Refresh the Event tree due to an editor change */
	void EditorRefreshEventTree();

	/** Gets a Tag Container containing all of the tags in the hierarchy that are children of this tag, and were explicitly added to the dictionary */
	FEventContainer RequestEventChildrenInDictionary(const FEventInfo& Event) const;
#if WITH_EDITORONLY_DATA
	/** Gets a Tag Container containing all of the tags in the hierarchy that are children of this tag, were explicitly added to the dictionary, and do not have any explicitly added tags between them and the specified tag */
	FEventContainer RequestEventDirectDescendantsInDictionary(const FEventInfo& Event, EEventSelectionType SelectionType) const;
#endif // WITH_EDITORONLY_DATA

	/** This is called when EditorRefreshEventTree. Useful if you need to do anything editor related when tags are added or removed */
	static FSimpleMulticastDelegate OnEditorRefreshEventTree;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEventDoubleClickedEditor, FEventInfo, FSimpleMulticastDelegate& /* OUT */)
	FOnEventDoubleClickedEditor OnGatherEventDoubleClickedEditor;

	/** Chance to dynamically change filter string based on a property handle */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGetCategoriesMetaFromPropertyHandle, TSharedPtr<IPropertyHandle>, FString& /* OUT */)
	FOnGetCategoriesMetaFromPropertyHandle OnGetCategoriesMetaFromPropertyHandle;

	/** Allows dynamic hiding of gameplay tags in SEventWidget. Allows higher order structs to dynamically change which tags are visible based on its own data */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFilterEventChildren, const FString&  /** FilterString */, TSharedPtr<FEventNode>& /* TagNode */, bool& /* OUT OutShouldHide */)
	FOnFilterEventChildren OnFilterEventChildren;
	
	void NotifyEventDoubleClickedEditor(FString TagName);
	
	bool ShowEventAsHyperLinkEditor(FString TagName);


#endif //WITH_EDITOR

	UE_DEPRECATED(4.15, "Call MatchesTag on FEventBase instead")
	FORCEINLINE_DEBUGGABLE bool EventsMatch(const FEventInfo& EventOne, TEnumAsByte<EEventMatchType::Type> MatchTypeOne, const FEventInfo& EventTwo, TEnumAsByte<EEventMatchType::Type> MatchTypeTwo) const
	{
		SCOPE_CYCLE_COUNTER(STAT_UEventsManager_EventsMatch);
		bool bResult = false;
		if (MatchTypeOne == EEventMatchType::Explicit && MatchTypeTwo == EEventMatchType::Explicit)
		{
			bResult = EventOne == EventTwo;
		}
		else
		{
			// Convert both to their containers and do that match
			const FEventContainer* ContainerOne = GetSingleTagContainer(EventOne);
			const FEventContainer* ContainerTwo = GetSingleTagContainer(EventTwo);
			if (ContainerOne && ContainerTwo)
			{
				bResult = ContainerOne->DoesTagContainerMatch(*ContainerTwo, MatchTypeOne, MatchTypeTwo, EEventContainerMatchType::Any);
			}
		}
		return bResult;
	}

	void PrintReplicationIndices();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Mechanism for tracking what tags are frequently replicated */

	void PrintReplicationFrequencyReport();
	void NotifyTagReplicated(FEventInfo Tag, bool WasInContainer);

	TMap<FEventInfo, int32>	ReplicationCountMap;
	TMap<FEventInfo, int32>	ReplicationCountMap_SingleTags;
	TMap<FEventInfo, int32>	ReplicationCountMap_Containers;
#endif

private:

	/** Initializes the manager */
	static void InitializeManager();

	/** finished loading/adding native tags */
	static FSimpleMulticastDelegate& OnDoneAddingNativeTagsDelegate();

	/** The Tag Manager singleton */
	static UEventsManager* SingletonManager;

	friend class FEventTest;
	friend class FGameplayEffectsTest;
	friend class FEventsModule;
	friend class FEventsEditorModule;
	friend class UEventsSettings;
	friend class SAddNewEventSourceWidget;

	/**
	 * Helper function to insert a tag into a tag node array
	 *
	 * @param Tag							Short name of tag to insert
	 * @param FullTag						Full tag, passed in for performance
	 * @param ParentNode					Parent node, if any, for the tag
	 * @param NodeArray						Node array to insert the new node into, if necessary (if the tag already exists, no insertion will occur)
	 * @param SourceName					File tag was added from
	 * @param DevComment					Comment from developer about this tag
	 * @param bIsExplicitTag				Is the tag explicitly defined or is it implied by the existence of a child tag
	 * @param bIsRestrictedTag				Is the tag a restricted tag or a regular gameplay tag
	 * @param bAllowNonRestrictedChildren	If the tag is a restricted tag, can it have regular gameplay tag children or should all of its children be restricted tags as well?
	 *
	 * @return Index of the node of the tag
	 */
	int32 InsertTagIntoNodeArray(FName Tag, FName FullTag, TSharedPtr<FEventNode> ParentNode, TArray< TSharedPtr<FEventNode> >& NodeArray, FName SourceName, const FEventTableRow& TagRow, const FString& DevComment, bool bIsExplicitTag, bool bIsRestrictedTag, bool bAllowNonRestrictedChildren);

	/** Helper function to populate the tag tree from each table */
	void PopulateTreeFromDataTable(class UDataTable* Table);

	void AddTagTableRow(const FEventTableRow& TagRow, FName SourceName, bool bIsRestrictedTag = false);

	void AddChildrenTags(FEventContainer& TagContainer, TSharedPtr<FEventNode> EventNode, bool RecurseAll=true, bool OnlyIncludeDictionaryTags=false) const;

	void AddTagsFromAdditionalLooseIniFiles(const TArray<FString>& IniFileList);

	/**
	 * Helper function for EventsMatch to get all parents when doing a parent match,
	 * NOTE: Must never be made public as it uses the FNames which should never be exposed
	 * 
	 * @param NameList		The list we are adding all parent complete names too
	 * @param Event	The current Tag we are adding to the list
	 */
	void GetAllParentNodeNames(TSet<FName>& NamesList, TSharedPtr<FEventNode> Event) const;

	/** Returns the tag source for a given tag source name, or null if not found */
	FEventSource* FindOrAddTagSource(FName TagSourceName, EEventSourceType SourceType);

	/** Constructs the net indices for each tag */
	void ConstructNetIndex();

	/** Marks all of the nodes that descend from CurNode as having an ancestor node that has a source conflict. */
	void MarkChildrenOfNodeConflict(TSharedPtr<FEventNode> CurNode);

	/** Roots of gameplay tag nodes */
	TSharedPtr<FEventNode> GameplayRootTag;

	/** Map of Tags to Nodes - Internal use only. FEventBase is inside node structure, do not use FindKey! */
	TMap<FEventInfo, TSharedPtr<FEventNode>> EventNodeMap;

	/** Our aggregated, sorted list of commonly replicated tags. These tags are given lower indices to ensure they replicate in the first bit segment. */
	TArray<FEventInfo> CommonlyReplicatedTags;

	/** List of gameplay tag sources */
	UPROPERTY()
	TArray<FEventSource> TagSources;

	/** List of native tags to add when reconstructing tree */
	TSet<FName> NativeTagsToAdd;

	TSet<FName> RestrictedEventSourceNames;

	TArray<FString> ExtraTagIniList;

	bool bIsConstructingEventTree = false;

	/** Cached runtime value for whether we are using fast replication or not. Initialized from config setting. */
	bool bUseFastReplication;

	/** Cached runtime value for whether we should warn when loading invalid tags */
	bool bShouldWarnOnInvalidTags;

	/** True if native tags have all been added and flushed */
	bool bDoneAddingNativeTags;

	/** String with outlawed characters inside tags */
	FString InvalidTagCharacters;

#if WITH_EDITOR
	// This critical section is to handle an editor-only issue where tag requests come from another thread when async loading from a background thread in FEventContainer::Serialize.
	// This class is not generically threadsafe.
	mutable FCriticalSection EventMapCritical;

	// Transient editor-only tags to support quick-iteration PIE workflows
	TSet<FName> TransientEditorTags;
#endif

	/** Sorted list of nodes, used for network replication */
	TArray<TSharedPtr<FEventNode>> NetworkEventNodeIndex;

	uint32 NetworkEventNodeIndexHash;

	/** Holds all of the valid gameplay-related tags that can be applied to assets */
	UPROPERTY()
	TArray<UDataTable*> EventTables;

	/** The map of ini-configured tag redirectors */
	TMap<FName, FEventInfo> TagRedirects;

	const static FName NAME_Categories;
	const static FName NAME_EventFilter;
};
