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

USTRUCT(BlueprintType)
struct FEventHandle
{
	GENERATED_USTRUCT_BODY();
public:
	FEventHandle() :
	Listener(nullptr),
	EventName(TEXT("")),
	MsgId(TEXT(""))
	{};

	FEventHandle(UObject* InListener,FName InEventName,FName InMsgID)
		:Listener(InListener), 
		EventName(InEventName),
		MsgId(InMsgID)
	{}

	friend bool operator==(const FEventHandle& Lhs, const FEventHandle& Rhs)
	{
		return Lhs.EventName == Rhs.EventName 
			&& Lhs.Listener == Rhs.Listener && Lhs.MsgId == Rhs.MsgId;
	}

	friend bool operator!=(const FEventHandle& Lhs, const FEventHandle& Rhs)
	{
		return Lhs.EventName != Rhs.EventName
			|| Lhs.Listener != Rhs.Listener || Lhs.MsgId != Rhs.MsgId;
	}
	friend uint32 GetTypeHash(const FEventHandle& Handle)
	{
		return (GetTypeHash(Handle.MsgId) + 12 * GetTypeHash(Handle.Listener) + 23 * GetTypeHash(Handle.EventName));
	}

	FORCEINLINE FString ToString() const
	{
		FString ListenerName = Listener && Listener->IsValidLowLevel() ? Listener->GetName() : TEXT("");
		return FString::Printf(TEXT("MsgID : %s; EventName: %s; Listener : %s"),*MsgId.ToString(),*EventName.ToString(), *ListenerName);
	}
public:
	UObject* Listener;
	FName EventName;
	FName MsgId;
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
	const FEventHandle ListenMessage(const FString& MessageId, UObject* Listener, FName EventName);

	static UGIEventSubsystem* Get(const UObject* WorldContext);

private:
	TMap<FString, TSet<FEventHandle>> ListenerMap;
};
