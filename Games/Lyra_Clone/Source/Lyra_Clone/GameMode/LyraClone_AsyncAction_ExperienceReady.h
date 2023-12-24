#pragma once
#include "Kismet/BlueprintAsyncActionBase.h"
#include "LyraClone_AsyncAction_ExperienceReady.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExperienceReadyAsyncDelegate);
class ULyra_CloneExperienceDefinition;

UCLASS()
class ULyraClone_AsyncAction_ExperienceReady:public UBlueprintAsyncActionBase
{
   GENERATED_BODY()
public:
   ULyraClone_AsyncAction_ExperienceReady(const FObjectInitializer&ObejctInitializer=FObjectInitializer::Get());
  
   
   UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
   static ULyraClone_AsyncAction_ExperienceReady* WaitForExperienceReady(UObject* WorldContextObject);

   virtual void Activate()override;



   void Step1_HandleGameStateSet(AGameStateBase* GameState);
   void Step2_ListenToExperienceLoading(AGameStateBase* GameState);
   void Step3_HandleExperienceLoaded(const ULyra_CloneExperienceDefinition* CurrentExperience);
   void Step4_BroadcastReady();
   
   UPROPERTY(BlueprintAssignable)//Bp에서 할당가능 
	FExperienceReadyAsyncDelegate OnReady;

   TWeakObjectPtr<UWorld>WorldPtr;



};
