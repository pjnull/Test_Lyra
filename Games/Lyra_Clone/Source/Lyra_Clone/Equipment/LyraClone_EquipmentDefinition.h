#pragma once
#include "LyraClone_EquipmentDefinition.generated.h"

class ULyraClone_EquipmentInstance;





USTRUCT()
struct FLyraClone_EquipmentActorToSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Equipment)
	TSubclassOf<AActor>ActorToSpawn;//Spawn할 대상 Actor클래스

	UPROPERTY(EditAnywhere, Category = Equipment)
	FName AttachSocket;//BoneSocket의 이름

	UPROPERTY(EditAnywhere, Category = Equipment)
	FTransform AttachTransform;//Socket의 Transform

};





//아이템에 대한 클래스정보를 들고 있는곳
UCLASS(BlueprintType,Blueprintable)
class ULyraClone_EquipmentDefinition:public UObject
{
   GENERATED_BODY()
public:
   ULyraClone_EquipmentDefinition(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());

   UPROPERTY(EditDefaultsOnly,Category = Equipment)
	TSubclassOf<ULyraClone_EquipmentInstance> InstanceType;

   UPROPERTY(EditDefaultsOnly,Category = Equipment)
   TArray<FLyraClone_EquipmentActorToSpawn> ActorToSpawn;

};
