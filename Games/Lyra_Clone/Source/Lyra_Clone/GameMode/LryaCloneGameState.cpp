#include "LryaCloneGameState.h"
#include "Lyra_Clone_ExperienceManagerComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LryaCloneGameState)




ALyaCloneGameState::ALyaCloneGameState(const FObjectInitializer& ObjectInitializer)
{
	ExperienceManagerComponent = CreateDefaultSubobject<ULyra_Clone_ExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));
}

