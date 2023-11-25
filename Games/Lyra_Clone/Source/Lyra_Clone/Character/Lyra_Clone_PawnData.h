#pragma once

#include "Engine/DataAsset.h"
#include "Lyra_Clone_PawnData.generated.h"


class ULyraClone_CameraMode;
class ULyraClone_InputConfig;


UCLASS()
class ULyra_Clone_PawnData :public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	ULyra_Clone_PawnData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Clone|Pawn")
	TSubclassOf<APawn>PawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Camera")
	TSubclassOf<ULyraClone_CameraMode>DefaultCameraMode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clone|Camera")
	TObjectPtr<ULyraClone_InputConfig>InputConfig;
};