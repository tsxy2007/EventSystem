// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "IPropertyTypeCustomization.h"


/**
 * The public interface to this module
 */
class IEventsEditorModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IEventsEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IEventsEditorModule >( "EventsEditor" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "EventsEditor" );
	}

	/** Tries to add a new gameplay tag to the ini lists */
	EVENTSEDITOR_API virtual bool AddNewEventToINI(const FString& NewTag, const FString& Comment = TEXT(""), FName TagSourceName = NAME_None, TArray<FEventParameter> Parameters = {}, bool bIsRestrictedTag = false, bool bAllowNonRestrictedChildren = true) = 0;

	/** Tries to delete a tag from the library. This will pop up special UI or error messages as needed. It will also delete redirectors if that is specified. */
	EVENTSEDITOR_API virtual bool DeleteTagFromINI(TSharedPtr<struct FEventNode> TagNodeToDelete) = 0;

	/** Tries to rename a tag, leaving a rediretor in the ini, and adding the new tag if it does not exist yet */
	EVENTSEDITOR_API virtual bool RenameTagInINI(const FString& TagToRename, const FString& TagToRenameTo) = 0;

	/** Updates info about a tag */
	EVENTSEDITOR_API virtual bool UpdateTagInINI(const FString& TagToUpdate, const FString& Comment, bool bIsRestrictedTag, bool bAllowNonRestrictedChildren) = 0;

	/** Adds a transient gameplay tag (only valid for the current editor session) */
	EVENTSEDITOR_API virtual bool AddTransientEditorEvent(const FString& NewTransientTag) = 0;
};

/** This is public so that child structs of FEvent can use the details customization */
struct EVENTSEDITOR_API FEventCustomizationPublic
{
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
};

struct EVENTSEDITOR_API FRestrictedEventCustomizationPublic
{
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
};