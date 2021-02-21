// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAddNewEventWidget.h"
#include "DetailLayoutBuilder.h"
#include "EventsSettings.h"
#include "EventsEditorModule.h"
#include "EventsRuntimeModule.h"
#include "Widgets/Input/SButton.h"
#include "Misc/MessageDialog.h"
#include "PropertyCustomizationHelpers.h"
#include "SPinTypeSelector.h"
#include "UnrealCompatibility.h"
#include "EventSystemBPLibrary.h"
#include "BlueprintCompilerCppBackend/Private/BlueprintCompilerCppBackendUtils.h"

#define LOCTEXT_NAMESPACE "AddNewEventWidget"

SAddNewEventWidget::~SAddNewEventWidget()
{
	if (!GExitPurge)
	{
		IEventsModule::OnTagSettingsChanged.RemoveAll(this);
	}
}

void SAddNewEventWidget::Construct(const FArguments& InArgs)
{
	
	FText HintText = LOCTEXT("NewTagNameHint", "X.Y.Z");
	DefaultNewName = InArgs._NewTagName;
	if (DefaultNewName.IsEmpty() == false)
	{
		HintText = FText::FromString(DefaultNewName);
	}


	bAddingNewTag = false;
	bShouldGetKeyboardFocus = false;

	OnEventAdded = InArgs._OnEventAdded;
	IsValidTag = InArgs._IsValidTag;
	PopulateTagSources();

	IEventsModule::OnTagSettingsChanged.AddRaw(this, &SAddNewEventWidget::PopulateTagSources);

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
				.Text(LOCTEXT("NewTagName", "Name:"))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SAssignNew(TagNameTextBox, SEditableTextBox)
				.MinDesiredWidth(240.0f)
				.HintText(HintText)
				.OnTextCommitted(this, &SAddNewEventWidget::OnCommitNewTagName)
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
				.Text(LOCTEXT("TagParameters", "Parameters:"))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("AddNewParameter", "New Parameter"))
					.OnClicked(this, &SAddNewEventWidget::OnAddNewParameterButtonPressed)
				]
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
				.Text(LOCTEXT("CreateTagSource", "Source:"))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SAssignNew(TagSourcesComboBox, SComboBox<TSharedPtr<FName> >)
				.OptionsSource(&TagSources)
				.OnGenerateWidget(this, &SAddNewEventWidget::OnGenerateTagSourcesComboBox)
				.ContentPadding(2.0f)
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &SAddNewEventWidget::CreateTagSourcesComboBoxContent)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		]
		// Tag Parameters
		+ SVerticalBox::Slot()
		.MaxHeight(100)
		.VAlign(VAlign_Top)
		[
			SAssignNew(ListViewWidget, SListView<TSharedPtr<FEventParameterDetail>>)
			.ItemHeight(24)
			.ListItemsSource(&MessageTables)  //The Items array is the source of this listview
			.OnGenerateRow(this, &SAddNewEventWidget::OnGenerateParameterRow)
			.SelectionMode(ESelectionMode::None)
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
				.Text(LOCTEXT("AddNew", "Add New Tag"))
				.OnClicked(this, &SAddNewEventWidget::OnAddNewTagButtonPressed)
			]
		]
	];

	Reset(EResetType::ResetAll);
}

void SAddNewEventWidget::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bShouldGetKeyboardFocus)
	{
		bShouldGetKeyboardFocus = false;
		FSlateApplication::Get().SetKeyboardFocus(TagNameTextBox.ToSharedRef(), EFocusCause::SetDirectly);
	}
}

void SAddNewEventWidget::PopulateTagSources()
{
	UEventsManager& Manager = UEventsManager::Get();
	TagSources.Empty();

	FName DefaultSource = FEventSource::GetDefaultName();

	// Always ensure that the default source is first
	TagSources.Add( MakeShareable( new FName( DefaultSource ) ) );

	TArray<const FEventSource*> Sources;
	Manager.FindTagSourcesWithType(EEventSourceType::TagList, Sources);

	for (const FEventSource* Source : Sources)
	{
		if (Source != nullptr && Source->SourceName != DefaultSource)
		{
			TagSources.Add(MakeShareable(new FName(Source->SourceName)));
		}
	}

	//Set selection to the latest added source
	if (TagSourcesComboBox != nullptr)
	{
		TagSourcesComboBox->SetSelectedItem(TagSources.Last());
	}	
}

void SAddNewEventWidget::Reset(EResetType ResetType)
{
	SetTagName();
	if (ResetType != EResetType::DoNotResetSource)
	{
		SelectTagSource();
	}
}

void SAddNewEventWidget::SetTagName(const FText& InName)
{
	TagNameTextBox->SetText(InName.IsEmpty() ? FText::FromString(DefaultNewName) : InName);
}

void SAddNewEventWidget::SelectTagSource(const FName& InSource)
{
	// Attempt to find the location in our sources, otherwise just use the first one
	int32 SourceIndex = 0;

	if (!InSource.IsNone())
	{
		for (int32 Index = 0; Index < TagSources.Num(); ++Index)
		{
			TSharedPtr<FName> Source = TagSources[Index];

			if (Source.IsValid() && *Source.Get() == InSource)
			{
				SourceIndex = Index;
				break;
			}
		}
	}

	TagSourcesComboBox->SetSelectedItem(TagSources[SourceIndex]);
}

void SAddNewEventWidget::OnCommitNewTagName(const FText& InText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter)
	{
		CreateNewEvent();
	}
}

FReply SAddNewEventWidget::OnAddNewTagButtonPressed()
{
	CreateNewEvent();
	return FReply::Handled();
}


