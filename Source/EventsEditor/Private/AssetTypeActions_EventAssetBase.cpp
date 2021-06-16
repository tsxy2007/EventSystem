// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "AssetTypeActions_EventAssetBase.h"
#include "ToolMenus.h"
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"

#include "EventsRuntime/Classes/EventContainer.h"
#include "SEventWidget.h"
#include "Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_EventAssetBase::FAssetTypeActions_EventAssetBase(FName InTagPropertyName)
	: OwnedEventPropertyName(InTagPropertyName)
{}

bool FAssetTypeActions_EventAssetBase::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_EventAssetBase::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<UObject*> ContainerObjectOwners;
	TArray<FEventContainer*> Containers;
	for (int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ++ObjIdx)
	{
		UObject* CurObj = InObjects[ObjIdx];
		if (CurObj)
		{
			FStructProperty* StructProp = FindFProperty<FStructProperty>(CurObj->GetClass(), OwnedEventPropertyName);
			if(StructProp != NULL)
			{
				ContainerObjectOwners.Add(CurObj);
				Containers.Add(StructProp->ContainerPtrToValuePtr<FEventContainer>(CurObj));
			}
		}
	}

	ensure(Containers.Num() == ContainerObjectOwners.Num());
	if (Containers.Num() > 0 && (Containers.Num() == ContainerObjectOwners.Num()))
	{
		Section.AddMenuEntry(
			"Events_Edit",
			LOCTEXT("Events_Edit", "Edit Events..."),
			LOCTEXT("Events_EditToolTip", "Opens the Events Editor."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FAssetTypeActions_EventAssetBase::OpenEventEditor, ContainerObjectOwners, Containers), FCanExecuteAction()));
	}
}

void FAssetTypeActions_EventAssetBase::OpenEventEditor(TArray<UObject*> Objects, TArray<FEventContainer*> Containers)
{
	TArray<SEventWidget::FEditableEventContainerDatum> EditableContainers;
	for (int32 ObjIdx = 0; ObjIdx < Objects.Num() && ObjIdx < Containers.Num(); ++ObjIdx)
	{
		EditableContainers.Add(SEventWidget::FEditableEventContainerDatum(Objects[ObjIdx], Containers[ObjIdx]));
	}

	FText Title;
	FText AssetName;

	const int32 NumAssets = EditableContainers.Num();
	if (NumAssets > 1)
	{
		AssetName = FText::Format( LOCTEXT("AssetTypeActions_EventAssetBaseMultipleAssets", "{0} Assets"), FText::AsNumber( NumAssets ) );
		Title = FText::Format( LOCTEXT("AssetTypeActions_EventAssetBaseEditorTitle", "Event Editor: Owned Events: {0}"), AssetName );
	}
	else if (NumAssets > 0 && EditableContainers[0].TagContainerOwner.IsValid())
	{
		AssetName = FText::FromString( EditableContainers[0].TagContainerOwner->GetName() );
		Title = FText::Format( LOCTEXT("AssetTypeActions_EventAssetBaseEditorTitle", "Event Editor: Owned Events: {0}"), AssetName );
	}

	TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(Title)
		.ClientSize(FVector2D(600, 400))
		[
			SNew(SEventWidget, EditableContainers)
		];

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	if (MainFrameModule.GetParentWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window.ToSharedRef(), MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window.ToSharedRef());
	}
}

uint32 FAssetTypeActions_EventAssetBase::GetCategories()
{ 
	return EAssetTypeCategories::Misc; 
}

#undef LOCTEXT_NAMESPACE
