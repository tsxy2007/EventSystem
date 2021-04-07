// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "SEventGraphPin.h"
#include "Widgets/Input/SComboButton.h"
#include "EventsRuntimeModule.h"
#include "Widgets/Layout/SScaleBox.h"
#include "EventPinUtilities.h"

#define LOCTEXT_NAMESPACE "EventGraphPin"

void SEventGraphPin::Construct( const FArguments& InArgs, UEdGraphPin* InGraphPinObj )
{
	TagContainer = MakeShareable( new FEventContainer() );
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
}

TSharedRef<SWidget>	SEventGraphPin::GetDefaultValueWidget()
{
	ParseDefaultValueData();

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew( ComboButton, SComboButton )
			.OnGetMenuContent( this, &SEventGraphPin::GetListContent )
			.ContentPadding( FMargin( 2.0f, 2.0f ) )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.MenuPlacement(MenuPlacement_BelowAnchor)
			.ButtonContent()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("EventWidget_Edit", "Edit") )
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SelectedTags()
		];
}

void SEventGraphPin::ParseDefaultValueData()
{
	FString TagString = GraphPinObj->GetDefaultAsString();

	FilterString = EventPinUtilities::ExtractTagFilterStringFromGraphPin(GraphPinObj);

	if (TagString.StartsWith(TEXT("("), ESearchCase::CaseSensitive) && TagString.EndsWith(TEXT(")"), ESearchCase::CaseSensitive))
	{
		TagString.LeftChopInline(1, false);
		TagString.RightChopInline(1, false);
		TagString.Split(TEXT("="), nullptr, &TagString, ESearchCase::CaseSensitive);
		if (TagString.StartsWith(TEXT("\""), ESearchCase::CaseSensitive) && TagString.EndsWith(TEXT("\""), ESearchCase::CaseSensitive))
		{
			TagString.LeftChopInline(1, false);
			TagString.RightChopInline(1, false);
		}
	}

	if (!TagString.IsEmpty())
	{
		FEventInfo Event = FEventInfo::RequestEvent(FName(*TagString));
		TagContainer->AddTag(Event);
	}
}

TSharedRef<SWidget> SEventGraphPin::GetListContent()
{
	EditableContainers.Empty();
	EditableContainers.Add( SEventWidget::FEditableEventContainerDatum( GraphPinObj->GetOwningNode(), TagContainer.Get() ) );

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew( SEventWidget, EditableContainers )
			.OnTagChanged( this, &SEventGraphPin::RefreshTagList )
			.TagContainerName( TEXT("SEventGraphPin") )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.MultiSelect(false)
			.Filter(FilterString)
		];
}

TSharedRef<SWidget> SEventGraphPin::SelectedTags()
{
	RefreshTagList();

	SAssignNew( TagListView, SListView<TSharedPtr<FString>> )
		.ListItemsSource(&TagNames)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SEventGraphPin::OnGenerateRow);

	return TagListView->AsShared();
}

TSharedRef<ITableRow> SEventGraphPin::OnGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
		[
			SNew(STextBlock) .Text( FText::FromString(*Item.Get()) )
		];
}

void SEventGraphPin::RefreshTagList()
{	
	// Clear the list
	TagNames.Empty();

	// Add tags to list
	FString TagName;
	if (TagContainer.IsValid())
	{
		for (auto It = TagContainer->CreateConstIterator(); It; ++It)
		{
			TagName = It->ToString();
			TagNames.Add( MakeShareable( new FString( TagName ) ) );
		}
	}

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}

	// Set Pin Data
	FString TagString;
	if (!TagName.IsEmpty())
	{
		TagString = TEXT("(");
		TagString += TEXT("TagName=\"");
		TagString += TagName;
		TagString += TEXT("\"");
		TagString += TEXT(")");
	}
	FString CurrentDefaultValue = GraphPinObj->GetDefaultAsString();
	if (CurrentDefaultValue.IsEmpty())
	{
		CurrentDefaultValue = FString(TEXT(""));
	}
	if (!CurrentDefaultValue.Equals(TagString))
	{
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagString);
	}
}

#undef LOCTEXT_NAMESPACE
