// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/CustomizableObjectNodeObject.h"

#include "CustomizableObjectNodeObjectChild.generated.h"


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeObjectChild : public UCustomizableObjectNodeObject
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeObjectChild();

public:
	// UEdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	void PrepareForCopying() override;
	FText GetTooltipText() const override;
	bool CanUserDeleteNode() const override;
	bool CanDuplicateNode() const override;
};
