// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventsK2Node_EventBase.h"
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

namespace
{
	static FName DefaultPinName(TEXT("Default"));
	static FName EventPinName(TEXT("CustomEvent"));
	static FName SenderPinName(TEXT("Sernder"));
}


UEventsK2Node_EventBase::UEventsK2Node_EventBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OrphanedPinSaveMode = ESaveOrphanPinMode::SaveNone;
}

void UEventsK2Node_EventBase::PostLoad()
{
	Super::PostLoad();

}

void UEventsK2Node_EventBase::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (Pin == GetEventPin())
	{
	
		TArray<UEdGraphPin*> OldPins = Pins;
		TArray<UEdGraphPin*> RemovedPins;
		for (int32 PinIdx = Pins.Num() - 1; PinIdx > 0; PinIdx--)
		{
			if (!Pins[PinIdx])
			{
				continue;
			}
			if (Pins[PinIdx]->PinName.ToString().StartsWith(MessageParamPrefix))
			{
				RemovedPins.Add(Pins[PinIdx]);
				PinNames.Remove(Pins[PinIdx]->PinName);
				Pins.RemoveAt(PinIdx);
			}
		}

		FString MsgKey = Pin->GetDefaultAsString();
		FString Left;
		FString Right;
		FString Key;
		MsgKey.Split("(TagName=\"", &Left,&Right);
		Right.Split("\")",&Key,&Left);
		auto MsgTag = FEventInfo::RequestEvent(*Key, false);
		auto Node = UEventsManager::Get().FindTagNode(MsgTag.GetTagName());
		if (Node)
		{
			for (auto& ParamterInfo : Node->Parameters)
			{
				FEdGraphPinType PinType;
				if (UEventSystemBPLibrary::GetPinTypeFromStr(ParamterInfo.Type.ToString(), PinType))
				{
					const FName PinName(GetUniquePinName());
					CreatePin(EGPD_Input, PinType, PinName);
					PinNames.Add(PinName);
				}
			}
		}
#if ENGINE_MINOR_VERSION < 20
		RewireOldPinsToNewPins(RemovedPins, Pins);
#else
		RewireOldPinsToNewPins(RemovedPins, Pins);
#endif
		GetGraph()->NotifyGraphChanged();
		Super::PinDefaultValueChanged(Pin);
		return;
	}

	// See if this is a user defined pin
	//for (int32 Index = 0; Index < ParameterTypes.Num(); ++Index)
	//{
	//	auto PinInfo = ParameterTypes[Index].Info;
	//	Pin = GetMessagePin(Index);
	//	FString DefaultsString = Pin->GetDefaultAsString();
	//	if (DefaultsString != PinInfo->PinDefaultValue)
	//	{
	//		TGuardValue<bool> CircularGuard(bRecursivelyChangingDefaultValue, true);
	//		// Make sure this doesn't get called recursively
	//		ModifyUserDefinedPinDefaultValue(PinInfo, Pin->GetDefaultAsString());
	//	}
	//}
}

void UEventsK2Node_EventBase::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	static const FName FuncName = GET_FUNCTION_NAME_CHECKED(UEventSystemBPLibrary, NotifyMessageByKeyVariadic);

	UEventsK2Node_EventBase* SpawnNode = this;
	UEdGraphPin* EventExec = GetExecPin();
	UEdGraphPin* MsgPin = GetEventPin();
	UEdGraphPin* SenderPin = GetSenderPin();
	UEdGraphPin* ThenPin = FindPinChecked(UEdGraphSchema_K2::PN_Then);


	UK2Node_CallFunction* CallNotifyFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
	CallNotifyFuncNode->FunctionReference.SetExternalMember(FuncName, UEventSystemBPLibrary::StaticClass());
	CallNotifyFuncNode->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(*EventExec, *CallNotifyFuncNode->GetExecPin()).CanSafeConnect();
	UEdGraphPin* CallMessageIdPin = CallNotifyFuncNode->FindPinChecked(TEXT("MessageId"));
	CompilerContext.MovePinLinksToIntermediate(*MsgPin, *CallMessageIdPin);

	UEdGraphPin* CallSenderPin = CallNotifyFuncNode->FindPinChecked(TEXT("Sender"));
	CompilerContext.MovePinLinksToIntermediate(*SenderPin, *CallSenderPin);

	UEdGraphPin* CallThenPin = CallNotifyFuncNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
	CompilerContext.MovePinLinksToIntermediate(*ThenPin, *CallThenPin);

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

FString UEventsK2Node_EventBase::MessageParamPrefix = TEXT("Param");

void UEventsK2Node_EventBase::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UEdGraphSchema_K2::PSC_Self, UEdGraphSchema_K2::PN_Self);
	CreateSelectionPin();
}

FText UEventsK2Node_EventBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "Test_EVENT_Tag", "Test Event");
}

FText UEventsK2Node_EventBase::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "TestTag_EVENT_ToolTip", "Selects an output that matches the input value");
}

void UEventsK2Node_EventBase::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UEventsK2Node_EventBase::RewireOldPinsToNewPins(TArray<UEdGraphPin*>& InOldPins, TArray<UEdGraphPin*>& InNewPins)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	// @TODO: we should account for redirectors, orphaning etc. here too!

	for (UEdGraphPin* OldPin : InOldPins)
	{
		for (UEdGraphPin* NewPin : InNewPins)
		{
			if (OldPin->PinName == NewPin->PinName && OldPin->PinType == NewPin->PinType && OldPin->Direction == NewPin->Direction)
			{
				NewPin->MovePersistentDataFromOldPin(*OldPin);
				break;
			}
		}
	}

	DestroyPinList(InOldPins);
}

void UEventsK2Node_EventBase::DestroyPinList(TArray<UEdGraphPin*>& InPins)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UBlueprint* Blueprint = GetBlueprint();
	bool bNotify = false;
	if (Blueprint != nullptr)
	{
		bNotify = !Blueprint->bIsRegeneratingOnLoad;
	}

	// Throw away the original pins
	for (UEdGraphPin* Pin : InPins)
	{
		Pin->BreakAllPinLinks(bNotify);

		UEdGraphNode::DestroyPin(Pin);
	}
}

void UEventsK2Node_EventBase::CreateSelectionPin()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* Pin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FEventInfo::StaticStruct(), EventPinName);
	K2Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
}

UEdGraphPin* UEventsK2Node_EventBase::GetEventPin() const
{
	return FindPin(EventPinName);
}

UEdGraphPin* UEventsK2Node_EventBase::GetSenderPin() const
{
	return FindPin(UEdGraphSchema_K2::PN_Self);
}

FName UEventsK2Node_EventBase::GetEventPinName()
{
	return EventPinName;
}

FName UEventsK2Node_EventBase::GetUniquePinName()
{
	FName NewPinName;
	int32 i = 0;
	while (true)
	{
		NewPinName = *FString::Printf(TEXT("%s%d"), *UEventsK2Node_EventBase::MessageParamPrefix,i++);
		if (!FindPin(NewPinName))
		{
			break;
		}
	}
	return NewPinName;
}
