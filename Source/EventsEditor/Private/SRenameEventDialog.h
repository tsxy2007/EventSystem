// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EventsManager.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SEditableTextBox.h"

class SRenameEventDialog : public SCompoundWidget
{
public:

	DECLARE_DELEGATE_TwoParams( FOnEventRenamed, FString/* OldName */, FString/* NewName */);

	SLATE_BEGIN_ARGS( SRenameEventDialog )
		: _EventNode()
		{}
		SLATE_ARGUMENT( TSharedPtr<FEventNode>, EventNode )		// The gameplay tag we want to rename
		SLATE_EVENT( FOnEventRenamed, OnEventRenamed )	// Called when the tag is renamed
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	/** Checks if we're in a valid state to rename the tag */
	bool IsRenameEnabled() const;

	/** Renames the tag based on dialog parameters */
	FReply OnRenameClicked();

	/** Callback for when Cancel is clicked */
	FReply OnCancelClicked();

	/** Renames the tag and attempts to close the active window */
	void RenameAndClose();

	/** Attempts to rename the tag if enter is pressed while editing the tag name */
	void OnRenameTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	/** Closes the window that contains this widget */
	void CloseContainingWindow();

private:

	TSharedPtr<FEventNode> EventNode;

	TSharedPtr<SEditableTextBox> NewTagNameTextBox;

	FOnEventRenamed OnEventRenamed;
};
