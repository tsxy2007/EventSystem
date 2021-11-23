// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/Tuple.h"
#include <tuple>
#include "GIEventSubsystem.generated.h"

struct FOutputParam
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

	FEventHandle(UObject* InListener, FName InEventName, FName InMsgID);

	friend bool operator==(const FEventHandle& Lhs, const FEventHandle& Rhs)
	{
		return Lhs.EventName == Rhs.EventName 
			&& Lhs.Listener.Get() == Rhs.Listener.Get() && Lhs.MsgId == Rhs.MsgId;
	}

	friend bool operator!=(const FEventHandle& Lhs, const FEventHandle& Rhs)
	{
		return Lhs.EventName != Rhs.EventName
			|| Lhs.Listener.Get() != Rhs.Listener.Get() || Lhs.MsgId != Rhs.MsgId;
	}
	friend uint32 GetTypeHash(const FEventHandle& Handle)
	{
		return (GetTypeHash(Handle.MsgId) + 12 * GetTypeHash(Handle.Listener.Get()) + 23 * GetTypeHash(Handle.EventName));
	}

	FORCEINLINE FString ToString() const
	{
		FString ListenerName = Listener.Get() && Listener.Get()->IsValidLowLevel() ? Listener.Get()->GetName() : TEXT("");
		return FString::Printf(TEXT("MsgID : %s; EventName: %s; Listener : %s"),*MsgId.ToString(),*EventName.ToString(), *ListenerName);
	}
	FName GetMsgId() { return MsgId; }
public:
	TWeakObjectPtr<UObject> Listener;
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

	void NotifyEventWithParams(const FString& EventId, UObject* Sender, const TArray<FOutputParam, TInlineAllocator<8>>& Outparames);
	const FEventHandle ListenEvent(const FString& MessageId, UObject* Listener, FName EventName);
	void UnListenEvent(const FEventHandle& InHandle);
	void UnListenEvents(UObject* Listener); // FIX (blowpunch)

	static UGIEventSubsystem* Get(const UObject* WorldContext);

	template<typename... TArgs>
	void NotifyEvent(const FString& EventId, UObject* Sender, TArgs&&... Args);

private:
	TMap<FString, TSet<FEventHandle>> ListenerMap;
};

template<typename T>
FOutputParam MakeOutputParam(T& t)
{
	FOutputParam OutputParam;
	OutputParam.PropAddr = (uint8*)std::addressof(t);
	return OutputParam;
}

template<typename T,size_t... Is>
TArray<FOutputParam, TInlineAllocator<8>> MakeOutputParamFromTuple(T& Tuple, const std::index_sequence<Is...>&)
{
	return TArray<FOutputParam, TInlineAllocator<8>>{MakeOutputParam(std::get<Is>(Tuple))...};
}

template<typename T>
TArray<FOutputParam, TInlineAllocator<8>> MakeParam(T& tup)
{
	return MakeOutputParamFromTuple(tup, std::make_index_sequence<std::tuple_size<T>::value>());
}


template<typename... TArgs>
void UGIEventSubsystem::NotifyEvent(const FString& EventId, UObject* Sender, TArgs&&... Args)
{
	std::tuple<TArgs...> InParams(std::forward<TArgs>(Args)...);

	TArray<FOutputParam, TInlineAllocator<8>> OutputParam = MakeParam(InParams);

	this->NotifyEventWithParams(EventId, Sender, OutputParam);
}
