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

void UGIEventSubsystem::Broadcast(FString MsgType, const FEventBase& Event)
{
	if (DelegateMap.Contains(MsgType))
	{
		const FEventSystemDelegates* Delegate = DelegateMap.Find(MsgType);
		if (Delegate->IsBound())
		{
			Delegate->Broadcast(Event);
		}
	}

}

FEventHandle UGIEventSubsystem::Bind(FString MsgType, FEventSystemDelegates::FDelegate Delegate)
{
	FEventSystemDelegates& NewDelegate = DelegateMap.FindOrAdd(MsgType);
	FEventSystemDelegate Delegate1(Delegate);
	NewDelegate.Add(Delegate1);
	return FEventHandle(Delegate1, MsgType);
}

FEventHandle UGIEventSubsystem::Bind(FString MsgType, FEventSystemDelegate Delegate)
{
	FEventSystemDelegates& NewDelegate = DelegateMap.FindOrAdd(MsgType);
	NewDelegate.Add(Delegate);
	return FEventHandle(Delegate, MsgType);
}

void UGIEventSubsystem::UnBind(const FEventHandle& InHandle)
{
	if (DelegateMap.Contains(InHandle.MsgType))
	{
		FEventSystemDelegates* Delegates = DelegateMap.Find(InHandle.MsgType);
		Delegates->Remove(InHandle.Delegate);
	}
}

void UGIEventSubsystem::UnBindObject(const UObject* Object)
{
	for (auto& Delegate : DelegateMap)
	{
		Delegate.Value.RemoveAll(Object);
	}
}

void UGIEventSubsystem::UnBindObjectAndMsgType(FString MsgType, const UObject* Object)
{
	if (DelegateMap.Contains(MsgType))
	{
		FEventSystemDelegates* Delegates = DelegateMap.Find(MsgType);
		Delegates->RemoveAll(Object);
	}
}

void UGIEventSubsystem::UnBindObjectFunction(UObject* Object, FString FunctionName)
{
	for (auto& Delegate : DelegateMap)
	{
		FEventSystemDelegate TmpDelegate;
		TmpDelegate.BindUFunction(Object, *FunctionName);
		Delegate.Value.Remove(TmpDelegate);
	}
}

void UGIEventSubsystem::ClearAllByMsgType(FString MsgType)
{
	if (DelegateMap.Contains(MsgType))
	{
		FEventSystemDelegates* Delegates = DelegateMap.Find(MsgType);
		Delegates->Clear();
	}
}
