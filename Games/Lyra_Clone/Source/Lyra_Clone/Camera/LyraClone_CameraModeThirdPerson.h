#pragma once
#include "LyraClone_CameraMode.h"
#include "LyraClone_CameraModeThirdPerson.generated.h"

class UCurveVector;

UCLASS(Abstract,Blueprintable)
class ULyraClone_CameraModeThirdPerson :public ULyraClone_CameraMode
{
	GENERATED_BODY()
public:
	ULyraClone_CameraModeThirdPerson(const FObjectInitializer& ObjectInitializer= FObjectInitializer::Get());
	
	virtual void UpdateView(float DeltaTime)override;

	UPROPERTY(EditDefaultsOnly,Category="Third Person")
	TObjectPtr<const UCurveVector>TargetOffsetCurve;
};