// Copyright Epic Games, Inc. All Rights Reserved.

#include "SEventWidget.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "Widgets/SWindow.h"
#include "Misc/MessageDialog.h"
#include "EventsRuntimeModule.h"
#include "ScopedTransaction.h"
#include "Textures/SlateIcon.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SSearchBox.h"
#include "EventsEditorModule.h"
#include "Widgets/Layout/SScaleBox.h"

#include "AssetToolsModule.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "EventsSettings.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "SAddNewEventWidget.h"
#include "SAddNewEventSourceWidget.h"
#include "SAddNewRestrictedEventWidget.h"
#include "SRenameEventDialog.h"
#include "AssetData.h"
#include "Editor.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "EventWidget"

const FString SEventWidget::SettingsIniSection = TEXT("EventWidget");

void SEventWidget::Construct(const FArguments& InArgs, const TArray<FEditableEventContainerDatum>& EditableTagContainers)
{
	// If we're in management mode, we don't need to have editable tag containers.
	ensure(EditableTagContainers.Num() > 0 || InArgs._EventUIMode == EEventUIMode::ManagementMode);
	TagContainers = EditableTagContainers;

	OnTagChanged = InArgs._OnTagChanged;
	bReadOnly = InArgs._ReadOnly;
	TagContainerName = InArgs._TagContainerName;
	bMultiSelect = InArgs._MultiSelect;
	PropertyHandle = InArgs._PropertyHandle;
	RootFilterString = InArgs._Filter;
	EventUIMode = InArgs._EventUIMode;

	bAddTagSectionExpanded = InArgs._NewTagControlsInitiallyExpanded;
	bDelayRefresh = false;
	MaxHeight = InArgs._MaxHeight;

	bRestrictedTags = InArgs._RestrictedTags;

	UEventsManager::OnEditorRefreshEventTree.AddSP(this, &SEventWidget::RefreshOnNextTick);
	UEventsManager& Manager = UEventsManager::Get();

	Manager.GetFilteredGameplayRootTags(RootFilterString, TagItems);

	if (bRestrictedTags)
	{
		// We only want to show the restricted gameplay tags
		for (int32 Idx = TagItems.Num() - 1; Idx >= 0; --Idx)
		{
			if (!TagItems[Idx]->IsRestrictedEvent())
			{
				TagItems.RemoveAtSwap(Idx);
			}
		}
	}

	// Tag the assets as transactional so they can support undo/redo
	TArray<UObject*> ObjectsToMarkTransactional;
	if (PropertyHandle.IsValid())
	{
		// If we have a property handle use that to find the objects that need to be transactional
		PropertyHandle->GetOuterObjects(ObjectsToMarkTransactional);
	}
	else
	{
		// Otherwise use the owner list
		for (int32 AssetIdx = 0; AssetIdx < TagContainers.Num(); ++AssetIdx)
		{
			ObjectsToMarkTransactional.Add(TagContainers[AssetIdx].TagContainerOwner.Get());
		}
	}

	// Now actually mark the assembled objects
	for (UObject* ObjectToMark : ObjectsToMarkTransactional)
	{
		if (ObjectToMark)
		{
			ObjectToMark->SetFlags(RF_Transactional);
		}
	}

	const FText NewTagText = bRestrictedTags ? LOCTEXT("AddNewRestrictedTag", "Add New Restricted Gameplay Tag") : LOCTEXT("AddNewTag", "Add New Gameplay Tag");

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			// Expandable UI controls
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SCheckBox )
					.IsChecked(this, &SEventWidget::GetAddTagSectionExpansionState) 
					.OnCheckStateChanged(this, &SEventWidget::OnAddTagSectionExpansionStateChanged)
					.CheckedImage(FEditorStyle::GetBrush("TreeArrow_Expanded"))
					.CheckedHoveredImage(FEditorStyle::GetBrush("TreeArrow_Expanded_Hovered"))
					.CheckedPressedImage(FEditorStyle::GetBrush("TreeArrow_Expanded"))
					.UncheckedImage(FEditorStyle::GetBrush("TreeArrow_Collapsed"))
					.UncheckedHoveredImage(FEditorStyle::GetBrush("TreeArrow_Collapsed_Hovered"))
					.UncheckedPressedImage(FEditorStyle::GetBrush("TreeArrow_Collapsed"))
					.Visibility( this, &SEventWidget::DetermineExpandableUIVisibility )
					[
						SNew( STextBlock )
						.Text( NewTagText )
					]
				]
			]


			// Expandable UI for adding restricted tags
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.Padding(16.0f, 0.0f)
			[
				SAssignNew(AddNewRestrictedTagWidget, SAddNewRestrictedEventWidget)
				.Visibility(this, &SEventWidget::DetermineAddNewRestrictedTagWidgetVisibility)
				.OnRestrictedEventAdded(this, &SEventWidget::OnEventAdded)
				.NewRestrictedTagName(InArgs._NewTagName)
			]

			// Expandable UI for adding non-restricted tags
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.Padding(16.0f, 0.0f)
			[
				SAssignNew( AddNewTagWidget, SAddNewEventWidget )
				.Visibility(this, &SEventWidget::DetermineAddNewTagWidgetVisibility)
				.OnEventAdded(this, &SEventWidget::OnEventAdded)
				.NewTagName(InArgs._NewTagName)
			]

			// Expandable UI controls
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked(this, &SEventWidget::GetAddSourceSectionExpansionState)
					.OnCheckStateChanged(this, &SEventWidget::OnAddSourceSectionExpansionStateChanged)
					.CheckedImage(FEditorStyle::GetBrush("TreeArrow_Expanded"))
					.CheckedHoveredImage(FEditorStyle::GetBrush("TreeArrow_Expanded_Hovered"))
					.CheckedPressedImage(FEditorStyle::GetBrush("TreeArrow_Expanded"))
					.UncheckedImage(FEditorStyle::GetBrush("TreeArrow_Collapsed"))
					.UncheckedHoveredImage(FEditorStyle::GetBrush("TreeArrow_Collapsed_Hovered"))
					.UncheckedPressedImage(FEditorStyle::GetBrush("TreeArrow_Collapsed"))
					.Visibility(this, &SEventWidget::DetermineAddNewSourceExpandableUIVisibility)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AddNewSource", "Add New Tag Source"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.Padding(16.0f, 0.0f)
			[
				SAssignNew(AddNewTagSourceWidget, SAddNewEventSourceWidget)
				.Visibility(this, &SEventWidget::DetermineAddNewSourceWidgetVisibility)
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("DetailsView.CategoryMiddle"))
				.Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
				.Visibility(this, &SEventWidget::DetermineAddNewTagWidgetVisibility)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("DetailsView.AdvancedDropdownBorder.Open"))
				]
			]

			// Gameplay Tag Tree controls
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)

				// Expand All nodes
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &SEventWidget::OnExpandAllClicked)
					.Text(LOCTEXT("EventWidget_ExpandAll", "Expand All"))
				]
			
				// Collapse All nodes
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &SEventWidget::OnCollapseAllClicked)
					.Text(LOCTEXT("EventWidget_CollapseAll", "Collapse All"))
				]

				// Clear selections
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.IsEnabled(this, &SEventWidget::CanSelectTags)
					.OnClicked(this, &SEventWidget::OnClearAllClicked)
					.Text(LOCTEXT("EventWidget_ClearAll", "Clear All"))
					.Visibility(this, &SEventWidget::DetermineClearSelectionVisibility)
				]

				// Search
				+SHorizontalBox::Slot()
				.VAlign( VAlign_Center )
				.FillWidth(1.f)
				.Padding(5,1,5,1)
				[
					SAssignNew(SearchTagBox, SSearchBox)
					.HintText(LOCTEXT("EventWidget_SearchBoxHint", "Search Events"))
					.OnTextChanged( this, &SEventWidget::OnFilterTextChanged )
				]
			]

			// Events tree
			+SVerticalBox::Slot()
			.MaxHeight(MaxHeight)
			[
				SAssignNew(TagTreeContainerWidget, SBorder)
				.Padding(FMargin(4.f))
				[
					SAssignNew(TagTreeWidget, STreeView< TSharedPtr<FEventNode> >)
					.TreeItemsSource(&TagItems)
					.OnGenerateRow(this, &SEventWidget::OnGenerateRow)
					.OnGetChildren(this, &SEventWidget::OnGetChildren)
					.OnExpansionChanged( this, &SEventWidget::OnExpansionChanged)
					.SelectionMode(ESelectionMode::Multi)
				]
			]
		]
	];

	// Force the entire tree collapsed to start
	SetTagTreeItemExpansion(false);
	 
	LoadSettings();

	// Strip any invalid tags from the assets being edited
	VerifyAssetTagValidity();
}

void SEventWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bDelayRefresh)
{
		RefreshTags();
		bDelayRefresh = false;
}
	}

FVector2D SEventWidget::ComputeDesiredSize(float LayoutScaleMultiplier) const
	{
	FVector2D WidgetSize = SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);

	FVector2D TagTreeContainerSize = TagTreeContainerWidget->GetDesiredSize();

	if (TagTreeContainerSize.Y < MaxHeight)
	{
		WidgetSize.Y += MaxHeight - TagTreeContainerSize.Y;
	}

	return WidgetSize;
}

void SEventWidget::OnFilterTextChanged( const FText& InFilterText )
{
	FilterString = InFilterText.ToString();	

	FilterTagTree();
}

void SEventWidget::FilterTagTree()
{
	if (FilterString.IsEmpty())
	{
		TagTreeWidget->SetTreeItemsSource(&TagItems);

		for (int32 iItem = 0; iItem < TagItems.Num(); ++iItem)
		{
			SetDefaultTagNodeItemExpansion(TagItems[iItem]);
		}
	}
	else
	{
		FilteredTagItems.Empty();

		for (int32 iItem = 0; iItem < TagItems.Num(); ++iItem)
		{
			if (FilterChildrenCheck(TagItems[iItem]))
			{
				FilteredTagItems.Add(TagItems[iItem]);
				SetTagNodeItemExpansion(TagItems[iItem], true);
			}
			else
			{
				SetTagNodeItemExpansion(TagItems[iItem], false);
			}
		}

		TagTreeWidget->SetTreeItemsSource(&FilteredTagItems);
	}

	TagTreeWidget->RequestTreeRefresh();
}

