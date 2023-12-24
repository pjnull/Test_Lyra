#include "Lyra_Clone_ExperienceManagerComponent.h"
#include "Lyra_CloneExperienceDefinition.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameFeaturePluginOperationResult.h"
#include "../System/LyraCloneAssetManager.h"
#include "LyraClone_ExperienceActionSet.h"

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
	//FramNumber가 뭐하는 놈인가?
	static int32 OnExperienceLoadComplete_FrameNumber = GFrameNumber;

	check(LoadState==ELyraCloneExperienceLoadState::Loading);
	check(CurrentExperience);

	//이전 활성화된 GameFeature Plugin의 URL을 클리어해준다.
	GameFeaturePluginURLs.Reset();

	auto CollectGameFeaturePluginURLs = [This=this](const UPrimaryDataAsset* Context,const TArray<FString>& FeaturePluginList)
		{
			//FeaturePluginList를 순회하며, PluginURL을 ExperienceManagerComponent의
			//GameFeaturePluginURL에 추가한다.
			for (const FString& PluginName : FeaturePluginList)
			{
				FString PluginURL;
				if (UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, PluginURL))
				{
					This->GameFeaturePluginURLs.AddUnique(PluginURL);
				}
			}
		};
	//GameFeatureToEnable에 있는 Plugin만 활성화할 GameFeature Plugin후보군으로 등록
	CollectGameFeaturePluginURLs(CurrentExperience,CurrentExperience->GameFeaturesToEnable);

	//GameFeaturePluginURL에 등록된 Plugin을 로딩및 활성화
	NumGameFeaturePluginsLoading = GameFeaturePluginURLs.Num();
	if (NumGameFeaturePluginsLoading > 0)
	{
		LoadState = ELyraCloneExperienceLoadState::LoadingGameFeature;
		for (const FString& PluginURL : GameFeaturePluginURLs)
		{
			//매 Plugin이 로딩 및 활성화 이후 OnGameFeaturePluginLoadComplete 콜백 함수 등록
			UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(PluginURL, FGameFeaturePluginLoadComplete::CreateUObject(this, &ThisClass::OnGameFeaturePluginLoadComplete));
		}
	}
	else
	{	//해당 함수가 불리는 것은 StreamalbeDelegateDelayHelper에 의해 불림
		OnExperienceFullLoadCompleted();
	}
}

void ULyra_Clone_ExperienceManagerComponent::OnExperienceFullLoadCompleted()
{
	check(LoadState!=ELyraCloneExperienceLoadState::Loaded);
	
	{//GameFeature Plugin의 로딩과 활성화 이후, GameFeature Action들을 활성화시켜야함.
		LoadState = ELyraCloneExperienceLoadState::ExecutingActions;

		FGameFeatureActivatingContext Context;
		//GameFeatureAction 활성화를 위한 Context 준비
		{
			const FWorldContext* ExistWorldContext = GEngine->GetWorldContextFromWorld(GetWorld());
			//월드의 핸들을 세팅한다.
			if (ExistWorldContext)
			{
				Context.SetRequiredWorldContextHandle(ExistWorldContext->ContextHandle);
			}
		}
		auto ActivateListOfActions = [&Context](const TArray<UGameFeatureAction*>& ActionList) 
			{
				for (UGameFeatureAction* Action : ActionList)
				{
					if (Action)
					//명시적으로 GameFeatureAction에 대해 Registering->Loading->Activating순으로 호출
					{
						Action->OnGameFeatureRegistering();
						Action->OnGameFeatureLoading();
						Action->OnGameFeatureActivating(Context);
					}
				}
			};
		ActivateListOfActions(CurrentExperience->Actions);
		//Experience의 Action
		
		for (const TObjectPtr< ULyraClone_ExperienceActionSet>& ActionSet : CurrentExperience->ActionsSets)
		//Experience의 ActionSet
		{
			ActivateListOfActions(ActionSet->Actions);
		}

	}
	
	
	
	LoadState = ELyraCloneExperienceLoadState::Loaded;
	
	OnExperienceLoaded.Broadcast(CurrentExperience);
	OnExperienceLoaded.Clear();
}

void ULyra_Clone_ExperienceManagerComponent::OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& result)
{
	NumGameFeaturePluginsLoading--;
	if (NumGameFeaturePluginsLoading == 0)
	{
		//GameFeaturePlugin 로딩이 다 끝났을 경우, 기존의 Loaded로서 OnExpereinceFullLoadCompleted를 호출한다.
		//GameFeaturePlugin 로딩과 활성화가 끝나면 UGameFeatureAction을 활성화한다.
		OnExperienceFullLoadCompleted();
	}

}

const ULyra_CloneExperienceDefinition* ULyra_Clone_ExperienceManagerComponent::GetCurrentExperienceChecked() const
{
	check(LoadState==ELyraCloneExperienceLoadState::Loaded);
	check(CurrentExperience!=nullptr);
	return CurrentExperience;
}
