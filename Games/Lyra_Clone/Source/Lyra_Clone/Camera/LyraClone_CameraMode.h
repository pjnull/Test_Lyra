#pragma once

#include "LyraClone_CameraMode.generated.h"

class ULyraClone_CameraComponent;

UENUM(BlueprintType)
enum class ECloneCameraModeBlendFunction :uint8
{
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut,
	COUNT,
};
struct FLyraClone_CameraModeView
{
	FLyraClone_CameraModeView();

	void Blend(const FLyraClone_CameraModeView&other,float Otherweight);
	
	FVector Location;
	FRotator Rotation;
	FRotator ControlRotation;
	float FieldOfView;
};

UCLASS(Abstract,NotBlueprintable)
class ULyraClone_CameraMode :public UObject
{
	GENERATED_BODY()
public:
	ULyraClone_CameraMode(const FObjectInitializer& ObejctInitializer = FObjectInitializer::Get());

	
	void UpdateCameraMode(float DeltaTime);
	void UpdateBlend(float DeltaTime);
	FVector GetPivotLocation()const;
	FRotator GetPivotRotation()const;
	AActor* GetTargetActor()const;
	ULyraClone_CameraComponent* GetCloneCameraComponent()const;
	
	virtual void UpdateView(float DeltaTime);



	UPROPERTY(EditDefaultsOnly,Category="View",Meta=(UIMin="5.0",UIMax="170",ClampMin="5.0",Clampmax="170"))
	float FieldOfView;
	
	UPROPERTY(EditDefaultsOnly,Category="View",Meta=(UIMin="-89.9",UIMax="89.9",ClampMin="-89.9",Clampmax="89.9"))
	float ViewPitchMin;

	UPROPERTY(EditDefaultsOnly,Category="View",Meta=(UIMin="-89.9",UIMax="89.9",ClampMin="-89.9",Clampmax="89.9"))
	float ViewPitchMax;


	UPROPERTY(EditDefaultsOnly,Category="Blending")
	float BlendTime;//블랜딩을 실행할 시간

	float BlendAlpha;//선형적인 블랜딩값[0,1]

	float BlendWeight;
	//해당 카메라모드의 최종 블랜딩값 BlendAlpha를 매핑하여 최종 BlendWeight를 계산
	UPROPERTY(EditDefaultsOnly,Category="Blending")
	float BlendExponent;

	ECloneCameraModeBlendFunction BlendFunction;
	
	FLyraClone_CameraModeView View;
	//CameraMode에 의해 생성된 CameraView캐싱


};


UCLASS()
class ULyraClone_CameraModeStack :public UObject
{
	GENERATED_BODY()
public:
	ULyraClone_CameraModeStack(const FObjectInitializer& ObejctInitializer = FObjectInitializer::Get());
	
	ULyraClone_CameraMode* GetCameraModeInstance(TSubclassOf<ULyraClone_CameraMode>& CameraModeClass);
	void PushCameraMode(TSubclassOf<ULyraClone_CameraMode>& CameraModeClass);
	void UpdateStack(float DeltaTime);
	void BlendStack(FLyraClone_CameraModeView& OutCameraModeView)const ;
	void EvaluateStack(float DeltaTime, FLyraClone_CameraModeView& OutCameraModeView);

	UPROPERTY()
	TArray<TObjectPtr<ULyraClone_CameraMode>>CameraModeInstance;

	UPROPERTY()
	TArray<TObjectPtr<ULyraClone_CameraMode>>CameraModeStack;
};