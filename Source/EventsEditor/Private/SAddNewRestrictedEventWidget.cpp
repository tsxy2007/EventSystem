// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "SAddNewRestrictedEventWidget.h"
#include "DetailLayoutBuilder.h"
#include "EventsSettings.h"
#include "EventsEditorModule.h"
#include "EventsRuntimeModule.h"
#include "SEventWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#define LOCTEXT_NAMESPACE "AddNewRestrictedEventWidget"

SAddNewRestrictedEventWidget::~SAddNewRestrictedEventWidget()
{
	if (!GExitPurge)
	{
		IEventsModule::OnTagSettingsChanged.RemoveAll(this);
	}
}

void SAddNewRestrictedEventWidget::Construct(const FArguments& InArgs)
{
	FText HintText = LOCTEXT("NewEventNameHint", "X.Y.Z");
	DefaultNewName = InArgs._NewRestrictedTagName;
	if (DefaultNewName.IsEmpty() == false)
	{
		HintText = FText::FromString(DefaultNewName);
	}


	bAddingNewRestrictedTag = false;
	bShouldGetKeyboardFocus = false;

	OnRestrictedEventAdded = InArgs._OnRestrictedEventAdded;
	PopulateTagSources();

	IEventsModule::OnTagSettingsChanged.AddRaw(this, &SAddNewRestrictedEventWidget::PopulateTagSources);

	ChildSlot
	[
		SNew(SVerticalBox)

		// Restricted Tag Name
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
				.Text(LOCTEXT("NewEventName", "Name:"))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SAssignNew(TagNameTextBox, SEditableTextBox)
				.MinDesiredWidth(240.0f)
				.HintText(HintText)
				.OnTextCommitted(this, &SAddNewRestrictedEventWidget::OnCommitNewTagName)
			]
		]

		// Tag Comment
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
				.Text(LOCTEXT("EventComment", "Comment:"))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SAssignNew(TagCommentTextBox, SEditableTextBox)
				.MinDesiredWidth(240.0f)
				.HintText(LOCTEXT("EventCommentHint", "Comment"))
				.OnTextCommitted(this, &SAddNewRestrictedEventWidget::OnCommitNewTagName)
			]
		]

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
				.Text(LOCTEXT("AllowNonRestrictedChildren", "Allow non-restricted children:"))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SAssignNew(AllowNonRestrictedChildrenCheckBox, SCheckBox)
			]
			]

		// Tag Location
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 6.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CreateEventSource", "Source:"))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SAssignNew(TagSourcesComboBox, SComboBox<TSharedPtr<FName> >)
				.OptionsSource(&RestrictedTagSources)
				.OnGenerateWidget(this, &SAddNewRestrictedEventWidget::OnGenerateTagSourcesComboBox)
				.ContentPadding(2.0f)
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &SAddNewRestrictedEventWidget::CreateTagSourcesComboBoxContent)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		]

		// Add Tag Button
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
				.Text(LOCTEXT("AddNew", "Add New Event"))
				.OnClicked(this, &SAddNewRestrictedEventWidget::OnAddNewTagButtonPressed)
			]
		]
	];

	Reset();
}

void SAddNewRestrictedEventWidget::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bShouldGetKeyboardFocus)
	{
		bShouldGetKeyboardFocus = false;
		FSlateApplication::Get().SetKeyboardFocus(TagNameTextBox.ToSharedRef(), EFocusCause::SetDirectly);
	}
}

void SAddNewRestrictedEventWidget::PopulateTagSources()
{
	UEventsManager& Manager = UEventsManager::Get();
	RestrictedTagSources.Empty();

	TArray<const FEventSource*> Sources;
	Manager.GetRestrictedTagSources(Sources);

	// Used to make sure we have a non-empty list of restricted tag sources. Not an actual source.
	FName PlaceholderSource = NAME_None;

	// Add the placeholder source if no other sources exist
	if (Sources.Num() == 0)
	{
		RestrictedTagSources.Add(MakeShareable(new FName(PlaceholderSource)));
	}

	for (const FEventSource* Source : Sources)
	{
		if (Source != nullptr && Source->SourceName != PlaceholderSource)
		{
			RestrictedTagSources.Add(MakeShareable(new FName(Source->SourceName)));
		}
	}
}

void SAddNewRestrictedEventWidget::Reset(FName TagSource)
{
	SetTagName();
	SelectTagSource(TagSource);
	SetAllowNonRestrictedChildren();
	TagCommentTextBox->SetText(FText());
}

void SAddNewRestrictedEventWidget::SetTagName(const FText& InName)
{
	TagNameTextBox->SetText(InName.IsEmpty() ? FText::FromString(DefaultNewName) : InName);
}

void SAddNewRestrictedEventWidget::SelectTagSource(const FName& InSource)
{
	// Attempt to find the location in our sources, otherwise just use the first one
	int32 SourceIndex = 0;

	if (!InSource.IsNone())
	{
		for (int32 Index = 0; Index < RestrictedTagSources.Num(); ++Index)
		{
			TSharedPtr<FName> Source = RestrictedTagSources[Index];

			if (Source.IsValid() && *Source.Get() == InSource)
			{
				SourceIndex = Index;
				break;
			}
		}
	}

	TagSourcesComboBox->SetSelectedItem(RestrictedTagSources[SourceIndex]);
}

