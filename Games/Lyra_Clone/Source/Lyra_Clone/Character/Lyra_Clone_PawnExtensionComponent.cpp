#include "Lyra_Clone_PawnExtensionComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "../Character/Lyra_Clone_PawnData.h"
#include"../LyraClone_GameplayTag.h"
#include"../Player/LyraClonePlayerState.h"
#include "../Camera/LyraClone_CameraComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Lyra_Clone_PawnExtensionComponent)

const FName ULyra_Clone_PawnExtensionComponent::NAME_ActorFeatureName("PawnExtension");
//Feature Name은 Component 단위이니까 PawnExtension으로 이름지음

ULyra_Clone_PawnExtensionComponent::ULyra_Clone_PawnExtensionComponent(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	//기본적으로 Tick을 꺼야된다=>이벤트방식으로 할것이며, 상시로 확인하는 컴포넌트는 아님	
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

}

PRAGMA_DISABLE_OPTIMIZATION


void ULyra_Clone_PawnExtensionComponent::SetPawnData(const ULyra_Clone_PawnData* InPawnData)
{
	APawn* Pawn = GetPawnChecked<APawn>();
	if (Pawn->GetLocalRole() != ROLE_Authority)return;
	if (PawnData)return;
	PawnData = InPawnData;
}

void ULyra_Clone_PawnExtensionComponent::SetupPlayerInputComponent()
{
	//끊어진 연결고리를 강제업데이트를 통해 다시 연결함
	CheckDefaultInitialization();
}


void ULyra_Clone_PawnExtensionComponent::OnRegister()
{
	Super::OnRegister();
	{
		if (!GetPawn<APawn>())
		{
			return;
		}

	}

	RegisterInitStateFeature();
	//UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(GetOwningActor());
}
PRAGMA_ENABLE_OPTIMIZATION


void ULyra_Clone_PawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);

	//1.현재 상태를 가져와서 상태변환 가능성 판단
	//2.내부 상태 변경
	//3.BindOnActorInitState로 등록한 Delegate를 Initstate의 상태변화에 따라 Delegate호출
	ensure(TryToChangeInitState(FLyraClone_GameplayTag::Get().InitState_Spawned));

	CheckDefaultInitialization();//강제 업데이트 진행
}

void ULyra_Clone_PawnExtensionComponent::EndGame(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterInitStateFeature();
	Super::EndPlay(EndPlayReason);
}

void ULyra_Clone_PawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName != NAME_ActorFeatureName)
	{
		const FLyraClone_GameplayTag& InitTags = FLyraClone_GameplayTag::Get();
		if (Params.FeatureState == InitTags.InitState_DataAvailable)
		{
			CheckDefaultInitialization();
		}
	}
}

bool ULyra_Clone_PawnExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const
{
	check(Manager);
	APawn* Pawn = GetPawn<APawn>();
	const FLyraClone_GameplayTag& InitTags = FLyraClone_GameplayTag::Get();

	if (!CurrentState.IsValid() && DesiredState == InitTags.InitState_Spawned)
	{
		if(Pawn)
		{
			return true;
		}
	}
	if (CurrentState == InitTags.InitState_Spawned && DesiredState == InitTags.InitState_DataAvailable)
	{
		if (!PawnData)
		{
			return false;

		}
		const bool bHasAuthority = Pawn->HasAuthority();
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		
		if (bIsLocallyControlled||bHasAuthority)
		{
			if (!GetController<AController>())
			{
				return false;
			}
		}
		return true;
	}
	if (CurrentState == InitTags.InitState_DataAvailable && DesiredState == InitTags.InitState_DataInitialized)
	{
		return Manager->HaveAllFeaturesReachedInitState(Pawn, InitTags.InitState_DataAvailable);
	}

	if (CurrentState == InitTags.InitState_DataInitialized && DesiredState == InitTags.InitState_GameplayReady)
	{
		return true;
	}
	return false;

}

void ULyra_Clone_PawnExtensionComponent::CheckDefaultInitialization()
{
	CheckDefaultInitializationForImplementers();

	const FLyraClone_GameplayTag& InitTags = FLyraClone_GameplayTag::Get();

	static const TArray<FGameplayTag>StateChain = {InitTags.InitState_Spawned,InitTags.InitState_DataAvailable,InitTags.InitState_DataInitialized,InitTags.InitState_GameplayReady};

	ContinueInitStateChain(StateChain);

}

