#include "Lyra_Clone_ExperienceManagerComponent.h"
#include "Lyra_CloneExperienceDefinition.h"
#include "GameFeaturesSubsystemSettings.h"
#include "../System/LyraCloneAssetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Lyra_Clone_ExperienceManagerComponent)

ULyra_Clone_ExperienceManagerComponent::ULyra_Clone_ExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}

void ULyra_Clone_ExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FOnLyraClonExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded.Add(MoveTemp(Delegate));
	}
}

void ULyra_Clone_ExperienceManagerComponent::ServerSetCurrentExperience(FPrimaryAssetId ExperienceId)
{
	ULyraCloneAssetManager& AssetManager = ULyraCloneAssetManager::Get();

	TSubclassOf<ULyra_CloneExperienceDefinition>AssetClass;
	{
		FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(ExperienceId);
		AssetClass = Cast<UClass>(AssetPath.TryLoad());
	}

	const ULyra_CloneExperienceDefinition* Experience = GetDefault< ULyra_CloneExperienceDefinition>(AssetClass);
	check(Experience!=nullptr);
	check(CurrentExperience==nullptr);

	{
		CurrentExperience = Experience;
	}
	StartExperienceLoad();
}

void ULyra_Clone_ExperienceManagerComponent::StartExperienceLoad()
{
	check(CurrentExperience);
	check(LoadState == ELyraCloneExperienceLoadState::Unloaded);

	LoadState = ELyraCloneExperienceLoadState::Loading;

	ULyraCloneAssetManager& AssetManager = ULyraCloneAssetManager::Get();

	TSet<FPrimaryAssetId>BundleAssetList;

	BundleAssetList.Add(CurrentExperience->GetPrimaryAssetId());

	TArray<FName>BundlesToLoad;
	{
		const ENetMode OwnerNetMode = GetOwner()->GetNetMode();
		bool bLoadedClient = GIsEditor || (OwnerNetMode!=NM_DedicatedServer);
		bool bLoadedServer = GIsEditor || (OwnerNetMode!=NM_Client);
	
		if (bLoadedClient)
		{
			BundlesToLoad.Add(UGameFeaturesSubsystemSettings::LoadStateClient);
		}
		if (bLoadedServer)
		{
			BundlesToLoad.Add(UGameFeaturesSubsystemSettings::LoadStateServer);
		}
	}

	FStreamableDelegate OnAssetsLoadedDelegate = FStreamableDelegate::CreateUObject(this,&ThisClass::OnExperienceLoadComplete);

	TSharedPtr<FStreamableHandle>Handle = AssetManager.ChangeBundleStateForPrimaryAssets
	(
		BundleAssetList.Array(),
		BundlesToLoad,
		{},false,FStreamableDelegate(),FStreamableManager::AsyncLoadHighPriority
	);

	if (!Handle.IsValid() || Handle->HasLoadCompleted())
	{
		FStreamableHandle::ExecuteDelegate(OnAssetsLoadedDelegate);
	}
	else
	{
		Handle->BindCompleteDelegate(OnAssetsLoadedDelegate);
		Handle->BindCompleteDelegate(FStreamableDelegate::CreateLambda
		(
			[OnAssetsLoadedDelegate]() {OnAssetsLoadedDelegate.ExecuteIfBound(); }
		));
	}

	static int32 StartExperienceLoad_FrameNumber = GFrameNumber;
}

void ULyra_Clone_ExperienceManagerComponent::OnExperienceLoadComplete()
{
	static int32 OnExperienceLoadComplete_FrameNumber = GFrameNumber;

	OnExperienceFullLoadCompleted();
}

void ULyra_Clone_ExperienceManagerComponent::OnExperienceFullLoadCompleted()
{
	check(LoadState!=ELyraCloneExperienceLoadState::Loaded);

	LoadState = ELyraCloneExperienceLoadState::Loaded;
	
	OnExperienceLoaded.Broadcast(CurrentExperience);
	OnExperienceLoaded.Clear();
}
