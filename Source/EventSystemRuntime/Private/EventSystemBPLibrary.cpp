// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventSystemBPLibrary.h"
#include "Engine/UserDefinedEnum.h"
#include "JsonUtilities/Public/JsonObjectConverter.h"
#include "GIEventSubsystem.h"
#include "Engine/GameInstance.h"


UEventSystemBPLibrary::UEventSystemBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}



void UEventSystemBPLibrary::NotifyEventByKeyVariadic(const FString& MessageId, UObject* Sender)
{

}

FEventHandle UEventSystemBPLibrary::ListenEventByKey(const FString& MessageId, UObject* Listener, FName EventName)
{
	UGIEventSubsystem* System = UGIEventSubsystem::Get(Listener);
	if (System)
	{
		return System->ListenEvent(MessageId, Listener, EventName);
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

uint8 UEventSystemBPLibrary::Localuint8(uint8 Value)
{
	return Value;
}

int32 UEventSystemBPLibrary::LocalInt(int32 Value)
{
	return Value;
}

int64 UEventSystemBPLibrary::LocalInt64(int64 Value)
{
	return Value;
}

bool UEventSystemBPLibrary::LocalBool(bool Value)
{
	return Value;
}

FText UEventSystemBPLibrary::LocalText(FText Value)
{
	return Value;
}

float UEventSystemBPLibrary::Localfloat(float Value)
{
	return Value;
}

FString UEventSystemBPLibrary::LocalFString(const FString& Value)
{
	return Value;
}

FLinearColor UEventSystemBPLibrary::LocalLinearColor(FLinearColor Value)
{
	return Value;
}

FVector UEventSystemBPLibrary::LocalVector(FVector Value)
{
	return Value;
}

FVector2D UEventSystemBPLibrary::LocalVector2D(FVector2D Value)
{
	return Value;
}

FRotator UEventSystemBPLibrary::LocalRotator(FRotator Value)
{
	return Value;
}

FName UEventSystemBPLibrary::LocalName(const FName& Value)
{
	return Value;
}

void UEventSystemBPLibrary::NotifyEventByKey(const FString& EventId, UObject* Sender, const TArray<FPyOutputParam, TInlineAllocator<8>>& Outparames)
{
	UGIEventSubsystem* System = UGIEventSubsystem::Get(Sender);
	if (System)
	{
		System->NotifyEvent(EventId, Sender, Outparames);
	}
}

DEFINE_FUNCTION(UEventSystemBPLibrary::execNotifyEventByKeyVariadic)
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

