#pragma once

#include "LyraClone_InventoryItem_Instance.generated.h"

class ULyraClone_InventoryItem_Definition;
class ULyraClone_InventoryItemFragment;


UCLASS(BlueprintType)
class ULyraClone_InventoryItem_Instance:public UObject
{
   GENERATED_BODY()
public:
   ULyraClone_InventoryItem_Instance(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());

   const ULyraClone_InventoryItemFragment* FindFragmentByClass(TSubclassOf<ULyraClone_InventoryItemFragment>FragmentClass) const;

   template<typename ResultClass>
   const ResultClass* FindFragmentByClass()const
   {
	   return (ResultClass*)FindFragmentByClass(ResultClass::StaticClass());
   }


   UPROPERTY()
   TSubclassOf<ULyraClone_InventoryItem_Definition>ItemDef;
   //Inventory Item의 인스턴스에는 무엇으로 정의되었는지
   // 메타 클래스인 LyraClone_InventoryDefinition을 들고있다.
};
