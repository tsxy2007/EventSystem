// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/GIEventSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

namespace UE4Event_Private
{
	TAtomic<uint64> GNextHandle(1);
}

EVENTSYSTEMRUNTIME_API uint64 FEventHandle::GenerateNewID()
{
	// Just increment a counter to generate an ID.
	uint64 Result = ++UE4Event_Private::GNextHandle;

	// Check for the next-to-impossible event that we wrap round to 0, because we reserve 0 for null delegates.
	if (Result == 0)
	{
		// Increment it again - it might not be zero, so don't just assign it to 1.
		Result = ++UE4Event_Private::GNextHandle;
	}

	return Result;
}


void UGIEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UGIEventSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UGIEventSubsystem::NotifyMessage(const FString& EventId, UObject* Sender, const TArray<FPyOutputParam, TInlineAllocator<8>>& Outparames)
{
	bool bCanNotify = ListenerMap.Contains(EventId);
	if (!bCanNotify)
	{
		return;
	}
	TArray<FListener> & Listeners = ListenerMap.FindChecked(EventId);
	for (const auto& Listen :Listeners)
	{
		if (Listen.Listener)
		{
			UFunction* Function = Listen.Listener->FindFunction(Listen.EventName);
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
				Listen.Listener->ProcessEvent(Function, Params);
			}
		}
	}
}

const FEventHandle UGIEventSubsystem::ListenMessage(const FString& MessageId, UObject* Listener, FName EventName)
{
	FName MsgID = FName (*MessageId);
	FListener Lis(Listener, EventName, MsgID);
	TArray<FListener>& Listeners = ListenerMap.FindOrAdd(MessageId);
	for (auto& TmpListener : Listeners)
	{
		if (TmpListener == Lis)
		{
			return TmpListener.Handle();
		}
	}

	
	Listeners.Add(Lis);
	return Lis.Handle();
}

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
