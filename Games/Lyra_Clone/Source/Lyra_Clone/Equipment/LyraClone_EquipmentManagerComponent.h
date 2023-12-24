#pragma once
#include "Components/PawnComponent.h"
#include "LyraClone_EquipmentManagerComponent.generated.h"

class ULyraClone_EquipmentDefinition;
class ULyraClone_EquipmentInstance;

USTRUCT(BlueprintType)
struct FLyraClone_AppliedEquipmentEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<ULyraClone_EquipmentDefinition>EquipmentDefinition;

	UPROPERTY()
	TObjectPtr<ULyraClone_EquipmentInstance>Instance;

};


USTRUCT(BlueprintType)
struct FLyraClone_EquipmentList
{
	GENERATED_BODY()

	FLyraClone_EquipmentList(UActorComponent* InOwnerComponent = nullptr) :OwnerComponent(InOwnerComponent) {}
	
	ULyraClone_EquipmentInstance* AddEntry(TSubclassOf<ULyraClone_EquipmentDefinition>EquipmentDefinition);
	void RemoveEntry(ULyraClone_EquipmentInstance* Instance);

		
	UPROPERTY()
	TArray<FLyraClone_AppliedEquipmentEntry>Entries;

	UPROPERTY()
	TObjectPtr<UActorComponent>OwnerComponent;
};




UCLASS()
class ULyraClone_EquipmentManagerComponent:public UPawnComponent
{
   GENERATED_BODY()
public:
   ULyraClone_EquipmentManagerComponent(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());

   ULyraClone_EquipmentInstance* EquipItem(TSubclassOf<ULyraClone_EquipmentDefinition>EquipmentDefinition);
   void UnEquipItem(ULyraClone_EquipmentInstance* Instance);


   UPROPERTY()
   FLyraClone_EquipmentList EquipmentList;
};
