// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGraphNode_MultiCompareEvent.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "GraphEditorSettings.h"
#include "EventsK2Node_MultiCompareBase.h"
#include "ScopedTransaction.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"

void SGraphNode_MultiCompareEvent::Construct(const FArguments& InArgs, UEventsK2Node_MultiCompareBase* InNode)
{
	GraphNode = InNode;

	SetCursor(EMouseCursor::CardinalCross);

	UpdateGraphNode();
	CreateOutputSideRemoveButton(RightNodeBox);
}

void SGraphNode_MultiCompareEvent::CreateOutputSideRemoveButton(TSharedPtr<SVerticalBox> OutputBox)
{
	TSharedPtr<SWidget> ButtonContent;
	SAssignNew(ButtonContent, SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("CompareNode", "CompareNodeRemovePinButton", "Remove Case"))
			.ColorAndOpacity(FLinearColor::White)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(7, 0, 0, 0)
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_RemoveFromArray")))
		];

	TSharedPtr<SToolTip> Tooltip;
	Tooltip = IDocumentation::Get()->CreateToolTip(NSLOCTEXT("CompareNode", "CompareNodeRemoveCaseButton_Tooltip", "Remove last case pins"), NULL, GraphNode->GetDocumentationLink(), FString());

	TSharedRef<SButton> RemovePinButton = SNew(SButton)
		.ContentPadding(0.0f)
		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		.OnClicked(this, &SGraphNode_MultiCompareEvent::OnRemovePin)
		.ToolTipText(NSLOCTEXT("CompareNode", "CompareNodeRemovePinButton_Tooltip", "Remove last pin"))
		.ToolTip(Tooltip)
		.Visibility(this, &SGraphNode_MultiCompareEvent::IsRemovePinButtonVisible)
		[
			ButtonContent.ToSharedRef()
		];

	RemovePinButton->SetCursor(EMouseCursor::Hand);

	FMargin AddPinPadding = Settings->GetOutputPinPadding();
	AddPinPadding.Top += 6.0f;

	OutputBox->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(AddPinPadding)
		[
			RemovePinButton
		];
}

void SGraphNode_MultiCompareEvent::CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox)
{
	TSharedRef<SWidget> AddPinButton = AddPinButtonContent(
		NSLOCTEXT("CompareNode", "CompareNodeAddPinButton", "Add Case"),
		NSLOCTEXT("CompareNode", "CompareNodeAddPinButton_Tooltip", "Add new case pins"));

	FMargin AddPinPadding = Settings->GetOutputPinPadding();
	AddPinPadding.Top += 6.0f;

	OutputBox->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(AddPinPadding)
		[
			AddPinButton
		];
}

EVisibility SGraphNode_MultiCompareEvent::IsRemovePinButtonVisible() const
{
	UEventsK2Node_MultiCompareBase* CompareNode = CastChecked<UEventsK2Node_MultiCompareBase>(GraphNode);
	if (CompareNode && CompareNode->NumberOfPins > 1)
	{		
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

EVisibility SGraphNode_MultiCompareEvent::IsAddPinButtonVisible() const
{
	if (GraphNode->IsA<UEventsK2Node_MultiCompareBase>())
	{
		return SGraphNode::IsAddPinButtonVisible();
	}
	return EVisibility::Collapsed;
}

FReply SGraphNode_MultiCompareEvent::OnRemovePin()
{
	UEventsK2Node_MultiCompareBase* CompareNode = CastChecked<UEventsK2Node_MultiCompareBase>(GraphNode);

	const FScopedTransaction Transaction(NSLOCTEXT("CompareNode", "AddExecutionPin", "Add Execution Pin"));
	CompareNode->Modify();

	CompareNode->RemovePin();
	CompareNode->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsModified(CompareNode->GetBlueprint());

	UpdateGraphNode();
	GraphNode->GetGraph()->NotifyGraphChanged();

	return FReply::Handled();
}

FReply SGraphNode_MultiCompareEvent::OnAddPin()
{
	UEventsK2Node_MultiCompareBase* CompareNode = CastChecked<UEventsK2Node_MultiCompareBase>(GraphNode);

	const FScopedTransaction Transaction(NSLOCTEXT("CompareNode", "AddExecutionPin", "Add Execution Pin"));
	CompareNode->Modify();

	CompareNode->AddPin();
	CompareNode->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsModified(CompareNode->GetBlueprint());

	UpdateGraphNode();
	GraphNode->GetGraph()->NotifyGraphChanged();

	return FReply::Handled();
}
