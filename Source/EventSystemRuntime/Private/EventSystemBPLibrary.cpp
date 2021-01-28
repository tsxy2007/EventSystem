// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventSystemBPLibrary.h"

UEventSystemBPLibrary::UEventSystemBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}


FEventHandle UEventSystemBPLibrary::BindEventToSystemDelegate(FString EventName, FEventSystemDelegate Delegate)
{
	FEventHandle Handle;
	if (Delegate.IsBound())
	{
		const UWorld* const World = GEngine->GetWorldFromContextObject(Delegate.GetUObject(), EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			UGIEventSubsystem* System =World->GetGameInstance()->GetSubsystem<UGIEventSubsystem>();
			if (System)
			{
				Handle = System->Bind(EventName, Delegate);
			}
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning,
			TEXT("BindEventSystem passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}

	return Handle;
}

FEventHandle UEventSystemBPLibrary::BindEventToSystem(FString MsgType, UObject* Object, FString FunctionName)
{
	FName const FunctionFName(*FunctionName);

	if (Object)
	{
		UFunction* const Func = Object->FindFunction(FunctionFName);
		if (Func && (Func->ParmsSize <= 0))
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("BindEvent passed a function (%s) that expects parameters."), *FunctionName);
			return FEventHandle();
		}
	}

	FEventSystemDelegate Delegate;
	Delegate.BindUFunction(Object, FunctionFName);
	return UEventSystemBPLibrary::BindEventToSystemDelegate(MsgType, Delegate);
}

void UEventSystemBPLibrary::UnBindEventByHandle(const UObject* WorldContextObject, const FEventHandle& Handle)
{
	const UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World)
	{
		UGIEventSubsystem* System = World->GetGameInstance()->GetSubsystem<UGIEventSubsystem>();
		if (System)
		{
			System->UnBind(Handle);
		}
	}
}

void UEventSystemBPLibrary::UnBindEventByObject(const UObject* WorldContextObject, FString MsgType, UObject* Object)
{
	const UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World)
	{
		UGIEventSubsystem* System = World->GetGameInstance()->GetSubsystem<UGIEventSubsystem>();
		if (System)
		{
			System->UnBindObjectAndMsgType(MsgType,Object);
		}
	}
}

void UEventSystemBPLibrary::Broadcast(const UObject* WorldContextObject, FString MsgType, const FEventBase& Event)
{
	const UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World)
	{
		UGIEventSubsystem* System = World->GetGameInstance()->GetSubsystem<UGIEventSubsystem>();
		if (System)
		{
			System->Broadcast(MsgType, Event);
		}
	}
}
