// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LyraUserFacingExperienceDefinition.generated.h"

/**
 * 
 */
UCLASS()
class LYRA_CLONE_API ULyraUserFacingExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience, meta = (AllowTypes = "Map"))
	FPrimaryAssetId MapID;

	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category=Experience,meta=(AllowTypes="Lyra_CloneExperienceDefinition"))
	FPrimaryAssetId ExperienceID;


	
};
