#pragma once
#include "LyraClone_CameraMode.h"
#include "LyraClone_CameraModeThirdPerson.generated.h"

UCLASS(Abstract,Blueprintable)
class ULyraClone_CameraModeThirdPerson :public ULyraClone_CameraMode
{
	GENERATED_BODY()
public:
	ULyraClone_CameraModeThirdPerson(const FObjectInitializer& ObjectInitializer= FObjectInitializer::Get());
	
};