#include "LyraClonePlayerController.h"
#include "../Camera/LyraClone_PlayerCameraManager.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClonePlayerController)



ALyraClonePlayerController::ALyraClonePlayerController(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	PlayerCameraManagerClass = ALyraClone_PlayerCameraManager::StaticClass();
}