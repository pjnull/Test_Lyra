#pragma once

#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "GameplayTagContainer.h"

class UGameplayTagsManager;


struct FLyraClone_GameplayTag
{
	static const FLyraClone_GameplayTag& Get() { return GameplayTags; }
	static void InitializeNativeTags();

	void AddTag(FGameplayTag& OutTag,const ANSICHAR* TagName,const ANSICHAR* TagComment);
	void AddAllTags(UGameplayTagsManager& Manager);


	//InitState
	FGameplayTag InitState_Spawned;
	FGameplayTag InitState_DataAvailable;
	FGameplayTag InitState_DataInitialized;
	FGameplayTag InitState_GameplayReady;
	


	//Input

	FGameplayTag InputTag_Move;
	FGameplayTag InputTag_Look_Mouse;



private:
	static FLyraClone_GameplayTag GameplayTags;


};