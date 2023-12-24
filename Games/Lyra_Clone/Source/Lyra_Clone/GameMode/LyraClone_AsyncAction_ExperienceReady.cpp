#include "LyraClone_AsyncAction_ExperienceReady.h"
#include "Lyra_Clone_ExperienceManagerComponent.h"
#include "Lyra_CloneExperienceDefinition.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraClone_AsyncAction_ExperienceReady)

ULyraClone_AsyncAction_ExperienceReady::ULyraClone_AsyncAction_ExperienceReady(const FObjectInitializer& ObejctInitializer)
	:Super(ObejctInitializer)
{
}

ULyraClone_AsyncAction_ExperienceReady* ULyraClone_AsyncAction_ExperienceReady::WaitForExperienceReady(UObject* WorldContextObject)
{
	ULyraClone_AsyncAction_ExperienceReady* Action = nullptr;
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		Action = NewObject<ULyraClone_AsyncAction_ExperienceReady>();
		Action->WorldPtr = World;
		Action->RegisterWithGameInstance(World);
	}
	return Action;
}

void ULyraClone_AsyncAction_ExperienceReady::Activate()
{
	if (UWorld* World = WorldPtr.Get())
	{
		if (AGameStateBase* GameState = World->GetGameState())
		{
			Step2_ListenToExperienceLoading(GameState);
		}
		else
		{
			World->GameStateSetEvent.AddUObject(this, &ThisClass::Step1_HandleGameStateSet);
		}

	}
	else
	{
		SetReadyToDestroy();
	}
}

void ULyraClone_AsyncAction_ExperienceReady::Step1_HandleGameStateSet(AGameStateBase* GameState)
{
	if (UWorld* World = WorldPtr.Get())
	{
		World->GameStateSetEvent.RemoveAll(this);
	}
	Step2_ListenToExperienceLoading(GameState);
}

void ULyraClone_AsyncAction_ExperienceReady::Step2_ListenToExperienceLoading(AGameStateBase* GameState)
{
	check(GameState);

	ULyra_Clone_ExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyra_Clone_ExperienceManagerComponent>();
	check(ExperienceComponent);

	if (ExperienceComponent->IsExperienceLoaded())
	{
		UWorld* World = GameState->GetWorld();
		check(World);

		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::Step4_BroadcastReady));
	}
	else
	{
		ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnLyraClonExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::Step3_HandleExperienceLoaded));
	}
}

void ULyraClone_AsyncAction_ExperienceReady::Step3_HandleExperienceLoaded(const ULyra_CloneExperienceDefinition* CurrentExperience)
{
	Step4_BroadcastReady();
}

void ULyraClone_AsyncAction_ExperienceReady::Step4_BroadcastReady()
{
	OnReady.Broadcast();
	SetReadyToDestroy();
}
