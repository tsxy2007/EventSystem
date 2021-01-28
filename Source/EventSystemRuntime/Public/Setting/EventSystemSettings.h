// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/DeveloperSettings.h"
#include "EventSystemSettings.generated.h"


USTRUCT()
struct FEventProperties
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category = EventSystem)
	FString ParamType;

	UPROPERTY(config, EditAnywhere, Category = EventSystem)
	FString ParamName;
};


USTRUCT()
struct FEventSystemEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config, EditAnywhere, Category = "EventSystem")
	FName EventType;

	UPROPERTY(config,EditAnywhere, Category = "EventSystem")
	TArray< struct FEventProperties> EventProperties;
};
/**
 *
 */
UCLASS(config = Game, defaultconfig, notplaceable, meta = (DisplayName = "EventSystem"))
class EVENTSYSTEMRUNTIME_API UEventSystemSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SaveEventes();

	UFUNCTION(BlueprintPure, Category = Settings)
	static UEventSystemSettings* GetEventSystemSettings();
public:
	UPROPERTY(config, EditAnywhere, Category = "Event System", meta = (TitleProperty = "Event Message"))
	TArray<FEventSystemEntry> EventList;
};
