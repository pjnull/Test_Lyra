#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LyraClone_ExperienceActionSet.generated.h"

class UGameFeatureAction;

UCLASS(BlueprintType)
class ULyraClone_ExperienceActionSet :public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	ULyraClone_ExperienceActionSet();

	UPROPERTY(EditAnywhere,Category="Actions to Perform")
	TArray<TObjectPtr<UGameFeatureAction>>Actions;
};