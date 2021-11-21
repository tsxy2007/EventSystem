// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/Tuple.h"
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

	void NotifyEvent(const FString& EventId, UObject* Sender, const TArray<FOutputParam, TInlineAllocator<8>>& Outparames);
	const FEventHandle ListenEvent(const FString& MessageId, UObject* Listener, FName EventName);
	void UnListenEvent(const FEventHandle& InHandle);
	void UnListenEvents(UObject* Listener); // FIX (blowpunch)

	static UGIEventSubsystem* Get(const UObject* WorldContext);

	template<typename... TArgs>
	void NotifyEvent(const FString& EventId,TArgs&&... Args);

private:
	TMap<FString, TSet<FEventHandle>> ListenerMap;
};

template<typename... TArgs>
void UGIEventSubsystem::NotifyEvent(const FString& EventId,TArgs&&... Args)
{
	TTuple<TArgs...> InParams(Forward<TArgs>(Args)...);
	//拿到字节码首地址 我们通过一个个字节码进行访问
	uint8* Code = (uint8*)&InParams;


	if (!ListenerMap.Contains(EventId)) return;

	TArray<FEventHandle> ListenersToRemove;

	{
		TSet<FEventHandle> Listeners = ListenerMap.FindChecked(EventId);
		for (const auto& Listen : Listeners)
		{
			if (!Listen.Listener.Get())
			{
				ListenersToRemove.Add(Listen);
				continue;
			}

			// FIX (blowpunch)
			if (Listen.Listener.Get()->IsPendingKillOrUnreachable())
			{
				//UE_LOG(EventSystem, Warning, TEXT("Listener %s is pending kill or unreachable!"), *Listen.Listener.Get()->GetFName().ToString());
				continue;
			}
			///

			UFunction* Function = Listen.Listener.Get()->FindFunction(Listen.EventName);
			//Listen.Listener.Get()->ProcessEvent(Function, Args);

			/*if (Function)
			{
				auto Params = FMemory_Alloca(Function->ParmsSize);
				FMemory::Memzero(Params, Function->ParmsSize);
				int32 Index = 0;
				for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
				{
					if (It->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						break;
					}
					FProperty* Prop = *It;

					Prop->CopyCompleteValue(Prop->ContainerPtrToValuePtr<void>(Params), Code);
					Code += Prop->GetSize();
				}
				Listen.Listener.Get()->ProcessEvent(Function, Params);
			}*/
		}
	}
}
