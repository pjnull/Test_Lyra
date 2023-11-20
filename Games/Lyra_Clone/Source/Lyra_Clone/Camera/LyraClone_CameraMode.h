#pragma once

#include "LyraClone_CameraMode.generated.h"

UCLASS(Abstract,NotBlueprintable)
class ULyraClone_CameraMode :public UObject
{
	GENERATED_BODY()
public:
	ULyraClone_CameraMode(const FObjectInitializer& ObejctInitializer = FObjectInitializer::Get());
	
};


UCLASS()
class ULyraClone_CameraModeStack :public UObject
{
	GENERATED_BODY()
public:
	ULyraClone_CameraModeStack(const FObjectInitializer& ObejctInitializer = FObjectInitializer::Get());
	
	UPROPERTY()
	TArray<TObjectPtr<ULyraClone_CameraMode>>CameraModeInstance;

	UPROPERTY()
	TArray<TObjectPtr<ULyraClone_CameraMode>>CameraModeStack;
};