#include "Lyra_Clone_HeroComponent.h"
#include "../LryaCloneLogChannel.h"
#include "../LyraClone_GameplayTag.h"
#include "../Player/LyraClonePlayerState.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Lyra_Clone_PawnExtensionComponent.h"
#include "../Camera/LyraClone_CameraComponent.h"
#include "Lyra_Clone_PawnData.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(Lyra_Clone_HeroComponent)


const FName ULyra_Clone_HeroComponent::NAME_ActorFeatureName("Hero");

ULyra_Clone_HeroComponent::ULyra_Clone_HeroComponent(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	//기본적으로 Tick을 꺼야된다=>이벤트방식으로 할것이며, 상시로 확인하는 컴포넌트는 아님	
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
}

void ULyra_Clone_HeroComponent::OnRegister()
{
	Super::OnRegister();

	{
		//올바른 Actor에 등록되었는지 확인
		if (!GetPawn<APawn>())
		{
			UE_LOG(LogClone,Error,TEXT("Bp base class is not a pawn"));
			return;

		}
	}
	RegisterInitStateFeature();
}

void ULyra_Clone_HeroComponent::BeginPlay()
{
	Super::BeginPlay();
	
	//PawnExtensionComponent의 상태에 대해서만 받는다
	BindOnActorInitStateChanged(ULyra_Clone_PawnExtensionComponent::NAME_ActorFeatureName,FGameplayTag(),false);
	
	//initstate_Spawned로 초기화
	ensure(TryToChangeInitState(FLyraClone_GameplayTag::Get().InitState_Spawned));

	//강제업데이트
	CheckDefaultInitialization();
}

void ULyra_Clone_HeroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterInitStateFeature();
	Super::EndPlay(EndPlayReason);
}

void ULyra_Clone_HeroComponent::CheckDefaultInitialization()
{
	//PawnExtension과 다르게 나만 강제 업데이트를 진행시킨다.

	const FLyraClone_GameplayTag& InitTags = FLyraClone_GameplayTag::Get();
	static const TArray<FGameplayTag>StateChain = {InitTags.InitState_Spawned,InitTags.InitState_DataAvailable,InitTags.InitState_DataInitialized,InitTags.InitState_GameplayReady};
	ContinueInitStateChain(StateChain);
}

void ULyra_Clone_HeroComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	const FLyraClone_GameplayTag& InitTags = FLyraClone_GameplayTag::Get();
	if (Params.FeatureName == ULyra_Clone_PawnExtensionComponent::NAME_ActorFeatureName)
	{
		//PawnExtension의 상태가 DataInitialized면 강제 업데이트?모니터링?
		if (Params.FeatureState == InitTags.InitState_DataInitialized)
		{
			CheckDefaultInitialization();
		}
	}
}

bool ULyra_Clone_HeroComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const
{
	check(Manager);

	const FLyraClone_GameplayTag& InitTags = FLyraClone_GameplayTag::Get();
	APawn* Pawn = GetPawn<APawn>();
	ALyraClonePlayerState* ClonePs = GetPlayerState<ALyraClonePlayerState>();
	if (!CurrentState.IsValid() && DesiredState == InitTags.InitState_Spawned)
	{
		if (Pawn)
		{
			return true;
		}
	}

	if (CurrentState == InitTags.InitState_Spawned && DesiredState == InitTags.InitState_DataAvailable)
	{
		if (!ClonePs)
		{
			return false;

		}
		return true;

	}

	if (CurrentState == InitTags.InitState_DataAvailable && DesiredState == InitTags.InitState_DataInitialized)
	{
		return ClonePs&&Manager->HasFeatureReachedInitState(Pawn,ULyra_Clone_PawnExtensionComponent::NAME_ActorFeatureName ,InitTags.InitState_DataAvailable);

	}
	if (CurrentState == InitTags.InitState_DataInitialized && DesiredState == InitTags.InitState_GameplayReady)
	{
		return true;
	}
	return false;

	
	
}

void ULyra_Clone_HeroComponent::HandleChageInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	const FLyraClone_GameplayTag& InitTags = FLyraClone_GameplayTag::Get();

	if (CurrentState == InitTags.InitState_DataAvailable && DesiredState == InitTags.InitState_DataInitialized)
	{
		APawn* Pawn = GetPawn<APawn>();
		ALyraClonePlayerState* ClonePs = GetPlayerState<ALyraClonePlayerState>();
		if (!ensure(Pawn && ClonePs))
		{
			return;
		}


		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		const ULyra_Clone_PawnData* PawnData = nullptr;
		if (ULyra_Clone_PawnExtensionComponent* PawnExtComp = ULyra_Clone_PawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			PawnData = PawnExtComp->GetPawnData<ULyra_Clone_PawnData>();
		}
		if (bIsLocallyControlled && PawnData)
		{
			if (ULyraClone_CameraComponent* CameraComponent = ULyraClone_CameraComponent::FindCameraComponent(Pawn))
			{
				CameraComponent->DetermineCameraModeDelegate.BindUObject(this,&ThisClass::DetermineCameraMode);
			}
		}


	}

}

TSubclassOf<ULyraClone_CameraMode> ULyra_Clone_HeroComponent::DetermineCameraMode() const
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return nullptr;

	}

	if (ULyra_Clone_PawnExtensionComponent* pawnextComp = ULyra_Clone_PawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (const ULyra_Clone_PawnData* pawndata = pawnextComp->GetPawnData<ULyra_Clone_PawnData>())
		{
			return pawndata->DefaultCameraMode;
		}
	}
	return nullptr;
}

