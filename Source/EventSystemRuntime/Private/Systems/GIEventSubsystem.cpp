// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "Systems/GIEventSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY(EventSystem);

FEventHandle::FEventHandle(UObject* InListener, FName InEventName, FName InMsgID)
{
	Listener = TWeakObjectPtr<UObject>(InListener);
	EventName = InEventName;
	MsgId = InMsgID;
}

void UGIEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UGIEventSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UGIEventSubsystem::NotifyEvent(const FString& EventId, UObject* Sender, const TArray<FPyOutputParam, TInlineAllocator<8>>& Outparames)
{
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
				UE_LOG(EventSystem, Warning, TEXT("Listener %s is pending kill or unreachable!"), *Listen.Listener.Get()->GetFName().ToString());
				continue;
			}
			///

			UFunction* Function = Listen.Listener.Get()->FindFunction(Listen.EventName);
			if (Function)
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

					Prop->CopyCompleteValue(Prop->ContainerPtrToValuePtr<void>(Params), Outparames[Index].PropAddr);
					++Index;
				}
				Listen.Listener.Get()->ProcessEvent(Function, Params);
			}
		}
	}

	if (ListenersToRemove.Num())
	{
		TSet<FEventHandle>& Listeners = ListenerMap.FindChecked(EventId);
		for (const auto& ListenerToRemove : ListenersToRemove)
		{
			Listeners.Remove(ListenerToRemove);
		}

		if (!Listeners.Num()) ListenerMap.Remove(EventId);
		UE_LOG(EventSystem, Log, TEXT("Removed invalid listeners."));
	}
}

const FEventHandle UGIEventSubsystem::ListenEvent(const FString& MessageId, UObject* Listener, FName EventName)
{
	FName MsgID = FName (*MessageId);
	FEventHandle Lis(Listener, EventName, MsgID);
	TSet<FEventHandle>& Listeners = ListenerMap.FindOrAdd(MessageId);
	if (!Listeners.Contains(Lis))
	{
		Listeners.Add(Lis);
	}
	return Lis;
}

void UGIEventSubsystem::UnListenEvent(const FEventHandle& InHandle)
{
	FString MsgID = InHandle.MsgId.ToString();
	if (ListenerMap.Contains(MsgID))
	{
		TSet<FEventHandle>& Listeners = ListenerMap.FindChecked(MsgID);
		Listeners.Remove(InHandle);
		if (Listeners.Num() == 0)
		{
			ListenerMap.Remove(MsgID);
		}
	}
}

// FIX (blowpunch)
void UGIEventSubsystem::UnListenEvents(UObject* Listener)
{
	for (auto It = ListenerMap.CreateConstIterator(); It; ++It)
	{
		auto Listeners = *It;
		for (auto SubIt = Listeners.Value.CreateConstIterator(); SubIt; ++SubIt)
		{
			FEventHandle Listen = *SubIt;
			if (Listen.Listener.Get() == Listener) Listeners.Value.Remove(Listen);
		}

		if (Listeners.Value.Num() == 0) ListenerMap.Remove(Listeners.Key);
	}
}
///

UGIEventSubsystem* UGIEventSubsystem::Get(const UObject* WorldContext)
{
	if (WorldContext)
	{
		const UWorld* const World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			UGIEventSubsystem* System = World->GetGameInstance()->GetSubsystem<UGIEventSubsystem>();
			if (System)
			{
				return System;
			}
		}
	}
	return nullptr;
}
