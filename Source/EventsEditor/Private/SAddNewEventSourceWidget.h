// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EventsManager.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"

/** Widget allowing the user to create new gameplay tags */
class SAddNewEventSourceWidget : public SCompoundWidget
{
public:

	DECLARE_DELEGATE_OneParam( FOnEventSourceAdded, const FString& /*SourceName*/);

	SLATE_BEGIN_ARGS(SAddNewEventSourceWidget)
		: _NewSourceName(TEXT(""))
		{}
		SLATE_EVENT(FOnEventSourceAdded, OnEventSourceAdded )	// Callback for when a new source is added	
		SLATE_ARGUMENT( FString, NewSourceName ) // String that will initially populate the New Source Name field
	SLATE_END_ARGS();

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	void Construct( const FArguments& InArgs);

	/** Resets all input fields */
	void Reset();

private:

	/** Sets the name of the source. Uses the default if the name is not specified */
	void SetSourceName(const FText& InName = FText());

	/** Callback for when the Add New Tag button is pressed */
	FReply OnAddNewSourceButtonPressed();

private:

	/** The name of the next gameplay tag source to create */
	TSharedPtr<SEditableTextBox> SourceNameTextBox;

	/** Callback for when a new gameplay tag has been added to the INI files */
	FOnEventSourceAdded OnEventSourceAdded;

	/** Tracks if this widget should get keyboard focus */
	bool bShouldGetKeyboardFocus;

	FString DefaultNewName;
};
