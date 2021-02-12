// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventsK2Node_LiteralEvent.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"

#include "KismetCompiler.h"
#include "EventContainer.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintEventLibrary.h"

#define LOCTEXT_NAMESPACE "EventsK2Node_LiteralEvent"

UEventsK2Node_LiteralEvent::UEventsK2Node_LiteralEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UEventsK2Node_LiteralEvent::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("LiteralEventContainer"), TEXT("TagIn"));
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, FEventContainer::StaticStruct(), UEdGraphSchema_K2::PN_ReturnValue);
}

FLinearColor UEventsK2Node_LiteralEvent::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.51f, 0.0f);
}

FText UEventsK2Node_LiteralEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "LiteralEvent", "Make Literal EventContainer");
}

bool UEventsK2Node_LiteralEvent::CanCreateUnderSpecifiedSchema( const UEdGraphSchema* Schema ) const
{
	return Schema->IsA(UEdGraphSchema_K2::StaticClass());
}

void UEventsK2Node_LiteralEvent::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	ensureMsgf(0, TEXT("UEventsK2Node_LiteralEvent is deprecated and should never make it to compile time"));
}

void UEventsK2Node_LiteralEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	
}

FText UEventsK2Node_LiteralEvent::GetMenuCategory() const
{
	return LOCTEXT("ActionMenuCategory", "Event");
}

FEdGraphNodeDeprecationResponse UEventsK2Node_LiteralEvent::GetDeprecationResponse(EEdGraphNodeDeprecationType DeprecationType) const
{
	FEdGraphNodeDeprecationResponse Response = Super::GetDeprecationResponse(DeprecationType);
	Response.MessageText = LOCTEXT("NodeDeprecated_Warning", "@@ is deprecated, replace with Make Literal EventContainer function call");

	return Response;
}

void UEventsK2Node_LiteralEvent::ConvertDeprecatedNode(UEdGraph* Graph, bool bOnlySafeChanges)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	TMap<FName, FName> OldPinToNewPinMap;

	UFunction* MakeFunction = UBlueprintEventLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintEventLibrary, MakeLiteralEventContainer));
	OldPinToNewPinMap.Add(TEXT("TagIn"), TEXT("Value"));

	ensure(Schema->ConvertDeprecatedNodeToFunctionCall(this, MakeFunction, OldPinToNewPinMap, Graph) != nullptr);
}

#undef LOCTEXT_NAMESPACE
