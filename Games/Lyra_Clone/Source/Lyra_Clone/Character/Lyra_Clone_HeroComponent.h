#pragma once

#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "Lyra_Clone_HeroComponent.generated.h"


class ULyraClone_CameraMode;
struct FLyraClone_MappableConfigPair;
struct FInputActionValue;

UCLASS(Blueprintable,Meta=(BlueprintSpawnableComponent))
class ULyra_Clone_HeroComponent :public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()
public:
	ULyra_Clone_HeroComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


	virtual void OnRegister()final;

	virtual void BeginPlay()final;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason)final;


	virtual void CheckDefaultInitialization()final;
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params)final;
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)const final;
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager,FGameplayTag CurrentState, FGameplayTag DesiredState)final;


	virtual FName GetFeatureName()const final { return NAME_ActorFeatureName; }
	static const FName NAME_ActorFeatureName;

	TSubclassOf<ULyraClone_CameraMode>DetermineCameraMode()const;
	void InitializePlayerInput(UInputComponent* PlayerInputComponent);

	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Mouse(const FInputActionValue& InputActionValue);

	UPROPERTY(EditAnywhere)
	TArray<FLyraClone_MappableConfigPair>DefaultInputConfigs;
};