void SAddNewRestrictedEventWidget::SetAllowNonRestrictedChildren(bool bInAllowNonRestrictedChildren)
{
	AllowNonRestrictedChildrenCheckBox->SetIsChecked(bInAllowNonRestrictedChildren ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SAddNewRestrictedEventWidget::OnCommitNewTagName(const FText& InText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter)
	{
		ValidateNewRestrictedTag();
	}
}

FReply SAddNewRestrictedEventWidget::OnAddNewTagButtonPressed()
{
	ValidateNewRestrictedTag();
	return FReply::Handled();
}

void SAddNewRestrictedEventWidget::AddSubtagFromParent(const FString& ParentTagName, const FName& ParentTagSource, bool bAllowNonRestrictedChildren)
{
	FText SubtagBaseName = !ParentTagName.IsEmpty() ? FText::Format(FText::FromString(TEXT("{0}.")), FText::FromString(ParentTagName)) : FText();

	SetTagName(SubtagBaseName);
	SelectTagSource(ParentTagSource);
	SetAllowNonRestrictedChildren(bAllowNonRestrictedChildren);

	bShouldGetKeyboardFocus = true;
}

void SAddNewRestrictedEventWidget::ValidateNewRestrictedTag()
{
	UEventsManager& Manager = UEventsManager::Get();

	FString TagName = TagNameTextBox->GetText().ToString();
	FString TagComment = TagCommentTextBox->GetText().ToString();
	bool bAllowNonRestrictedChildren = AllowNonRestrictedChildrenCheckBox->IsChecked();
	FName TagSource = *TagSourcesComboBox->GetSelectedItem().Get();

	if (TagSource == NAME_None)
	{
		FNotificationInfo Info(LOCTEXT("NoRestrictedSource", "You must specify a source file for restricted events."));
		Info.ExpireDuration = 10.f;
		Info.bUseSuccessFailIcons = true;
		Info.Image = FEditorStyle::GetBrush(TEXT("MessageLog.Error"));

		AddRestrictedEventDialog = FSlateNotificationManager::Get().AddNotification(Info);

		return;
	}

	TArray<FString> TagSourceOwners;
	Manager.GetOwnersForTagSource(TagSource.ToString(), TagSourceOwners);

	bool bHasOwner = false;
	for (const FString& Owner : TagSourceOwners)
	{
		if (!Owner.IsEmpty())
		{
			bHasOwner = true;
			break;
		}
	}

	if (bHasOwner)
	{
		// check if we're one of the owners; if we are then we don't need to pop up the permission dialog
		bool bRequiresPermission = true;
		const FString& UserName = FPlatformProcess::UserName();
		for (const FString& Owner : TagSourceOwners)
		{
			if (Owner.Equals(UserName))
			{
				CreateNewRestrictedEvent();
				bRequiresPermission = false;
			}
		}

		if (bRequiresPermission)
		{
			FString StringToDisplay = TEXT("Do you have permission from ");
			StringToDisplay.Append(TagSourceOwners[0]);
			for (int Idx = 1; Idx < TagSourceOwners.Num(); ++Idx)
			{
				StringToDisplay.Append(TEXT(" or "));
				StringToDisplay.Append(TagSourceOwners[Idx]);
			}
			StringToDisplay.Append(TEXT(" to modify "));
			StringToDisplay.Append(TagSource.ToString());
			StringToDisplay.Append(TEXT("?"));

			FNotificationInfo Info(FText::FromString(StringToDisplay));
			Info.ExpireDuration = 10.f;
			Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("RestrictedEventPopupButtonAccept", "Yes"), FText(), FSimpleDelegate::CreateSP(this, &SAddNewRestrictedEventWidget::CreateNewRestrictedEvent), SNotificationItem::CS_None));
			Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("RestrictedEventPopupButtonReject", "No"), FText(), FSimpleDelegate::CreateSP(this, &SAddNewRestrictedEventWidget::CancelNewTag), SNotificationItem::CS_None));

			AddRestrictedEventDialog = FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
	else
	{
		CreateNewRestrictedEvent();
	}
}

void SAddNewRestrictedEventWidget::CreateNewRestrictedEvent()
{
	if (AddRestrictedEventDialog.IsValid())
	{
		AddRestrictedEventDialog->SetVisibility(EVisibility::Collapsed);
	}

	UEventsManager& Manager = UEventsManager::Get();

	// Only support adding tags via ini file
	if (Manager.ShouldImportTagsFromINI() == false)
	{
		return;
	}

	FString TagName = TagNameTextBox->GetText().ToString();
	FString TagComment = TagCommentTextBox->GetText().ToString();
	bool bAllowNonRestrictedChildren = AllowNonRestrictedChildrenCheckBox->IsChecked();
	FName TagSource = *TagSourcesComboBox->GetSelectedItem().Get();

	if (TagName.IsEmpty())
	{
		return;
	}

	// set bIsAddingNewTag, this guards against the window closing when it loses focus due to source control checking out a file
	TGuardValue<bool>	Guard(bAddingNewRestrictedTag, true);

	TArray<FEventParameter> ParameterData;
	IEventsEditorModule::Get().AddNewEventToINI(TagName, TagComment, TagSource, ParameterData, true, bAllowNonRestrictedChildren);

	OnRestrictedEventAdded.ExecuteIfBound(TagName, TagComment, TagSource);

	Reset(TagSource);
}

void SAddNewRestrictedEventWidget::CancelNewTag()
{
	AddRestrictedEventDialog->SetVisibility(EVisibility::Collapsed);
}

TSharedRef<SWidget> SAddNewRestrictedEventWidget::OnGenerateTagSourcesComboBox(TSharedPtr<FName> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromName(*InItem.Get()));
}

FText SAddNewRestrictedEventWidget::CreateTagSourcesComboBoxContent() const
{
	const bool bHasSelectedItem = TagSourcesComboBox.IsValid() && TagSourcesComboBox->GetSelectedItem().IsValid();

	return bHasSelectedItem ? FText::FromName(*TagSourcesComboBox->GetSelectedItem().Get()) : LOCTEXT("NewEventLocationNotSelected", "Not selected");
}

#undef LOCTEXT_NAMESPACE
