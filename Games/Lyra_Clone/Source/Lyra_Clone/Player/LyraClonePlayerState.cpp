#include "LyraClonePlayerState.h"
#include "../GameMode/Lyra_CloneExperienceDefinition.h"
#include "../GameMode/Lyra_CloneGameMode.h"
#include"../Character/Lyra_Clone_PawnData.h"
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
	if (ALyra_CloneGameMode* GameMode = GetWorld()->GetAuthGameMode<ALyra_CloneGameMode>())
	{
		const ULyra_Clone_PawnData* NewPawnData = GameMode->GetPawnDataForController(GetOwningController());
		check(NewPawnData);

		SetPawnData(NewPawnData);
	}
}

void ALyraClonePlayerState::SetPawnData(const ULyra_Clone_PawnData* InPawnData)
{
	check(InPawnData);
	check(!PawnData);
	PawnData = InPawnData;

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

