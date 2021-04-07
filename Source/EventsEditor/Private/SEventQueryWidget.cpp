// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "SEventQueryWidget.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "EventQueryWidget"

void SEventQueryWidget::Construct(const FArguments& InArgs, const TArray<FEditableEventQueryDatum>& EditableTagQueries)
{
	ensure(EditableTagQueries.Num() > 0);
	TagQueries = EditableTagQueries;

	bReadOnly = InArgs._ReadOnly;
	bAutoSave = InArgs._AutoSave;
	OnClosePreSave = InArgs._OnClosePreSave;
	OnSaveAndClose = InArgs._OnSaveAndClose;
	OnCancel = InArgs._OnCancel;
	OnQueryChanged = InArgs._OnQueryChanged;

	// Tag the assets as transactional so they can support undo/redo
	for (int32 AssetIdx = 0; AssetIdx < TagQueries.Num(); ++AssetIdx)
	{
		UObject* TagQueryOwner = TagQueries[AssetIdx].TagQueryOwner.Get();
		if (TagQueryOwner)
		{
			TagQueryOwner->SetFlags(RF_Transactional);
		}
	}

	// build editable query object tree from the runtime query data
	UEditableEventQuery* const EQ = CreateEditableQuery(*TagQueries[0].TagQuery);
	EditableQuery = EQ;

	// create details view for the editable query object
	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = false;
	ViewArgs.bHideSelectionTip = true;
	ViewArgs.bShowActorLabel = false;
	
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	Details = PropertyModule.CreateDetailView(ViewArgs);
	Details->SetObject(EQ);
	Details->OnFinishedChangingProperties().AddSP(this, &SEventQueryWidget::OnFinishedChangingProperties);

	ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.IsEnabled(!bReadOnly)
		.Visibility(this, &SEventQueryWidget::GetSaveAndCloseButtonVisibility)
		.OnClicked(this, &SEventQueryWidget::OnSaveAndCloseClicked)
		.Text(LOCTEXT("EventQueryWidget_SaveAndClose", "Save and Close"))
		]
	+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility(this, &SEventQueryWidget::GetCancelButtonVisibility)
		.OnClicked(this, &SEventQueryWidget::OnCancelClicked)
		.Text(LOCTEXT("EventQueryWidget_Cancel", "Close Without Saving"))
		]
		]
	// to delete!
	+ SVerticalBox::Slot()
		[
			Details.ToSharedRef()
		]
		]
		];
}

void SEventQueryWidget::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	// Auto saved changes will not call pre and post notify; auto save should only be used to make changes coming from blueprints
	if (bAutoSave)
	{
		SaveToTagQuery();
	}

	OnQueryChanged.ExecuteIfBound();
}

EVisibility SEventQueryWidget::GetSaveAndCloseButtonVisibility() const
{
	return bAutoSave ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SEventQueryWidget::GetCancelButtonVisibility() const
{
	return bAutoSave ? EVisibility::Collapsed : EVisibility::Visible;
}

UEditableEventQuery* SEventQueryWidget::CreateEditableQuery(FEventQuery& Q)
{
	UEditableEventQuery* const AnEditableQuery = Q.CreateEditableQuery();
	if (AnEditableQuery)
	{
		// prevent GC, will explicitly remove from root later
		AnEditableQuery->AddToRoot();
	}

	return AnEditableQuery;
}

SEventQueryWidget::~SEventQueryWidget()
{
	// clean up our temp editing uobjects
	UEditableEventQuery* const Q = EditableQuery.Get();
	if (Q)
	{
		Q->RemoveFromRoot();
	}
}


void SEventQueryWidget::SaveToTagQuery()
{
	// translate obj tree to token stream
	if (EditableQuery.IsValid() && !bReadOnly)
	{
		// write to all selected queries
		for (auto& TQ : TagQueries)
		{
			TQ.TagQuery->BuildFromEditableQuery(*EditableQuery.Get());
			if (TQ.TagQueryExportText != nullptr)
			{
				*TQ.TagQueryExportText = EditableQuery.Get()->GetTagQueryExportText(*TQ.TagQuery);
			}
			if (TQ.TagQueryOwner.IsValid())
			{
				TQ.TagQueryOwner->MarkPackageDirty();
			}
		}
	}
}

FReply SEventQueryWidget::OnSaveAndCloseClicked()
{
	OnClosePreSave.ExecuteIfBound();

	SaveToTagQuery();

	OnSaveAndClose.ExecuteIfBound();
	return FReply::Handled();
}

FReply SEventQueryWidget::OnCancelClicked()
{
	OnCancel.ExecuteIfBound();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE