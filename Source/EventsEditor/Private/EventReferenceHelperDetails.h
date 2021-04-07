// Copyright 2019 - 2021, butterfly, Event System Plugin, All Rights Reserved.

#pragma once

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "AssetData.h"
#include "DetailWidgetRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "PropertyHandle.h"

class IPropertyHandle;

class FEventReferenceHelperDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              

private:

	struct FEventReferenceTreeItem : public TSharedFromThis<FEventReferenceTreeItem>
	{
		FEventReferenceTreeItem()
		{
		}

		FName EventName;
		FAssetIdentifier AssetIdentifier;	
	};

	typedef STreeView< TSharedPtr< FEventReferenceTreeItem > > SEventReferenceTree;

	TArray< TSharedPtr<FEventReferenceTreeItem> > TreeItems;

	TSharedRef<ITableRow> OnGenerateWidgetForGameplayCueListView(TSharedPtr< FEventReferenceTreeItem > InItem, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<IPropertyHandle> StatBackendNameProp;

	TSharedPtr<class IPropertyHandle> PropertyHandle;

	struct FEventReferenceHelper* GetValue();
};

class FEventCreationWidgetHelperDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              
	TSharedPtr<class SEventWidget> TagWidget;
};