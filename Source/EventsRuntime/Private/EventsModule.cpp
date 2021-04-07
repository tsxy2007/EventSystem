// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventsRuntimeModule.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"

FSimpleMulticastDelegate IEventsModule::OnEventTreeChanged;
FSimpleMulticastDelegate IEventsModule::OnTagSettingsChanged;

class FEventsModule : public IEventsModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface
};

IMPLEMENT_MODULE( FEventsModule, EventsRuntime )
DEFINE_LOG_CATEGORY(LogEvents);

void FEventsModule::StartupModule()
{
	// This will force initialization
	UEventsManager::Get();
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 EventPrintReportOnShutdown = 0;
static FAutoConsoleVariableRef CVarEventPrintReportOnShutdown(TEXT("Events.PrintReportOnShutdown"), EventPrintReportOnShutdown, TEXT("Print gameplay tag replication report on shutdown"), ECVF_Default );
#endif


void FEventsModule::ShutdownModule()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (EventPrintReportOnShutdown)
	{
		UEventsManager::Get().PrintReplicationFrequencyReport();
	}
#endif

	UEventsManager::SingletonManager = nullptr;
}
