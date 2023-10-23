#include "LyraClonePlayerState.h"
#include "../GameMode/Lyra_CloneExperienceDefinition.h"
#include "../GameMode/Lyra_CloneGameMode.h"
#include"../GameMode/LryaCloneGameState.h"
#include"../GameMode/Lyra_Clone_ExperienceManagerComponent.h"



#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClonePlayerState)

PRAGMA_DISABLE_OPTIMIZATION
ALyraClonePlayerState::ALyraClonePlayerState(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}
PRAGMA_ENABLE_OPTIMIZATION

void ALyraClonePlayerState::OnExperienceLoaded(const ULyra_CloneExperienceDefinition* CurrentExperience)
{
}


void ALyraClonePlayerState::PostInitializerComponent()
{
	Super::PostInitializeComponents();

	AGameStateBase* Gamestate = GetWorld()->GetGameState();
	check(Gamestate);
	
	ULyra_Clone_ExperienceManagerComponent* ExperienceComponent = Gamestate->FindComponentByClass<ULyra_Clone_ExperienceManagerComponent>();
	check(ExperienceComponent);

	ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnLyraClonExperienceLoaded::FDelegate::CreateUObject(this,&ThisClass::OnExperienceLoaded));
	
}
