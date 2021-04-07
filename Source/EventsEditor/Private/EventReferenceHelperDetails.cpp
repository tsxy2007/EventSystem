// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventReferenceHelperDetails.h"
#include "AssetRegistryModule.h"
#include "Widgets/Input/SHyperlink.h"

#include "ObjectTools.h"
#include "EventContainer.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "EventsManager.h"
#include "SEventWidget.h"
#include "IDetailChildrenBuilder.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "EventReferenceHelperDetails"

static const FName EventColumnName("EventColumn");

TSharedRef<IPropertyTypeCustomization> FEventReferenceHelperDetails::MakeInstance()
{
	return MakeShareable(new FEventReferenceHelperDetails());
}

void FEventReferenceHelperDetails::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	PropertyHandle = StructPropertyHandle;

	TreeItems.Reset();
	if (FEventReferenceHelper* Helper = GetValue())
	{
		// We need the raw data pointer to the struct (UStruct or UClass) that owns the FEventReferenceHelper property.
		// Its not enough to just bind the raw 'this' pointer in the owning struct's cstor, since lists or data tables of structs
		// will be copied around as the list changes sizes (overloading copy and assignment operators on the owning struct to clean/update 
		// the delegate is also a major pain).
		// 
		// We cheat a bit here and use GetOffset_ForGC to work backwards up the property change and get the raw, castable, address of the owning
		// structure so that the delegate can just do a static_cast and do whatever they want.
		//
		//
		// Note: this currently does NOT handle the owning struct changing and auto updating. This is a bit tricky since we don't know, in this context, when
		// an update has to happen, since this thing is not tied directly to a tag uproperty. (E.g, a data table row where the row's key name is the tag tag name)

		void* OwnerStructRawData = nullptr;
		if (FProperty* MyProperty = PropertyHandle->GetProperty())
		{
			if (UStruct* OwnerStruct = MyProperty->GetOwnerStruct())
			{				
				// Manually calculate the owning struct raw data pointer
				checkf(MyProperty->ArrayDim == 1, TEXT("FEventReferenceHelper should never be in an array"));
				uint8* PropertyRawData = (uint8*)GetValue();
				OwnerStructRawData = (void*) (PropertyRawData - MyProperty->GetOffset_ForGC());
			}
		}
		
		if (ensureMsgf(OwnerStructRawData != nullptr, TEXT("Unable to get outer struct's raw data")))
		{
			FName TagName = Helper->OnGetEventName.Execute(OwnerStructRawData);
		
			FAssetIdentifier TagId = FAssetIdentifier(FEventInfo::StaticStruct(), TagName);
			TArray<FAssetIdentifier> Referencers;

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().GetReferencers(TagId, Referencers, EAssetRegistryDependencyType::SearchableName);

			for (FAssetIdentifier& AssetIdentifier : Referencers)
			{
				TSharedPtr<FEventReferenceTreeItem> Item =  TSharedPtr<FEventReferenceTreeItem>(new FEventReferenceTreeItem());
				Item->EventName = TagName;
				Item->AssetIdentifier = AssetIdentifier;
				TreeItems.Add(Item);
			}
		}
	}

	HeaderRow
	.NameContent()
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolBar.Background"))
		[
			SNew(SEventReferenceTree)
			.ItemHeight(24)
			.TreeItemsSource(&TreeItems)
			.OnGenerateRow(this, &FEventReferenceHelperDetails::OnGenerateWidgetForGameplayCueListView)
			.OnGetChildren( SEventReferenceTree::FOnGetChildren::CreateLambda([](TSharedPtr<FEventReferenceTreeItem> Item, TArray< TSharedPtr<FEventReferenceTreeItem> >& Children) { } ) )
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(EventColumnName)
				.DefaultLabel(NSLOCTEXT("EventReferenceHelper", "EventReferenceHelperColumn", "Event Referencers (does not include native code)"))
				.FillWidth(0.50)
			)
		]
	];
}

