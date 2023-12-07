#pragma once
#include "LyraClone_WeaponInstance.h"
#include "LyraClone_RangedWeapon_Instance.generated.h"

UCLASS()
class ULyraClone_RangedWeapon_Instance:public ULyraClone_WeaponInstance
{
   GENERATED_BODY()
public:
   ULyraClone_RangedWeapon_Instance(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());
};
