// Fill out your copyright notice in the Description page of Project Settings.


#include "Systems/GIEventSubsystem.h"

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
	TArray<FListener> Listeners;
	ListenerMap.MultiFind(EventId,Listeners);
	for (const auto& Listen :Listeners)
	{
		if (Listen.Listener)
		{
			UFunction* Function = Listen.Listener->FindFunction(Listen.EventName);
			if (Function)
			{
				auto p = FMemory_Alloca(Function->ParmsSize);
				FMemory::Memzero(p, Function->ParmsSize);

				bool bSucc = true;
				int32 Index = 0;
				for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
				{
					if (It->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						bSucc = false;
						break;
					}
					FProperty* Prop = *It;

					Prop->CopyCompleteValue(Prop->ContainerPtrToValuePtr<void>(p), Outparames[Index].PropAddr);
					++Index;
				}
				Listen.Listener->ProcessEvent(Function, p);
			}
		}
	}
}

void UGIEventSubsystem::ListenMessage(const FString& MessageId, UObject* Listener, FName EventName)
{
	FListener Lis(Listener, EventName);
	ListenerMap.Add(MessageId, Lis);
}

UGIEventSubsystem* UGIEventSubsystem::Get(const UObject* WorldContext)
{
	if (WorldContext)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
		if (World)
		{
			UGameInstance* GI = World->GetGameInstance();
			if (GI)
			{
				return UGameInstance::GetSubsystem<UGIEventSubsystem>(GI);
			}
			
		}
	}
	return nullptr;
}
