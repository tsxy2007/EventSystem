// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"
#include "EventContainer.h"
#include "EventAssetInterface.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "BlueprintEventLibrary.generated.h"

UCLASS(MinimalAPI, meta=(ScriptName="EventLibrary"))
class UBlueprintEventLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Determine if TagOne matches against TagTwo
	 * 
	 * @param TagOne			Tag to check for match
	 * @param TagTwo			Tag to check match against
	 * @param bExactMatch		If true, the tag has to be exactly present, if false then TagOne will include it's parent tags while matching			
	 * 
	 * @return True if TagOne matches TagTwo
	 */
	UFUNCTION(BlueprintPure, Category="Events", meta = (Keywords = "DoEventsMatch", BlueprintThreadSafe))
	static bool MatchesTag(FEventInfo TagOne, FEventInfo TagTwo, bool bExactMatch);

	/**
	 * Determine if TagOne matches against any tag in OtherContainer
	 * 
	 * @param TagOne			Tag to check for match
	 * @param OtherContainer	Container to check against.
	 * @param bExactMatch		If true, the tag has to be exactly present, if false then TagOne will include it's parent tags while matching
	 * 
	 * @return True if TagOne matches any tags explicitly present in OtherContainer
	 */
	UFUNCTION(BlueprintPure, Category="Events", meta = (BlueprintThreadSafe))
	static bool MatchesAnyTags(FEventInfo TagOne, const FEventContainer& OtherContainer, bool bExactMatch);

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Equal (Event)", CompactNodeTitle="==", BlueprintThreadSafe), Category="Events")
	static bool EqualEqual_Event( FEventInfo A, FEventInfo B );
	
	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="NotEqual (Event)", CompactNodeTitle="!=", BlueprintThreadSafe), Category="Events")
	static bool NotEqual_Event( FEventInfo A, FEventInfo B );

	/** Returns true if the passed in gameplay tag is non-null */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static bool IsEventValid(FEventInfo Event);

	/** Returns FName of this tag */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static FName GetTagName(const FEventInfo& Event);

	/** Creates a literal FEventBase */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static FEventInfo MakeLiteralEvent(FEventInfo Value);

	/**
	 * Get the number of gameplay tags in the specified container
	 * 
	 * @param TagContainer	Tag container to get the number of tags from
	 * 
	 * @return The number of tags in the specified container
	 */
	UFUNCTION(BlueprintPure, Category="Events", meta = (BlueprintThreadSafe))
	static int32 GetNumEventsInContainer(const FEventContainer& TagContainer);

	/**
	 * Check if the tag container has the specified tag
	 *
	 * @param TagContainer			Container to check for the tag
	 * @param Tag					Tag to check for in the container
	 * @param bExactMatch			If true, the tag has to be exactly present, if false then TagContainer will include it's parent tags while matching			
	 *
	 * @return True if the container has the specified tag, false if it does not
	 */
	UFUNCTION(BlueprintPure, Category="Events", meta = (Keywords = "DoesContainerHaveTag", BlueprintThreadSafe))
	static bool HasTag(const FEventContainer& TagContainer, FEventInfo Tag, bool bExactMatch);

	/**
	 * Check if the specified tag container has ANY of the tags in the other container
	 * 
	 * @param TagContainer			Container to check if it matches any of the tags in the other container
	 * @param OtherContainer		Container to check against.
	 * @param bExactMatch			If true, the tag has to be exactly present, if false then TagContainer will include it's parent tags while matching			
	 * 
	 * @return True if the container has ANY of the tags in the other container
	 */
	UFUNCTION(BlueprintPure, Category="Events", meta = (Keywords = "DoesContainerMatchAnyTagsInContainer", BlueprintThreadSafe))
	static bool HasAnyTags(const FEventContainer& TagContainer, const FEventContainer& OtherContainer, bool bExactMatch);

	/**
	 * Check if the specified tag container has ALL of the tags in the other container
	 * 
	 * @param TagContainer			Container to check if it matches all of the tags in the other container
	 * @param OtherContainer		Container to check against. If this is empty, the check will succeed
	 * @param bExactMatch			If true, the tag has to be exactly present, if false then TagContainer will include it's parent tags while matching			
	 * 
	 * @return True if the container has ALL of the tags in the other container
	 */
	UFUNCTION(BlueprintPure, Category="Events", meta = (Keywords = "DoesContainerMatchAllTagsInContainer", BlueprintThreadSafe))
	static bool HasAllTags(const FEventContainer& TagContainer, const FEventContainer& OtherContainer, bool bExactMatch);

	/**
	 * Check if the specified tag query is empty
	 * 
	 * @param TagQuery				Query to check
	 * 
	 * @return True if the query is empty, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static bool IsTagQueryEmpty(const FEventQuery& TagQuery);

	/**
	 * Check if the specified tag container matches the given Tag Query
	 * 
	 * @param TagContainer			Container to check if it matches all of the tags in the other container
	 * @param TagQuery				Query to match against
	 * 
	 * @return True if the container matches the query, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static bool DoesContainerMatchTagQuery(const FEventContainer& TagContainer, const FEventQuery& TagQuery);

	/**
	 * Get an array of all actors of a specific class (or subclass of that class) which match the specified gameplay tag query.
	 * 
	 * @param ActorClass			Class of actors to fetch
	 * @param EventQuery		Query to match against
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category="Events",  meta=(WorldContext="WorldContextObject", DeterminesOutputType="ActorClass", DynamicOutputParam="OutActors"))
	static void GetAllActorsOfClassMatchingTagQuery(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FEventQuery& EventQuery, TArray<AActor*>& OutActors);

	/**
	 * Adds a single tag to the passed in tag container
	 *
	 * @param InOutTagContainer		The container that will be appended too.
	 * @param Tag					The tag to add to the container
	 */
	UFUNCTION(BlueprintCallable, Category = "Events", meta = (BlueprintThreadSafe))
	static void AddEvent(UPARAM(ref) FEventContainer& TagContainer, FEventInfo Tag);

	/**
	 * Remove a single tag from the passed in tag container, returns true if found
	 *
	 * @param InOutTagContainer		The container that will be appended too.
	 * @param Tag					The tag to add to the container
	 */
	UFUNCTION(BlueprintCallable, Category = "Events", meta = (BlueprintThreadSafe))
	static bool RemoveEvent(UPARAM(ref) FEventContainer& TagContainer, FEventInfo Tag);

	/**
	 * Appends all tags in the InTagContainer to InOutTagContainer
	 *
	 * @param InOutTagContainer		The container that will be appended too.
	 * @param InTagContainer		The container to append.
	 */
	UFUNCTION(BlueprintCallable, Category = "Events", meta = (BlueprintThreadSafe))
	static void AppendEventContainers(UPARAM(ref) FEventContainer& InOutTagContainer, const FEventContainer& InTagContainer);

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Equal (EventContainer)", CompactNodeTitle="==", BlueprintThreadSafe), Category="Events")
	static bool EqualEqual_EventContainer( const FEventContainer& A, const FEventContainer& B );
	
	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="NotEqual (EventContainer)", CompactNodeTitle="!=", BlueprintThreadSafe), Category="Events")
	static bool NotEqual_EventContainer( const FEventContainer& A, const FEventContainer& B );

	/** Creates a literal FEventContainer */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static FEventContainer MakeLiteralEventContainer(FEventContainer Value);

	/** Creates a FEventContainer from the array of passed in tags */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static FEventContainer MakeEventContainerFromArray(const TArray<FEventInfo>& Events);

	/** Creates a FEventContainer containing a single tag */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static FEventContainer MakeEventContainerFromTag(FEventInfo SingleTag);

	/** Breaks tag container into explicit array of tags */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static void BreakEventContainer(const FEventContainer& EventContainer, TArray<FEventInfo>& Events);

	/**
	 * Creates a literal FEventQuery
	 *
	 * @param	TagQuery	value to set the FEventQuery to
	 *
	 * @return	The literal FEventQuery
	 */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static FEventQuery MakeEventQuery(FEventQuery TagQuery);

	/**
	 * Check Gameplay tags in the interface has all of the specified tags in the tag container (expands to include parents of asset tags)
	 *
	 * @param TagContainerInterface		An Interface to a tag container
	 * @param OtherContainer			A Tag Container
	 *
	 * @return True if the tagcontainer in the interface has all the tags inside the container.
	 */
	UFUNCTION(BlueprintPure, meta = (BlueprintInternalUseOnly = "TRUE"))
	static bool HasAllMatchingEvents(TScriptInterface<IEventAssetInterface> TagContainerInterface, const FEventContainer& OtherContainer);

	/**
	 * Check if the specified tag container has the specified tag, using the specified tag matching types
	 *
	 * @param TagContainerInterface		An Interface to a tag container
	 * @param Tag						Tag to check for in the container
	 *
	 * @return True if the container has the specified tag, false if it does not
	 */
	UFUNCTION(BlueprintPure, meta = (BlueprintInternalUseOnly = "TRUE"))
	static bool DoesTagAssetInterfaceHaveTag(TScriptInterface<IEventAssetInterface> TagContainerInterface, FEventInfo Tag);

	/** Checks if a gameplay tag's name and a string are not equal to one another */
	UFUNCTION(BlueprintPure, Category = PinOptions, meta = (BlueprintInternalUseOnly = "TRUE"))
	static bool NotEqual_TagTag(FEventInfo A, FString B);

	/** Checks if a gameplay tag containers's name and a string are not equal to one another */
	UFUNCTION(BlueprintPure, Category = PinOptions, meta = (BlueprintInternalUseOnly = "TRUE"))
	static bool NotEqual_TagContainerTagContainer(FEventContainer A, FString B);
	
	/**
	 * Returns an FString listing all of the gameplay tags in the tag container for debugging purposes.
	 *
	 * @param TagContainer	The tag container to get the debug string from.
	 */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static FString GetDebugStringFromEventContainer(const FEventContainer& TagContainer);

	/**
	 * Returns an FString representation of a gameplay tag for debugging purposes.
	 *
	 * @param Event	The tag to get the debug string from.
	 */
	UFUNCTION(BlueprintPure, Category = "Events", meta = (BlueprintThreadSafe))
	static FString GetDebugStringFromEvent(FEventInfo Event);

};
