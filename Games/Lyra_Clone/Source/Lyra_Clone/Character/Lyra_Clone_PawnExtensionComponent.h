#pragma once

#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "Lyra_Clone_PawnExtensionComponent.generated.h"


class ULyra_Clone_PawnData;

UCLASS()
//초기화전반을 담당하는 컴포넌트
class ULyra_Clone_PawnExtensionComponent :public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()
public:
	
	ULyra_Clone_PawnExtensionComponent(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get());


	static ULyra_Clone_PawnExtensionComponent* FindPawnExtensionComponent(const APawn* Actor) { return (Actor?Actor->FindComponentByClass<ULyra_Clone_PawnExtensionComponent>():nullptr); }
	
	template<class T>
	const T* GetPawnData() const{ return Cast<T>(PawnData); }
	void SetPawnData(const ULyra_Clone_PawnData* InPawnData);
	void SetupPlayerInputComponent();
	

	static const FName NAME_ActorFeatureName;
	//PawnCOmponent를 상속받은 Component가 바인딩을 하기 위해서는 GameFeature의 이름이 있어야한다. 
	virtual void OnRegister()final;
	virtual void BeginPlay()override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason)final;



	virtual FName GetFeatureName()const final { return NAME_ActorFeatureName; }
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params)final;
	virtual	bool CanChangeInitState(UGameFrameworkComponentManager* Manager,FGameplayTag CurrentState,FGameplayTag DesiredState)const final;
	virtual void CheckDefaultInitialization()final;

	UPROPERTY(EditInstanceOnly, Category = "Clone|Pawn")
	TObjectPtr<const ULyra_Clone_PawnData>PawnData;


};