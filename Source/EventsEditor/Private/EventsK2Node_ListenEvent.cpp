// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventsK2Node_ListenEvent.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintEventLibrary.h"
#include "EventsManager.h"
#include "EventSystemBPLibrary.h"
#include "KismetCompilerMisc.h"
#include "KismetCompiler.h"
#include "K2Node_CustomEvent.h"


namespace
{
	static FName OutEventPinName(TEXT("OutMessage"));
	static FName OutReturnEventHandleName(TEXT("ReturnEventHandle"));
}

UEventsK2Node_ListenEvent::UEventsK2Node_ListenEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OrphanedPinSaveMode = ESaveOrphanPinMode::SaveNone;
}

UEdGraphPin* UEventsK2Node_ListenEvent::CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo)
{
	return CreatePin(NewPinInfo->DesiredPinDirection, NewPinInfo->PinType, NewPinInfo->PinName);
}

void UEventsK2Node_ListenEvent::AddInnerPin(FName PinName, const FEdGraphPinType& PinType)
{
	CreateUserDefinedPin(PinName, PinType, EEdGraphPinDirection::EGPD_Output);
}

void UEventsK2Node_ListenEvent::CreateOutEventPin()
{
	DefaultPins.Add(CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, OutEventPinName));
}

void UEventsK2Node_ListenEvent::CreateReturnEventHandlePin()
{
	FEdGraphPinType PinType;
	PinType.ResetToDefaults();

	PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	PinType.PinSubCategoryObject = FEventHandle::StaticStruct();

	DefaultPins.Add(CreatePin(EGPD_Output, PinType, OutReturnEventHandleName));
}

UEdGraphPin* UEventsK2Node_ListenEvent::GetOutMessagePin() const
{
	return FindPinChecked(OutEventPinName);
}

UEdGraphPin* UEventsK2Node_ListenEvent::GetReturnEventHandlePin() const
{
	return FindPinChecked(OutReturnEventHandleName);
}

void UEventsK2Node_ListenEvent::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);


	UK2Node_CustomEvent* CustomEventNode = CompilerContext.SpawnIntermediateNode<UK2Node_CustomEvent>(this, SourceGraph);
	CustomEventNode->CustomFunctionName = *("CustomEventFUNC_" + CompilerContext.GetGuid(CustomEventNode));
	CustomEventNode->AllocateDefaultPins();

	
	for (int32 ArgIdx = 0; ArgIdx < PinNames.Num(); ++ArgIdx)
	{
		UEdGraphPin* ParameterPin = FindPin(PinNames[ArgIdx]);
		if (ParameterPin)
		{
			 auto OutPin = CustomEventNode->CreateUserDefinedPin(*FString::Printf(TEXT("p%d"), ArgIdx), ParameterPin->PinType, EGPD_Output, true);
			CompilerContext.MovePinLinksToIntermediate(*ParameterPin, *OutPin);
		}
	}

	static const FName FuncName = GET_FUNCTION_NAME_CHECKED(UEventSystemBPLibrary, ListenEventByKey);

	UEventsK2Node_ListenEvent* SpawnNode = this;
	UEdGraphPin* SpawnEventExec = GetExecPin();
	UEdGraphPin* SpawnMsgPin = GetEventPin();
	UEdGraphPin* SpawnSelfPin = GetSelfPin();
	UEdGraphPin* SpawnNodeThen = GetThenPin();
	UEdGraphPin* SpawnOutMessagePin = GetOutMessagePin();
	UEdGraphPin* SpawnReturnHandlePin = GetReturnEventHandlePin();

	UK2Node_CallFunction* CallNotifyFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
	CallNotifyFuncNode->FunctionReference.SetExternalMember(FuncName, UEventSystemBPLibrary::StaticClass());
	CallNotifyFuncNode->AllocateDefaultPins();


	UEdGraphPin* CallExecPin = CallNotifyFuncNode->GetExecPin();
	CompilerContext.MovePinLinksToIntermediate(*SpawnEventExec, *CallExecPin);

	UEdGraphPin* CallMessageIdPin = CallNotifyFuncNode->FindPinChecked(TEXT("MessageId"));
	CompilerContext.MovePinLinksToIntermediate(*SpawnMsgPin, *CallMessageIdPin);

	UEdGraphPin* CallListenerPin = CallNotifyFuncNode->FindPinChecked(TEXT("Listener"));
	CompilerContext.MovePinLinksToIntermediate(*SpawnSelfPin, *CallListenerPin);

	UEdGraphPin* CallEventNamePin = CallNotifyFuncNode->FindPinChecked(TEXT("EventName"));
	CallEventNamePin->DefaultValue = CustomEventNode->CustomFunctionName.ToString();

	UEdGraphPin* CallThen = CallNotifyFuncNode->GetThenPin();
	CompilerContext.MovePinLinksToIntermediate(*SpawnNodeThen, *CallThen);

	UEdGraphPin* CallOutMessageThen = CustomEventNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
	CompilerContext.MovePinLinksToIntermediate(*SpawnOutMessagePin, *CallOutMessageThen);

	UEdGraphPin* CallReturnPin = CallNotifyFuncNode->GetReturnValuePin();
	CompilerContext.MovePinLinksToIntermediate(*SpawnReturnHandlePin, *CallReturnPin);

	SpawnNode->BreakAllNodeLinks();
}


void UEventsK2Node_ListenEvent::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();
}

FText UEventsK2Node_ListenEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "Listen_event_Event", "Listen Event");
}

FText UEventsK2Node_ListenEvent::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "TestEvent_EVENT_ToolTip_Listen", "Listen Event Notify");
}