void FEventReferenceHelperDetails::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	
}

FEventReferenceHelper* FEventReferenceHelperDetails::GetValue()
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	if (RawData.Num() != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("Unexpected raw data count of %d"), RawData.Num());
		return nullptr;
	}
	return static_cast<FEventReferenceHelper*>(RawData[0]);
}

/** Builds widget for rows in the GameplayCue Editor tab */
TSharedRef<ITableRow> FEventReferenceHelperDetails::OnGenerateWidgetForGameplayCueListView(TSharedPtr< FEventReferenceTreeItem > InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	class SEventWidgetItem : public SMultiColumnTableRow< TSharedPtr< FEventReferenceTreeItem > >
	{
	public:
		SLATE_BEGIN_ARGS(SEventWidgetItem){}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<FEventReferenceTreeItem> InListItem)
		{
			Item = InListItem;
			SMultiColumnTableRow< TSharedPtr< FEventReferenceTreeItem > >::Construct(
				FSuperRowType::FArguments()
			, InOwnerTable);
		}
	private:

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == EventColumnName)
			{
				if (Item->AssetIdentifier.ToString().IsEmpty() == false)
				{
					return
					SNew(SBox)
					.HAlign(HAlign_Left)
					[
						SNew(SHyperlink)
						.Style(FEditorStyle::Get(), "Common.GotoBlueprintHyperlink")
						.Text(FText::FromString(Item->AssetIdentifier.ToString()))
						.OnNavigate(this, &SEventWidgetItem::NavigateToReference)
					];
				}
				else
				{
					return
					SNew(SBox)
					.HAlign(HAlign_Left);
				}
			}
			else
			{
				return SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column"));
			}
		}

		void NavigateToReference()
		{
			UPackage* Pkg = FindPackage(nullptr, *Item->AssetIdentifier.PackageName.ToString());
			if (Pkg == nullptr)
			{
				Pkg = LoadPackage(nullptr, *Item->AssetIdentifier.PackageName.ToString(), LOAD_None);
			}

			if (Pkg)
			{
				ForEachObjectWithOuter(Pkg,[](UObject* Obj)
				{
					if (ObjectTools::IsObjectBrowsable(Obj))
					{
						GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Obj);
					}
				});
			}
		}

		TSharedPtr< FEventReferenceTreeItem > Item;
	};

	if ( InItem.IsValid() )
	{
		return SNew(SEventWidgetItem, OwnerTable, InItem);
	}
	else
	{
		return
		SNew(STableRow< TSharedPtr<FEventReferenceTreeItem> >, OwnerTable)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("UnknownItemType", "Unknown Item Type"))
		];
	}
}

// --------------------------------------------------------------------------------------

TSharedRef<IPropertyTypeCustomization> FEventCreationWidgetHelperDetails::MakeInstance()
{
	return MakeShareable(new FEventCreationWidgetHelperDetails());
}

void FEventCreationWidgetHelperDetails::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{

}

void FEventCreationWidgetHelperDetails::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	FString FilterString = UEventsManager::Get().GetCategoriesMetaFromPropertyHandle(StructPropertyHandle);
	const float MaxPropertyWidth = 480.0f;
	const float MaxPropertyHeight = 240.0f;

	StructBuilder.AddCustomRow( LOCTEXT("NewTag", "NewTag") )
	.ValueContent()
	.MaxDesiredWidth(MaxPropertyWidth)
	[
		SAssignNew(TagWidget, SEventWidget, TArray<SEventWidget::FEditableEventContainerDatum>())
		.Filter(FilterString)
		.NewTagName(FilterString)
		.MultiSelect(false)
		.EventUIMode(EEventUIMode::ManagementMode)
		.MaxHeight(MaxPropertyHeight)
		.NewTagControlsInitiallyExpanded(true)
		//.OnTagChanged(this, &FEventsSettingsCustomization::OnTagChanged)
	];
	
}



#undef LOCTEXT_NAMESPACE
