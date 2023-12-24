#pragma once
#include "../Equipment/LyraClone_EquipmentInstance.h"
#include "../Cosmetics/LyraClone_CosmeticAnimationTypes.h"
#include "LyraClone_WeaponInstance.generated.h"

UCLASS()
class ULyraClone_WeaponInstance:public ULyraClone_EquipmentInstance
{
   GENERATED_BODY()
public:
   ULyraClone_WeaponInstance(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());
	

   UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Animation)
   TSubclassOf<UAnimInstance>PickBestAnimLayer(bool bEquipped,const FGameplayTagContainer& CosmeticsTags)const;


   UPROPERTY(EditAnywhere,BlueprintReadOnly,Category=Animation)
   FLyraClone_AnimLayerSelectionSet EquippedAnimSet;

   UPROPERTY(EditAnywhere,BlueprintReadOnly,Category=Animation)
   FLyraClone_AnimLayerSelectionSet UnEquippedAnimSet;

};
