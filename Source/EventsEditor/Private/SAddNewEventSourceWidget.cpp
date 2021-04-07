// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "SAddNewEventSourceWidget.h"
#include "DetailLayoutBuilder.h"
#include "EventsSettings.h"
#include "EventsEditorModule.h"
#include "EventsRuntimeModule.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "AddNewEventSourceWidget"

void SAddNewEventSourceWidget::Construct(const FArguments& InArgs)
{
	FText HintText = LOCTEXT("NewSourceNameHint", "SourceName.ini");
	DefaultNewName = InArgs._NewSourceName;
	if (DefaultNewName.IsEmpty() == false)
	{
		HintText = FText::FromString(DefaultNewName);
	}

	bShouldGetKeyboardFocus = false;

	OnEventSourceAdded = InArgs._OnEventSourceAdded;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Tag Name
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 4.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NewSourceName", "Name:"))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SAssignNew(SourceNameTextBox, SEditableTextBox)
				.MinDesiredWidth(240.0f)
				.HintText(HintText)
			]
		]

		// Add Source Button
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("AddNew", "Add New Source"))
				.OnClicked(this, &SAddNewEventSourceWidget::OnAddNewSourceButtonPressed)
			]
		]
	];

	Reset();
}

void SAddNewEventSourceWidget::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bShouldGetKeyboardFocus)
	{
		bShouldGetKeyboardFocus = false;
		FSlateApplication::Get().SetKeyboardFocus(SourceNameTextBox.ToSharedRef(), EFocusCause::SetDirectly);
	}
}

void SAddNewEventSourceWidget::Reset()
{
	SetSourceName();
}

void SAddNewEventSourceWidget::SetSourceName(const FText& InName)
{
	SourceNameTextBox->SetText(InName.IsEmpty() ? FText::FromString(DefaultNewName) : InName);
}

FReply SAddNewEventSourceWidget::OnAddNewSourceButtonPressed()
{
	UEventsManager& Manager = UEventsManager::Get();
		
	if (!SourceNameTextBox->GetText().EqualTo(FText::FromString(DefaultNewName)))
	{
		Manager.FindOrAddTagSource(*SourceNameTextBox->GetText().ToString(), EEventSourceType::TagList);
	}

	IEventsModule::OnTagSettingsChanged.Broadcast();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