bool SEventWidget::FilterChildrenCheck( TSharedPtr<FEventNode> InItem )
{
	if( !InItem.IsValid() )
	{
		return false;
	}

	if (bRestrictedTags && !InItem->IsRestrictedEvent())
	{
		return false;
	}

	auto FilterChildrenCheck_r = ([=]()
	{
		TArray< TSharedPtr<FEventNode> > Children = InItem->GetChildTagNodes();
		for( int32 iChild = 0; iChild < Children.Num(); ++iChild )
		{
			if( FilterChildrenCheck( Children[iChild] ) )
			{
				return true;
			}
		}
		return false;
	});


	bool DelegateShouldHide = false;
	UEventsManager::Get().OnFilterEventChildren.Broadcast(RootFilterString, InItem, DelegateShouldHide);
	if (DelegateShouldHide)
	{
		// The delegate wants to hide, see if any children need to show
		return FilterChildrenCheck_r();
	}

	if( InItem->GetCompleteTagString().Contains( FilterString ) || FilterString.IsEmpty() )
	{
		return true;
	}

	return FilterChildrenCheck_r();
}

TSharedRef<ITableRow> SEventWidget::OnGenerateRow(TSharedPtr<FEventNode> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	FText TooltipText;
	if (InItem.IsValid())
	{
		UEventsManager& Manager = UEventsManager::Get();

		FName TagName = InItem.Get()->GetCompleteTagName();
		TSharedPtr<FEventNode> Node = Manager.FindTagNode(TagName);

		FString TooltipString = TagName.ToString();

		if (Node.IsValid())
		{
			// Add Tag source if we're in management mode
			if (EventUIMode == EEventUIMode::ManagementMode)
			{
				FName TagSource;

				if (Node->bIsExplicitTag)
				{
					TagSource = Node->SourceName;
				}
				else
				{
					TagSource = FName(TEXT("Implicit"));
				}

				TooltipString.Append(FString::Printf(TEXT(" (%s)"), *TagSource.ToString()));
			}

			// tag comments
			if (!Node->DevComment.IsEmpty())
			{
				TooltipString.Append(FString::Printf(TEXT("\n\n%s"), *Node->DevComment));
			}

			// info related to conflicts
			if (Node->bDescendantHasConflict)
			{
				TooltipString.Append(TEXT("\n\nA tag that descends from this tag has a source conflict."));
			}

			if (Node->bAncestorHasConflict)
			{
				TooltipString.Append(TEXT("\n\nThis tag is descended from a tag that has a conflict. No operations can be performed on this tag until the conflict is resolved."));
			}

			if (Node->bNodeHasConflict)
			{
				TooltipString.Append(TEXT("\n\nThis tag comes from multiple sources. Tags may only have one source."));
			}
		}

		TooltipText = FText::FromString(TooltipString);
	}

	return SNew(STableRow< TSharedPtr<FEventNode> >, OwnerTable)
		.Style(FEditorStyle::Get(), "EventTreeView")
		[
			SNew( SHorizontalBox )

			// Tag Selection (selection mode only)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Left)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SEventWidget::OnTagCheckStatusChanged, InItem)
				.IsChecked(this, &SEventWidget::IsTagChecked, InItem)
				.ToolTipText(TooltipText)
				.IsEnabled(this, &SEventWidget::CanSelectTags)
				.Visibility( EventUIMode == EEventUIMode::SelectionMode ? EVisibility::Visible : EVisibility::Collapsed )
				[
					SNew(STextBlock)
					.Text(FText::FromName(InItem->GetSimpleTagName()))
				]
			]

			// Normal Tag Display (management mode only)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Left)
			[
				SNew( STextBlock )
				.ToolTip( FSlateApplication::Get().MakeToolTip(TooltipText) )
				.Text(FText::FromName( InItem->GetSimpleTagName()) )
				.ColorAndOpacity(this, &SEventWidget::GetTagTextColour, InItem)
				.Visibility( EventUIMode != EEventUIMode::SelectionMode ? EVisibility::Visible : EVisibility::Collapsed )
			]

			// Allows non-restricted children checkbox
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("AllowsChildren", "Does this restricted tag allow non-restricted children"))
				.OnCheckStateChanged(this, &SEventWidget::OnAllowChildrenTagCheckStatusChanged, InItem)
				.IsChecked(this, &SEventWidget::IsAllowChildrenTagChecked, InItem)
				.Visibility(this, &SEventWidget::DetermineAllowChildrenVisible, InItem)
			]

			// Add Subtag
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew( SButton )
				.ToolTipText( LOCTEXT("AddSubtag", "Add Subtag") )
				.Visibility(this, &SEventWidget::DetermineAddNewSubTagWidgetVisibility, InItem)
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.OnClicked( this, &SEventWidget::OnAddSubtagClicked, InItem )
				.DesiredSizeScale(FVector2D(0.75f, 0.75f))
				.ContentPadding(4.0f)
				.ForegroundColor(FSlateColor::UseForeground())
				.IsEnabled( !bReadOnly )
				.IsFocusable( false )
				[
					SNew( SImage )
					.Image(FEditorStyle::GetBrush("PropertyWindow.Button_AddToArray"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// More Actions Menu
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew( SComboButton )
				.ToolTipText( LOCTEXT("MoreActions", "More Actions...") )
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ContentPadding(0)
				.ForegroundColor(FSlateColor::UseForeground())
				.HasDownArrow(true)
				.MenuContent()
				[
					MakeTagActionsMenu(InItem)
				]
			]
		];
}

