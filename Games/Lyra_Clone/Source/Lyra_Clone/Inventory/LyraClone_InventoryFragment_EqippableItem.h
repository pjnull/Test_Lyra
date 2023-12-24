#pragma once
#include "LyraClone_InventoryItem_Definition.h"
#include "Templates/SubclassOf.h"
#include "LyraClone_InventoryFragment_EqippableItem.generated.h"

class ULyraClone_EquipmentDefinition;

UCLASS()
class ULyraClone_InventoryFragment_EqippableItem : public ULyraClone_InventoryItemFragment
{
   GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = LyraClone)
	TSubclassOf<ULyraClone_EquipmentDefinition>EquipmentDefinition;

};
