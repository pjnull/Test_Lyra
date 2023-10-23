#pragma once
#include "GameFramework/PlayerController.h"
#include "LyraClonePlayerController.generated.h"

UCLASS()
class ALyraClonePlayerController:public APlayerController
{
	GENERATED_BODY()
public:
	ALyraClonePlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};