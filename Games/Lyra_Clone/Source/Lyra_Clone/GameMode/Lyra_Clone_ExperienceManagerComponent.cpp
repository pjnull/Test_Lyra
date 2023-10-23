#include "Lyra_Clone_ExperienceManagerComponent.h"
#include "Lyra_CloneExperienceDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Lyra_Clone_ExperienceManagerComponent)

ULyra_Clone_ExperienceManagerComponent::ULyra_Clone_ExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}

void ULyra_Clone_ExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FOnLyraClonExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded.Add(MoveTemp(Delegate));
	}
}