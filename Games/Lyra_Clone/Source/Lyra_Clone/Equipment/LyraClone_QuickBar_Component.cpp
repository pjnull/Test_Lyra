#include "LyraClone_QuickBar_Component.h"
#include "LyraClone_EquipmentManagerComponent.h"
#include "LyraClone_EquipmentDefinition.h"
#include "LyraClone_EquipmentInstance.h"
#include "../Inventory/LyraClone_InventoryFragment_EqippableItem.h"
#include "../Inventory/LyraClone_InventoryItem_Definition.h"
#include "../Inventory/LyraClone_InventoryItem_Instance.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_QuickBar_Component)

ULyraClone_QuickBar_Component::ULyraClone_QuickBar_Component(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer)

{

}

void ULyraClone_QuickBar_Component::BeginPlay()
{
	if (Slots.Num() < NumSlot)
	{
		Slots.AddDefaulted(NumSlot-Slots.Num());
	}


	Super::BeginPlay();
}

ULyraClone_EquipmentManagerComponent* ULyraClone_QuickBar_Component::FindEquipmentManager() const
{
	if (AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (APawn* Pawn = OwnerController->GetPawn())
		{
			return Pawn->FindComponentByClass<ULyraClone_EquipmentManagerComponent>();
		}
	}
	return nullptr;
}

void ULyraClone_QuickBar_Component::UnequipItemInSlot()
{
	if (ULyraClone_EquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
	{//QuickBar는 Controller에 붙어있는 Component지만 
	 //EquipmentManagerComponent는 Controller가 소유하고 있는 Pawn의 Component이다.
		if (EquippedItem)
		{
			EquipmentManager->UnEquipItem(EquippedItem);
			EquippedItem = nullptr;
		}
	}

}
void ULyraClone_QuickBar_Component::EquipItemInSlot()
{
	check(Slots.IsValidIndex(ActiveSlotIndex));
	check(EquippedItem==nullptr);

	if (ULyraClone_InventoryItem_Instance* SlotItem = Slots[ActiveSlotIndex])
	{
		if (const ULyraClone_InventoryFragment_EqippableItem* EquipInfo = SlotItem->FindFragmentByClass<ULyraClone_InventoryFragment_EqippableItem>())
		{
			TSubclassOf<ULyraClone_EquipmentDefinition>EquipDef = EquipInfo->EquipmentDefinition;
			if (EquipDef)
			{
				if (ULyraClone_EquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
				{
					EquippedItem = EquipmentManager->EquipItem(EquipDef);
					if (EquippedItem)
					{
						EquippedItem->Instigator = SlotItem;
					}
				}
			}
		}
	}
}	



void ULyraClone_QuickBar_Component::AddItemToSlot(int32 SlotIndex, ULyraClone_InventoryItem_Instance* Item)
{
	if (Slots.IsValidIndex(SlotIndex) && (Item != nullptr))
	{
		if (Slots[SlotIndex] == nullptr)
		{
			Slots[SlotIndex] = Item;
		}
	}
}

void ULyraClone_QuickBar_Component::SetActivateSlotIndex(int32 NewIndex)
{
	if (Slots.IsValidIndex(NewIndex) && (ActiveSlotIndex != NewIndex))
	{
		UnequipItemInSlot();
		ActiveSlotIndex = NewIndex;
		EquipItemInSlot();
	}
}
