#include "LyraClone_EquipmentInstance.h"
#include "LyraClone_EquipmentDefinition.h"
#include "GameFramework/Character.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_EquipmentInstance)

ULyraClone_EquipmentInstance::ULyraClone_EquipmentInstance(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer)
{
}

APawn* ULyraClone_EquipmentInstance::GetPawn() const
{ 
	return Cast<APawn>(GetOuter());
}

APawn* ULyraClone_EquipmentInstance::GetTypedPawn(TSubclassOf<APawn> PawnType) const
{
	APawn* Result = nullptr;
	if (UClass* ActualPawnType = PawnType)
	{
		if (GetOuter()->IsA(ActualPawnType))
		{
			Result = Cast<APawn>(GetOuter());
		}
	}
	return Result;
}

void ULyraClone_EquipmentInstance::SpawnEquipmentActors(const TArray<FLyraClone_EquipmentActorToSpawn>& ActorsToSpawn)
{
	if (APawn* OwningPawn = GetPawn())
	{
		USceneComponent* AttachTarget = OwningPawn->GetRootComponent();
		if (ACharacter* Char = Cast<ACharacter>(OwningPawn))
		{
			AttachTarget = Char->GetMesh();

		}
		for (const FLyraClone_EquipmentActorToSpawn& SpawnInfo : ActorsToSpawn)
		{
			AActor* NewActor = GetWorld()->SpawnActorDeferred<AActor>(SpawnInfo.ActorToSpawn,FTransform::Identity,OwningPawn);
			NewActor->FinishSpawning(FTransform::Identity,true);
			NewActor->SetActorRelativeTransform(SpawnInfo.AttachTransform);
			NewActor->AttachToComponent(AttachTarget,FAttachmentTransformRules::KeepRelativeTransform,SpawnInfo.AttachSocket);
			
			SpawnedActors.Add(NewActor);
		}
	}
}

void ULyraClone_EquipmentInstance::DestroyEquipmentActors()
{
	for (AActor* Actor : SpawnedActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
}

void ULyraClone_EquipmentInstance::OnEquipped()
{
	K2_OnEquipped();
}

void ULyraClone_EquipmentInstance::OnUnEquipped()
{
	K2_OnUnEquipped();
}
