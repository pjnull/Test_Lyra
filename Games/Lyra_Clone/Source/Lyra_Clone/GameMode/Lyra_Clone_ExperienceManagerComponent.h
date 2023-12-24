#pragma once
#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "Lyra_Clone_ExperienceManagerComponent.generated.h"


namespace UE::GameFeatures { struct FResult; }

enum class ELyraCloneExperienceLoadState
{
	Unloaded,
	Loading,
	LoadingGameFeature,
	ExecutingActions,
	Loaded,
	Deactivating,

};

class ULyra_CloneExperienceDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLyraClonExperienceLoaded,const ULyra_CloneExperienceDefinition*);

//
UCLASS()
class ULyra_Clone_ExperienceManagerComponent final :public UGameStateComponent
{
	GENERATED_BODY()
public:
	ULyra_Clone_ExperienceManagerComponent(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get());
	
	void CallOrRegister_OnExperienceLoaded(FOnLyraClonExperienceLoaded::FDelegate&&Delegate);

	UPROPERTY()
	TObjectPtr<const ULyra_CloneExperienceDefinition> CurrentExperience;

	ELyraCloneExperienceLoadState LoadState = ELyraCloneExperienceLoadState::Unloaded;

	FOnLyraClonExperienceLoaded OnExperienceLoaded;

	bool IsExperienceLoaded() { return (LoadState == ELyraCloneExperienceLoadState::Loaded)&&(CurrentExperience!=nullptr); }
	
	void ServerSetCurrentExperience(FPrimaryAssetId ExperienceId);
	void StartExperienceLoad();

	void OnExperienceLoadComplete();
	void OnExperienceFullLoadCompleted();
	void OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& result);


	const ULyra_CloneExperienceDefinition* GetCurrentExperienceChecked()const;


	int32 NumGameFeaturePluginsLoading = 0;
	TArray<FString>GameFeaturePluginURLs;
};