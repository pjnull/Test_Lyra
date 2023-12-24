#pragma once

#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "Containers/Array.h"
#include "LyraClone_EquipmentInstance.generated.h"


struct FLyraClone_EquipmentActorToSpawn;

UCLASS(BlueprintType,Blueprintable)
class ULyraClone_EquipmentInstance:public UObject
{
   GENERATED_BODY()
public:
   ULyraClone_EquipmentInstance(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());

   UFUNCTION(BlueprintPure, Category = Equipment)
   APawn* GetPawn() const;

   UFUNCTION(BlueprintPure, Category = Equipment, meta = (DeterminesOutputType = PawnType))
   APawn* GetTypedPawn(TSubclassOf<APawn>PawnType)const;

   void SpawnEquipmentActors(const TArray<FLyraClone_EquipmentActorToSpawn>&ActorsToSpawn);
   void DestroyEquipmentActors();

   UFUNCTION(BlueprintImplementableEvent, Category = Equipment, meta = (DisplayName = "OnEquipped"))
   void K2_OnEquipped();

   UFUNCTION(BlueprintImplementableEvent, Category = Equipment, meta = (DisplayName = "OnUnEquipped"))
   void K2_OnUnEquipped();

   virtual void OnEquipped();
   virtual void OnUnEquipped();


   UPROPERTY()
   TObjectPtr<UObject>Instigator;//어떤 InventoryItemInstance에 의해 활성화되었는지

   UPROPERTY()
   TArray<TObjectPtr<AActor>>SpawnedActors;//EquipmentDefinition에 맞게 Spawn된 Actor Instance들



};
