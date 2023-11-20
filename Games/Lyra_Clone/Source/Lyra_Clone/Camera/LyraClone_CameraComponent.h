#pragma once
#include "Camera/CameraComponent.h"
#include "LyraClone_CameraComponent.generated.h"

class ULyraClone_CameraMode;
class ULyraClone_CameraModeStack;
template <class TClass>class TSubclassOf;

DECLARE_DELEGATE_RetVal(TSubclassOf<ULyraClone_CameraMode>,FLyraClone_CameraModeDelegate)
UCLASS()
class ULyraClone_CameraComponent :public UCameraComponent
{
	GENERATED_BODY()
public:
	ULyraClone_CameraComponent(const FObjectInitializer& ObjectInitializer= FObjectInitializer::Get());
	
	static ULyraClone_CameraComponent* FindCameraComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<ULyraClone_CameraComponent>() : nullptr); }

	virtual void OnRegister()final;
	virtual void GetCameraView(float DeltaTime, OUT FMinimalViewInfo& DesiredView)final;

	void UpdateCameraModes();

	UPROPERTY()
	TObjectPtr<ULyraClone_CameraModeStack>CameraModeStack;

	FLyraClone_CameraModeDelegate DetermineCameraModeDelegate;
		
};