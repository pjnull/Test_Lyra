#pragma once
#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "Lyra_Clone_ExperienceManagerComponent.generated.h"


enum class ELyraCloneExperienceLoadState
{
	Unloaded,
	Loading,
	Loaded,
	Deactivating,

};

class ULyra_CloneExperienceDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLyraClonExperienceLoaded,const class ULyra_CloneExperienceDefinition*);

//
UCLASS()
class ULyra_Clone_ExperienceManagerComponent :public UGameStateComponent
{
	GENERATED_BODY()
public:
	ULyra_Clone_ExperienceManagerComponent(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get());
	
	void CallOrRegister_OnExperienceLoaded(FOnLyraClonExperienceLoaded::FDelegate&& Delegate);

	UPROPERTY()
	TObjectPtr<const ULyra_CloneExperienceDefinition> CurrentExperience;

	ELyraCloneExperienceLoadState LoadState = ELyraCloneExperienceLoadState::Unloaded;

	FOnLyraClonExperienceLoaded OnExperienceLoaded;

	bool IsExperienceLoaded() { return (LoadState == ELyraCloneExperienceLoadState::Loaded)&&(CurrentExperience!=nullptr); }
	

};