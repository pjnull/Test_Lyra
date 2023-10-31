// Copyright Epic Games, Inc. All Rights Reserved.


#include "Lyra_CloneGameMode.h"
#include "LryaCloneGameState.h"
#include "../Character/Lyra_Clone_Character.h"
#include "../Player/LyraClonePlayerState.h"
#include "../Player/LyraClonePlayerController.h"
#include "Lyra_Clone_ExperienceManagerComponent.h"
#include "../LryaCloneLogChannel.h"
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

APawn* ALyra_CloneGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* Newplayer, const FTransform& SpawnTransform)
{
	UE_LOG(LogClone, Log, TEXT("spawnpawnImple called"));
	return Super::SpawnDefaultPawnAtTransform_Implementation(Newplayer, SpawnTransform);
}

void ALyra_CloneGameMode::HandleStartingNewPlayer_Implementation(APlayerController* Newplayer)
{
	if (IsExperienceLoaded())
	{
		Super::HandleStartingNewPlayer_Implementation(Newplayer);
	}
}


void ALyra_CloneGameMode::HandleMatchAssignmentIfNotExpectingOne()
{
	FPrimaryAssetId ExperienceId;

	UWorld* World = GetWorld();

	if (!ExperienceId.IsValid())
	{
		ExperienceId = FPrimaryAssetId(FPrimaryAssetType("Lyra_CloneExperienceDefinition"),FName("BP_DefaultExperienceDefinition"));
	}

	OnMatchAssignmentGiven(ExperienceId);


}
bool ALyra_CloneGameMode::IsExperienceLoaded() const
{
	check(GameState);
	ULyra_Clone_ExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyra_Clone_ExperienceManagerComponent>();
	check(ExperienceComponent);

	return ExperienceComponent->IsExperienceLoaded();
}
void ALyra_CloneGameMode::OnExperienceLoaded(const ULyra_CloneExperienceDefinition* CurrentExperience)
{
}

void ALyra_CloneGameMode::OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId)
{
	check(ExperienceId.IsValid());

	ULyra_Clone_ExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyra_Clone_ExperienceManagerComponent>();
	check(ExperienceComponent);
	ExperienceComponent->ServerSetCurrentExperience(ExperienceId);
}						
		