FReply SAddNewEventWidget::OnAddNewParameterButtonPressed()
{
	static FEdGraphPinType StringType;
	StringType.PinCategory = UEdGraphSchema_K2::PC_String;
	auto& Ref = Add_GetRef(MessageTables, MakeShared<FEventParameterDetail>());
	Ref->Name = *FString::Printf(TEXT("Param%d"), MessageTables.Num() - 1);
	Ref->Type = TEXT("String");
	Ref->PinType = StringType;

	ListViewWidget->RequestListRefresh();
	return FReply::Handled();
}

void SAddNewEventWidget::AddSubtagFromParent(const FString& ParentTagName, const FName& ParentTagSource)
{
	FText SubtagBaseName = !ParentTagName.IsEmpty() ? FText::Format(FText::FromString(TEXT("{0}.")), FText::FromString(ParentTagName)) : FText();

	SetTagName(SubtagBaseName);
	SelectTagSource(ParentTagSource);

	bShouldGetKeyboardFocus = true;
}

void SAddNewEventWidget::CreateNewEvent()
{
	UEventsManager& Manager = UEventsManager::Get();

	// Only support adding tags via ini file
	if (Manager.ShouldImportTagsFromINI() == false)
	{
		return;
	}

	if (TagSourcesComboBox->GetSelectedItem().Get() == nullptr)
	{
		return;
	}

	FText TagNameAsText = TagNameTextBox->GetText();
	FString TagName = TagNameAsText.ToString();
	FString TagComment = "";
	FName TagSource = *TagSourcesComboBox->GetSelectedItem().Get();

	if (TagName.IsEmpty())
	{
		return;
	}

	// check to see if this is a valid tag
	// first check the base rules for all tags then look for any additional rules in the delegate
	FText ErrorMsg;
	if (!UEventsManager::Get().IsValidEventString(TagName, &ErrorMsg) || 
		(IsValidTag.IsBound() && !IsValidTag.Execute(TagName, &ErrorMsg))
		)
	{
		FText MessageTitle(LOCTEXT("InvalidTag", "Invalid Tag"));
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMsg, &MessageTitle);
		return;
	}


	// set bIsAddingNewTag, this guards against the window closing when it loses focus due to source control checking out a file
	TGuardValue<bool>	Guard(bAddingNewTag, true);

	TArray<FEventParameter> ParamterData;
	for (auto EventParam : MessageTables)
	{
		ParamterData.Add({ EventParam->Name, EventParam->Type });
	}

	IEventsEditorModule::Get().AddNewEventToINI(TagName, TagComment, TagSource,ParamterData);

	OnEventAdded.ExecuteIfBound(TagName, TagComment, TagSource);

	Reset(EResetType::DoNotResetSource);
}

TSharedRef<SWidget> SAddNewEventWidget::OnGenerateTagSourcesComboBox(TSharedPtr<FName> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromName(*InItem.Get()));
}

FText SAddNewEventWidget::CreateTagSourcesComboBoxContent() const
{
	const bool bHasSelectedItem = TagSourcesComboBox.IsValid() && TagSourcesComboBox->GetSelectedItem().IsValid();

	return bHasSelectedItem ? FText::FromName(*TagSourcesComboBox->GetSelectedItem().Get()) : LOCTEXT("NewTagLocationNotSelected", "Not selected");
}


TSharedRef<class ITableRow> SAddNewEventWidget::OnGenerateParameterRow(TSharedPtr<FEventParameterDetail> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FEventParameterDetail>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.1f)
			.Padding(0.f, 1.f, 0.f, 1.f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([InItem] { return FText::FromName(InItem->Name); })
				// .OnTextChanged_Lambda([InItem](const FText& Text) { InItem->Name = *Text.ToString(); })
				.OnTextCommitted_Lambda([InItem](const FText& Text, ETextCommit::Type InTextCommit) { InItem->Name = *Text.ToString(); })
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.IsEnabled(true)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			.AutoWidth()
			[
				SAssignNew(PinTypeSelector, SPinTypeSelector, FGetPinTypeTree::CreateUObject(GetDefault<UEdGraphSchema_K2>(), &UEdGraphSchema_K2::GetVariableTypeTree))
				.TargetPinType_Lambda([this, InItem] { return GetPinInfo(InItem); })
				.OnPinTypeChanged_Lambda([this, InItem](const FEdGraphPinType& Type) {
				if (InItem->PinType != Type)
				{
					InItem->PinType = Type;
					InItem->Type = FName(*UEventSystemBPLibrary::GetParameterType(Type));
				}
				if (ListViewWidget.IsValid())
				{
					FSlateApplication::Get().SetKeyboardFocus(ListViewWidget->AsWidget(), EFocusCause::SetDirectly);
				}
					})
				.Schema(GetDefault<UEdGraphSchema_K2>())
						.TypeTreeFilter(ETypeTreeFilter::None)
						.bAllowArrays(true)
						.IsEnabled(true)
						.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(10, 0, 0, 0)
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateLambda([this, InItem] { OnRemoveClicked(InItem); }), LOCTEXT("FunctionArgDetailsClearTooltip", "Remove this parameter."), true)
			]
		];
}


FEdGraphPinType SAddNewEventWidget::GetPinInfo(const TSharedPtr<FEventParameterDetail>& InItem)
{
	return InItem->PinType;
}


void SAddNewEventWidget::OnRemoveClicked(const TSharedPtr<FEventParameterDetail>& InItem)
{
	MessageTables.Remove(InItem);
	ListViewWidget->RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE
