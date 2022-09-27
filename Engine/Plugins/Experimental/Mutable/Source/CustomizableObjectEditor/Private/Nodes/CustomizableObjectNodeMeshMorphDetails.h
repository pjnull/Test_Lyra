// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Types/SlateEnums.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "PropertyHandle.h"

class FCustomizableObjectNodeMeshMorphDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:

	class UCustomizableObjectNodeMeshMorph* Node = nullptr;
	TArray< TSharedPtr<FString> > MorphTargetComboOptions;

	void OnMorphTargetComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> Property);
};