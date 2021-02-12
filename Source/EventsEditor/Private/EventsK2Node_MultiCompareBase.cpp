// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventsK2Node_MultiCompareBase.h"
#include "UObject/UnrealType.h"
#include "EdGraph/EdGraph.h"

UEventsK2Node_MultiCompareBase::UEventsK2Node_MultiCompareBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumberOfPins = 1;
	OrphanedPinSaveMode = ESaveOrphanPinMode::SaveNone;
}

void UEventsK2Node_MultiCompareBase::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	// If the number of pins is changed mark the node as dirty and reconstruct
	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UEventsK2Node_MultiCompareBase, NumberOfPins))
	{
		if (NumberOfPins < 0)
		{
			NumberOfPins = 1;
		}

		ReconstructNode();
		GetGraph()->NotifyGraphChanged();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FText UEventsK2Node_MultiCompareBase::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "MultiCompareTagContainer_ToolTip", "Sets the an output for each input value");
}

FText UEventsK2Node_MultiCompareBase::GetMenuCategory() const
{
	return NSLOCTEXT("K2Node", "MultiCompareTagContainer_ActionMenuCategory", "Events|Tag Container");
}

FString UEventsK2Node_MultiCompareBase::GetUniquePinName()
{
	FString NewPinName;
	int32 Index = 0;
	while (true)
	{
		NewPinName = FString::Printf(TEXT("Case_%d"), Index++);
		bool bFound = false;
		for (int32 PinIdx = 0; PinIdx < PinNames.Num(); PinIdx++)
		{
			FString PinName = PinNames[PinIdx].ToString();
			if (PinName == NewPinName)
			{
				bFound = true;
				break;
			}			
		}
		if (!bFound)
		{
			break;
		}
	}
	return NewPinName;
}

void UEventsK2Node_MultiCompareBase::AddPin()
{
	NumberOfPins++;
}

void UEventsK2Node_MultiCompareBase::RemovePin()
{
	if (NumberOfPins > 1)
	{
		NumberOfPins--;
	}	
}
