#include "LyraClone_CameraMode.h"
#include "LyraClone_PlayerCameraManager.h"
#include "LyraClone_CameraComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_CameraMode)



FLyraClone_CameraModeView::FLyraClone_CameraModeView()
	:Location(ForceInit),
	Rotation(ForceInit),
	ControlRotation(ForceInit),
	FieldOfView(CLONE_CAMERA_DEFAULT_FOV)
{
}

void FLyraClone_CameraModeView::Blend(const FLyraClone_CameraModeView& other, float Otherweight)
{
	if (Otherweight <= 0.f)
	{
		return;
	}
	else if (Otherweight >= 1.f)
	{
		*this = other;
		return;
	}
	Location = FMath::Lerp(Location,other.Location,Otherweight);

	const FRotator DeltaRotation = (other.Rotation - Rotation).GetNormalized();
	Rotation = Rotation + (Otherweight * DeltaRotation);

	const FRotator DeltaCotrolRotation = (other.ControlRotation-ControlRotation).GetNormalized();
	ControlRotation = ControlRotation + (Otherweight* DeltaCotrolRotation);
	
	FieldOfView = FMath::Lerp(FieldOfView,other.FieldOfView,Otherweight);
}


ULyraClone_CameraMode::ULyraClone_CameraMode(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer)
{
	FieldOfView = CLONE_CAMERA_DEFAULT_FOV;
	ViewPitchMin = CLONE_CAMERA_DEFAULT_PITCH_MIN;
	ViewPitchMax = CLONE_CAMERA_DEFAULT_PITCH_MAX;

	BlendTime = 0.f;
	BlendAlpha = 1.f;
	BlendWeight = 1.f;

	BlendFunction = ECloneCameraModeBlendFunction::EaseOut;
	BlendExponent = 4.0f;

}

void ULyraClone_CameraMode::UpdateCameraMode(float DeltaTime)
{
	UpdateView(DeltaTime);//Actor를 활용하여 Pivot을 계산하여 View를 업데이트
	UpdateBlend(DeltaTime);
	//BlendWeight와 Deltatime을 이용하여 
	//BlendAlpha를 계산후 BlednFunction에 맞게 재매핑하여 최종계산

}

void ULyraClone_CameraMode::UpdateView(float DeltaTime)
{
	//CameraMode를 가지고 있는 CameraComponent의 Owner인 Pawn을 활용하여
	//PivotLocation/Rotation을 가져옴
	FVector PivotLocation = GetPivotLocation();
	FRotator PivotRotation = GetPivotRotation();

	//Pitch값의 Min/Max를 Clamp
	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch,ViewPitchMin,ViewPitchMax);
	
	View.Location = PivotLocation;
	View.Rotation = PivotRotation;

	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;


}

void ULyraClone_CameraMode::UpdateBlend(float DeltaTime)
{
	if (BlendTime > 0.f)
	{
		BlendAlpha += (DeltaTime / BlendTime);
	}
	else
	{
		BlendAlpha = 1.f;
	}

	const float Exponent = (BlendExponent > 0.f) ? BlendExponent : 1.0f;
	switch (BlendFunction)
	{
	case ECloneCameraModeBlendFunction::Linear:
		BlendWeight = BlendAlpha;
		break;
	case ECloneCameraModeBlendFunction::EaseIn:
		BlendWeight = FMath::InterpEaseIn(0.f,1.f,BlendAlpha,Exponent);
		break;
	case ECloneCameraModeBlendFunction::EaseOut:
		BlendWeight = FMath::InterpEaseOut(0.f, 1.f, BlendAlpha, Exponent);
		break;
	case ECloneCameraModeBlendFunction::EaseInOut:
		BlendWeight = FMath::InterpEaseInOut(0.f, 1.f, BlendAlpha, Exponent);
		break;
	default:
		break;
	}

}

FVector ULyraClone_CameraMode::GetPivotLocation() const
{
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);
	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		return TargetPawn->GetPawnViewLocation();
	}
	return TargetActor->GetActorLocation();
}

FRotator ULyraClone_CameraMode::GetPivotRotation() const
{
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);
	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		return TargetPawn->GetViewRotation();

	}
	return TargetActor->GetActorRotation();
}

AActor* ULyraClone_CameraMode::GetTargetActor() const
{
	const ULyraClone_CameraComponent* CloneCameraComponent = GetCloneCameraComponent();
	return CloneCameraComponent->GetTargetActor();
	
}

ULyraClone_CameraComponent* ULyraClone_CameraMode::GetCloneCameraComponent() const
{
	//우리가 UCameraMode을 생성하는 곳은 UCameraModeStack이다.
	//GetOuter를 CameraMode로 CameraComponent로 설정되어있다.

	return CastChecked<ULyraClone_CameraComponent>(GetOuter());
}



