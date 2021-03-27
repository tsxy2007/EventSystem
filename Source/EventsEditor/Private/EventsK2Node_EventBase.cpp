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
#include "K2Node_EditablePinBase.h"
#include "UObject/FrameworkObjectVersion.h"

namespace
{
	static FName DefaultPinName(TEXT("Default"));
	static FName EventPinName(TEXT("CustomEvent"));
	static FName SenderPinName(TEXT("Sernder"));
}



FArchive& operator<<(FArchive& Ar, FUserPinInfo& Info)
{
	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	if (Ar.CustomVer(FFrameworkObjectVersion::GUID) >= FFrameworkObjectVersion::PinsStoreFName)
	{
		Ar << Info.PinName;
	}
	else
	{
		FString PinNameStr;
		Ar << PinNameStr;
		Info.PinName = *PinNameStr;
	}

	if (Ar.UE4Ver() >= VER_UE4_SERIALIZE_PINTYPE_CONST)
	{
		Info.PinType.Serialize(Ar);
		Ar << Info.DesiredPinDirection;
	}
	else
	{
		check(Ar.IsLoading());

		bool bIsArray = (Info.PinType.ContainerType == EPinContainerType::Array);
		Ar << bIsArray;

		bool bIsReference = Info.PinType.bIsReference;
		Ar << bIsReference;

		Info.PinType.ContainerType = (bIsArray ? EPinContainerType::Array : EPinContainerType::None);
		Info.PinType.bIsReference = bIsReference;

		FString PinCategoryStr;
		FString PinSubCategoryStr;

		Ar << PinCategoryStr;
		Ar << PinSubCategoryStr;

		Info.PinType.PinCategory = *PinCategoryStr;
		Info.PinType.PinSubCategory = *PinSubCategoryStr;

		Ar << Info.PinType.PinSubCategoryObject;
	}

	Ar << Info.PinDefaultValue;

	return Ar;
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
				RemoveUserDefinedPinByName(Pins[PinIdx]->PinName);
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
					AddInnerPin(*UEventsK2Node_EventBase::MessageParamPrefix, PinType);
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
	static bool bRecursivelyChangingDefaultValue = false;
	for (int32 Index = 0; Index < UserDefinedPins.Num(); ++Index)
	{
		TSharedPtr<FUserPinInfo> PinInfo = UserDefinedPins[Index];
		if (Pin->PinName == PinInfo->PinName && Pin->Direction == PinInfo->DesiredPinDirection)
		{
			FString DefaultsString = Pin->GetDefaultAsString();

			if (DefaultsString != PinInfo->PinDefaultValue)
			{
				// Make sure this doesn't get called recursively
				TGuardValue<bool> CircularGuard(bRecursivelyChangingDefaultValue, true);
				ModifyUserDefinedPinDefaultValue(PinInfo, Pin->GetDefaultAsString());
			}
		}
	}
}

FString UEventsK2Node_EventBase::MessageParamPrefix = TEXT("Param");

void UEventsK2Node_EventBase::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UEdGraphSchema_K2::PSC_Self, UEdGraphSchema_K2::PN_Self);
	CreateSelectionPin();

	for (int32 i = 0; i < UserDefinedPins.Num(); i++)
	{
		if (!FindPin(UserDefinedPins[i]->PinName))
		{
			CreatePinFromUserDefinition(UserDefinedPins[i]);
		}
	}
}

FText UEventsK2Node_EventBase::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "TestTag_EVENT_ToolTip", "Selects an output that matches the input value");
}

void UEventsK2Node_EventBase::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
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


UEdGraphPin* UEventsK2Node_EventBase::CreateUserDefinedPin(const FName InPinName, const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection)
{
	// Sanitize the name, if needed
	const FName NewPinName = CreateUniquePinName(InPinName) ;
	PinNames.Add(NewPinName);
	// First, add this pin to the user-defined pins
	TSharedPtr<FUserPinInfo> NewPinInfo = MakeShareable(new FUserPinInfo());
	NewPinInfo->PinName = NewPinName;
	NewPinInfo->PinType = InPinType;
	NewPinInfo->DesiredPinDirection = InDesiredDirection;
	UserDefinedPins.Add(NewPinInfo);

	// Then, add the pin to the actual Pins array
	UEdGraphPin* NewPin = CreatePinFromUserDefinition(NewPinInfo);

	return NewPin;
}

void UEventsK2Node_EventBase::RemoveUserDefinedPinByName(const FName PinName)
{
	// Remove the description from the user-defined pins array
	UserDefinedPins.RemoveAll([&](const TSharedPtr<FUserPinInfo>& UDPin)
		{
			return UDPin.IsValid() && (UDPin->PinName == PinName);
		});
}

void UEventsK2Node_EventBase::Serialize(FArchive& Ar)
{

	// Do not call parent, but call grandparent
	Super::Serialize(Ar);
	

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	TArray<FUserPinInfo> SerializedItems;
	if (Ar.IsLoading())
	{
		Ar << SerializedItems;

		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		UserDefinedPins.Empty(SerializedItems.Num());
		for (int32 Index = 0; Index < SerializedItems.Num(); ++Index)
		{
			TSharedPtr<FUserPinInfo> PinInfo = MakeShareable(new FUserPinInfo(SerializedItems[Index]));

			UserDefinedPins.Add(PinInfo);
		}
	}
	else if (Ar.IsSaving())
	{
		SerializedItems.Empty(UserDefinedPins.Num());

		for (int32 Index = 0; Index < UserDefinedPins.Num(); ++Index)
		{
			SerializedItems.Add(*(UserDefinedPins[Index].Get()));
		}

		Ar << SerializedItems;
	}
	else
	{
		for (TSharedPtr<FUserPinInfo>& PinInfo : UserDefinedPins)
		{
			Ar << *PinInfo;
		}
	}
}

bool UEventsK2Node_EventBase::ModifyUserDefinedPinDefaultValue(TSharedPtr<FUserPinInfo> PinInfo, const FString& InDefaultValue)
{
	FString NewDefaultValue = InDefaultValue;

	if (!UpdateEdGraphPinDefaultValue(PinInfo, NewDefaultValue))
	{
		return false;
	}

	PinInfo->PinDefaultValue = NewDefaultValue;
	return true;
}

bool UEventsK2Node_EventBase::UpdateEdGraphPinDefaultValue(TSharedPtr<FUserPinInfo> PinInfo, FString& NewDefaultValue)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	// Find and modify the current pin
	if (UEdGraphPin* OldPin = FindPin(PinInfo->PinName))
	{
		FString SavedDefaultValue = OldPin->DefaultValue;

		K2Schema->SetPinAutogeneratedDefaultValue(OldPin, NewDefaultValue);

		// Validate the new default value
		FString ErrorString = K2Schema->IsCurrentPinDefaultValid(OldPin);

		if (!ErrorString.IsEmpty())
		{
			NewDefaultValue = SavedDefaultValue;
			K2Schema->SetPinAutogeneratedDefaultValue(OldPin, SavedDefaultValue);

			return false;
		}
	}

	return true;
}

UEdGraphPin* UEventsK2Node_EventBase::GetEventPin() const
{
	return FindPin(EventPinName);
}

UEdGraphPin* UEventsK2Node_EventBase::GetSelfPin() const
{
	return FindPin(UEdGraphSchema_K2::PN_Self);
}

UEdGraphPin* UEventsK2Node_EventBase::GetThenPin() const
{
	UEdGraphPin* Pin = FindPinChecked(UEdGraphSchema_K2::PN_Then);
	check(Pin->Direction == EGPD_Output);
	return Pin;
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
