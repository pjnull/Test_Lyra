 
#pragma once
#include "GameplayTagContainer.h"
#include "LyraClone_CosmeticAnimationTypes.generated.h"

class USkeletalMesh;
class UPhysicsAsset;


USTRUCT(BlueprintType)
struct FLyraClone_AnimLayerSelectionEntry
{
	GENERATED_BODY()
	
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TSubclassOf<UAnimInstance>Layer;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FGameplayTagContainer RequiredTags;

};

USTRUCT(BlueprintType)
struct FLyraClone_AnimLayerSelectionSet
{
	GENERATED_BODY()

	TSubclassOf<UAnimInstance>SelectBestLayer(const FGameplayTagContainer& Cosmetics)const;


	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<FLyraClone_AnimLayerSelectionEntry>LayerRules;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TSubclassOf<UAnimInstance>DefaultLayer;

};


USTRUCT(BlueprintType)
struct FLyraClone_AnimBodyStyleSelectionEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TObjectPtr<USkeletalMesh>Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Category = "Cosmetics"))
	FGameplayTagContainer RequiredTags;
};



USTRUCT(BlueprintType)
struct FLyraClone_AnimBodyStyleSelectionSet
{
	GENERATED_BODY()

	USkeletalMesh* SelectBestBodyStyle(const FGameplayTagContainer& CosmeticTags)const;


	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<FLyraClone_AnimBodyStyleSelectionEntry>MeshRules;
	//SkeletalMesh의 정보를 들고 있다.


	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TObjectPtr<USkeletalMesh>DefaultMesh = nullptr;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UPhysicsAsset>ForcedPhysicsAsset = nullptr;
};