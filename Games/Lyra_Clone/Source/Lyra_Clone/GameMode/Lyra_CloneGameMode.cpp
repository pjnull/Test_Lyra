// Copyright Epic Games, Inc. All Rights Reserved.


#include "Lyra_CloneGameMode.h"
#include "LryaCloneGameState.h"
#include "../Character/Lyra_Clone_Character.h"
#include "../Player/LyraClonePlayerState.h"
#include "../Player/LyraClonePlayerController.h"
#include "Lyra_Clone_ExperienceManagerComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(Lyra_CloneGameMode)



ALyra_CloneGameMode::ALyra_CloneGameMode(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	GameStateClass = ALyaCloneGameState::StaticClass();
	PlayerControllerClass = ALyraClonePlayerController::StaticClass();
	PlayerStateClass = ALyraClonePlayerState::StaticClass();
	DefaultPawnClass = ALyra_Clone_Character::StaticClass();

}

void ALyra_CloneGameMode::InitGame(const FString& Mapname, const FString& option, FString& ErrorMessage)
{
	Super::InitGame(Mapname, option, ErrorMessage);
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

void ALyra_CloneGameMode::InitGameState()
{
	Super::InitGameState();

	ULyra_Clone_ExperienceManagerComponent* ExperienceManagerComponent = GameState->FindComponentByClass<ULyra_Clone_ExperienceManagerComponent>();
	
	check(ExperienceManagerComponent);

	ExperienceManagerComponent->CallOrRegister_OnExperienceLoaded(FOnLyraClonExperienceLoaded::FDelegate::CreateUObject(this,&ThisClass::OnExperienceLoaded));
}

void ALyra_CloneGameMode::HandleMatchAssignmentIfNotExpectingOne()
{
}

void ALyra_CloneGameMode::OnExperienceLoaded(const ULyra_CloneExperienceDefinition* CurrentExperience)
{
}
		