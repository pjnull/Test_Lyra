#pragma once

#include "GameFramework/PlayerState.h"
#include "LyraClonePlayerState.generated.h"


class ULyra_Clone_PawnData;
class ULyra_CloneExperienceDefinition;


UCLASS()
class ALyraClonePlayerState :public APlayerState
{
	GENERATED_BODY()
public:
	ALyraClonePlayerState(const FObjectInitializer& ObjectInitializer= FObjectInitializer::Get());


	virtual void PostInitializerComponent()final;

	void OnExperienceLoaded(const ULyra_CloneExperienceDefinition* CurrentExperience);

	
	void SetPawnData(const ULyra_Clone_PawnData* InPawnData);

	template<class T>
	const T* GetPawnData()const { return Cast<T>(PawnData); }

	

	
	UPROPERTY()
	TObjectPtr<const ULyra_Clone_PawnData>PawnData;

};