// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"


/** Base asset type actions for any classes with gameplay tagging */
class EVENTSEDITOR_API FAssetTypeActions_EventAssetBase : public FAssetTypeActions_Base
{
public:

	/** Constructor */
	FAssetTypeActions_EventAssetBase(FName InTagPropertyName);

	/** Overridden to specify that the gameplay tag base has actions */
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;

	/** Overridden to offer the gameplay tagging options */
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;

	/** Overridden to specify misc category */
	virtual uint32 GetCategories() override;

private:
	/**
	 * Open the gameplay tag editor
	 * 
	 * @param TagAssets	Assets to open the editor with
	 */
	void OpenEventEditor(TArray<class UObject*> Objects, TArray<struct FEventContainer*> Containers);

	/** Name of the property of the owned gameplay tag container */
	FName OwnedEventPropertyName;
};
