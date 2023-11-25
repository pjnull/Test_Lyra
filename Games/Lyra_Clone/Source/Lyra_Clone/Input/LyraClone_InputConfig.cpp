#include "LyraClone_InputConfig.h"
#include "InputAction.h"
#include "../LryaCloneLogChannel.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_InputConfig)

ULyraClone_InputConfig::ULyraClone_InputConfig(const FObjectInitializer& ObjectInitializer)
{
}

const UInputAction* ULyraClone_InputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FLyraClone_InputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && (Action.InputTag == InputTag))
		{
			return Action.InputAction;
		}
	}
	if (bLogNotFound) 
	{
		UE_LOG(LogClone,Error,TEXT("InputConfig Not Find!"));
	}


	return nullptr;
}

const UInputAction* ULyraClone_InputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FLyraClone_InputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && (Action.InputTag == InputTag))
		{
			return Action.InputAction;
		}
	
	}

	if (bLogNotFound)
	{
		UE_LOG(LogClone, Error, TEXT("Ability Config Not Find!"));
	}

	return nullptr;
}
