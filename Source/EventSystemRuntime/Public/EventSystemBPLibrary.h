// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Systems/GIEventSubsystem.h"
#include "EventSystemBPLibrary.generated.h"

UCLASS()
class EVENTSYSTEMRUNTIME_API UEventSystemBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	static void NotifyEventByKey(const FString& EventId, UObject* Sender, const TArray<FPyOutputParam, TInlineAllocator<8>>& Outparames);
public:
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CallableWithoutWorldContext, BlueprintInternalUseOnly = true, HidePin = "Sender", DefaultToSelf = "Sender", AutoCreateRefTerm = "MessageId", Variadic), Category = "EventSystem")
	static void NotifyEventByKeyVariadic(const FString& MessageId, UObject* Sender); 
	DECLARE_FUNCTION(execNotifyEventByKeyVariadic);

	UFUNCTION(BlueprintCallable, meta = (CallableWithoutWorldContext, BlueprintInternalUseOnly = true, HidePin = "Listener", DefaultToSelf = "Listener", AutoCreateRefTerm = "MessageId", Variadic), Category = "EventSystem")
	static FEventHandle ListenEventByKey(const FString& MessageId, UObject* Listener,FName EventName);

	UFUNCTION(BlueprintCallable, Category = "EventSystem")
	static void UnListenEvent(const UObject* WorldContext, const FEventHandle& Handle);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToString (EventHandle)", CompactNodeTitle = "->", BlueprintAutocast), Category = "EventSystem")
	static FString Conv_EventHandleToString(const FEventHandle& InRot);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
		static uint8 Localuint8(uint8 Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
	static int32 LocalInt(int32 Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
	static int64 LocalInt64(int64 Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
		static bool LocalBool(bool Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
		static FText LocalText(FText Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
		static float Localfloat(float Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
		static FString LocalFString(const FString& Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
		static FName LocalName(const FName& Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
		static FLinearColor LocalLinearColor(FLinearColor Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
		static FVector LocalVector(FVector Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
		static FVector2D LocalVector2D(FVector2D Value);

	UFUNCTION(BlueprintPure, Category = "EventSystem")
		static FRotator LocalRotator(FRotator Value);
};
