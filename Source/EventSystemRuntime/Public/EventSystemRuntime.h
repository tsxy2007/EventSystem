// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.
#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(EventSystem, All, All);

class FEventSystemRuntimeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override {};
	virtual void ShutdownModule() override {};
};
