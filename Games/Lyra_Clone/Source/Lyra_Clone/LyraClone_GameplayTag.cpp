#include "LyraClone_GameplayTag.h"
#include "LyraCloneLogChannel.h"
#include "GameplayTagsManager.h"

FLyraClone_GameplayTag FLyraClone_GameplayTag::GameplayTags;


void FLyraClone_GameplayTag::InitializeNativeTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	GameplayTags.AddAllTags(Manager);

}

void FLyraClone_GameplayTag::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName),FString(TEXT("(Native)")) + FString(TagComment));

	
}

void FLyraClone_GameplayTag::AddAllTags(UGameplayTagsManager& Manager)
{
	AddTag(InitState_Spawned,"InitState.Spawned","1:Actor/Component has initially spawned +can be extend");
	AddTag(InitState_DataAvailable,"InitState.DataAvailable","2:all required");
	AddTag(InitState_DataInitialized,"InitState.DataInitialized","3:The available data");
	AddTag(InitState_GameplayReady,"InitState.GameplayReady","4:The actor/component");
}
