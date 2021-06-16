// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventContainerCustomization.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SHyperlink.h"
#include "EditorFontGlyphs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EventContainer.h"

#define LOCTEXT_NAMESPACE "EventContainerCustomization"

void FEventContainerCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	FSimpleDelegate OnTagContainerChanged = FSimpleDelegate::CreateSP(this, &FEventContainerCustomization::RefreshTagList);
	StructPropertyHandle->SetOnPropertyValueChanged(OnTagContainerChanged);

	BuildEditableContainerList();

	FUIAction SearchForReferencesAction(FExecuteAction::CreateSP(this, &FEventContainerCustomization::OnWholeContainerSearchForReferences));

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
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(EditButton, SComboButton)
					.OnGetMenuContent(this, &FEventContainerCustomization::GetListContent)
					.OnMenuOpenChanged(this, &FEventContainerCustomization::OnEventListMenuOpenStateChanged)
					.ContentPadding(FMargin(2.0f, 2.0f))
					.MenuPlacement(MenuPlacement_BelowAnchor)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("EventContainerCustomization_Edit", "Edit..."))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.IsEnabled(!StructPropertyHandle->IsEditConst())
					.Text(LOCTEXT("EventContainerCustomization_Clear", "Clear All"))
					.OnClicked(this, &FEventContainerCustomization::OnClearAllButtonClicked)
					.Visibility(this, &FEventContainerCustomization::GetClearAllVisibility)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(4.0f)
				.Visibility(this, &FEventContainerCustomization::GetTagsListVisibility)
				[
					ActiveTags()
				]
			]
		]
		.AddCustomContextMenuAction(SearchForReferencesAction,
			LOCTEXT("WholeContainerSearchForReferences", "Search For References"),
			LOCTEXT("WholeContainerSearchForReferencesTooltip", "Find referencers that reference *any* of the tags in this container"),
			FSlateIcon());

	GEditor->RegisterForUndo(this);
}

TSharedRef<SWidget> FEventContainerCustomization::ActiveTags()
{	
	RefreshTagList();
	
	SAssignNew( TagListView, SListView<TSharedPtr<FString>> )
	.ListItemsSource(&TagNames)
	.SelectionMode(ESelectionMode::None)
	.OnGenerateRow(this, &FEventContainerCustomization::MakeListViewWidget);

	return TagListView->AsShared();
}

void FEventContainerCustomization::RefreshTagList()
{
	// Rebuild Editable Containers as container references can become unsafe
	BuildEditableContainerList();

	// Clear the list
	TagNames.Empty();

	// Add tags to list
	for (int32 ContainerIdx = 0; ContainerIdx < EditableContainers.Num(); ++ContainerIdx)
	{
		FEventContainer* Container = EditableContainers[ContainerIdx].TagContainer;
		if (Container)
		{
			for (auto It = Container->CreateConstIterator(); It; ++It)
			{
				TagNames.Add(MakeShareable(new FString(It->ToString())));
			}
		}
	}

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> FEventContainerCustomization::MakeListViewWidget(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SWidget> TagItem;

	if (UEventsManager::Get().ShowEventAsHyperLinkEditor(*Item.Get()))
	{
		TagItem = SNew(SHyperlink)
			.Text(FText::FromString(*Item.Get()))
			.OnNavigate(this, &FEventContainerCustomization::OnTagDoubleClicked, *Item.Get());
	}
	else
	{
		TagItem = SNew(STextBlock).Text(FText::FromString(*Item.Get()));
	}

	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
	[
		SNew(SBorder)
		.OnMouseButtonDown(this, &FEventContainerCustomization::OnSingleTagMouseButtonPressed, *Item.Get())
		.Padding(0.0f)
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0,0,2,0)
			[
				SNew(SButton)
				.IsEnabled(!StructPropertyHandle->IsEditConst())
				.ContentPadding(FMargin(0))
				.ButtonStyle(FEditorStyle::Get(), "FlatButton.Danger")
				.ForegroundColor(FSlateColor::UseForeground())
				.OnClicked(this, &FEventContainerCustomization::OnRemoveTagClicked, *Item.Get())
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.9"))
					.Text(FEditorFontGlyphs::Times)
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				TagItem.ToSharedRef()
			]
		]
	];
}