void SEventWidget::OnGetChildren(TSharedPtr<FEventNode> InItem, TArray< TSharedPtr<FEventNode> >& OutChildren)
{
	TArray< TSharedPtr<FEventNode> > FilteredChildren;
	TArray< TSharedPtr<FEventNode> > Children = InItem->GetChildTagNodes();

	for( int32 iChild = 0; iChild < Children.Num(); ++iChild )
	{
		if( FilterChildrenCheck( Children[iChild] ) )
		{
			FilteredChildren.Add( Children[iChild] );
		}
	}
	OutChildren += FilteredChildren;
}

void SEventWidget::OnTagCheckStatusChanged(ECheckBoxState NewCheckState, TSharedPtr<FEventNode> NodeChanged)
{
	if (NewCheckState == ECheckBoxState::Checked)
	{
		OnTagChecked(NodeChanged);
	}
	else if (NewCheckState == ECheckBoxState::Unchecked)
	{
		OnTagUnchecked(NodeChanged);
	}
}

void SEventWidget::OnTagChecked(TSharedPtr<FEventNode> NodeChecked)
{
	FScopedTransaction Transaction( LOCTEXT("EventWidget_AddTags", "Add Events") );

	UEventsManager& TagsManager = UEventsManager::Get();

	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		TSharedPtr<FEventNode> CurNode(NodeChecked);
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FEventContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FEventContainer EditableContainer = *Container;

			bool bRemoveParents = false;

			while (CurNode.IsValid())
			{
				FEventInfo Event = CurNode->GetCompleteTag();

				if (bRemoveParents == false)
				{
					bRemoveParents = true;
					if (bMultiSelect == false)
					{
						EditableContainer.Reset();
					}
					EditableContainer.AddTag(Event);
				}
				else
				{
					EditableContainer.RemoveTag(Event);
				}

				CurNode = CurNode->GetParentTagNode();
			}
			SetContainer(Container, &EditableContainer, OwnerObj);
		}
	}
}

void SEventWidget::OnTagUnchecked(TSharedPtr<FEventNode> NodeUnchecked)
{
	FScopedTransaction Transaction( LOCTEXT("EventWidget_RemoveTags", "Remove Events"));
	if (NodeUnchecked.IsValid())
	{
		UEventsManager& TagsManager = UEventsManager::Get();

		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
			FEventContainer* Container = TagContainers[ContainerIdx].TagContainer;
			FEventInfo Event = NodeUnchecked->GetCompleteTag();

			if (Container)
			{
				FEventContainer EditableContainer = *Container;
				EditableContainer.RemoveTag(Event);

				TSharedPtr<FEventNode> ParentNode = NodeUnchecked->GetParentTagNode();
				if (ParentNode.IsValid())
				{
					// Check if there are other siblings before adding parent
					bool bOtherSiblings = false;
					for (auto It = ParentNode->GetChildTagNodes().CreateConstIterator(); It; ++It)
					{
						Event = It->Get()->GetCompleteTag();
						if (EditableContainer.HasTagExact(Event))
						{
							bOtherSiblings = true;
							break;
						}
					}
					// Add Parent
					if (!bOtherSiblings)
					{
						Event = ParentNode->GetCompleteTag();
						EditableContainer.AddTag(Event);
					}
				}

				// Uncheck Children
				for (const auto& ChildNode : NodeUnchecked->GetChildTagNodes())
				{
					UncheckChildren(ChildNode, EditableContainer);
				}

				SetContainer(Container, &EditableContainer, OwnerObj);
			}
			
		}
	}
}

void SEventWidget::UncheckChildren(TSharedPtr<FEventNode> NodeUnchecked, FEventContainer& EditableContainer)
{
	UEventsManager& TagsManager = UEventsManager::Get();

	FEventInfo Event = NodeUnchecked->GetCompleteTag();
	EditableContainer.RemoveTag(Event);

	// Uncheck Children
	for (const auto& ChildNode : NodeUnchecked->GetChildTagNodes())
	{
		UncheckChildren(ChildNode, EditableContainer);
	}
}

ECheckBoxState SEventWidget::IsTagChecked(TSharedPtr<FEventNode> Node) const
{
	int32 NumValidAssets = 0;
	int32 NumAssetsTagIsAppliedTo = 0;

	if (Node.IsValid())
	{
		UEventsManager& TagsManager = UEventsManager::Get();

		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			FEventContainer* Container = TagContainers[ContainerIdx].TagContainer;
			if (Container&&Container->IsValid())
			{
				NumValidAssets++;
				FEventInfo Event = Node->GetCompleteTag();
				if (Event.IsValid())
				{
					if (Container->HasTag(Event))
					{
						++NumAssetsTagIsAppliedTo;
					}
				}
			}
		}
	}

	if (NumAssetsTagIsAppliedTo == 0)
	{
		return ECheckBoxState::Unchecked;
	}
	else if (NumAssetsTagIsAppliedTo == NumValidAssets)
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Undetermined;
	}
}

