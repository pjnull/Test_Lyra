// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LyraUserFacingExperienceDefinition.generated.h"

class UCommonSession_HostSessionRequest;

/**
 * 
 */
UCLASS(BlueprintType)
class LYRA_CLONE_API ULyraUserFacingExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, BlueprintPure = false)
	UCommonSession_HostSessionRequest* CreateHostingRequest()const;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience, meta = (AllowTypes = "Map"))
	FPrimaryAssetId MapID;

	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category=Experience,meta=(AllowTypes="Lyra_CloneExperienceDefinition"))
	FPrimaryAssetId ExperienceID;


	
};
