// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.
#pragma once

#include "Modules/ModuleManager.h"

class FEventSystemRuntimeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
