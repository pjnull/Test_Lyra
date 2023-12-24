#include "LyraClone_InventoryItem_Instance.h"
#include "LyraClone_InventoryItem_Definition.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_InventoryItem_Instance)

ULyraClone_InventoryItem_Instance::ULyraClone_InventoryItem_Instance(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer)
{
}
PRAGMA_DISABLE_OPTIMIZATION
const ULyraClone_InventoryItemFragment* ULyraClone_InventoryItem_Instance::FindFragmentByClass(TSubclassOf<ULyraClone_InventoryItemFragment> FragmentClass) const
{
	if ((ItemDef != nullptr) && (FragmentClass != nullptr))
	{
		//InventoryItemDefinition은 모든 맴버변수가 EditDefaultOnly로 선언되어 있으므로,GetDefault로 가져와도 무관하다
		//Fragment 정보는 Instance가 아닌 Definition에 있다.
		return GetDefault<ULyraClone_InventoryItem_Definition>(ItemDef)->FindFragmentByClass(FragmentClass);
	}
	return nullptr;
}

PRAGMA_ENABLE_OPTIMIZATION