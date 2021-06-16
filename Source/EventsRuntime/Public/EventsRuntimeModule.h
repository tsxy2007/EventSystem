// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "EngineGlobals.h"
#include "EventsManager.h"

/**
 * The public interface to this module, generally you should access the manager directly instead
 */
class IEventsModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IEventsModule& Get()
	{
		static const FName EventModuleName(TEXT("EventsRuntime"));
		return FModuleManager::LoadModuleChecked< IEventsModule >(EventModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		static const FName EventModuleName(TEXT("EventsRuntime"));
		return FModuleManager::Get().IsModuleLoaded(EventModuleName);
	}

	/** Delegate for when assets are added to the tree */
	static EVENTSRUNTIME_API FSimpleMulticastDelegate OnEventTreeChanged;

	/** Delegate that gets called after the settings have changed in the editor */
	static EVENTSRUNTIME_API FSimpleMulticastDelegate OnTagSettingsChanged;
};
