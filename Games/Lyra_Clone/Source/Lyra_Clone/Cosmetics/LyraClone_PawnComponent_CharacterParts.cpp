
#include "LyraClone_PawnComponent_CharacterParts.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "Components/SceneComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_PawnComponent_CharacterParts)

ULyraClone_PawnComponent_CharacterParts::ULyraClone_PawnComponent_CharacterParts(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer),CharacterPartList(this)
{

}

USkeletalMeshComponent* ULyraClone_PawnComponent_CharacterParts::GetParentMeshComponent() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (ACharacter* OwningCharacter = Cast<ACharacter>(OwnerActor))
		{
			if (USkeletalMeshComponent* MeshComponent = OwningCharacter->GetMesh())
			{
				return MeshComponent;
			}
		}
	}
	return nullptr;
}

USceneComponent* ULyraClone_PawnComponent_CharacterParts::GetSceneComponentToAttachTo() const
{
	if (USkeletalMeshComponent* MeshComponent = GetParentMeshComponent())
	{
		return MeshComponent;
	}
	else if (AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetRootComponent();
	}
	return nullptr;
}

void ULyraClone_PawnComponent_CharacterParts::BroadcastChanged()
{
	const bool bReinitPose = true;

	if (USkeletalMeshComponent* MeshComponent = GetParentMeshComponent())
	{//현재 Owner의 SkeletalmeshComponent가 존재하는가?
		const FGameplayTagContainer MergedTags = GetCombinedTags(FGameplayTag());
		//BodyMeshes를 통해 GameplayTag를 활용하여 알맞은 SkeletalMesh로 재설정
		
		USkeletalMesh* DesiredMesh = BodyMeshes.SelectBestBodyStyle(MergedTags);
		
		MeshComponent->SetSkeletalMesh(DesiredMesh,bReinitPose);

		if (UPhysicsAsset* PhysicsAsset = BodyMeshes.ForcedPhysicsAsset)
		{
			MeshComponent->SetPhysicsAsset(PhysicsAsset, bReinitPose);
		}
	
	}
}

FLyraClone_CharacterPartHandle ULyraClone_PawnComponent_CharacterParts::AddCharacterPart(const FLyraClone_CharacterPart& NewPart)
{
	return CharacterPartList.AddEntry(NewPart);
}

void ULyraClone_PawnComponent_CharacterParts::RemoveCharacterParts(FLyraClone_CharacterPartHandle Handle)
{
	
	return CharacterPartList.RemoveEntry(Handle);

}

FGameplayTagContainer ULyraClone_PawnComponent_CharacterParts::GetCombinedTags(FGameplayTag RequiredPrefix) const
{
	FGameplayTagContainer Result = CharacterPartList.CollectCombinedTags();
	if (RequiredPrefix.IsValid())
	{
		return Result.Filter(FGameplayTagContainer(RequiredPrefix));
	}

	return FGameplayTagContainer();
}

bool FLyraClone_CharacterPartList::SpawnActorForEntry(FLyraClone_AppliedCharacterPartEntry& Entry)
{
	bool bCreatedAnyActor = false;
	
	if (Entry.part.PartClass != nullptr)
	{   //전달된 AppliedCharacterPartEntry의 PartClass의 세팅 검증
		UWorld* World = OwnerComponent->GetWorld();
		

		//GetSceneComponentToAttachTo=PawnComponent_CharacterParts에 붙일 Component를 정한다.
		if (USceneComponent* ComponentToAttachTo=OwnerComponent->GetSceneComponentToAttachTo())
		{
			const FTransform SpawnTransform = ComponentToAttachTo->GetSocketTransform(Entry.part.SocketName);
			//ComponentToAttachTo의 Bone/SocketName을 통해 Transform을 찾는다.

			UChildActorComponent* PartComponent = NewObject<UChildActorComponent>(OwnerComponent->GetOwner());
			//Actor-Actor의 결합이므로 ChildActorComponent를 활용한다.
			
			PartComponent->SetupAttachment(ComponentToAttachTo,Entry.part.SocketName);
			PartComponent->SetChildActorClass(Entry.part.PartClass);
			PartComponent->RegisterComponent();
			//RegisterComponent를 통해 마지막으로 RenderWorld인 FScene에 변경 내용을 전달한다

			if (AActor* SpawnedActor = PartComponent->GetChildActor())
			{//ChildActor에서 Actor가 반환되면 실행
				if (USceneComponent* SpawnedRootComponent = SpawnedActor->GetRootComponent())
				{//해당 Actor가 Parent인 PawnComponentCharacterParts의 
				 //OwenrActor보다 먼저Tick이 실행되지 않도록 선행조건을 붙인다.
					SpawnedRootComponent->AddTickPrerequisiteComponent(ComponentToAttachTo);
				}
			}
			Entry.SpawnedComponent = PartComponent;
			bCreatedAnyActor = true;
		}
	}
	return bCreatedAnyActor;
}

void FLyraClone_CharacterPartList::DestroyActorForEntry(FLyraClone_AppliedCharacterPartEntry& Entry)
{
	if (Entry.SpawnedComponent)
	{
		Entry.SpawnedComponent->DestroyComponent();
		Entry.SpawnedComponent = nullptr;
	}
}

FLyraClone_CharacterPartHandle FLyraClone_CharacterPartList::AddEntry(FLyraClone_CharacterPart NewPart)
{
	FLyraClone_CharacterPartHandle Result;
	Result.PartHandle = PartHandleCount++;

	if (ensure(OwnerComponent && OwnerComponent->GetOwner() && OwnerComponent->GetOwner()->HasAuthority()))
	{
		FLyraClone_AppliedCharacterPartEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.part = NewPart;
		NewEntry.PartHandle = Result.PartHandle;


		//실제로 Actor를 생성하고 OwnerComponent의 Owner Actor에 Actor끼리 RootComponent에 붙인다.
		if (SpawnActorForEntry(NewEntry))
		{
			OwnerComponent->BroadcastChanged();
			//BroadcastChanged를 통해서 OwnerComponent에서 Owner의 SkeletalMeshComponent를 활용하여
			//Animation 및 Physics를 Re-Initialize해준다.
		}
	}

	return Result;
}

void FLyraClone_CharacterPartList::RemoveEntry(FLyraClone_CharacterPartHandle Handle)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FLyraClone_AppliedCharacterPartEntry& Entry = *EntryIt;

		if (Entry.PartHandle == Handle.PartHandle)
		{
			DestroyActorForEntry(Entry);
		}
	}
}

FGameplayTagContainer FLyraClone_CharacterPartList::CollectCombinedTags() const
{
	FGameplayTagContainer Result;
	
	for (const FLyraClone_AppliedCharacterPartEntry& Entry : Entries)
	{//SpanwedComponent에 캐싱되어 있다면
		if (Entry.SpawnedComponent)
		{
			if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Entry.SpawnedComponent->GetChildActor()))
			{
				//해당Actor의 IGameplayTagAssetInterface를 통해 GameplayTag를 검색
				//TaggedActor는 IGameplayTagAssetInterface를 상속받고 있지 않고있다.
				//각 파츠에 GameplayTag를 넣고 싶으면 인터페이스를 상속받아서 정의해라
				//ex)레벨제한 Lv100
				TagInterface->GetOwnedGameplayTags(Result);
			}
		}
	}
	return FGameplayTagContainer();
}
