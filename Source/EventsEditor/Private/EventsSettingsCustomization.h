// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editor/PropertyEditor/Public/IDetailCustomization.h"
#include "SEventWidget.h"

class IDetailLayoutBuilder;

//////////////////////////////////////////////////////////////////////////
// FEventsSettingsCustomization

class FEventsSettingsCustomization : public IDetailCustomization
{
public:
	FEventsSettingsCustomization();
	virtual ~FEventsSettingsCustomization();

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:

	/** Callback for when a tag changes */
	void OnTagChanged();

	/** Module callback for when the tag tree changes */
	void OnTagTreeChanged();

	TSharedPtr<SEventWidget> TagWidget;

	TSharedPtr<SEventWidget> RestrictedTagWidget;
};
