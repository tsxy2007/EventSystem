// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EdGraphUtilities.h"
#include "SGraphNode.h"
#include "SGraphNode_MultiCompareEvent.h"
#include "EventsK2Node_MultiCompareBase.h"

class FEventsGraphPanelNodeFactory : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(class UEdGraphNode* InNode) const override
	{
		if (UEventsK2Node_MultiCompareBase* CompareNode = Cast<UEventsK2Node_MultiCompareBase>(InNode))
		{
			return SNew(SGraphNode_MultiCompareEvent, CompareNode);
		}

		return NULL;
	}
};
