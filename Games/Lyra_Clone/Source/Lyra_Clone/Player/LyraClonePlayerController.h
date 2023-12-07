#pragma once
#include "../Plugins/ModularGameplayActors/Source/ModularGameplayActors/Public/ModularPlayerController.h"
#include "LyraClonePlayerController.generated.h"

UCLASS()
class ALyraClonePlayerController:public AModularPlayerController
{
	GENERATED_BODY()
public:
	ALyraClonePlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};