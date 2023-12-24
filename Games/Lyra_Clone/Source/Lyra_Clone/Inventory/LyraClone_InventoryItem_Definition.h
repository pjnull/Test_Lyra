#pragma once
#include "LyraClone_InventoryItem_Definition.generated.h"

UCLASS(Abstract,DefaultToInstanced, EditInlineNew)
class ULyraClone_InventoryItemFragment :public UObject
{
	GENERATED_BODY()
public:
};



UCLASS(Blueprintable)
class ULyraClone_InventoryItem_Definition :public UObject
{
	GENERATED_BODY()
public:
	ULyraClone_InventoryItem_Definition(const FObjectInitializer& ObejctInitializer = FObjectInitializer::Get());


	const ULyraClone_InventoryItemFragment* FindFragmentByClass(TSubclassOf<ULyraClone_InventoryItemFragment> FragmentClass) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Display)
	FText DisplayName;



	UPROPERTY(EditDefaultsOnly,Instanced, BlueprintReadOnly, Category = Display)
	TArray<TObjectPtr<ULyraClone_InventoryItemFragment>>Fragments;


};
