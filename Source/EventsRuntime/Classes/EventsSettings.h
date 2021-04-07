// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "EventsManager.h"
#include "EventsSettings.generated.h"

/** A single redirect from a deleted tag to the new tag that should replace it */
USTRUCT()
struct EVENTSRUNTIME_API FEventRedirect
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Events)
	FName OldTagName;

	UPROPERTY(EditAnywhere, Category = Events)
	FName NewTagName;

	friend inline bool operator==(const FEventRedirect& A, const FEventRedirect& B)
	{
		return A.OldTagName == B.OldTagName && A.NewTagName == B.NewTagName;
	}

	// This enables lookups by old tag name via FindByKey
	bool operator==(FName OtherOldTagName) const
	{
		return OldTagName == OtherOldTagName;
	}
};

/** Category remapping. This allows base engine tag category meta data to remap to multiple project-specific categories. */
USTRUCT()
struct EVENTSRUNTIME_API FEventCategoryRemap
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Events)
	FString BaseCategory;

	UPROPERTY(EditAnywhere, Category = Events)
	TArray<FString> RemapCategories;

	friend inline bool operator==(const FEventCategoryRemap& A, const FEventCategoryRemap& B)
	{
		return A.BaseCategory == B.BaseCategory && A.RemapCategories == B.RemapCategories;
	}
};

/** Base class for storing a list of gameplay tags as an ini list. This is used for both the central list and additional lists */
UCLASS(config = EventsList, notplaceable)
class EVENTSRUNTIME_API UEventsList : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Relative path to the ini file that is backing this list */
	UPROPERTY()
	FString ConfigFileName;

	/** List of tags saved to this file */
	UPROPERTY(config, EditAnywhere, Category = Events)
	TArray<FEventTableRow> EventList;

	/** Sorts tags alphabetically */
	void SortTags();
};

/** Base class for storing a list of restricted gameplay tags as an ini list. This is used for both the central list and additional lists */
UCLASS(config = Events, notplaceable)
class EVENTSRUNTIME_API URestrictedEventsList : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Relative path to the ini file that is backing this list */
	UPROPERTY()
	FString ConfigFileName;

	/** List of restricted tags saved to this file */
	UPROPERTY(config, EditAnywhere, Category = Events)
	TArray<FRestrictedEventTableRow> RestrictedEventList;

	/** Sorts tags alphabetically */
	void SortTags();
};

USTRUCT()
struct EVENTSRUNTIME_API FEventRestrictedConfigInfo
{
	GENERATED_BODY()

	/** Allows new tags to be saved into their own INI file. This is make merging easier for non technical developers by setting up their own ini file. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Events)
	FString RestrictedConfigName;

	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Events)
	TArray<FString> Owners;

	bool operator==(const FEventRestrictedConfigInfo& Other) const;
	bool operator!=(const FEventRestrictedConfigInfo& Other) const;
};

/**
 *	Class for importing Events directly from a config file.
 *	FEventsEditorModule::StartupModule adds this class to the Project Settings menu to be edited.
 *	Editing this in Project Settings will output changes to Config/DefaultEvents.ini.
 *	
 *	Primary advantages of this approach are:
 *	-Adding new tags doesn't require checking out external and editing file (CSV or xls) then reimporting.
 *	-New tags are mergeable since .ini are text and non exclusive checkout.
 *	
 *	To do:
 *	-Better support could be added for adding new tags. We could match existing tags and autocomplete subtags as
 *	the user types (e.g, autocomplete 'Damage.Physical' as the user is adding a 'Damage.Physical.Slash' tag).
 *	
 */
UCLASS(config=Events, defaultconfig, notplaceable)
class EVENTSRUNTIME_API UEventsSettings : public UEventsList
{
	GENERATED_UCLASS_BODY()

	/** If true, will import tags from ini files in the config/tags folder */
	UPROPERTY(config, EditAnywhere, Category = Events)
	bool ImportTagsFromConfig;

	/** If true, will give load warnings when reading in saved tag references that are not in the dictionary */
	UPROPERTY(config, EditAnywhere, Category = Events)
	bool WarnOnInvalidTags;

	/** If true, will replicate gameplay tags by index instead of name. For this to work, tags must be identical on client and server */
	UPROPERTY(config, EditAnywhere, Category = "Advanced Replication")
	bool FastReplication;

	/** These characters cannot be used in gameplay tags, in addition to special ones like newline*/
	UPROPERTY(config, EditAnywhere, Category = Events)
	FString InvalidTagCharacters;

	/** Category remapping. This allows base engine tag category meta data to remap to multiple project-specific categories. */
	UPROPERTY(config, EditAnywhere, Category = Events)
	TArray<FEventCategoryRemap> CategoryRemapping;

	/** List of data tables to load tags from */
	UPROPERTY(config, EditAnywhere, Category = Events, meta = (AllowedClasses = "DataTable"))
	TArray<FSoftObjectPath> EventTableList;

	/** List of active tag redirects */
	UPROPERTY(config, EditAnywhere, Category = Events)
	TArray<FEventRedirect> EventRedirects;

	/** List of most frequently replicated tags */
	UPROPERTY(config, EditAnywhere, Category = "Advanced Replication")
	TArray<FName> CommonlyReplicatedTags;

	/** Numbers of bits to use for replicating container size, set this based on how large your containers tend to be */
	UPROPERTY(config, EditAnywhere, Category = "Advanced Replication")
	int32 NumBitsForContainerSize;

	/** The length in bits of the first segment when net serializing tags. We will serialize NetIndexFirstBitSegment + 1 bit to indicate "more", which is slower to replicate */
	UPROPERTY(config, EditAnywhere, Category= "Advanced Replication")
	int32 NetIndexFirstBitSegment;

	/** A list of .ini files used to store restricted gameplay tags. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Advanced Events")
	TArray<FEventRestrictedConfigInfo> RestrictedConfigFiles;
#if WITH_EDITORONLY_DATA
	// Dummy parameter used to hook the editor UI
	/** Restricted Events.
	 * 
	 *  Restricted tags are intended to be top level tags that are important for your data hierarchy and modified by very few people.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, transient, Category = "Advanced Events")
	FString RestrictedTagList;
#endif

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
	// temporary copy of RestrictedConfigFiles that we use to identify changes in the list
	// this is required to autopopulate the owners field
	TArray<FEventRestrictedConfigInfo> RestrictedConfigFilesTempCopy;
#endif
};

UCLASS(config=Events, notplaceable)
class EVENTSRUNTIME_API UEventsDeveloperSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Allows new tags to be saved into their own INI file. This is make merging easier for non technical developers by setting up their own ini file. */
	UPROPERTY(config, EditAnywhere, Category=Events)
	FString DeveloperConfigName;
};
