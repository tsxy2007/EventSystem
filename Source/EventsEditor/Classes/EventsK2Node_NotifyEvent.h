// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphPin.h"
#include "EventsK2Node_EventBase.h"
#include "EventsRuntime/Classes/EventContainer.h"
#include "Stats/StatsHierarchical.h"
#include "EventsK2Node_NotifyEvent.generated.h"

UCLASS()
class UEventsK2Node_NotifyEvent : public UEventsK2Node_EventBase
{
	GENERATED_UCLASS_BODY()


	virtual void AllocateDefaultPins() override;
	// UObject interface
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	// End of UObject interface

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

	virtual void AddInnerPin(FName PinName, const FEdGraphPinType& PinType) override;
public:
};
