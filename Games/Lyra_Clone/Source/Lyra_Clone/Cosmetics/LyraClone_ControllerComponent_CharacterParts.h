 
#pragma once
#include "LyraClone_CharacterPartTypes.h"
#include "Components/ControllerComponent.h"
#include "LyraClone_PawnComponent_CharacterParts.h"
#include "LyraClone_ControllerComponent_CharacterParts.generated.h"

class ULyraClone_ControllerComponent_CharacterParts;

USTRUCT()
struct FLyraClone_Controller_CharacterPartEntry
{
	GENERATED_BODY()
public:

	//Character Part에 대한 정의(메타데이터==MetaData?)
	//Actor의 Class를 가지고 있고, class별로 어떠한 것을 가지고 있나를 정의
	UPROPERTY(EditAnywhere)
	FLyraClone_CharacterPart Part;


	FLyraClone_CharacterPartHandle Handle;

};



UCLASS(meta=(BlueprintSpawnableComponent))
class ULyraClone_ControllerComponent_CharacterParts:public UControllerComponent
{
   GENERATED_BODY()
public:
   ULyraClone_ControllerComponent_CharacterParts(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());

   ULyraClone_PawnComponent_CharacterParts* GetPawnCustomizer()const;

   virtual void BeginPlay()override;
   virtual void EndPlay(const EEndPlayReason::Type EndPlayReason)override;



   void AddCharacterPartInternal(const FLyraClone_CharacterPart& NewPart);
   
   void RemoveAllCharacterParts();


   UFUNCTION(BlueprintCallable, Category = Cosmetics)
   void AddCharacterPart(const FLyraClone_CharacterPart& NewPart);

   UFUNCTION()
   void OnPossessedPawnChanged(APawn* Oldpawn, APawn* Newpawn);
   //UFUNCTION으로 정의하지 않으면 AddDynamic이 안된다.

   UPROPERTY(EditAnywhere, Category = Cosmetics)
   TArray<FLyraClone_Controller_CharacterPartEntry>CharacterParts;


};
