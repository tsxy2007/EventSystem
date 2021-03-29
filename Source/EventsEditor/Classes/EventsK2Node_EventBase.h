// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Switch.h"
#include "EventsRuntime/Classes/EventContainer.h"
#include "Stats/StatsHierarchical.h"
#include "K2Node_EditablePinBase.h"
#include "EventsK2Node_EventBase.generated.h"

class FBlueprintActionDatabaseRegistrar;


#define DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT() \
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

UCLASS()
class UEventsK2Node_EventBase : public UK2Node
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = PinOptions)
	TArray<FEventInfo> PinTags;

	TArray< TSharedPtr<FUserPinInfo> >UserDefinedPins;

	UPROPERTY()
	TArray<FName> PinNames;

	static FString MessageParamPrefix;
	virtual void AllocateDefaultPins() override;
	// UObject interface
	virtual void PostLoad() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	// End of UObject interface

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	//virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	// End of UEdGraphNode interface

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void RewireOldPinsToNewPins(TArray<UEdGraphPin*>& InOldPins, TArray<UEdGraphPin*>& InNewPins);

	void DestroyPinList(TArray<UEdGraphPin*>& InPins);

	void CreateSelectionPin();
	virtual void CreateOutEventPin() {};
	virtual UEdGraphPin* CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo) { return nullptr; }
	virtual void AddInnerPin(FName PinName, const FEdGraphPinType& PinType) {};
	UEdGraphPin* CreateUserDefinedPin(const FName InPinName, const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection = EEdGraphPinDirection::EGPD_Input);
	void RemoveUserDefinedPinByName(const FName PinName);

	//~ Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject interface

	friend FArchive& operator <<(FArchive& Ar, FUserPinInfo& Info);

	bool ModifyUserDefinedPinDefaultValue(TSharedPtr<FUserPinInfo> PinInfo, const FString& InDefaultValue);
	bool UpdateEdGraphPinDefaultValue(TSharedPtr<FUserPinInfo> PinInfo, FString& NewDefaultValue);
protected:
	UEdGraphPin* GetEventPin() const;
	UEdGraphPin* GetSelfPin() const;
	UEdGraphPin* GetThenPin() const;
	static FName GetEventPinName();
	FName GetUniquePinName();
public:
};
