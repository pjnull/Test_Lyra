// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Lyra_CloneGameMode.generated.h"

/**
 * 
 */

class ULyra_CloneExperienceDefinition;
class ULyra_Clone_PawnData;

UCLASS()
class LYRA_CLONE_API ALyra_CloneGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ALyra_CloneGameMode(const FObjectInitializer& ObjectInitializer);

	virtual void InitGame(const FString& Mapname, const FString& option, FString& ErrorMessage)final;
	virtual void InitGameState()final;

	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* Newplayer,const FTransform& SpawnTransform)final;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* Newplayer)final;

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController)final;
	
	void HandleMatchAssignmentIfNotExpectingOne();
	bool IsExperienceLoaded()const;
	void OnExperienceLoaded(const ULyra_CloneExperienceDefinition* CurrentExperience);
	
	void OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId);

	const ULyra_Clone_PawnData* GetPawnDataForController(const AController* InController)const;
	
	
};
