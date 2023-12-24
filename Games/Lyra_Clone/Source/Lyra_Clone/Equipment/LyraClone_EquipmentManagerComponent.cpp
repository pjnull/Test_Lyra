#include "LyraClone_EquipmentManagerComponent.h"
#include "LyraClone_EquipmentDefinition.h"
#include "LyraClone_EquipmentInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_EquipmentManagerComponent)

PRAGMA_DISABLE_OPTIMIZATION
ULyraClone_EquipmentManagerComponent::ULyraClone_EquipmentManagerComponent(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer),EquipmentList(this)
{
}
PRAGMA_ENABLE_OPTIMIZATION


ULyraClone_EquipmentInstance* ULyraClone_EquipmentManagerComponent::EquipItem(TSubclassOf<ULyraClone_EquipmentDefinition> EquipmentDefinition)
{
	ULyraClone_EquipmentInstance* Result = nullptr;
	if (EquipmentDefinition)
	{
		Result = EquipmentList.AddEntry(EquipmentDefinition);
		if (Result)
		{
			Result->OnEquipped();
		}
	}
	return Result;
}

void ULyraClone_EquipmentManagerComponent::UnEquipItem(ULyraClone_EquipmentInstance* Instance)
{
	if (Instance)
	{
		Instance->OnUnEquipped();
		EquipmentList.RemoveEntry(Instance);
	}
}

ULyraClone_EquipmentInstance* FLyraClone_EquipmentList::AddEntry(TSubclassOf<ULyraClone_EquipmentDefinition>EquipmentDefinition)
{
	ULyraClone_EquipmentInstance* Result = nullptr;
	check(EquipmentDefinition!=nullptr);
	check(OwnerComponent);
	check(OwnerComponent->GetOwner()->HasAuthority());

	const ULyraClone_EquipmentDefinition* EquipmentCDO = GetDefault<ULyraClone_EquipmentDefinition>(EquipmentDefinition);
	
	TSubclassOf<ULyraClone_EquipmentInstance>InstanceType = EquipmentCDO->InstanceType;

	if (!InstanceType)
	{
		InstanceType = ULyraClone_EquipmentInstance::StaticClass();
	}

	FLyraClone_AppliedEquipmentEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.EquipmentDefinition = EquipmentDefinition;
	NewEntry.Instance = NewObject<ULyraClone_EquipmentInstance>(OwnerComponent->GetOwner(),InstanceType);
	Result = NewEntry.Instance;

	Result->SpawnEquipmentActors(EquipmentCDO->ActorToSpawn);

	return Result;

;
}

void FLyraClone_EquipmentList::RemoveEntry(ULyraClone_EquipmentInstance* Instance)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FLyraClone_AppliedEquipmentEntry& Entry = *EntryIt;
		if (Entry.Instance == Instance)
		{
			Instance->DestroyEquipmentActors();
			EntryIt.RemoveCurrent();
		}
	}
}
