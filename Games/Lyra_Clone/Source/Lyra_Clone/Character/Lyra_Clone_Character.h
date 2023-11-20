#pragma once
#include "GameFramework/Character.h"
#include "Lyra_Clone_Character.generated.h"


class ULyra_Clone_PawnExtensionComponent;
class ULyraClone_CameraComponent;
UCLASS()

class ALyra_Clone_Character :public ACharacter
{
	GENERATED_BODY()
public:
	ALyra_Clone_Character(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)final;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clone|Character")
	TObjectPtr<ULyra_Clone_PawnExtensionComponent>PawnExtComponent;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clone|Character")
	TObjectPtr<ULyraClone_CameraComponent>CameraComponent;

};