// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Switch.h"
#include "EventsRuntime/Classes/EventContainer.h"
#include "Stats/StatsHierarchical.h"
#include "EventsK2Node_ListenEvent.generated.h"


UCLASS()
class UEventsK2Node_ListenEvent : public UEventsK2Node_EventBase
{
	GENERATED_UCLASS_BODY()


	virtual void AllocateDefaultPins() override;
	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual UEdGraphPin* CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo) override;
	virtual void AddInnerPin(FName PinName, const FEdGraphPinType& PinType) override;
	virtual void CreateOutEventPin() override;
	virtual void CreateReturnEventHandlePin() override;
protected:
	UEdGraphPin* GetOutMessagePin() const;
	UEdGraphPin* GetReturnEventHandlePin() const;
};
