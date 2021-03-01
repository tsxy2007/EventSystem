// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Switch.h"
#include "EventsRuntime/Classes/EventContainer.h"
#include "EventsK2Node_EventBase.generated.h"

class FBlueprintActionDatabaseRegistrar;

UCLASS()
class UEventsK2Node_EventBase : public UK2Node
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = PinOptions)
	TArray<FEventInfo> PinTags;

	//UPROPERTY(EditAnywhere, Category = PinOptions)
	//bool UseInputsOnGraph;

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
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UK2Node interface


	void CreateSelectionPin();

private:
	UEdGraphPin* GetEventPin() const;
	static FName GetEventPinName();

public:
};
