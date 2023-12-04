#pragma once

#include "Containers/Map.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeature_World_ActionBase.generated.h"

class FDelegateHandle;
class UGameInstance;
struct FGameFeatureActivatingContext;
struct FGameFeatureDeactivatingContext;
struct FWorldContext;

UCLASS(Abstract)
class UGameFeature_World_ActionBase :public UGameFeatureAction
{
	GENERATED_BODY()
public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context)override;

	virtual void AddToWorld(const FWorldContext& WorldContext,const FGameFeatureStateChangeContext& ChangeContext)PURE_VIRTUAL(UGameFeatureAction_WorldActionBase::AddToWorld,);
};