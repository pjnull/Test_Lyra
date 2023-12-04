// Copyright Epic Games, Inc. All Rights Reserved.


#include "Lyra_CloneGameMode.h"
#include "LryaCloneGameState.h"
#include "../Character/Lyra_Clone_Character.h"
#include "../Player/LyraClonePlayerState.h"
#include "../Player/LyraClonePlayerController.h"
#include "Lyra_Clone_ExperienceManagerComponent.h"
#include "../LryaCloneLogChannel.h"
#include "../Character/Lyra_Clone_PawnData.h"
#include "Lyra_CloneExperienceDefinition.h"
#include "../Character/Lyra_Clone_PawnExtensionComponent.h"
#include <Kismet/GameplayStatics.h>
#include "Engine/DataAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Lyra_CloneGameMode)


PRAGMA_DISABLE_OPTIMIZATION
ALyra_CloneGameMode::ALyra_CloneGameMode(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	GameStateClass = ALyaCloneGameState::StaticClass();
	PlayerControllerClass = ALyraClonePlayerController::StaticClass();
	PlayerStateClass = ALyraClonePlayerState::StaticClass();
	DefaultPawnClass = ALyra_Clone_Character::StaticClass();

}
PRAGMA_ENABLE_OPTIMIZATION



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
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save default player pawns into a map
	SpawnInfo.bDeferConstruction = true;
	if (UClass* PawnClass = GetDefaultPawnClassForController(Newplayer))
	{
		if (APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo))
		{
			if (ULyra_Clone_PawnExtensionComponent* PawnExtcomp = ULyra_Clone_PawnExtensionComponent::FindPawnExtensionComponent(SpawnedPawn))
			{
				if (const ULyra_Clone_PawnData* PawnData = GetPawnDataForController(Newplayer))
				{
					PawnExtcomp->SetPawnData(PawnData);
				}
			}
			SpawnedPawn->FinishSpawning(SpawnTransform);
			return SpawnedPawn;
		}
	}
	
	return nullptr;
}

void ALyra_CloneGameMode::HandleStartingNewPlayer_Implementation(APlayerController* Newplayer)
{
	if (IsExperienceLoaded())
	{
		Super::HandleStartingNewPlayer_Implementation(Newplayer);
	}
}

UClass* ALyra_CloneGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (const ULyra_Clone_PawnData* PawnData = GetPawnDataForController(InController))
	{
		if (PawnData->PawnClass)
		{
			return PawnData->PawnClass;
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}


void ALyra_CloneGameMode::HandleMatchAssignmentIfNotExpectingOne()
{
	FPrimaryAssetId ExperienceId;
	FString ExperienceIdSource;

	UWorld* World = GetWorld();


		if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(OptionsString, TEXT("Experience")))
		{
			const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString,TEXT("Experience"));
			ExperienceId = FPrimaryAssetId(FPrimaryAssetType(ULyra_CloneExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromOptions));
			ExperienceIdSource = TEXT("OptionsString");
		}
	
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
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Cast<APlayerController>(*Iterator);

		if (PC && PC->GetPawn() == nullptr)
		{
			if (PlayerCanRestart(PC))
			{
				RestartPlayer(PC);
			}
		}
	}
}

void ALyra_CloneGameMode::OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId)
{
	check(ExperienceId.IsValid());

	ULyra_Clone_ExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyra_Clone_ExperienceManagerComponent>();
	check(ExperienceComponent);
	ExperienceComponent->ServerSetCurrentExperience(ExperienceId);
}

const ULyra_Clone_PawnData* ALyra_CloneGameMode::GetPawnDataForController(const AController* InController) const
{
	if (InController)
	{
		if (const ALyraClonePlayerState* LyraPs = InController->GetPlayerState<ALyraClonePlayerState>())
		{	
			if (const ULyra_Clone_PawnData* PawnData = LyraPs->GetPawnData<ULyra_Clone_PawnData>())
				return PawnData;
		}
	}

	check(GameState);
	ULyra_Clone_ExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyra_Clone_ExperienceManagerComponent>();
	check(ExperienceComponent);

	if (ExperienceComponent->IsExperienceLoaded())
	{
		const ULyra_CloneExperienceDefinition* Experience = ExperienceComponent->GetCurrentExperienceChecked();

		if (Experience->DefaultPawnData != nullptr)
		{
			return Experience->DefaultPawnData;
		}
	}
	return nullptr;
}

