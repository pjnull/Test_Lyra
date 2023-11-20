#pragma once
#include "Camera/PlayerCameraManager.h"
#include "LyraClone_PlayerCameraManager.generated.h"

#define CLONE_CAMERA_DEFAULT_FOV (80.f)
#define CLONE_CAMERA_DEFAULT_PITCH_MIN (-89.f)
#define CLONE_CAMERA_DEFAULT_MAX (89.f)

UCLASS()
class ALyraClone_PlayerCameraManager :public APlayerCameraManager
{
	GENERATED_BODY()
public:
	ALyraClone_PlayerCameraManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};