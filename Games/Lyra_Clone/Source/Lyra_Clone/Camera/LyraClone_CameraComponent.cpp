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

}

void ULyraClone_CameraComponent::UpdateCameraModes()
{
	check(CameraModeStack);

	if (DetermineCameraModeDelegate.IsBound())
	{
		if (const TSubclassOf<ULyraClone_CameraMode>CameraMode = DetermineCameraModeDelegate.Execute())
		{
			CameraModeStack->PushCameraMode(CameraMode);
		}
	}
}
