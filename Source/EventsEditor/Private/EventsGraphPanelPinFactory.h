// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EdGraphUtilities.h"
#include "EventContainer.h"
#include "EdGraphSchema_K2.h"
#include "SGraphPin.h"
#include "SEventGraphPin.h"
#include "SEventContainerGraphPin.h"
#include "SEventQueryGraphPin.h"

class FEventsGraphPanelPinFactory: public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			if (UScriptStruct* PinStructType = Cast<UScriptStruct>(InPin->PinType.PinSubCategoryObject.Get()))
			{
				if (PinStructType->IsChildOf(FEventInfo::StaticStruct()))
				{
					return SNew(SEventGraphPin, InPin);
				}
				else if (PinStructType->IsChildOf(FEventContainer::StaticStruct()))
				{
					return SNew(SEventContainerGraphPin, InPin);
				}
				else if (PinStructType->IsChildOf(FEventQuery::StaticStruct()))
				{
					return SNew(SEventQueryGraphPin, InPin);
				}
			}
		}
		else if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_String && InPin->PinType.PinSubCategory == TEXT("LiteralEventContainer"))
		{
			return SNew(SEventContainerGraphPin, InPin);
		}

		return nullptr;
	}
};
