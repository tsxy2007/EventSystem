// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventQueryCustomization.h"
#include "UObject/UnrealType.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Docking/TabManager.h"

#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "IPropertyTypeCustomization.h"
#include "IPropertyUtilities.h"
#include "Misc/NotifyHook.h"

#define LOCTEXT_NAMESPACE "EventQueryCustomization"

void FEventQueryCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;
	check(StructPropertyHandle.IsValid());

	PropertyUtilities = StructCustomizationUtils.GetPropertyUtilities();
	
	RefreshQueryDescription(); // will call BuildEditableQueryList();

	bool const bReadOnly = StructPropertyHandle->IsEditConst();

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
	.ValueContent()
		.MaxDesiredWidth(512)
		[
			SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.Text(this, &FEventQueryCustomization::GetEditButtonText)
		.OnClicked(this, &FEventQueryCustomization::OnEditButtonClicked)
		]
			+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.IsEnabled(!bReadOnly)
		.Text(LOCTEXT("EventQueryCustomization_Clear", "Clear All"))
		.OnClicked(this, &FEventQueryCustomization::OnClearAllButtonClicked)
		.Visibility(this, &FEventQueryCustomization::GetClearAllVisibility)
		]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(4.0f)
		.Visibility(this, &FEventQueryCustomization::GetQueryDescVisibility)
		[
			SNew(STextBlock)
			.Text(this, &FEventQueryCustomization::GetQueryDescText)
		.AutoWrapText(true)
		]
		]
		];

	GEditor->RegisterForUndo(this);
}

FText FEventQueryCustomization::GetQueryDescText() const
{
	return FText::FromString(QueryDescription);
}

FText FEventQueryCustomization::GetEditButtonText() const
{
	if (StructPropertyHandle.IsValid())
	{
		bool const bReadOnly = StructPropertyHandle->IsEditConst();
		return
			bReadOnly
			? LOCTEXT("EventQueryCustomization_View", "View...")
			: LOCTEXT("EventQueryCustomization_Edit", "Edit...");
	}

	return FText();
}

FReply FEventQueryCustomization::OnClearAllButtonClicked()
{
	if (StructPropertyHandle.IsValid())
	{
		StructPropertyHandle->NotifyPreChange();
	}

	for (auto& EQ : EditableQueries)
	{
		if (EQ.TagQuery)
		{
			EQ.TagQuery->Clear();
		}
	}

	RefreshQueryDescription();

	if (StructPropertyHandle.IsValid())
	{
		StructPropertyHandle->NotifyPostChange();
	}

	return FReply::Handled();
}

