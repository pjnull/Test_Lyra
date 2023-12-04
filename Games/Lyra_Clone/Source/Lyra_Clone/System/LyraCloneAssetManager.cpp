#include "LyraCloneAssetManager.h"
#include "Lyra_Clone/LyraCloneLogChannel.h"
#include "../LyraClone_GameplayTag.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCloneAssetManager)

ULyraCloneAssetManager::ULyraCloneAssetManager()
{
}

ULyraCloneAssetManager& ULyraCloneAssetManager::Get()
{
	check(GEngine)
	
	if(ULyraCloneAssetManager* singleton=Cast<ULyraCloneAssetManager>(GEngine->AssetManager))
	{
		return *singleton;
	}
	UE_LOG(LogClone, Fatal, TEXT("singleton Faild"));

	return *NewObject<ULyraCloneAssetManager>();
}


PRAGMA_DISABLE_OPTIMIZATION
//Release 최적화단계 끔
bool ULyraCloneAssetManager::TestClone()
{

	static bool bTest = ShouldLogAssetLoads();
	return true;

}

bool ULyraCloneAssetManager::ShouldLogAssetLoads()
{
	const TCHAR* CommandLineContent = FCommandLine::Get();
	static bool bLogAssetLoads = FParse::Param(CommandLineContent, TEXT("LogAssetLoads"));
	return bLogAssetLoads;
}
UObject* ULyraCloneAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (AssetPath.IsValid())
	{
		TUniquePtr<FScopeLogTime>LogTimePtr;
		if (ShouldLogAssetLoads())
		{
			LogTimePtr = MakeUnique<FScopeLogTime>(*FString::Printf(TEXT("sync load asset[%s]"),*AssetPath.ToString()),nullptr,FScopeLogTime::ScopeLog_Seconds);
		}

		if (UAssetManager::IsValid())
		{
			return UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath);
		}
		return AssetPath.TryLoad();
	}
	
	return nullptr;

}

void ULyraCloneAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (ensureAlways(Asset))
	{
		FScopeLock Lock(&SyncObject);
		LoadedAssets.Add(Asset);
	}
}

void ULyraCloneAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	UE_LOG(LogClone,Display,TEXT("StartInitialLoading"));

	FLyraClone_GameplayTag::InitializeNativeTags();
}


PRAGMA_ENABLE_OPTIMIZATION