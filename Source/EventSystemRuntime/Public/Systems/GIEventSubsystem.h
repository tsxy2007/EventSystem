// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GIEventSubsystem.generated.h"


/**
 * 
 */
UCLASS()
class EVENTSYSTEMRUNTIME_API UGIEventSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UGIEventSubsystem() {};
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
};
