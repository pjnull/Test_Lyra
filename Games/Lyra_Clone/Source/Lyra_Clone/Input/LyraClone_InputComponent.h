#pragma once

#include "EnhancedInputComponent.h"
#include "InputTriggers.h"
#include "inputActionValue.h"
#include "LyraClone_InputConfig.h"
#include "LyraClone_InputComponent.generated.h"



UCLASS()
class ULyraClone_InputComponent :public UEnhancedInputComponent
{
	GENERATED_BODY()
public:
	ULyraClone_InputComponent(const FObjectInitializer& ObjectInitializer= FObjectInitializer::Get());
	
	template<class UserClass,typename FuncType>
	void BindNativeAction(const ULyraClone_InputConfig* Inputconfig,const FGameplayTag& InputTag,ETriggerEvent TriggerEvent,UserClass* Object,FuncType Func,bool bLogIfNotFound);
	
	template<class UserClass,typename PressedFunType, typename ReleasedFunType>
	void BindAbilityActions(const ULyraClone_InputConfig* inputconfig, UserClass* Object, PressedFunType pressedfunc, ReleasedFunType releasedfunc, TArray<uint32>& BindHandles);

	

};

template<class UserClass, typename FuncType>
void ULyraClone_InputComponent::BindNativeAction(const ULyraClone_InputConfig* Inputconfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bLogIfNotFound)
{
	check(Inputconfig);

	if (const UInputAction* IA = Inputconfig->FindNativeInputActionForTag(InputTag, bLogIfNotFound))
	{
		BindAction(IA, TriggerEvent, Object, Func);
	}
}

template<class UserClass, typename PressedFunType, typename ReleasedFunType>
void ULyraClone_InputComponent::BindAbilityActions(const ULyraClone_InputConfig* inputconfig, UserClass* Object, PressedFunType pressedfunc, ReleasedFunType releasedfunc, TArray<uint32>& BindHandles)
{
	check(inputconfig);

	for (const FLyraClone_InputAction& Action : inputconfig->AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			if (PressedFunc)
			{
				BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Triggered, Object, PressFunc, Action.InputTag).GetHandle());
			}
			if (ReleasedFunc)
			{
				BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag).GetHandle());
			}
		}
	}
}