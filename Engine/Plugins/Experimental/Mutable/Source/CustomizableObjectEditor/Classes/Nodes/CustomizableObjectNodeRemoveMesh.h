// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/CustomizableObjectNodeEditMaterialBase.h"

#include "CustomizableObjectNodeRemoveMesh.generated.h"

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeRemoveMesh : public UCustomizableObjectNodeEditMaterialBase
{
public:
	GENERATED_BODY()

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	void PinConnectionListChanged(UEdGraphPin * Pin);

	UEdGraphPin* RemoveMeshPin() const
	{
		return FindPin(TEXT("Remove Mesh"));
	}

	bool IsSingleOutputNode() const override;
};