EVisibility FEventQueryCustomization::GetClearAllVisibility() const
{
	bool bAtLeastOneQueryIsNonEmpty = false;

	for (auto& EQ : EditableQueries)
	{
		if (EQ.TagQuery && EQ.TagQuery->IsEmpty() == false)
		{
			bAtLeastOneQueryIsNonEmpty = true;
			break;
		}
	}

	return bAtLeastOneQueryIsNonEmpty ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FEventQueryCustomization::GetQueryDescVisibility() const
{
	return QueryDescription.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

void FEventQueryCustomization::RefreshQueryDescription()
{
	// Rebuild Editable Containers as container references can become unsafe
	BuildEditableQueryList();

	// Clear the list
	QueryDescription.Empty();

	if (EditableQueries.Num() > 1)
	{
		QueryDescription = TEXT("Multiple Selected");
	}
	else if ((EditableQueries.Num() == 1) && (EditableQueries[0].TagQuery != nullptr))
	{
		QueryDescription = EditableQueries[0].TagQuery->GetDescription();
	}
}


FReply FEventQueryCustomization::OnEditButtonClicked()
{
	if (EventQueryWidgetWindow.IsValid())
	{
		// already open, just show it
		EventQueryWidgetWindow->BringToFront(true);
	}
	else
	{
		if (StructPropertyHandle.IsValid())
		{
			TArray<UObject*> OuterObjects;
			StructPropertyHandle->GetOuterObjects(OuterObjects);

			bool bReadOnly = StructPropertyHandle->IsEditConst();

			FText Title;
			if (OuterObjects.Num() > 1)
			{
				FText const AssetName = FText::Format(LOCTEXT("EventDetailsBase_MultipleAssets", "{0} Assets"), FText::AsNumber(OuterObjects.Num()));
				FText const PropertyName = StructPropertyHandle->GetPropertyDisplayName();
				Title = FText::Format(LOCTEXT("EventQueryCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName);
			}
			else if (OuterObjects.Num() > 0 && OuterObjects[0])
			{
				FText const AssetName = FText::FromString(OuterObjects[0]->GetName());
				FText const PropertyName = StructPropertyHandle->GetPropertyDisplayName();
				Title = FText::Format(LOCTEXT("EventQueryCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName);
			}

			EventQueryWidgetWindow = SNew(SWindow)
				.Title(Title)
				.HasCloseButton(false)
				.ClientSize(FVector2D(600, 400))
				[
					SNew(SEventQueryWidget, EditableQueries)
					.OnClosePreSave(this, &FEventQueryCustomization::PreSave)
				.OnSaveAndClose(this, &FEventQueryCustomization::CloseWidgetWindow, false)
				.OnCancel(this, &FEventQueryCustomization::CloseWidgetWindow, true)
				.ReadOnly(bReadOnly)
				];

			// NOTE: FGlobalTabmanager::Get()-> is actually dereferencing a SharedReference, not a SharedPtr, so it cannot be null.
			if (FGlobalTabmanager::Get()->GetRootWindow().IsValid())
			{
				FSlateApplication::Get().AddWindowAsNativeChild(EventQueryWidgetWindow.ToSharedRef(), FGlobalTabmanager::Get()->GetRootWindow().ToSharedRef());
			}
			else
			{
				FSlateApplication::Get().AddWindow(EventQueryWidgetWindow.ToSharedRef());
			}
		}
	}

	return FReply::Handled();
}

FEventQueryCustomization::~FEventQueryCustomization()
{
	if (EventQueryWidgetWindow.IsValid())
	{
		EventQueryWidgetWindow->RequestDestroyWindow();
	}
	GEditor->UnregisterForUndo(this);
}

void FEventQueryCustomization::BuildEditableQueryList()
{	
	EditableQueries.Empty();

	if (StructPropertyHandle.IsValid())
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		TArray<UObject*> OuterObjects;
		StructPropertyHandle->GetOuterObjects(OuterObjects);
		
		for (int32 Idx = 0; Idx < RawStructData.Num(); ++Idx)
		{
			// Null outer objects may mean that we are inside a UDataTable. This is ok though. We can still dirty the data table via FNotify Hook. (see ::CloseWidgetWindow). However undo will not work.
			UObject* Obj = OuterObjects.IsValidIndex(Idx) ? OuterObjects[Idx] : nullptr;

			EditableQueries.Add(SEventQueryWidget::FEditableEventQueryDatum(Obj, (FEventQuery*)RawStructData[Idx]));
		}
	}	
}

void FEventQueryCustomization::PreSave()
{
	if (StructPropertyHandle.IsValid())
	{
		StructPropertyHandle->NotifyPreChange();
	}
}

void FEventQueryCustomization::CloseWidgetWindow(bool WasCancelled)
{
	// Notify change.
	if (!WasCancelled && StructPropertyHandle.IsValid())
	{
		StructPropertyHandle->NotifyPostChange();
	}

	if (EventQueryWidgetWindow.IsValid())
	{
		EventQueryWidgetWindow->RequestDestroyWindow();
		EventQueryWidgetWindow = nullptr;

		RefreshQueryDescription();
	}
}

#undef LOCTEXT_NAMESPACE
