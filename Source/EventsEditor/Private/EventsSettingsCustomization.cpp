// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#include "EventsSettingsCustomization.h"
#include "EventsSettings.h"
#include "EventsRuntimeModule.h"
#include "PropertyHandle.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "FEventsSettingsCustomization"

TSharedRef<IDetailCustomization> FEventsSettingsCustomization::MakeInstance()
{
	return MakeShareable( new FEventsSettingsCustomization() );
}

FEventsSettingsCustomization::FEventsSettingsCustomization()
{
	IEventsModule::OnTagSettingsChanged.AddRaw(this, &FEventsSettingsCustomization::OnTagTreeChanged);
}

FEventsSettingsCustomization::~FEventsSettingsCustomization()
{
	IEventsModule::OnTagSettingsChanged.RemoveAll(this);
}

void FEventsSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const float MaxPropertyWidth = 480.0f;
	const float MaxPropertyHeight = 240.0f;

	IDetailCategoryBuilder& EventsCategory = DetailLayout.EditCategory("Events");
	{
		TArray<TSharedRef<IPropertyHandle>> EventsProperties;
		EventsCategory.GetDefaultProperties(EventsProperties, true, true);

		TSharedPtr<IPropertyHandle> TagListProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UEventsList, EventList), UEventsList::StaticClass());
		TagListProperty->MarkHiddenByCustomization();

		for (TSharedPtr<IPropertyHandle> Property : EventsProperties)
		{
			if (Property->GetProperty() != TagListProperty->GetProperty())
			{
				EventsCategory.AddProperty(Property);
			}
			else
			{
				// Create a custom widget for the tag list

				EventsCategory.AddCustomRow(TagListProperty->GetPropertyDisplayName(), false)
				.NameContent()
				[
					TagListProperty->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SAssignNew(TagWidget, SEventWidget, TArray<SEventWidget::FEditableEventContainerDatum>())
					.Filter(TEXT(""))
					.MultiSelect(false)
					.EventUIMode(EEventUIMode::ManagementMode)
					.MaxHeight(MaxPropertyHeight)
					.OnTagChanged(this, &FEventsSettingsCustomization::OnTagChanged)
					.RestrictedTags(false)
				];
			}
		}
	}

	IDetailCategoryBuilder& AdvancedEventsCategory = DetailLayout.EditCategory("Advanced Events");
	{
		TArray<TSharedRef<IPropertyHandle>> EventsProperties;
		AdvancedEventsCategory.GetDefaultProperties(EventsProperties, true, true);

		TSharedPtr<IPropertyHandle> RestrictedTagListProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UEventsSettings, RestrictedTagList));
		RestrictedTagListProperty->MarkHiddenByCustomization();

		for (TSharedPtr<IPropertyHandle> Property : EventsProperties)
		{
			if (Property->GetProperty() == RestrictedTagListProperty->GetProperty())
			{
				// Create a custom widget for the restricted tag list

				AdvancedEventsCategory.AddCustomRow(RestrictedTagListProperty->GetPropertyDisplayName(), true)
				.NameContent()
				[
					RestrictedTagListProperty->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SAssignNew(RestrictedTagWidget, SEventWidget, TArray<SEventWidget::FEditableEventContainerDatum>())
					.Filter(TEXT(""))
					.MultiSelect(false)
					.EventUIMode(EEventUIMode::ManagementMode)
					.MaxHeight(MaxPropertyHeight)
					.OnTagChanged(this, &FEventsSettingsCustomization::OnTagChanged)
					.RestrictedTags(true)
				];
			}
			else
			{
				AdvancedEventsCategory.AddProperty(Property);
			}
		}
	}
}

void FEventsSettingsCustomization::OnTagChanged()
{
	if (TagWidget.IsValid())
	{
		TagWidget->RefreshTags();
	}

	if (RestrictedTagWidget.IsValid())
	{
		RestrictedTagWidget->RefreshTags();
	}
}

void FEventsSettingsCustomization::OnTagTreeChanged()
{
	if (TagWidget.IsValid())
	{
		TagWidget->RefreshOnNextTick();
	}

	if (RestrictedTagWidget.IsValid())
	{
		RestrictedTagWidget->RefreshOnNextTick();
	}
}

#undef LOCTEXT_NAMESPACE
