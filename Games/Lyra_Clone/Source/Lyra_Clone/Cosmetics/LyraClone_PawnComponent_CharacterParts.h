 
#pragma once
#include "LyraClone_CharacterPartTypes.h"
#include "LyraClone_CosmeticAnimationTypes.h"
#include "Components/PawnComponent.h"
#include "LyraClone_PawnComponent_CharacterParts.generated.h"


USTRUCT()
struct FLyraClone_AppliedCharacterPartEntry
{
	GENERATED_BODY()

	UPROPERTY()	
	FLyraClone_CharacterPart part;
	//CharacterParts의 메타데이터


	UPROPERTY()
	int32 PartHandle = INDEX_NONE;
	//PartList에서 할당받은 Part핸들값 


	UPROPERTY()
	TObjectPtr<UChildActorComponent>SpawnedComponent = nullptr;

	

};

USTRUCT(BlueprintType)
struct FLyraClone_CharacterPartList
{
   GENERATED_BODY()

   FLyraClone_CharacterPartList():OwnerComponent(nullptr){}
   FLyraClone_CharacterPartList(ULyraClone_PawnComponent_CharacterParts* InOwnerComponent ):OwnerComponent(InOwnerComponent){}


   bool SpawnActorForEntry(FLyraClone_AppliedCharacterPartEntry& Entry);
   void DestroyActorForEntry(FLyraClone_AppliedCharacterPartEntry& Entry);


   FLyraClone_CharacterPartHandle AddEntry(FLyraClone_CharacterPart NewPart);
   void RemoveEntry(FLyraClone_CharacterPartHandle Handle);
   FGameplayTagContainer CollectCombinedTags()const;

   UPROPERTY()
   TArray<FLyraClone_AppliedCharacterPartEntry>Entries;
   //현재 인스턴스화된 Character part

   UPROPERTY()
	TObjectPtr<ULyraClone_PawnComponent_CharacterParts>OwnerComponent;
	//해당 PartList의 Owner인 PawnComponent

   int32 PartHandleCount = 0;

};



UCLASS(meta=(BlueprintSpawnableComponent))
class ULyraClone_PawnComponent_CharacterParts:public UPawnComponent
{
   GENERATED_BODY()
public:
   ULyraClone_PawnComponent_CharacterParts(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());

   USkeletalMeshComponent* GetParentMeshComponent()const;
   USceneComponent* GetSceneComponentToAttachTo()const;
   void BroadcastChanged();
   
   
   FLyraClone_CharacterPartHandle AddCharacterPart(const FLyraClone_CharacterPart& NewPart);
   void RemoveCharacterParts(FLyraClone_CharacterPartHandle Handle);
   FGameplayTagContainer GetCombinedTags(FGameplayTag RequiredPrefix)const;


   UPROPERTY()
	FLyraClone_CharacterPartList CharacterPartList;
	//인스턴스화 된 CharacterParts



	UPROPERTY(EditAnywhere, Category = Cosmetics)
	FLyraClone_AnimBodyStyleSelectionSet BodyMeshes;
	//애니메이션 적용을 위한 메시와 연결고리


};
