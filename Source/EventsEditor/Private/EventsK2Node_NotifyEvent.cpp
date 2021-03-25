// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventsK2Node_NotifyEvent.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintEventLibrary.h"
#include "EventsManager.h"
#include "EventSystemBPLibrary.h"
#include "K2Node_CallFunction.h"
#include "KismetCompilerMisc.h"
#include "KismetCompiler.h"


UEventsK2Node_NotifyEvent::UEventsK2Node_NotifyEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OrphanedPinSaveMode = ESaveOrphanPinMode::SaveNone;
}

void UEventsK2Node_NotifyEvent::AddInnerPin(FName PinName, const FEdGraphPinType& PinType)
{
	CreatePin(EGPD_Input, PinType, PinName);
}

void UEventsK2Node_NotifyEvent::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	static const FName FuncName = GET_FUNCTION_NAME_CHECKED(UEventSystemBPLibrary, NotifyMessageByKeyVariadic);

	UEventsK2Node_NotifyEvent* SpawnNode = this;
	UEdGraphPin* SpawnEventExec = GetExecPin();
	UEdGraphPin* SpawnMsgPin = GetEventPin();
	UEdGraphPin* SpawnSenderPin = GetSelfPin();
	UEdGraphPin* SpawnNodeThen = GetThenPin();


	UK2Node_CallFunction* CallNotifyFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
	CallNotifyFuncNode->FunctionReference.SetExternalMember(FuncName, UEventSystemBPLibrary::StaticClass());
	CallNotifyFuncNode->AllocateDefaultPins();

	
	UEdGraphPin* CallExecPin = CallNotifyFuncNode->GetExecPin();
	CompilerContext.MovePinLinksToIntermediate(*SpawnEventExec, *CallExecPin);

	UEdGraphPin* CallMessageIdPin = CallNotifyFuncNode->FindPinChecked(TEXT("MessageId"));
	CompilerContext.MovePinLinksToIntermediate(*SpawnMsgPin, *CallMessageIdPin);

	UEdGraphPin* CallSenderPin = CallNotifyFuncNode->FindPinChecked(TEXT("Sender"));
	CompilerContext.MovePinLinksToIntermediate(*SpawnSenderPin, *CallSenderPin);

	UEdGraphPin* CallThen = CallNotifyFuncNode->GetThenPin();
	CompilerContext.MovePinLinksToIntermediate(*SpawnNodeThen, *CallThen);

	for (int32 ArgIdx = 0; ArgIdx < PinNames.Num(); ++ArgIdx)
	{
		UEdGraphPin* ParameterPin = FindPin(PinNames[ArgIdx]);
		if (ParameterPin)
		{
			auto InputPin = CallNotifyFuncNode->CreatePin(EGPD_Input, ParameterPin->PinType, ParameterPin->PinName);
			CompilerContext.MovePinLinksToIntermediate(*ParameterPin, *InputPin);
		}
	}
	SpawnNode->BreakAllNodeLinks();
}


void UEventsK2Node_NotifyEvent::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();
}

FText UEventsK2Node_NotifyEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "Notify_EVENT_Tag", "Notify Event");
}

FText UEventsK2Node_NotifyEvent::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "UEventsK2Node_NotifyEvent_ToolTip", "Selects an output that matches the input value");
}