FReply FEventContainerCustomization::OnSingleTagMouseButtonPressed(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FString TagName)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection=*/ true, /*CommandList=*/ nullptr);

		FUIAction SearchForReferencesAction(FExecuteAction::CreateSP(this, &FEventContainerCustomization::OnSingleTagSearchForReferences, TagName));

		MenuBuilder.BeginSection(NAME_None, FText::Format(LOCTEXT("SingleEventMenuHeading", "Event Actions ({0})"), FText::AsCultureInvariant(TagName)));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SingleEventSearchForReferences", "Search For References"),
			FText::Format(LOCTEXT("SingleEventSearchForReferencesTooltip", "Find references to the event {0}"), FText::AsCultureInvariant(TagName)),
			FSlateIcon(),
			SearchForReferencesAction);
		MenuBuilder.EndSection();

		// Spawn context menu
		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(TagListView.ToSharedRef(), WidgetPath, MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void FEventContainerCustomization::OnSingleTagSearchForReferences(FString TagName)
{
	FName TagFName(*TagName, FNAME_Find);
	if (FEditorDelegates::OnOpenReferenceViewer.IsBound() && !TagFName.IsNone())
	{
		TArray<FAssetIdentifier> AssetIdentifiers;
		AssetIdentifiers.Emplace(FEventInfo::StaticStruct(), TagFName);
		FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
	}
}

void FEventContainerCustomization::OnWholeContainerSearchForReferences()
{
	if (FEditorDelegates::OnOpenReferenceViewer.IsBound())
	{
		TArray<FAssetIdentifier> AssetIdentifiers;
		AssetIdentifiers.Reserve(TagNames.Num());
		for (TSharedPtr<FString>& TagNamePtr : TagNames)
		{
			FName TagFName(**TagNamePtr.Get(), FNAME_Find);
			if (!TagFName.IsNone())
			{
				AssetIdentifiers.Emplace(FEventInfo::StaticStruct(), TagFName);
			}
		}

		FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
	}
}

void FEventContainerCustomization::OnTagDoubleClicked(FString TagName)
{
	UEventsManager::Get().NotifyEventDoubleClickedEditor(TagName);
}

FReply FEventContainerCustomization::OnRemoveTagClicked(FString TagName)
{
	TArray<FString> NewValues;

	StructPropertyHandle->EnumerateRawData([TagName, &NewValues](void* RawTagContainer, const int32 /*DataIndex*/, const int32 /*NumDatas*/) -> bool {
		
		FEventContainer TagContainerCopy = *static_cast<FEventContainer*>(RawTagContainer);
		for (int32 TagIndex = 0; TagIndex < TagContainerCopy.Num(); TagIndex++)
		{
			FEventInfo Tag = TagContainerCopy.GetByIndex(TagIndex);
			if (Tag.GetTagName().ToString() == TagName)
			{
				TagContainerCopy.RemoveTag(Tag);
			}
		}
		
		NewValues.Add(TagContainerCopy.ToString());

		return true;
	});

	{
		FScopedTransaction Transaction(LOCTEXT("RemoveEventFromContainer", "Remove Event"));
		for (int i = 0; i < NewValues.Num(); i++)
		{
			StructPropertyHandle->SetPerObjectValue(i, NewValues[i]);
		}
	}

	RefreshTagList();

	return FReply::Handled();
}

TSharedRef<SWidget> FEventContainerCustomization::GetListContent()
{
	if (!StructPropertyHandle.IsValid() || StructPropertyHandle->GetProperty() == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	FString Categories = UEventsManager::Get().GetCategoriesMetaFromPropertyHandle(StructPropertyHandle);

	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);

	bool bReadOnly = StructPropertyHandle->IsEditConst();

	TSharedRef<SEventWidget> TagWidget = SNew(SEventWidget, EditableContainers)
		.Filter(Categories)
		.ReadOnly(bReadOnly)
		.TagContainerName(StructPropertyHandle->GetPropertyDisplayName().ToString())
		.OnTagChanged(this, &FEventContainerCustomization::RefreshTagList)
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

void FEventContainerCustomization::OnEventListMenuOpenStateChanged(bool bIsOpened)
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

FReply FEventContainerCustomization::OnClearAllButtonClicked()
{
	FScopedTransaction Transaction(LOCTEXT("EventContainerCustomization_RemoveAllEvents", "Remove All Events"));

	for (int32 ContainerIdx = 0; ContainerIdx < EditableContainers.Num(); ++ContainerIdx)
	{
		FEventContainer* Container = EditableContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FEventContainer EmptyContainer;
			StructPropertyHandle->SetValueFromFormattedString(EmptyContainer.ToString());
			RefreshTagList();
		}
	}
	return FReply::Handled();
}

EVisibility FEventContainerCustomization::GetClearAllVisibility() const
{
	return TagNames.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FEventContainerCustomization::GetTagsListVisibility() const
{
	return TagNames.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

void FEventContainerCustomization::PostUndo( bool bSuccess )
{
	if( bSuccess )
	{
		RefreshTagList();
	}
}

void FEventContainerCustomization::PostRedo( bool bSuccess )
{
	if( bSuccess )
	{
		RefreshTagList();
	}
}

FEventContainerCustomization::~FEventContainerCustomization()
{
	GEditor->UnregisterForUndo(this);
}

void FEventContainerCustomization::BuildEditableContainerList()
{
	EditableContainers.Empty();

	if( StructPropertyHandle.IsValid() )
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		for (int32 ContainerIdx = 0; ContainerIdx < RawStructData.Num(); ++ContainerIdx)
		{
			EditableContainers.Add(SEventWidget::FEditableEventContainerDatum(nullptr, (FEventContainer*)RawStructData[ContainerIdx]));
		}
	}	
}

#undef LOCTEXT_NAMESPACE
