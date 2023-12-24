#include "LyraClone_EquipmentDefinition.h"
#include "LyraClone_EquipmentInstance.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_EquipmentDefinition)

ULyraClone_EquipmentDefinition::ULyraClone_EquipmentDefinition(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer)
{
	InstanceType = ULyraClone_EquipmentInstance::StaticClass();
}
