#pragma once

#include "Containers/Array.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "LyraClone_InputConfig.generated.h"

class UInputAction;

USTRUCT(BlueprintType)
struct FLyraClone_InputAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly)
	TObjectPtr<const UInputAction>InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

};


UCLASS(BlueprintType)
class ULyraClone_InputConfig :public UDataAsset
{
	GENERATED_BODY()
public:
	ULyraClone_InputConfig(const FObjectInitializer& ObjectInitializer= FObjectInitializer::Get());
	
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag,bool bLogNotFound=true)const;
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true)const;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FLyraClone_InputAction>NativeInputActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FLyraClone_InputAction>AbilityInputActions;
};