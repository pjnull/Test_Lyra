// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_SkeletalControlBase.generated.h"

struct HActor;

UCLASS(Abstract, MinimalAPI)
class UAnimGraphNode_SkeletalControlBase : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

public:
	// UEdGraphNode interface
	ANIMGRAPH_API virtual FLinearColor GetNodeTitleColor() const override;
	ANIMGRAPH_API virtual FText GetTooltipText() const override;
	// End of UEdGraphNode interface

	// UAnimGraphNode_Base interface
	ANIMGRAPH_API virtual FString GetNodeCategory() const override;
	ANIMGRAPH_API virtual void CreateOutputPins() override;
	// End of UAnimGraphNode_Base interface

	// Draw function for supporting visualization
	ANIMGRAPH_API virtual void Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent * PreviewSkelMeshComp) const {};

	/**
	 * methods related to widget control
	 */
	ANIMGRAPH_API virtual FVector GetWidgetLocation(const USkeletalMeshComponent* SkelComp, struct FAnimNode_SkeletalControlBase* AnimNode)
	{
		return FVector::ZeroVector;
	}
	// to keep data consistency between anim nodes 
	ANIMGRAPH_API virtual void	CopyNodeDataTo(FAnimNode_Base* OutAnimNode){}
	ANIMGRAPH_API virtual void	CopyNodeDataFrom(const FAnimNode_Base* NewAnimNode){}

	/** Are we currently showing this pin */
	ANIMGRAPH_API bool IsPinShown(const FString& PinName) const;

	// return current widget mode this anim graph node supports
	ANIMGRAPH_API virtual int32 GetWidgetMode(const USkeletalMeshComponent* SkelComp);
	// called when the user changed widget mode by pressing "Space" key
	ANIMGRAPH_API virtual int32 ChangeToNextWidgetMode(const USkeletalMeshComponent* SkelComp, int32 CurWidgetMode);
	// called when the user set widget mode directly, returns true if InWidgetMode is available
	ANIMGRAPH_API virtual bool SetWidgetMode(const USkeletalMeshComponent* SkelComp, int32 InWidgetMode){ return false; }

	// 
	ANIMGRAPH_API virtual FName FindSelectedBone();

	// if anim graph node needs other actors to select other bones, move actor's positions when this is called
	ANIMGRAPH_API virtual void MoveSelectActorLocation(const USkeletalMeshComponent* SkelComp, FAnimNode_SkeletalControlBase* InAnimNode){}

	ANIMGRAPH_API virtual bool IsActorClicked(HActor* ActorHitProxy){ return false; }
	ANIMGRAPH_API virtual void ProcessActorClick(HActor* ActorHitProxy){}
	// if it has select-actors, should hide all actors when de-select is called  
	ANIMGRAPH_API virtual void	DeselectActor(USkeletalMeshComponent* SkelComp){}

	// called when the widget is dragged in translation mode
	ANIMGRAPH_API virtual void DoTranslation(const USkeletalMeshComponent* SkelComp, FVector& Drag, FAnimNode_Base* InOutAnimNode){}
	// called when the widget is dragged in rotation mode
	ANIMGRAPH_API virtual void DoRotation(const USkeletalMeshComponent* SkelComp, FRotator& Rotation, FAnimNode_Base* InOutAnimNode){}
	// called when the widget is dragged in scale mode
	ANIMGRAPH_API virtual void DoScale(const USkeletalMeshComponent* SkelComp, FVector& Scale, FAnimNode_Base* InOutAnimNode){}

protected:
	// Returns the short descriptive name of the controller
	ANIMGRAPH_API virtual FText GetControllerDescription() const;

	/**
	* helper functions for bone control preview
	*/
	// local conversion function for drawing
	void ConvertToComponentSpaceTransform(const USkeletalMeshComponent* SkelComp, const FTransform & InTransform, FTransform & OutCSTransform, int32 BoneIndex, EBoneControlSpace Space) const;
	// convert drag vector in component space to bone space 
	FVector ConvertCSVectorToBoneSpace(const USkeletalMeshComponent* SkelComp, FVector& InCSVector, FCSPose<FCompactPose>& MeshBases, const FName& BoneName, const EBoneControlSpace Space);
	// convert rotator in component space to bone space 
	FQuat ConvertCSRotationToBoneSpace(const USkeletalMeshComponent* SkelComp, FRotator& InCSRotator, FCSPose<FCompactPose>& MeshBases, const FName& BoneName, const EBoneControlSpace Space);
	// convert widget location according to bone control space
	FVector ConvertWidgetLocation(const USkeletalMeshComponent* InSkelComp, FCSPose<FCompactPose>& InMeshBases, const FName& BoneName, const FVector& InLocation, const EBoneControlSpace Space);
	// set literal value for FVector
	void SetDefaultValue(const FString& InDefaultValueName, const FVector& InValue);
	// get literal value for vector
	void GetDefaultValue(const FString& UpdateDefaultValueName, FVector& OutVec);

	void GetDefaultValue(const FString& PropName, FRotator& OutValue)
	{
		FVector Value;
		GetDefaultValue(PropName, Value);
		OutValue.Pitch = Value.X;
		OutValue.Yaw = Value.Y;
		OutValue.Roll = Value.Z;
	}

	template<class ValueType>
	ValueType GetNodeValue(const FString& PropName, const ValueType& CompileNodeValue)
	{
		if (IsPinShown(PropName))
		{
			ValueType Val;
			GetDefaultValue(PropName, Val);
			return Val;
		}
		return CompileNodeValue;
	}

	void SetDefaultValue(const FString& PropName, const FRotator& InValue)
	{
		FVector VecValue(InValue.Pitch, InValue.Yaw, InValue.Roll);
		SetDefaultValue(PropName, VecValue);
	}

	template<class ValueType>
	void SetNodeValue(const FString& PropName, ValueType& CompileNodeValue, const ValueType& InValue)
	{
		if (IsPinShown(PropName))
		{
			SetDefaultValue(PropName, InValue);
		}
		CompileNodeValue = InValue;
	}
};
