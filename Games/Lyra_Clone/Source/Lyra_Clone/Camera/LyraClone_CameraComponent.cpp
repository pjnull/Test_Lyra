#include "LyraClone_CameraComponent.h"
#include "LyraClone_CameraMode.h"
#include "../Character/Lyra_Clone_HeroComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_CameraComponent)
ULyraClone_CameraComponent::ULyraClone_CameraComponent(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer),CameraModeStack(nullptr)
{
}

void ULyraClone_CameraComponent::OnRegister()
{
	Super::OnRegister();
	if (!CameraModeStack)
	{
		CameraModeStack = NewObject<ULyraClone_CameraModeStack>(this);
	}
}


void ULyraClone_CameraComponent::GetCameraView(float DeltaTime,OUT FMinimalViewInfo& DesiredView)
{
	//Camera의 Tick의 역할을 수행
	check(CameraModeStack);

	UpdateCameraModes();

	FLyraClone_CameraModeView CameraModeView;
	CameraModeStack->EvaluateStack(DeltaTime,CameraModeView);

	if (APawn* TargetPawn = Cast<APawn>(GetTargetActor()))
	{
		if (APlayerController* PC = TargetPawn->GetController<APlayerController>())
		{
			PC->SetControlRotation(CameraModeView.ControlRotation);
		}
	}

	SetWorldLocationAndRotation(CameraModeView.Location,CameraModeView.Rotation);

	FieldOfView = CameraModeView.FieldOfView;

	DesiredView.Location = CameraModeView.Location;
	DesiredView.Rotation = CameraModeView.Rotation;
	DesiredView.FOV = CameraModeView.FieldOfView;
	DesiredView.OrthoWidth = OrthoWidth;
	DesiredView.OrthoNearClipPlane = OrthoNearClipPlane;
	DesiredView.OrthoFarClipPlane = OrthoFarClipPlane;
	DesiredView.AspectRatio = AspectRatio;
	DesiredView.bConstrainAspectRatio = bConstrainAspectRatio;
	DesiredView.bUseFieldOfViewForLOD = bUseFieldOfViewForLOD;
	DesiredView.ProjectionMode = ProjectionMode;
	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}
}


void ULyraClone_CameraComponent::UpdateCameraModes()
{
	check(CameraModeStack);

	if (DetermineCameraModeDelegate.IsBound())
	{
		if (TSubclassOf<ULyraClone_CameraMode>CameraMode = DetermineCameraModeDelegate.Execute())
		{
			CameraModeStack->PushCameraMode(CameraMode);
		}
	}
}
