#pragma once

#include "Engine/DataAsset.h"
#include "Lyra_CloneExperienceDefinition.generated.h"

class ULyra_Clone_PawnData;

UCLASS()

class ULyra_CloneExperienceDefinition :public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	ULyra_CloneExperienceDefinition(const FObjectInitializer& objectInitializer= FObjectInitializer::Get());
	
	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	TObjectPtr< ULyra_Clone_PawnData>DefaultPawnData;

	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	TArray<FString>GameFeaturesToEnable;
};