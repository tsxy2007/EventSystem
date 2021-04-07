// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "SEventContainerGraphPin.h"
#include "Widgets/Input/SComboButton.h"
#include "EventsRuntimeModule.h"
#include "Widgets/Layout/SScaleBox.h"
#include "EventPinUtilities.h"

#define LOCTEXT_NAMESPACE "EventGraphPin"

void SEventContainerGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	TagContainer = MakeShareable( new FEventContainer() );
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
}

TSharedRef<SWidget>	SEventContainerGraphPin::GetDefaultValueWidget()
{
	ParseDefaultValueData();

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew( ComboButton, SComboButton )
			.OnGetMenuContent(this, &SEventContainerGraphPin::GetListContent)
			.ContentPadding( FMargin( 2.0f, 2.0f ) )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
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

void SEventContainerGraphPin::ParseDefaultValueData()
{
	FString TagString = GraphPinObj->GetDefaultAsString();

	FilterString = EventPinUtilities::ExtractTagFilterStringFromGraphPin(GraphPinObj);

	if (TagString.StartsWith(TEXT("("), ESearchCase::CaseSensitive) && TagString.EndsWith(TEXT(")"), ESearchCase::CaseSensitive))
	{
		TagString.LeftChopInline(1, false);
		TagString.RightChopInline(1, false);

		TagString.Split(TEXT("="), nullptr, &TagString, ESearchCase::CaseSensitive);

		TagString.LeftChopInline(1, false);
		TagString.RightChopInline(1, false);

		FString ReadTag;
		FString Remainder;

		while (TagString.Split(TEXT(","), &ReadTag, &Remainder, ESearchCase::CaseSensitive))
		{
			ReadTag.Split(TEXT("="), NULL, &ReadTag, ESearchCase::CaseSensitive);
			if (ReadTag.EndsWith(TEXT(")"), ESearchCase::CaseSensitive))
			{
				ReadTag.LeftChopInline(1, false);
				if (ReadTag.StartsWith(TEXT("\""), ESearchCase::CaseSensitive) && ReadTag.EndsWith(TEXT("\""), ESearchCase::CaseSensitive))
				{
					ReadTag.LeftChopInline(1, false);
					ReadTag.RightChopInline(1, false);
				}
			}
			TagString = Remainder;
			FEventInfo Event = FEventInfo::RequestEvent(FName(*ReadTag));
			TagContainer->AddTag(Event);
		}
		if (Remainder.IsEmpty())
		{
			Remainder = TagString;
		}
		if (!Remainder.IsEmpty())
		{
			Remainder.Split(TEXT("="), nullptr, &Remainder, ESearchCase::CaseSensitive);
			if (Remainder.EndsWith(TEXT(")"), ESearchCase::CaseSensitive))
			{
				Remainder.LeftChopInline(1, false);
				if (Remainder.StartsWith(TEXT("\""), ESearchCase::CaseSensitive) && Remainder.EndsWith(TEXT("\""), ESearchCase::CaseSensitive))
				{
					Remainder.LeftChopInline(1, false);
					Remainder.RightChopInline(1, false);
				}
			}
			FEventInfo Event = FEventInfo::RequestEvent(FName(*Remainder));
			TagContainer->AddTag(Event);
		}
	}
}

TSharedRef<SWidget> SEventContainerGraphPin::GetListContent()
{
	EditableContainers.Empty();
	EditableContainers.Add( SEventWidget::FEditableEventContainerDatum( GraphPinObj->GetOwningNode(), TagContainer.Get() ) );

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew( SEventWidget, EditableContainers )
			.OnTagChanged(this, &SEventContainerGraphPin::RefreshTagList)
			.TagContainerName( TEXT("SEventContainerGraphPin") )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.Filter(FilterString)
		];
}

TSharedRef<SWidget> SEventContainerGraphPin::SelectedTags()
{
	RefreshTagList();

	SAssignNew( TagListView, SListView<TSharedPtr<FString>> )
		.ListItemsSource(&TagNames)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SEventContainerGraphPin::OnGenerateRow);

	return TagListView->AsShared();
}

TSharedRef<ITableRow> SEventContainerGraphPin::OnGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
		[
			SNew(STextBlock) .Text( FText::FromString(*Item.Get()) )
		];
}

void SEventContainerGraphPin::RefreshTagList()
{	
	// Clear the list
	TagNames.Empty();

	// Add tags to list
	if (TagContainer.IsValid())
	{
		for (auto It = TagContainer->CreateConstIterator(); It; ++It)
		{
			FString TagName = It->ToString();
			TagNames.Add( MakeShareable( new FString( TagName ) ) );
		}
	}

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}

	// Set Pin Data
	FString TagContainerString = TagContainer->ToString();
	FString CurrentDefaultValue = GraphPinObj->GetDefaultAsString();
	if (CurrentDefaultValue.IsEmpty())
	{
		CurrentDefaultValue = FString(TEXT("(Events=)"));
	}
	if (!CurrentDefaultValue.Equals(TagContainerString))
	{
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagContainerString);
	}
}

#undef LOCTEXT_NAMESPACE
