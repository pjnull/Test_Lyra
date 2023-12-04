#pragma once

#include "CoreMinimal.h"
#include "GameFeature_World_ActionBase.h"
#include "../Input/LyraClone_InputConfig.h"
#include "GameFeature_Action_AddInputConfig.generated.h"

struct FComponentRequestHandle;
struct FLyraClone_MappableConfigPair;
class APawn;

UCLASS()
class UGameFeature_Action_AddInputConfig :public UGameFeature_World_ActionBase
{
	GENERATED_BODY()
public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context)override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)override;

	virtual void AddToWorld(const FWorldContext& WorldContext,const FGameFeatureStateChangeContext& ChangeContext)override;



private:
	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>>ExtensionRequestHandles;
		TArray<TWeakObjectPtr<APawn>>PawnAddedTo;
	};

	void HandlePawnExtension(AActor* Actor,FName EventName,FGameFeatureStateChangeContext ChangeContext);

	void AddInputConfig(APawn* pawn, FPerContextData& ActivateData);
	void RemoveInputConfig(APawn* pawn, FPerContextData& ActivateData);

	void Reset(FPerContextData& ActivateData);

	
	//GameFeatureSateChangeContext:Feature의 상태가 바뀔때 마다 호출되는 이벤트 함수
	TMap<FGameFeatureStateChangeContext, FPerContextData>Contextdata;

	UPROPERTY(EditAnywhere)
	TArray<FLyraClone_MappableConfigPair>InputConfig;

};