ULyraClone_CameraModeStack::ULyraClone_CameraModeStack(const FObjectInitializer& ObejctInitializer)
{

}

ULyraClone_CameraMode* ULyraClone_CameraModeStack::GetCameraModeInstance(TSubclassOf<ULyraClone_CameraMode>& CameraModeClass)
{
	check(CameraModeClass);

	for (ULyraClone_CameraMode* CameraMode : CameraModeInstance)
	{
		if ((CameraMode != nullptr) && (CameraMode->GetClass() == CameraModeClass))
		{
			//CameraModeClass에 알맞은 Camera 인스턴스가 있다면 반환
			return CameraMode;
		}
	}

	//CameraModeClass에 알맞은 Camera의 인스턴스가 없다면 생성
	ULyraClone_CameraMode* NewCamera = NewObject<ULyraClone_CameraMode>(GetOuter(),CameraModeClass,NAME_None,RF_NoFlags);
	check(NewCamera);

	//CameraModeInstace에 추가
	CameraModeInstance.Add(NewCamera);
	return NewCamera;
	
}

void ULyraClone_CameraModeStack::PushCameraMode(TSubclassOf<ULyraClone_CameraMode>& CameraModeClass)
{
	if (!CameraModeClass)
	{
		return;
	}

	ULyraClone_CameraMode* CameraMode = GetCameraModeInstance(CameraModeClass);
	check(CameraMode);

	int32 StackSize = CameraModeStack.Num();
	if ((StackSize > 0) && CameraModeStack[0] == CameraMode)
	{
		return;
	}

	int32 ExistingStackIndex = INDEX_NONE;
	float ExistingStackContribution = 1.0f;


	for (int32 StackIndex = 0; StackIndex < StackSize; ++StackIndex)
	{
		if (CameraModeStack[StackIndex] == CameraMode)
		{
			ExistingStackIndex = StackIndex;
			ExistingStackContribution *= CameraMode->BlendWeight;
			break;
		}

		else
		{
			ExistingStackContribution *= (1.0f-CameraModeStack[StackIndex]->BlendWeight);
		}
	}

	if (ExistingStackIndex != INDEX_NONE)
	{
		CameraModeStack.RemoveAt(ExistingStackIndex);
		StackSize--;
	}
	else
	{
		ExistingStackContribution = 0.0f;
	}

	const bool bShouldBlend = ((CameraMode->BlendTime>0.0f)&&(StackSize>0));
	const float BlendWeight = ((bShouldBlend?ExistingStackContribution:1.0f));
	CameraMode->BlendWeight = BlendWeight;

	CameraModeStack.Insert(CameraMode,0);
	CameraModeStack.Last()->BlendWeight = 1.0f;
}

void ULyraClone_CameraModeStack::UpdateStack(float DeltaTime)
{
	const int32 StackSize = CameraModeStack.Num();
	if (StackSize <= 0)
	{
		return;
	}

	int32 RemoveCount = 0;
	int32 RemoveIndex = INDEX_NONE;
	for (int32 StackIndex = 0; StackIndex < StackSize; ++StackIndex)
	{
		ULyraClone_CameraMode* CameraMode = CameraModeStack[StackIndex];
		check(CameraMode);

		CameraMode->UpdateCameraMode(DeltaTime);

		if (CameraMode->BlendWeight >= 1.0f)
		{
			RemoveIndex = (StackIndex + 1);
			RemoveCount = (StackSize - RemoveIndex);
			break;
		}
	}

	if (RemoveCount > 0)
	{
		CameraModeStack.RemoveAt(RemoveIndex, RemoveCount);
	}
}

void ULyraClone_CameraModeStack::BlendStack(FLyraClone_CameraModeView& OutCameraModeView)const
{
	const int32 StackSize = CameraModeStack.Num();
	if (StackSize <= 0)
	{
		return;
	}

	const ULyraClone_CameraMode* CameraMode = CameraModeStack[StackSize - 1];
	check(CameraMode);

	OutCameraModeView = CameraMode->View;

	for (int32 StackIndex = (StackSize - 2); StackIndex >= 0; --StackIndex)
	{
		CameraMode = CameraModeStack[StackIndex];
		check(CameraMode);

		OutCameraModeView.Blend(CameraMode->View,CameraMode->BlendWeight);
	}

}

void ULyraClone_CameraModeStack::EvaluateStack(float DeltaTime, FLyraClone_CameraModeView& OutCameraModeView)
{
	//Top->Bottom[0->num]까지 순차적으로 Stack에 있는 CameraMode업데이트
	UpdateStack(DeltaTime);

	//Bottom->Top까지 CameraModeStack에 대해 Blending진행
	BlendStack(OutCameraModeView);
}

