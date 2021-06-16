// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventCustomization.h"
#include "Widgets/Input/SComboButton.h"

#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "EventsEditorModule.h"
#include "Widgets/Input/SHyperlink.h"

#define LOCTEXT_NAMESPACE "EventCustomization"

TSharedRef<IPropertyTypeCustomization> FEventCustomizationPublic::MakeInstance()
{
	return MakeShareable(new FEventCustomization);
}

void FEventCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TagContainer = MakeShareable(new FEventContainer);
	StructPropertyHandle = InStructPropertyHandle;

	FSimpleDelegate OnTagChanged = FSimpleDelegate::CreateSP(this, &FEventCustomization::OnPropertyValueChanged);
	StructPropertyHandle->SetOnPropertyValueChanged(OnTagChanged);

	BuildEditableContainerList();

	FUIAction SearchForReferencesAction(FExecuteAction::CreateSP(this, &FEventCustomization::OnSearchForReferences));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(512)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(EditButton, SComboButton)
			.OnGetMenuContent(this, &FEventCustomization::GetListContent)
			.OnMenuOpenChanged(this, &FEventCustomization::OnEventListMenuOpenStateChanged)
			.ContentPadding(FMargin(2.0f, 2.0f))
			.MenuPlacement(MenuPlacement_BelowAnchor)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EventCustomization_Edit", "Edit"))
			]
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Visibility(this, &FEventCustomization::GetVisibilityForTagTextBlockWidget, true)
			.Padding(4.0f)
			[
				SNew(STextBlock)
				.Text(this, &FEventCustomization::SelectedTag)
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Visibility(this, &FEventCustomization::GetVisibilityForTagTextBlockWidget, false)
			.Padding(4.0f)
			[
				SNew(SHyperlink)
				.Text(this, &FEventCustomization::SelectedTag)
				.OnNavigate( this, &FEventCustomization::OnTagDoubleClicked)
			]
		]
	]
	.AddCustomContextMenuAction(SearchForReferencesAction,
		LOCTEXT("FEventCustomization_SearchForReferences", "Search For References"),
		LOCTEXT("FEventCustomization_SearchForReferencesTooltip", "Find references for this event"),
		FSlateIcon());

	GEditor->RegisterForUndo(this);
}

void FEventCustomization::OnTagDoubleClicked()
{
	UEventsManager::Get().NotifyEventDoubleClickedEditor(TagName);
}

void FEventCustomization::OnSearchForReferences()
{
	FName TagFName(*TagName, FNAME_Find);
	if (FEditorDelegates::OnOpenReferenceViewer.IsBound() && !TagFName.IsNone())
	{
		TArray<FAssetIdentifier> AssetIdentifiers;
		AssetIdentifiers.Add(FAssetIdentifier(FEventInfo::StaticStruct(), TagFName));
		FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
	}
}

EVisibility FEventCustomization::GetVisibilityForTagTextBlockWidget(bool ForTextWidget) const
{
	return (UEventsManager::Get().ShowEventAsHyperLinkEditor(TagName) ^ ForTextWidget) ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> FEventCustomization::GetListContent()
{
	BuildEditableContainerList();
	
	FString Categories = UEventsManager::Get().GetCategoriesMetaFromPropertyHandle(StructPropertyHandle);

	bool bReadOnly = StructPropertyHandle->IsEditConst();

	TSharedRef<SEventWidget> TagWidget =
		SNew(SEventWidget, EditableContainers)
		.Filter(Categories)
		.ReadOnly(bReadOnly)
		.TagContainerName(StructPropertyHandle->GetPropertyDisplayName().ToString())
		.MultiSelect(false)
		.OnTagChanged(this, &FEventCustomization::OnTagChanged)
		.PropertyHandle(StructPropertyHandle);

	LastTagWidget = TagWidget;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(400)
		[
			TagWidget
		];
}

void FEventCustomization::OnEventListMenuOpenStateChanged(bool bIsOpened)
{
	if (bIsOpened)
	{
		TSharedPtr<SEventWidget> TagWidget = LastTagWidget.Pin();
		if (TagWidget.IsValid())
		{
			EditButton->SetMenuContentWidgetToFocus(TagWidget->GetWidgetToFocusOnOpen());
		}
	}
}

void FEventCustomization::OnPropertyValueChanged()
{
	TagName = TEXT("");
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() && EditableContainers.Num() > 0)
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			FEventInfo* Tag = (FEventInfo*)(RawStructData[0]);
			FEventContainer* Container = EditableContainers[0].TagContainer;			
			if (Tag && Container)
			{
				Container->Reset();
				Container->AddTag(*Tag);
				TagName = Tag->ToString();
			}			
		}
	}
}

void FEventCustomization::OnTagChanged()
{
	TagName = TEXT("");
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() && EditableContainers.Num() > 0)
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			FEventInfo* Tag = (FEventInfo*)(RawStructData[0]);			

			// Update Tag from the one selected from list
			FEventContainer* Container = EditableContainers[0].TagContainer;
			if (Tag && Container)
			{
				for (auto It = Container->CreateConstIterator(); It; ++It)
				{
					*Tag = *It;
					TagName = It->ToString();
				}
			}
		}
	}
}

void FEventCustomization::PostUndo(bool bSuccess)
{
	if (bSuccess && !StructPropertyHandle.IsValid())
	{
		OnTagChanged();
	}
}

void FEventCustomization::PostRedo(bool bSuccess)
{
	if (bSuccess && !StructPropertyHandle.IsValid())
	{
		OnTagChanged();
	}
}

FEventCustomization::~FEventCustomization()
{
	GEditor->UnregisterForUndo(this);
}

void FEventCustomization::BuildEditableContainerList()
{
	EditableContainers.Empty();

	if(StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty())
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		if (RawStructData.Num() > 0)
		{
			FEventInfo* Tag = (FEventInfo*)(RawStructData[0]);
			if (Tag->IsValid())
			{
				TagName = Tag->ToString();
				TagContainer->AddTag(*Tag);
			}
		}

		EditableContainers.Add(SEventWidget::FEditableEventContainerDatum(nullptr, TagContainer.Get()));
	}
}

FText FEventCustomization::SelectedTag() const
{
	return FText::FromString(*TagName);
}

#undef LOCTEXT_NAMESPACE
