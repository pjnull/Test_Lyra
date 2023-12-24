#pragma once

#include "Components/ControllerComponent.h"
#include "LyraClone_InventoryItem_Instance.h"
#include "LyraClone_InventoryManagerComponent.generated.h"


class ULyraClone_InventoryItem_Instance;
class ULyraClone_InventoryItem_Definition;


USTRUCT(BlueprintType)
struct FLyraClone_InventoryEntry
{
	GENERATED_BODY()
	
	UPROPERTY()
	TObjectPtr< ULyraClone_InventoryItem_Instance>Instance = nullptr;
};

USTRUCT()
struct FLyraClone_InventoryList
{
	GENERATED_BODY()

	FLyraClone_InventoryList(UActorComponent* InOwnerComponent= nullptr)
	:OwnerComponent(InOwnerComponent){}

	ULyraClone_InventoryItem_Instance* AddEntry(TSubclassOf<ULyraClone_InventoryItem_Definition>ItemDef);


	UPROPERTY()
	TArray<FLyraClone_InventoryEntry>Entries;

	UPROPERTY()
	TObjectPtr<UActorComponent>OwnerComponent;

};


UCLASS(BlueprintType)
class ULyraClone_InventoryManagerComponent :public UControllerComponent/*Lyra에서는 UActorComponent*/
{
	GENERATED_BODY()
public:
	ULyraClone_InventoryManagerComponent(const FObjectInitializer& ObejctInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = Inventory)
	ULyraClone_InventoryItem_Instance* AddItemDefinition(TSubclassOf<ULyraClone_InventoryItem_Definition>ItemDef);

	UPROPERTY()
	FLyraClone_InventoryList InventoryList;
};
