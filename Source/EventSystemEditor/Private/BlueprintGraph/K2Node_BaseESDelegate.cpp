// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintGraph/K2Node_BaseESDelegate.h"

UK2Node_BaseESDelegate::UK2Node_BaseESDelegate(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

UK2Node::ERedirectType UK2Node_BaseESDelegate::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	return Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
}

FString UK2Node_BaseESDelegate::GetDocumentationLink() const
{
	return Super::GetDocumentationLink();
}

FString UK2Node_BaseESDelegate::GetDocumentationExcerptName() const
{
	return Super::GetDocumentationExcerptName();
}

void UK2Node_BaseESDelegate::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{

}

bool UK2Node_BaseESDelegate::HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	return Super::HasExternalDependencies(OptionalOutput);
}

void UK2Node_BaseESDelegate::GetNodeAttributes(TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes) const
{

}

void UK2Node_BaseESDelegate::AutowireNewNode(UEdGraphPin* FromPin)
{

}

bool UK2Node_BaseESDelegate::HasDeprecatedReference() const
{
	return Super::HasDeprecatedReference();
}

FEdGraphNodeDeprecationResponse UK2Node_BaseESDelegate::GetDeprecationResponse(EEdGraphNodeDeprecationType DeprecationType) const
{
	return Super::GetDeprecationResponse(DeprecationType);
}

void UK2Node_BaseESDelegate::AllocateDefaultPins()
{

}

void UK2Node_BaseESDelegate::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{

}

bool UK2Node_BaseESDelegate::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	return Super::IsCompatibleWithGraph(TargetGraph);
}
