#pragma once
#include "LyraClone_Animinstance.generated.h"

UCLASS()
class ULyraClone_Animinstance:public UAnimInstance
{
   GENERATED_BODY()
public:
   ULyraClone_Animinstance(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());

protected:
   UPROPERTY(BlueprintReadOnly, Category = "Character State Data")
   float GroundDistance = -1.0f;
};
