// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventSearchFilter.h"
#include "Framework/Commands/UIAction.h"
#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "EventContainer.h"
#include "EventsManager.h"
#include "SEventWidget.h"
#include "IContentBrowserSingleton.h"
#include "UObject/NoExportTypes.h"


#define LOCTEXT_NAMESPACE "EventSearchFilter"

//////////////////////////////////////////////////////////////////////////
//

/** A filter that search for assets using a specific gameplay tag */
class FFrontendFilter_Events : public FFrontendFilter
{
public:
	FFrontendFilter_Events(TSharedPtr<FFrontendFilterCategory> InCategory)
		: FFrontendFilter(InCategory)
	{
		TagContainer = MakeShareable(new FEventContainer);

		EditableContainers.Add(SEventWidget::FEditableEventContainerDatum(/*TagContainerOwner=*/ nullptr, TagContainer.Get()));
	}

	// FFrontendFilter implementation
	virtual FLinearColor GetColor() const override { return FLinearColor::Red; }
	virtual FString GetName() const override { return TEXT("EventFilter"); }
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTipText() const override;
	virtual void ModifyContextMenu(FMenuBuilder& MenuBuilder) override;
	virtual void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const override;
	virtual void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) override;
	// End of FFrontendFilter implementation

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;
	// End of IFilter implementation

protected:
	// Container of selected search tags (the asset is shown if *any* of these match)
	TSharedPtr<FEventContainer> TagContainer;

	// Adaptor for the SEventWidget to edit our tag container
	TArray<SEventWidget::FEditableEventContainerDatum> EditableContainers;

protected:
	bool ProcessStruct(void* Data, UStruct* Struct) const;

	bool ProcessProperty(void* Data, FProperty* Prop) const;

	void OnTagWidgetChanged();
};

void FFrontendFilter_Events::ModifyContextMenu(FMenuBuilder& MenuBuilder)
{
	FUIAction Action;

	MenuBuilder.BeginSection(TEXT("ComparsionSection"), LOCTEXT("ComparisonSectionHeading", "Event(s) to search for"));

	TSharedRef<SWidget> TagWidget =
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300)
		[
			SNew(SEventWidget, EditableContainers)
			.MultiSelect(true)
			.OnTagChanged_Raw(this, &FFrontendFilter_Events::OnTagWidgetChanged)
		];
 	MenuBuilder.AddWidget(TagWidget, FText::GetEmpty(), /*bNoIndent=*/ false);
}

FText FFrontendFilter_Events::GetDisplayName() const
{
	if (TagContainer->Num() == 0)
	{
		return LOCTEXT("AnyEventDisplayName", "Events");
	}
	else
	{
		FString QueryString;

		int32 Count = 0;
		for (const FEventInfo& Tag : *TagContainer.Get())
		{
			if (Count > 0)
			{
				QueryString += TEXT(" | ");
			}

			QueryString += Tag.ToString();
			++Count;
		}


		return FText::Format(LOCTEXT("EventListDisplayName", "Events ({0})"), FText::AsCultureInvariant(QueryString));
	}
}

FText FFrontendFilter_Events::GetToolTipText() const
{
	if (TagContainer->Num() == 0)
	{
		return LOCTEXT("AnyEventFilterDisplayTooltip", "Search for any *loaded* Blueprint or asset that contains an event (right-click to choose events).");
	}
	else
	{
		return LOCTEXT("EventFilterDisplayTooltip", "Search for any *loaded* Blueprint or asset that has an event which matches any of the selected events (right-click to choose events).");
	}
}

void FFrontendFilter_Events::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	TArray<FString> TagStrings;
	TagStrings.Reserve(TagContainer->Num());
	for (const FEventInfo& Tag : *TagContainer.Get())
	{
		TagStrings.Add(Tag.GetTagName().ToString());
	}

	GConfig->SetArray(*IniSection, *(SettingsString + TEXT(".Tags")), TagStrings, IniFilename);
}

void FFrontendFilter_Events::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	UEventsManager& Manager = UEventsManager::Get();

	TArray<FString> TagStrings;
	GConfig->GetArray(*IniSection, *(SettingsString + TEXT(".Tags")), /*out*/ TagStrings, IniFilename);

	TagContainer->Reset();
	for (const FString& TagString : TagStrings)
	{
		FEventInfo NewTag = Manager.RequestEvent(*TagString, /*bErrorIfNotFound=*/ false);
		if (NewTag.IsValid())
		{
			TagContainer->AddTag(NewTag);
		}
	}
}

void FFrontendFilter_Events::OnTagWidgetChanged()
{
	BroadcastChangedEvent();
}

bool FFrontendFilter_Events::ProcessStruct(void* Data, UStruct* Struct) const
{
	for (TFieldIterator<FProperty> PropIt(Struct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;

		if (ProcessProperty(Data, Prop))
		{
			return true;
		}
	}

	return false;
}

bool FFrontendFilter_Events::ProcessProperty(void* Data, FProperty* Prop) const
{
	void* InnerData = Prop->ContainerPtrToValuePtr<void>(Data);

	if (FStructProperty* StructProperty = CastField<FStructProperty>(Prop))
	{
		if (StructProperty->Struct == FEventInfo::StaticStruct())
		{
			FEventInfo& ThisTag = *static_cast<FEventInfo*>(InnerData);

			const bool bAnyTagIsOK = TagContainer->Num() == 0;
			const bool bPassesTagSearch = bAnyTagIsOK || ThisTag.MatchesAny(*TagContainer);

			return bPassesTagSearch;
		}
		else
		{
			return ProcessStruct(InnerData, StructProperty->Struct);
		}
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Prop))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, InnerData);
		for (int32 ArrayIndex = 0; ArrayIndex < ArrayHelper.Num(); ++ArrayIndex)
		{
			void* ArrayData = ArrayHelper.GetRawPtr(ArrayIndex);

			if (ProcessProperty(ArrayData, ArrayProperty->Inner))
			{
				return true;
			}
		}
	}

	return false;
}

bool FFrontendFilter_Events::PassesFilter(FAssetFilterType InItem) const
{
	FAssetData ItemAssetData;
	// FIX (blowpunch)
	//if (InItem.Legacy_TryGetAssetData(ItemAssetData)) // UE 4.26
	if (1)
	///
	{
		if (UObject* Object = ItemAssetData.FastGetAsset(false))
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
			{
				return ProcessStruct(Blueprint->GeneratedClass->GetDefaultObject(), Blueprint->GeneratedClass);

				//@TODO: Check blueprint bytecode!
			}
			else if (UClass* Class = Cast<UClass>(Object))
			{
				return ProcessStruct(Class->GetDefaultObject(), Class);
			}
			else
			{
				return ProcessStruct(Object, Object->GetClass());
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
//

void UEventSearchFilter::AddFrontEndFilterExtensions(TSharedPtr<FFrontendFilterCategory> DefaultCategory, TArray< TSharedRef<class FFrontendFilter> >& InOutFilterList) const
{
	InOutFilterList.Add(MakeShareable(new FFrontendFilter_Events(DefaultCategory)));
}

#undef LOCTEXT_NAMESPACE
