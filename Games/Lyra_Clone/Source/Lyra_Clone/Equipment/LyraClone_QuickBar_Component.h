#pragma once
#include "Components/ControllerComponent.h"
#include "LyraClone_QuickBar_Component.generated.h"
class ULyraClone_InventoryItem_Instance;
class ULyraClone_EquipmentInstance;
class ULyraClone_EquipmentManagerComponent;

	

//PlayerController에 의해 조작되는 조작계중 하나
//HUD와 Inventory/Equipment 세가지의 중간 다리 역할을 하는 Component 

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class ULyraClone_QuickBar_Component:public UControllerComponent
{
   GENERATED_BODY()
public:
   ULyraClone_QuickBar_Component(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());
	

   virtual void BeginPlay()override;

   ULyraClone_EquipmentManagerComponent* FindEquipmentManager() const;

   void EquipItemInSlot();
   void UnequipItemInSlot();


   UFUNCTION(BlueprintCallable)
   void AddItemToSlot(int32 SlotIndex,ULyraClone_InventoryItem_Instance* Item);

   UFUNCTION(BlueprintCallable, Category = "LyraClone")
   void SetActivateSlotIndex(int32 NewIndex);

   

   UPROPERTY()
   int32 NumSlot = 3;//HUD QuickBar Slot갯수

   UPROPERTY()
   TArray<TObjectPtr<ULyraClone_InventoryItem_Instance>>Slots;
   //HUD QuickBark Slot 리스트


   UPROPERTY()
   int32 ActiveSlotIndex = -1;
   //현재 활성화된 Slot Index

   UPROPERTY()
   TObjectPtr<ULyraClone_EquipmentInstance>EquippedItem;
   //현재 장착한 장비 정보
};
