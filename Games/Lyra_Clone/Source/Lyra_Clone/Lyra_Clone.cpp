// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lyra_Clone.h"
#include "Modules/ModuleManager.h"
#include "System/LyraCloneAssetManager.h"


class FLyraCloneMoudule :public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule()override;
	virtual void ShutdownModule()override;

};

void FLyraCloneMoudule::StartupModule()
{
	ULyraCloneAssetManager::TestClone();	
}

void FLyraCloneMoudule::ShutdownModule()
{
}


IMPLEMENT_PRIMARY_GAME_MODULE(FLyraCloneMoudule, Lyra_Clone, "Lyra_Clone" );