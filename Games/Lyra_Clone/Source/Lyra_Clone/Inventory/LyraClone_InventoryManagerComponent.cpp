#include "LyraClone_InventoryManagerComponent.h"
#include"LyraClone_InventoryItem_Definition.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_InventoryManagerComponent)

ULyraClone_InventoryManagerComponent::ULyraClone_InventoryManagerComponent(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer),InventoryList(this)
{
}

ULyraClone_InventoryItem_Instance* ULyraClone_InventoryManagerComponent::AddItemDefinition(TSubclassOf<ULyraClone_InventoryItem_Definition> ItemDef)
{
	ULyraClone_InventoryItem_Instance* Result = nullptr;
	if (ItemDef)
	{
		Result = InventoryList.AddEntry(ItemDef);
	}
	return Result;
}

ULyraClone_InventoryItem_Instance* FLyraClone_InventoryList::AddEntry(TSubclassOf<ULyraClone_InventoryItem_Definition> ItemDef)
{
	ULyraClone_InventoryItem_Instance* Result = nullptr;
	check(ItemDef);
	check(OwnerComponent);

	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor->HasAuthority());

	FLyraClone_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = NewObject<ULyraClone_InventoryItem_Instance>(OwningActor);
	NewEntry.Instance->ItemDef = ItemDef;

	Result = NewEntry.Instance;

	return Result;
}
