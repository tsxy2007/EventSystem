// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "SRenameEventDialog.h"
#include "EventsEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "RenameEvent"

void SRenameEventDialog::Construct(const FArguments& InArgs)
{
	check(InArgs._EventNode.IsValid())

	EventNode = InArgs._EventNode;
	OnEventRenamed = InArgs._OnEventRenamed;

	ChildSlot
	[
		SNew( SBorder )
		.Padding(FMargin(15))
		[
			SNew( SVerticalBox )

			// Current name display
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.Padding(4.0f)
			[
				SNew( SHorizontalBox )

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( STextBlock )
					.Text( LOCTEXT("CurrentEvent", "Current Event:"))
				]

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				.Padding(8.0f, 0.0f)
				[
					SNew( STextBlock )
					.MinDesiredWidth(184.0f)
					.Text( FText::FromName(EventNode->GetCompleteTag().GetTagName() ) )
				]
			]

			
			// New name controls
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			.VAlign(VAlign_Top)
			[
				SNew( SHorizontalBox )

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 4.0f)
				[
					SNew( STextBlock )
					.Text( LOCTEXT("NewEvent", "New Event:" ))
				]

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				.Padding(8.0f, 0.0f)
				[
					SAssignNew( NewTagNameTextBox, SEditableTextBox )
					.Text( FText::FromName(EventNode->GetCompleteTag().GetTagName() ))
					.Padding(4.0f)
					.MinDesiredWidth(180.0f)
					.OnTextCommitted( this, &SRenameEventDialog::OnRenameTextCommitted )
				]
			]

			// Dialog controls
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SNew( SHorizontalBox )

				// Rename
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 8.0f)
				[
					SNew( SButton )
					.IsFocusable( false )
					.IsEnabled( this, &SRenameEventDialog::IsRenameEnabled )
					.OnClicked( this, &SRenameEventDialog::OnRenameClicked )
					[
						SNew( STextBlock )
						.Text( LOCTEXT("RenameEventButtonText", "Rename" ) )
					]
				]

				// Cancel
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 8.0f)
				[
					SNew( SButton )
					.IsFocusable( false )
					.OnClicked( this, & SRenameEventDialog::OnCancelClicked )
					[
						SNew( STextBlock )
						.Text( LOCTEXT("CancelRenameButtonText", "Cancel"))
					]
				]
			]
		]
	];
}

bool SRenameEventDialog::IsRenameEnabled() const
{
	FString CurrentTagText;

	if (NewTagNameTextBox.IsValid())
	{
		CurrentTagText = NewTagNameTextBox->GetText().ToString();
	}

	return !CurrentTagText.IsEmpty() && CurrentTagText != EventNode->GetCompleteTag().GetTagName().ToString();
}

void SRenameEventDialog::RenameAndClose()
{
	IEventsEditorModule& Module = IEventsEditorModule::Get();

	FString TagToRename = EventNode->GetCompleteTag().GetTagName().ToString();
	FString NewTagName = NewTagNameTextBox->GetText().ToString();

	if (Module.RenameTagInINI(TagToRename, NewTagName))
	{
		OnEventRenamed.ExecuteIfBound(TagToRename, NewTagName);
	}

	CloseContainingWindow();
}

void SRenameEventDialog::OnRenameTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter && IsRenameEnabled())
	{
		RenameAndClose();
	}
}

FReply SRenameEventDialog::OnRenameClicked()
{
	RenameAndClose();

	return FReply::Handled();
}

FReply SRenameEventDialog::OnCancelClicked()
{
	CloseContainingWindow();

	return FReply::Handled();
}

void SRenameEventDialog::CloseContainingWindow()
{
	TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().FindWidgetWindow( AsShared() );

	if (CurrentWindow.IsValid())
	{
		CurrentWindow->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
