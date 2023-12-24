#include "LyraClone_InventoryItem_Definition.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_InventoryItem_Definition)

ULyraClone_InventoryItem_Definition::ULyraClone_InventoryItem_Definition(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer)
{
}

const ULyraClone_InventoryItemFragment* ULyraClone_InventoryItem_Definition::FindFragmentByClass(TSubclassOf<ULyraClone_InventoryItemFragment> FragmentClass) const
{
	if (FragmentClass)
	{
		for (ULyraClone_InventoryItemFragment* Fragment : Fragments)
		{//Fragment를 순회하면서 ISA를 통해 해당 클래스를 가지고 있는지 확인
			if (Fragment && Fragment->IsA(FragmentClass))
			{
				return Fragment;
			}
		}
	}
	return nullptr;
}
