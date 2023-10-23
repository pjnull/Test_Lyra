#pragma once
#include "GameFramework/GamestateBase.h"
#include "LryaCloneGameState.generated.h"



class ULyra_Clone_ExperienceManagerComponent;

UCLASS()
class ALyaCloneGameState :public AGameStateBase
{
	GENERATED_BODY()
public:
	ALyaCloneGameState(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get());

	UPROPERTY()
	TObjectPtr<ULyra_Clone_ExperienceManagerComponent>ExperienceManagerComponent;
};