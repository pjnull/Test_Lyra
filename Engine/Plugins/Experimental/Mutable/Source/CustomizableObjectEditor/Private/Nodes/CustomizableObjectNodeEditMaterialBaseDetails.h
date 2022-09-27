// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CustomizableObjectNodeParentedMaterialDetails.h"

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "Misc/Guid.h"
#include "Types/SlateEnums.h"
#include "PropertyHandle.h"
#include "CustomizableObjectEditorModule.h"
#include "Nodes/CustomizableObjectNodeEditMaterialBase.h"

class FCustomizableObjectNodeEditMaterialBaseDetails : public FCustomizableObjectNodeParentedMaterialDetails
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// ILayoutDetails interface
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	// FCustomizableObjectNodeParentMaterialDetails interface
protected:
	void OnParentComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ParentProperty) override;

private:
	TSharedPtr<STextComboBox> LayoutComboBox;
	
	TArray<int> LayoutOptionReferences;
	TArray<TSharedPtr<FString>> LayoutOptionNames;
	
	UCustomizableObjectNodeEditMaterialBase* NodeEditMaterialBase;

	// Prepares combobox options for the Layout
	TSharedPtr<FString> GenerateLayoutComboboxOptions();

	// Callback when we select a different Layout
	void OnLayoutComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> LayoutProperty);
};