// Copyright Epic Games, Inc. All Rights Reserved.

#include "SEventQueryGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "EventQueryGraphPin"

void SEventQueryGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	TagQuery = MakeShareable(new FEventQuery());
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
}

TSharedRef<SWidget>	SEventQueryGraphPin::GetDefaultValueWidget()
{
	ParseDefaultValueData();

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew( ComboButton, SComboButton )
			.OnGetMenuContent(this, &SEventQueryGraphPin::GetListContent)
			.ContentPadding( FMargin( 2.0f, 2.0f ) )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.ButtonContent()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("EventQueryWidget_Edit", "Edit") )
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			QueryDesc()
		];
}

void SEventQueryGraphPin::ParseDefaultValueData()
{
	FString const TagQueryString = GraphPinObj->GetDefaultAsString();

	FProperty* const TQProperty = FindFProperty<FProperty>(UEditableEventQuery::StaticClass(), TEXT("TagQueryExportText_Helper"));
	if (TQProperty)
	{
		FEventQuery* const TQ = TagQuery.Get();
		TQProperty->ImportText(*TagQueryString, TQ, 0, nullptr, GLog);
	}
}

TSharedRef<SWidget> SEventQueryGraphPin::GetListContent()
{
	EditableQueries.Empty();
	EditableQueries.Add(SEventQueryWidget::FEditableEventQueryDatum(GraphPinObj->GetOwningNode(), TagQuery.Get(), &TagQueryExportText));

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew(SScaleBox)
			.HAlign(EHorizontalAlignment::HAlign_Left)
			.VAlign(EVerticalAlignment::VAlign_Top)
			.StretchDirection(EStretchDirection::DownOnly)
			.Stretch(EStretch::ScaleToFit)
			.Content()
			[
				SNew(SEventQueryWidget, EditableQueries)
				.OnQueryChanged(this, &SEventQueryGraphPin::OnQueryChanged)
				.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
				.AutoSave(true)
			]
		];
}

void SEventQueryGraphPin::OnQueryChanged()
{
	// Set Pin Data
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagQueryExportText);
	QueryDescription = TagQuery->GetDescription();
}

TSharedRef<SWidget> SEventQueryGraphPin::QueryDesc()
{
	QueryDescription = TagQuery->GetDescription();

	return SNew(STextBlock)
		.Text(this, &SEventQueryGraphPin::GetQueryDescText)
		.AutoWrapText(true);
}

FText SEventQueryGraphPin::GetQueryDescText() const
{
	return FText::FromString(QueryDescription);
}

#undef LOCTEXT_NAMESPACE
