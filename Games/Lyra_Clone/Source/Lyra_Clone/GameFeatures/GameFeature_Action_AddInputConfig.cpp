#include "GameFeature_Action_AddInputConfig.h"
#include "EnhancedInputSubsystems.h"
#include "PlayerMappableInputConfig.h"
#include "../Input/LyraClone_MappableConfigPair.h"
#include "Components/GameFrameworkComponentManager.h"
#include "../Character/Lyra_Clone_HeroComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeature_Action_AddInputConfig)

void UGameFeature_Action_AddInputConfig::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActivateData = Contextdata.FindOrAdd(Context);
	if (!ensure(ActivateData.ExtensionRequestHandles.IsEmpty())
		||!ensure(ActivateData.PawnAddedTo.IsEmpty()))
	{
		Reset(ActivateData);
	}
	//WorldActionBase를 호출하면서 AddToWorld를 호출한다
	Super::OnGameFeatureActivating(Context);

}

void UGameFeature_Action_AddInputConfig::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);
	FPerContextData* ActivateData = Contextdata.Find(Context);
	if (ensure(ActivateData))
	{
		Reset(*ActivateData);
	}
}

void UGameFeature_Action_AddInputConfig::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* world = WorldContext.World();
	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActivateData = Contextdata.FindOrAdd(ChangeContext);
	if (GameInstance && world && world->IsGameWorld())
	{
		//GFCM을 이용하여 ExtensionHandler를 추가하고 등록
		//HandlePawnExtension콜백함수 연결	
		if (UGameFrameworkComponentManager* ComponenetMan = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
		{
			UGameFrameworkComponentManager::FExtensionHandlerDelegate AddConfigDelegate =
				UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(this, &ThisClass::HandlePawnExtension, ChangeContext);
			
			//등록된 콜백함수의 핸들을 ActivateData의 ExtensionRequestHandle에 등록
			TSharedPtr<FComponentRequestHandle>ExtensionRequestHandle = ComponenetMan->AddExtensionHandler(APawn::StaticClass(),AddConfigDelegate);
			ActivateData.ExtensionRequestHandles.Add(ExtensionRequestHandle);
		}
	}
}

void UGameFeature_Action_AddInputConfig::HandlePawnExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext)
{
	APawn* AsPawn = CastChecked<APawn>(Actor);
	FPerContextData& ActivateData = Contextdata.FindOrAdd(ChangeContext);

	if (EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded|| EventName ==ULyra_Clone_HeroComponent::NAME_BindInputNow)
	{
		AddInputConfig(AsPawn, ActivateData);
	}
	else if (EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved || EventName == ULyra_Clone_HeroComponent::NAME_BindInputNow)
	{
		RemoveInputConfig(AsPawn, ActivateData);
	}

}

void UGameFeature_Action_AddInputConfig::AddInputConfig(APawn* pawn, FPerContextData& ActivateData)
{
	APlayerController* PlayerController = Cast<APlayerController>(pawn->GetController());
	if (ULocalPlayer* LP = PlayerController ? PlayerController->GetLocalPlayer() : nullptr)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			FModifyContextOptions Options = {};
			Options.bIgnoreAllPressedKeysUntilRelease = false;

			//추가된 InputConfig를 순회하며,EnhancedInputSubsystem에 PlayerMappableConfig를 직접추가
			for (const FLyraClone_MappableConfigPair& pair : InputConfig)
			{
				if (pair.bShouldActivateAutomatically)
				{
					Subsystem->AddPlayerMappableConfig(pair.Config.LoadSynchronous(),Options);
				}
			}
			//ActiveData에 Pawn을 관리대상으로 등록
			ActivateData.PawnAddedTo.AddUnique(pawn);
		}
		
	}
}

void UGameFeature_Action_AddInputConfig::RemoveInputConfig(APawn* pawn, FPerContextData& ActivateData)
{
	APlayerController* PlayerController = Cast<APlayerController>(pawn->GetController());
	if (ULocalPlayer* LP = PlayerController ? PlayerController->GetLocalPlayer() : nullptr)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			//InputConfig를 순회하면서 Config제거
			for (const FLyraClone_MappableConfigPair& pair : InputConfig)
			{
				Subsystem->RemovePlayerMappableConfig(pair.Config.LoadSynchronous());
			}
			ActivateData.PawnAddedTo.Remove(pawn);
		}
	}
}

void UGameFeature_Action_AddInputConfig::Reset(FPerContextData& ActivateData)
{
	//ExtensionRequestHandle을 초기화
	ActivateData.ExtensionRequestHandles.Empty();

	//PawnsAddedTo에 대해서 하나씩 Stack방식으로 위에서 아래로 직접 InputConfig를 제거
	while (!ActivateData.PawnAddedTo.IsEmpty())
	{
		TWeakObjectPtr<APawn>PawnPtr = ActivateData.PawnAddedTo.Top();
		if (PawnPtr.IsValid())
		{
			RemoveInputConfig(PawnPtr.Get(), ActivateData);
		}
		else
		{
			//weakobjectptr로 pawnaddedto를 관리하고 있기 때문에 GC가 되었다면,nullptr일수 있음
			ActivateData.PawnAddedTo.Pop();
		}
	}
}
