// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EventsK2Node_MultiCompareBase.h"
#include "EventsK2Node_MultiCompareEventAssetInterface.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;

UCLASS()
class UEventsK2Node_MultiCompareEventAssetInterface : public UEventsK2Node_MultiCompareBase
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	// End of UK2Node interface

private:

	virtual void AddPinToSwitchNode() override;
};