bool SEventWidget::IsExactTagInCollection(TSharedPtr<FEventNode> Node) const
{
	if (Node.IsValid())
	{
		UEventsManager& TagsManager = UEventsManager::Get();

		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			FEventContainer* Container = TagContainers[ContainerIdx].TagContainer;
			if (Container)
			{
				FEventInfo Event = Node->GetCompleteTag();
				if (Event.IsValid())
				{
					if (Container->HasTagExact(Event))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void SEventWidget::OnAllowChildrenTagCheckStatusChanged(ECheckBoxState NewCheckState, TSharedPtr<FEventNode> NodeChanged)
{
	IEventsEditorModule& TagsEditor = IEventsEditorModule::Get();

	if (TagsEditor.UpdateTagInINI(NodeChanged->GetCompleteTagString(), NodeChanged->DevComment, NodeChanged->bIsRestrictedTag, NewCheckState == ECheckBoxState::Checked))
	{
		if (NewCheckState == ECheckBoxState::Checked)
		{
			NodeChanged->bAllowNonRestrictedChildren = true;
		}
		else if (NewCheckState == ECheckBoxState::Unchecked)
		{
			NodeChanged->bAllowNonRestrictedChildren = false;
		}
	}
}

ECheckBoxState SEventWidget::IsAllowChildrenTagChecked(TSharedPtr<FEventNode> Node) const
{
	if (Node->GetAllowNonRestrictedChildren())
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}

EVisibility SEventWidget::DetermineAllowChildrenVisible(TSharedPtr<FEventNode> Node) const
{
	// We do not allow you to modify nodes that have a conflict or inherit from a node with a conflict
	if (Node->bNodeHasConflict || Node->bAncestorHasConflict)
	{
		return EVisibility::Hidden;
	}

	if (bRestrictedTags)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FReply SEventWidget::OnClearAllClicked()
{
	FScopedTransaction Transaction( LOCTEXT("EventWidget_RemoveAllTags", "Remove All Events") );

	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FEventContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FEventContainer EmptyContainer;
			SetContainer(Container, &EmptyContainer, OwnerObj);
		}
	}
	return FReply::Handled();
}

FSlateColor SEventWidget::GetTagTextColour(TSharedPtr<FEventNode> Node) const
{
	static const FLinearColor DefaultTextColour = FLinearColor::White;
	static const FLinearColor DescendantConflictTextColour = FLinearColor(1.f, 0.65f, 0.f); // orange
	static const FLinearColor NodeConflictTextColour = FLinearColor::Red;
	static const FLinearColor AncestorConflictTextColour = FLinearColor(1.f, 1.f, 1.f, 0.5f);

	if (Node->bNodeHasConflict)
	{
		return NodeConflictTextColour;
	}

	if (Node->bDescendantHasConflict)
	{
		return DescendantConflictTextColour;
	}

	if (Node->bAncestorHasConflict)
	{
		return AncestorConflictTextColour;
	}

	return DefaultTextColour;
}

FReply SEventWidget::OnExpandAllClicked()
{
	SetTagTreeItemExpansion(true);
	return FReply::Handled();
}

FReply SEventWidget::OnCollapseAllClicked()
{
	SetTagTreeItemExpansion(false);
	return FReply::Handled();
}

FReply SEventWidget::OnAddSubtagClicked(TSharedPtr<FEventNode> InTagNode)
{
	if (!bReadOnly && InTagNode.IsValid())
	{
		UEventsManager& Manager = UEventsManager::Get();

		FString TagName = InTagNode->GetCompleteTagString();
		FString TagComment;
		FName TagSource;
		bool bTagIsExplicit;
		bool bTagIsRestricted;
		bool bTagAllowsNonRestrictedChildren;

		Manager.GetTagEditorData(InTagNode->GetCompleteTagName(), TagComment, TagSource, bTagIsExplicit, bTagIsRestricted, bTagAllowsNonRestrictedChildren);

		if (!bRestrictedTags && AddNewTagWidget.IsValid())
		{
			bAddTagSectionExpanded = true; 
			AddNewTagWidget->AddSubtagFromParent(TagName, TagSource);
		}
		else if (bRestrictedTags && AddNewRestrictedTagWidget.IsValid())
		{
			bAddTagSectionExpanded = true;
			AddNewRestrictedTagWidget->AddSubtagFromParent(TagName, TagSource, bTagAllowsNonRestrictedChildren);
		}
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SEventWidget::MakeTagActionsMenu(TSharedPtr<FEventNode> InTagNode)
{
	bool bShowManagement = (EventUIMode == EEventUIMode::ManagementMode && !bReadOnly);
	UEventsManager& Manager = UEventsManager::Get();

	if (!Manager.ShouldImportTagsFromINI())
	{
		bShowManagement = false;
	}

	// You can't modify restricted tags in the normal tag menus
	if (!bRestrictedTags && InTagNode->IsRestrictedEvent())
	{
		bShowManagement = false;
	}

	// we can only rename or delete tags if they came from an ini file
	if (!InTagNode->SourceName.ToString().EndsWith(TEXT(".ini")))
	{
		bShowManagement = false;
	}

	FMenuBuilder MenuBuilder(true, nullptr);

	// Rename
	if (bShowManagement)
	{
		FExecuteAction RenameAction = FExecuteAction::CreateSP(this, &SEventWidget::OnRenameTag, InTagNode);

		MenuBuilder.AddMenuEntry(LOCTEXT("EventWidget_RenameTag", "Rename"), LOCTEXT("EventWidget_RenameTagTooltip", "Rename this tag"), FSlateIcon(), FUIAction(RenameAction));
	}

	// Delete
	if (bShowManagement)
	{
		FExecuteAction DeleteAction = FExecuteAction::CreateSP(this, &SEventWidget::OnDeleteTag, InTagNode);

		MenuBuilder.AddMenuEntry(LOCTEXT("EventWidget_DeleteTag", "Delete"), LOCTEXT("EventWidget_DeleteTagTooltip", "Delete this tag"), FSlateIcon(), FUIAction(DeleteAction));
	}

	if (IsExactTagInCollection(InTagNode))
	{
		FExecuteAction RemoveAction = FExecuteAction::CreateSP(this, &SEventWidget::OnRemoveTag, InTagNode);
		MenuBuilder.AddMenuEntry(LOCTEXT("EventWidget_RemoveTag", "Remove Exact Tag"), LOCTEXT("EventWidget_RemoveTagTooltip", "Remove this exact tag, Parent and Child Tags will not be effected."), FSlateIcon(), FUIAction(RemoveAction));
	}
	else
	{
		FExecuteAction AddAction = FExecuteAction::CreateSP(this, &SEventWidget::OnAddTag, InTagNode);
		MenuBuilder.AddMenuEntry(LOCTEXT("EventWidget_AddTag", "Add Exact Tag"), LOCTEXT("EventWidget_AddTagTooltip", "Add this exact tag, Parent and Child Child Tags will not be effected."), FSlateIcon(), FUIAction(AddAction));
	}

	// Search for References
	if (FEditorDelegates::OnOpenReferenceViewer.IsBound())
	{
		FExecuteAction SearchForReferencesAction = FExecuteAction::CreateSP(this, &SEventWidget::OnSearchForReferences, InTagNode);
		MenuBuilder.AddMenuEntry(LOCTEXT("EventWidget_SearchForReferences", "Search For References"), LOCTEXT("EventWidget_SearchForReferencesTooltip", "Find references for this tag"), FSlateIcon(), FUIAction(SearchForReferencesAction));
	}

	return MenuBuilder.MakeWidget();
}

void SEventWidget::OnRenameTag(TSharedPtr<FEventNode> InTagNode)
{
	if (InTagNode.IsValid())
	{
		OpenRenameEventDialog( InTagNode );
	}
}

void SEventWidget::OnDeleteTag(TSharedPtr<FEventNode> InTagNode)
{
	if (InTagNode.IsValid())
	{
		IEventsEditorModule& TagsEditor = IEventsEditorModule::Get();

		const bool bDeleted = TagsEditor.DeleteTagFromINI(InTagNode);

		if (bDeleted)
		{
			OnTagChanged.ExecuteIfBound();
		}
	}
}

void SEventWidget::OnAddTag(TSharedPtr<FEventNode> InTagNode)
{
	if (InTagNode.IsValid())
	{
		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			FEventContainer* Container = TagContainers[ContainerIdx].TagContainer;
			Container->AddTag(InTagNode->GetCompleteTag());
		}

		OnTagChanged.ExecuteIfBound();
	}
}

void SEventWidget::OnRemoveTag(TSharedPtr<FEventNode> InTagNode)
{
	if (InTagNode.IsValid())
	{
		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			FEventContainer* Container = TagContainers[ContainerIdx].TagContainer;
			Container->RemoveTag(InTagNode->GetCompleteTag());
		}

		OnTagChanged.ExecuteIfBound();
	}
}

void SEventWidget::OnSearchForReferences(TSharedPtr<FEventNode> InTagNode)
{
	if (InTagNode.IsValid())
	{
		TArray<FAssetIdentifier> AssetIdentifiers;
		AssetIdentifiers.Add(FAssetIdentifier(FEventInfo::StaticStruct(), InTagNode->GetCompleteTagName()));
		FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
	}
}

void SEventWidget::SetTagTreeItemExpansion(bool bExpand)
{
	TArray< TSharedPtr<FEventNode> > TagArray;
	UEventsManager::Get().GetFilteredGameplayRootTags(TEXT(""), TagArray);
	for (int32 TagIdx = 0; TagIdx < TagArray.Num(); ++TagIdx)
	{
		SetTagNodeItemExpansion(TagArray[TagIdx], bExpand);
	}
	
}

void SEventWidget::SetTagNodeItemExpansion(TSharedPtr<FEventNode> Node, bool bExpand)
{
	if (Node.IsValid() && TagTreeWidget.IsValid())
	{
		TagTreeWidget->SetItemExpansion(Node, bExpand);

		const TArray< TSharedPtr<FEventNode> >& ChildTags = Node->GetChildTagNodes();
		for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
		{
			SetTagNodeItemExpansion(ChildTags[ChildIdx], bExpand);
		}
	}
}

void SEventWidget::VerifyAssetTagValidity()
{
	FEventContainer LibraryTags;

	// Create a set that is the library of all valid tags
	TArray< TSharedPtr<FEventNode> > NodeStack;

	UEventsManager& TagsManager = UEventsManager::Get();
	
	TagsManager.GetFilteredGameplayRootTags(TEXT(""), NodeStack);

	while (NodeStack.Num() > 0)
	{
		TSharedPtr<FEventNode> CurNode = NodeStack.Pop();
		if (CurNode.IsValid())
		{
			LibraryTags.AddTag(CurNode->GetCompleteTag());
			NodeStack.Append(CurNode->GetChildTagNodes());
		}
	}

	// Find and remove any tags on the asset that are no longer in the library
	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FEventContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FEventContainer EditableContainer = *Container;

			// Use a set instead of a container so we can find and remove None tags
			TSet<FEventInfo> InvalidTags;

			for (auto It = Container->CreateConstIterator(); It; ++It)
			{
				FEventInfo TagToCheck = *It;

				// Check redirectors, these will get fixed on load time
				UEventsManager::Get().RedirectSingleEvent(TagToCheck, nullptr);

				if (!LibraryTags.HasTagExact(TagToCheck))
				{
					InvalidTags.Add(*It);
				}
			}
			if (InvalidTags.Num() > 0)
			{
				FString InvalidTagNames;

				for (auto InvalidIter = InvalidTags.CreateConstIterator(); InvalidIter; ++InvalidIter)
				{
					EditableContainer.RemoveTag(*InvalidIter);
					InvalidTagNames += InvalidIter->ToString() + TEXT("\n");
				}
				SetContainer(Container, &EditableContainer, OwnerObj);

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Objects"), FText::FromString( InvalidTagNames ));
				FText DialogText = FText::Format( LOCTEXT("EventWidget_InvalidTags", "Invalid Tags that have been removed: \n\n{Objects}"), Arguments );
				FText DialogTitle = LOCTEXT("EventWidget_Warning", "Warning");
				FMessageDialog::Open( EAppMsgType::Ok, DialogText, &DialogTitle );
			}
		}
	}
}

void SEventWidget::LoadSettings()
{
	TArray< TSharedPtr<FEventNode> > TagArray;
	UEventsManager::Get().GetFilteredGameplayRootTags(TEXT(""), TagArray);
	for (int32 TagIdx = 0; TagIdx < TagArray.Num(); ++TagIdx)
	{
		LoadTagNodeItemExpansion(TagArray[TagIdx] );
	}
}

void SEventWidget::SetDefaultTagNodeItemExpansion( TSharedPtr<FEventNode> Node )
{
	if ( Node.IsValid() && TagTreeWidget.IsValid() )
	{
		bool bExpanded = false;

		if ( IsTagChecked(Node) == ECheckBoxState::Checked )
		{
			bExpanded = true;
		}
		TagTreeWidget->SetItemExpansion(Node, bExpanded);

		const TArray< TSharedPtr<FEventNode> >& ChildTags = Node->GetChildTagNodes();
		for ( int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx )
		{
			SetDefaultTagNodeItemExpansion(ChildTags[ChildIdx]);
		}
	}
}

void SEventWidget::LoadTagNodeItemExpansion( TSharedPtr<FEventNode> Node )
{
	if (Node.IsValid() && TagTreeWidget.IsValid())
	{
		bool bExpanded = false;

		if( GConfig->GetBool(*SettingsIniSection, *(TagContainerName + Node->GetCompleteTagString() + TEXT(".Expanded")), bExpanded, GEditorPerProjectIni) )
		{
			TagTreeWidget->SetItemExpansion( Node, bExpanded );
		}
		else if( IsTagChecked( Node ) == ECheckBoxState::Checked ) // If we have no save data but its ticked then we probably lost our settings so we shall expand it
		{
			TagTreeWidget->SetItemExpansion( Node, true );
		}

		const TArray< TSharedPtr<FEventNode> >& ChildTags = Node->GetChildTagNodes();
		for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
		{
			LoadTagNodeItemExpansion(ChildTags[ChildIdx]);
		}
	}
}

void SEventWidget::OnExpansionChanged( TSharedPtr<FEventNode> InItem, bool bIsExpanded )
{
	// Save the new expansion setting to ini file
	GConfig->SetBool(*SettingsIniSection, *(TagContainerName + InItem->GetCompleteTagString() + TEXT(".Expanded")), bIsExpanded, GEditorPerProjectIni);
}

void SEventWidget::SetContainer(FEventContainer* OriginalContainer, FEventContainer* EditedContainer, UObject* OwnerObj)
{
	if (PropertyHandle.IsValid() && bMultiSelect)
	{
		// Case for a tag container 
		PropertyHandle->SetValueFromFormattedString(EditedContainer->ToString());
	}
	else if (PropertyHandle.IsValid() && !bMultiSelect)
	{
		// Case for a single Tag		
		FString FormattedString = TEXT("(TagName=\"");
		FormattedString += EditedContainer->First().GetTagName().ToString();
		FormattedString += TEXT("\")");
		PropertyHandle->SetValueFromFormattedString(FormattedString);
	}
	else
	{
		// Not sure if we should get here, means the property handle hasnt been setup which could be right or wrong.
		if (OwnerObj)
		{
			OwnerObj->PreEditChange(PropertyHandle.IsValid() ? PropertyHandle->GetProperty() : nullptr);
		}

		*OriginalContainer = *EditedContainer;

		if (OwnerObj)
		{
			OwnerObj->PostEditChange();
		}
	}	

	if (!PropertyHandle.IsValid())
	{
		OnTagChanged.ExecuteIfBound();
	}
}

void SEventWidget::OnEventAdded(const FString& TagName, const FString& TagComment, const FName& TagSource)
{
	UEventsManager& Manager = UEventsManager::Get();

	RefreshTags();
	TagTreeWidget->RequestTreeRefresh();

	if (EventUIMode == EEventUIMode::SelectionMode)
	{
		TSharedPtr<FEventNode> TagNode = Manager.FindTagNode(FName(*TagName));
		if (TagNode.IsValid())
		{
			OnTagChecked(TagNode);
		}

		// Filter on the new tag
		SearchTagBox->SetText(FText::FromString(TagName));

		// Close the Add New Tag UI
		bAddTagSectionExpanded = false;
	}
}

void SEventWidget::RefreshTags()
{
	UEventsManager& Manager = UEventsManager::Get();
	Manager.GetFilteredGameplayRootTags(RootFilterString, TagItems);

	if (bRestrictedTags)
	{
		// We only want to show the restricted gameplay tags
		for (int32 Idx = TagItems.Num() - 1; Idx >= 0; --Idx)
		{
			if (!TagItems[Idx]->IsRestrictedEvent())
			{
				TagItems.RemoveAtSwap(Idx);
			}
		}
	}

	FilterTagTree();
}

EVisibility SEventWidget::DetermineExpandableUIVisibility() const
{
	UEventsManager& Manager = UEventsManager::Get();

	if ( !Manager.ShouldImportTagsFromINI() )
	{
		// If we can't support adding tags from INI files, we should never see this widget
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SEventWidget::DetermineAddNewSourceExpandableUIVisibility() const
{
	if (bRestrictedTags)
	{
		return EVisibility::Collapsed;
	}

	return DetermineExpandableUIVisibility();
}

EVisibility SEventWidget::DetermineAddNewTagWidgetVisibility() const
{
	UEventsManager& Manager = UEventsManager::Get();

	if ( !Manager.ShouldImportTagsFromINI() || !bAddTagSectionExpanded || bRestrictedTags )
	{
		// If we can't support adding tags from INI files, we should never see this widget
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SEventWidget::DetermineAddNewRestrictedTagWidgetVisibility() const
{
	UEventsManager& Manager = UEventsManager::Get();

	if (!Manager.ShouldImportTagsFromINI() || !bAddTagSectionExpanded || !bRestrictedTags)
	{
		// If we can't support adding tags from INI files, we should never see this widget
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SEventWidget::DetermineAddNewSourceWidgetVisibility() const
{
	UEventsManager& Manager = UEventsManager::Get();

	if (!Manager.ShouldImportTagsFromINI() || !bAddSourceSectionExpanded || bRestrictedTags)
	{
		// If we can't support adding tags from INI files, we should never see this widget
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SEventWidget::DetermineAddNewSubTagWidgetVisibility(TSharedPtr<FEventNode> Node) const
{
	EVisibility LocalVisibility = DetermineExpandableUIVisibility();
	if (LocalVisibility != EVisibility::Visible)
	{
		return LocalVisibility;
	}

	// We do not allow you to add child tags under a conflict
	if (Node->bNodeHasConflict || Node->bAncestorHasConflict)
	{
		return EVisibility::Hidden;
	}

	// show if we're dealing with restricted tags exclusively or restricted tags that allow non-restricted children
	if (Node->GetAllowNonRestrictedChildren() || bRestrictedTags)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

EVisibility SEventWidget::DetermineClearSelectionVisibility() const
{
	return CanSelectTags() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SEventWidget::CanSelectTags() const
{
	return !bReadOnly && EventUIMode == EEventUIMode::SelectionMode;
}

bool SEventWidget::IsAddingNewTag() const
{
	return AddNewTagWidget.IsValid() && AddNewTagWidget->IsAddingNewTag();
}

ECheckBoxState SEventWidget::GetAddTagSectionExpansionState() const
{
	return bAddTagSectionExpanded ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SEventWidget::OnAddTagSectionExpansionStateChanged(ECheckBoxState NewState)
{
	bAddTagSectionExpanded = NewState == ECheckBoxState::Checked;
}

ECheckBoxState SEventWidget::GetAddSourceSectionExpansionState() const
{
	return bAddSourceSectionExpanded ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SEventWidget::OnAddSourceSectionExpansionStateChanged(ECheckBoxState NewState)
{
	bAddSourceSectionExpanded = NewState == ECheckBoxState::Checked;
}

void SEventWidget::RefreshOnNextTick()
{
	bDelayRefresh = true;
}

void SEventWidget::OnEventRenamed(FString OldTagName, FString NewTagName)
{
	OnTagChanged.ExecuteIfBound();
}

void SEventWidget::OpenRenameEventDialog(TSharedPtr<FEventNode> EventNode) const
{
	TSharedRef<SWindow> RenameTagWindow =
		SNew(SWindow)
		.Title(LOCTEXT("RenameTagWindowTitle", "Rename Gameplay Tag"))
		.ClientSize(FVector2D(320.0f, 110.0f))
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedRef<SRenameEventDialog> RenameTagDialog =
		SNew(SRenameEventDialog)
		.EventNode(EventNode)
		.OnEventRenamed(const_cast<SEventWidget*>(this), &SEventWidget::OnEventRenamed);

	RenameTagWindow->SetContent(RenameTagDialog);

	TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared() );

	FSlateApplication::Get().AddModalWindow(RenameTagWindow, CurrentWindow);
}

TSharedPtr<SWidget> SEventWidget::GetWidgetToFocusOnOpen()
{
	return SearchTagBox;
}

#undef LOCTEXT_NAMESPACE
