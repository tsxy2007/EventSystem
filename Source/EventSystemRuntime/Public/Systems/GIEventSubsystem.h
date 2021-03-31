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
	enum EGenerateNewHandleType
	{
		GenerateNewHandle
	};
	FEventHandle()
		: Handle(0)
	{
	}

	/** Creates a handle pointing to a new instance */
	explicit FEventHandle(EGenerateNewHandleType)
		: Handle(GenerateNewID())
	{
	}

	/** Returns true if this was ever bound to a delegate, but you need to check with the owning delegate to confirm it is still valid */
	bool IsValid() const
	{
		return Handle != 0;
	}

	/** Clear handle to indicate it is no longer bound */
	void Reset()
	{
		Handle = 0;
	}

	friend uint32 GetTypeHash(const FEventHandle& Key)
	{
		return GetTypeHash(Key.Handle);
	}

	void GenerateHandle()
	{
		Handle = GenerateNewID();
	}

private:
	friend bool operator==(const FEventHandle& Lhs, const FEventHandle& Rhs)
	{
		return Lhs.Handle == Rhs.Handle;
	}

	friend bool operator!=(const FEventHandle& Lhs, const FEventHandle& Rhs)
	{
		return Lhs.Handle != Rhs.Handle;
	}

private:
	static EVENTSYSTEMRUNTIME_API uint64 GenerateNewID();
public:
	UPROPERTY(Transient)
	uint64 Handle;
};


struct FListener
{
	FListener() :
	Listener(nullptr),
	EventName(TEXT("None")),
	MsgId(TEXT(""))
	{};

	FListener(UObject* InListener,FName InEventName,FName InMsgID)
		:Listener(InListener), 
		EventName(InEventName),
		MsgId(InMsgID)
	{}

	friend bool operator==(const FListener& Lhs, const FListener& Rhs)
	{
		return Lhs.EventName == Rhs.EventName 
			&& Lhs.Listener == Rhs.Listener;
	}

	const FEventHandle& Handle() 
	{
		if (!mHandle.IsValid())
		{
			mHandle.GenerateHandle();
		}
		return mHandle;
	};
public:
	FEventHandle mHandle;
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
	TMap<FString, TArray<FListener>> ListenerMap;
};
