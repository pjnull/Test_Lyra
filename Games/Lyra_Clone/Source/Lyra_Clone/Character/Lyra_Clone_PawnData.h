#pragma once

#include "Engine/DataAsset.h"
#include "Lyra_Clone_PawnData.generated.h"

UCLASS()
class ULyra_Clone_PawnData :public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	ULyra_Clone_PawnData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

};