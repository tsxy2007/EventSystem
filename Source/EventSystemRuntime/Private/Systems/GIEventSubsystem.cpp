// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/GIEventSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"


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
	TSet<FEventHandle> Listeners = ListenerMap.FindChecked(EventId); 
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
