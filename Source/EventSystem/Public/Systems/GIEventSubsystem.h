// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GIEventSubsystem.generated.h"

USTRUCT(BlueprintType, Blueprintable)
struct EVENTSYSTEM_API FEventBase
{
	GENERATED_BODY()
	FEventBase(){}
};

USTRUCT(BlueprintType, Blueprintable)
struct EVENTSYSTEM_API FEventTest : public FEventBase
{
	GENERATED_BODY()
	FEventTest() {}

	UPROPERTY(EditAnywhere)
		int32 i = 0;
};


DECLARE_DYNAMIC_DELEGATE_OneParam(FEventSystemDelegate, const FEventBase&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEventSystemDelegates, const FEventBase&, Event);

USTRUCT(BlueprintType, Blueprintable)
struct FEventHandle
{
	GENERATED_USTRUCT_BODY()
public:
	FEventHandle() {}
	FEventHandle(const FEventSystemDelegate& InDelegate ,FString InMsgType) :
		Delegate(InDelegate),
		MsgType(InMsgType)
	{}

	friend bool operator==(const FEventHandle& A, const FEventHandle& B)
	{
		return A.Delegate == B.Delegate && A.MsgType.Equals(B.MsgType);
	}
	
	bool operator==(const FEventHandle& A)
	{
		return A.Delegate == Delegate && A.MsgType.Equals(MsgType);
	}

	FEventSystemDelegate Delegate;
	FString MsgType;
};

/**
 * 
 */
UCLASS()
class EVENTSYSTEM_API UGIEventSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UGIEventSubsystem() {};
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
public:
	UFUNCTION(BlueprintCallable, Category = EventSystem)
	void Broadcast(FString MsgType, const FEventBase& Event);

	FEventHandle Bind(FString MsgType, FEventSystemDelegates::FDelegate Delegate);

	UFUNCTION(BlueprintCallable,Category = EventSystem)
	FEventHandle Bind(FString MsgType, FEventSystemDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category = EventSystem)
	void UnBind(const FEventHandle& InHandle);

	UFUNCTION(BlueprintCallable, Category = EventSystem)
	void UnBindObject(const UObject* Object);
	
	UFUNCTION(BlueprintCallable, Category = EventSystem)
	void UnBindObjectAndMsgType(FString MsgType, const UObject* Object);

	UFUNCTION(BlueprintCallable, Category = EventSystem)
	void UnBindObjectFunction(UObject* Object, FString FunctionName);

	UFUNCTION(BlueprintCallable, Category = EventSystem)
	void ClearAllByMsgType(FString MsgType);
private:
	UPROPERTY()
	TMap<FString, FEventSystemDelegates> DelegateMap;
};
