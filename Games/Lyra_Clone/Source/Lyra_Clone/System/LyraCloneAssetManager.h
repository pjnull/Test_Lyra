#pragma once


#include "Engine/AssetManager.h"
#include "LyraCloneAssetManager.generated.h"

UCLASS()
class LYRA_CLONE_API ULyraCloneAssetManager :public UAssetManager
{
	GENERATED_BODY()
public:
	ULyraCloneAssetManager();

	static ULyraCloneAssetManager& Get();

	static bool TestClone();//에디터를 열지않고 테스트를 할수 있는 함수

	static bool ShouldLogAssetLoads();

	static UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);

	template <typename AssetType>
	static AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssestPointer, bool bKeepInMemory = true);
	
	template <typename AssetType>
	static TSubclassOf<AssetType>GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);
	//BP

	void AddLoadedAsset(const UObject* Asset);

	virtual void StartInitialLoading()final;



	TSet<TObjectPtr<const UObject>>LoadedAssets;
	FCriticalSection SyncObject;
};


template <typename AssetType>
AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssestPointer, bool bKeepInMemory)
{
	AssetType* LoadAsset = nullptr;
	const TSoftObjectPtr& Assetpath=AssestPointer.ToSoftObjectPath();

	if (Assetpath.IsVaild())
	{
		LoadAsset = AssestPointer.Get();

		if (!LoadAsset)
		{
			LoadAsset = Cast<AssetType>(SynchronousLoadAsset(Assetpath));
			ensureAlwaysMsgf(LoadAsset,TEXT("Faild [%s]"),*AssestPointer.ToString());
		}
		if (LoadAsset && bKeepInMemory)
		{
			Get().AddLoadedAsset(Cast<UObject>(LoadAsset));

		}
	}
	return LoadAsset;
}

template <typename AssetType>
static TSubclassOf<AssetType>GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory)
{

}
