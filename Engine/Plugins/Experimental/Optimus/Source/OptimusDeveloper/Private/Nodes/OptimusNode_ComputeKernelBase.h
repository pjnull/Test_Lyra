// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusComputeKernelProvider.h"
#include "OptimusNode.h"

#include "OptimusNode_ComputeKernelBase.generated.h"

UCLASS(Abstract)
class UOptimusNode_ComputeKernelBase :
	public UOptimusNode, 
	public IOptimusComputeKernelProvider
{
	GENERATED_BODY()
	
public:
	/** Implement this to return the HLSL kernel's function name */
	virtual FString GetKernelName() const PURE_VIRTUAL(UOptimusNode_ComputeKernelBase::GetKernelName, return FString();)

	/** Implement this to return the complete HLSL code for this kernel */
	virtual FString GetKernelSourceText() const PURE_VIRTUAL(UOptimusNode_ComputeKernelBase::GetKernelSourceText, return FString();)
	
	// IOptimusComputeKernelProvider
	UOptimusKernelSource* CreateComputeKernel(
		UObject* InKernelSourceOuter,
		const FOptimus_NodeToDataInterfaceMap& InNodeDataInterfaceMap,
		const FOptimus_PinToDataInterfaceMap& InLinkDataInterfaceMap,
		const TSet<const UOptimusNode *>& InValueNodeSet,
		FOptimus_KernelParameterBindingList& OutParameterBindings,
		FOptimus_InterfaceBindingMap& OutInputDataBindings,
		FOptimus_InterfaceBindingMap& OutOutputDataBindings
		) const override;	

protected:
	static TArray<FString> GetIndexNamesFromDataDomainLevels(
		const TArray<FName> &InLevelNames
		)
	{
		TArray<FString> IndexNames;
	
		for (FName DomainName: InLevelNames)
		{
			IndexNames.Add(FString::Printf(TEXT("%sIndex"), *DomainName.ToString()));
		}
		return IndexNames;
	}
	
private:
	void ProcessInputPinForComputeKernel(
		const UOptimusNodePin* InInputPin,
		const UOptimusNodePin* InOutputPin,
		const FOptimus_NodeToDataInterfaceMap& InNodeDataInterfaceMap,
		const FOptimus_PinToDataInterfaceMap& InLinkDataInterfaceMap,
		const TSet<const UOptimusNode *>& InValueNodeSet,
		UOptimusKernelSource* InKernelSource,
		TArray<FString>& OutGeneratedFunctions,
		FOptimus_KernelParameterBindingList& OutParameterBindings,
		FOptimus_InterfaceBindingMap& OutInputDataBindings
		) const;

	void ProcessOutputPinForComputeKernel(
		const UOptimusNodePin* InOutputPin,
		const TArray<UOptimusNodePin *>& InInputPins,
		const FOptimus_NodeToDataInterfaceMap& InNodeDataInterfaceMap,
		const FOptimus_PinToDataInterfaceMap& InLinkDataInterfaceMap,
		UOptimusKernelSource* InKernelSource,
		TArray<FString>& OutGeneratedFunctions,
		FOptimus_InterfaceBindingMap& OutOutputDataBindings
		) const;

};

