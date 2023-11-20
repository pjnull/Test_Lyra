#include "Lyra_Clone_Character.h"
#include "Lyra_Clone_PawnExtensionComponent.h"
#include "../Camera/LyraClone_CameraComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(Lyra_Clone_Character)

ALyra_Clone_Character::ALyra_Clone_Character(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	//기본적으로 Tick을 꺼야된다=>이벤트방식으로 할것이며, 상시로 확인하는 컴포넌트는 아님	
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bCanEverTick = false;


	//Extension 생성
	PawnExtComponent = CreateDefaultSubobject<ULyra_Clone_PawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	
	CameraComponent = CreateDefaultSubobject<ULyraClone_CameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetRelativeLocation(FVector(-300.f,0.0f,75.0f));

}

void ALyra_Clone_Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PawnExtComponent->SetupPlayerInputComponent();
}


