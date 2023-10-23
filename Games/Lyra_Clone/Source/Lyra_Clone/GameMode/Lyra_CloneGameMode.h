// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Lyra_CloneGameMode.generated.h"

/**
 * 
 */

class ULyra_CloneExperienceDefinition;

UCLASS()
class LYRA_CLONE_API ALyra_CloneGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ALyra_CloneGameMode(const FObjectInitializer& ObjectInitializer);

	virtual void InitGame(const FString& Mapname, const FString& option, FString& ErrorMessage)final;
	virtual void InitGameState()final;


	void HandleMatchAssignmentIfNotExpectingOne();

	void OnExperienceLoaded(const ULyra_CloneExperienceDefinition* CurrentExperience);
};
