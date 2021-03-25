// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GIEventSubsystem.generated.h"

struct FPyOutputParam
{
	FProperty* Property = nullptr;
	uint8* PropAddr = nullptr;
};

USTRUCT()
struct FListener
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY()
	UObject* Listener;
	UPROPERTY()
	FName EventName;

	FListener() :
		Listener(nullptr),
		EventName(TEXT("None"))
	{};
	FListener(UObject* InListener,FName InEventName)
		:Listener(InListener),
		EventName(InEventName)
	{}
};
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

	void NotifyMessage(const FString& EventId, UObject* Sender, const TArray<FPyOutputParam, TInlineAllocator<8>>& Outparames);
	void ListenMessage(const FString& MessageId, UObject* Listener, FName EventName);

	static UGIEventSubsystem* Get(const UObject* WorldContext);

private:
	TMultiMap<FString, FListener> ListenerMap;
};
