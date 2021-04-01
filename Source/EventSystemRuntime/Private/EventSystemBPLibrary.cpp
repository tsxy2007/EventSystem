// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventSystemBPLibrary.h"
#include "Engine/UserDefinedEnum.h"
#include "JsonUtilities/Public/JsonObjectConverter.h"
#include "GIEventSubsystem.h"
#include "Engine/GameInstance.h"


UEventSystemBPLibrary::UEventSystemBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}



void UEventSystemBPLibrary::NotifyMessageByKeyVariadic(const FString& MessageId, UObject* Sender)
{

}

FEventHandle UEventSystemBPLibrary::ListenEventByKey(const FString& MessageId, UObject* Listener, FName EventName)
{
	UGIEventSubsystem* System = UGIEventSubsystem::Get(Listener);
	if (System)
	{
		return System->ListenMessage(MessageId, Listener, EventName);
	}
	return FEventHandle();
}


void UEventSystemBPLibrary::UnListenEvent(const UObject* WorldContext, const FEventHandle& Handle)
{
	UGIEventSubsystem* System = UGIEventSubsystem::Get(WorldContext);
	if (System)
	{
		System->UnListenEvent(Handle);
	}
}

FString UEventSystemBPLibrary::Conv_EventHandleToString(const FEventHandle& InRot)
{
	return InRot.ToString();
}

void UEventSystemBPLibrary::NotifyEventByKey(const FString& EventId, UObject* Sender, const TArray<FPyOutputParam, TInlineAllocator<8>>& Outparames)
{
	UGIEventSubsystem* System = UGIEventSubsystem::Get(Sender);
	if (System)
	{
		System->NotifyMessage(EventId, Sender, Outparames);
	}
}

DEFINE_FUNCTION(UEventSystemBPLibrary::execNotifyMessageByKeyVariadic)
{
	P_GET_PROPERTY(FStrProperty, MessageId);
	P_GET_OBJECT(UObject, Sender);

	

	// Read the output values and store them to write to later from the Python context
	TArray<FPyOutputParam, TInlineAllocator<8>> OutParms;
	while (Stack.PeekCode() != EX_EndFunctionParms)
	{
		Stack.StepCompiledIn<FProperty>(nullptr);
		check(Stack.MostRecentProperty&& Stack.MostRecentPropertyAddress);

		FPyOutputParam& OutParam = OutParms.AddDefaulted_GetRef();
		OutParam.Property = Stack.MostRecentProperty;
		OutParam.PropAddr = Stack.MostRecentPropertyAddress;

	}
	P_FINISH

	P_NATIVE_BEGIN
	UEventSystemBPLibrary::NotifyEventByKey(MessageId, Sender, OutParms);
	P_NATIVE_END
}

