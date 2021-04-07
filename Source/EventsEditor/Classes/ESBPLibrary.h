// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ESBPLibrary.generated.h"

UCLASS()
class EVENTSEDITOR_API UESBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	static FString GetParameterType(const FEdGraphPinType& Type);
	static FString GetCppName(FFieldVariant Field, bool bUInterface = false, bool bForceParameterNameModification = false);
	static int32 GetInheritenceLevel(const UStruct* Struct);
	static bool GetPinTypeFromStr(const FString& PinTypeStr, FEdGraphPinType& PinType);
};
