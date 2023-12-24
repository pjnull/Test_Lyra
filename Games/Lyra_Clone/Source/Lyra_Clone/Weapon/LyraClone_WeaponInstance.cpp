#include "LyraClone_WeaponInstance.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_WeaponInstance)

ULyraClone_WeaponInstance::ULyraClone_WeaponInstance(const FObjectInitializer& ObejctInitializer)
{
}

TSubclassOf<UAnimInstance> ULyraClone_WeaponInstance::PickBestAnimLayer(bool bEquipped, const FGameplayTagContainer& CosmeticsTags) const
{
	const FLyraClone_AnimLayerSelectionSet& SetToQuery = (bEquipped?EquippedAnimSet:UnEquippedAnimSet);
	return SetToQuery.SelectBestLayer(CosmeticsTags);
}
