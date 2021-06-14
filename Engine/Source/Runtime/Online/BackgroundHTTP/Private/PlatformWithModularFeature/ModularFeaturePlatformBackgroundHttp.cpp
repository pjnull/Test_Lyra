// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlatformWithModularFeature/ModularFeaturePlatformBackgroundHttp.h"

#include "GenericPlatform/GenericPlatformBackgroundHttp.h"

#include "Features/IModularFeatures.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"

IBackgroundHttpModularFeature* FModularFeaturePlatformBackgroundHttp::CachedModularFeature = nullptr;
bool FModularFeaturePlatformBackgroundHttp::bHasCheckedForModularFeature = false;

void FModularFeaturePlatformBackgroundHttp::Initialize()
{
	CacheModularFeature();

	if (nullptr != CachedModularFeature)
	{
		CachedModularFeature->Initialize();
	}
	else
	{
		FGenericPlatformBackgroundHttp::Initialize();
	}
}

void FModularFeaturePlatformBackgroundHttp::Shutdown()
{
	if (nullptr != CachedModularFeature)
	{
		CachedModularFeature->Shutdown();
	}
	else
	{
		FGenericPlatformBackgroundHttp::Shutdown();
	}
}

FBackgroundHttpManagerPtr FModularFeaturePlatformBackgroundHttp::CreatePlatformBackgroundHttpManager()
{
	CacheModularFeature();

	if (nullptr != CachedModularFeature)
	{
		return CachedModularFeature->CreatePlatformBackgroundHttpManager();
	}
	else
	{
		return FGenericPlatformBackgroundHttp::CreatePlatformBackgroundHttpManager();
	}
}

FBackgroundHttpRequestPtr FModularFeaturePlatformBackgroundHttp::ConstructBackgroundRequest()
{
	CacheModularFeature();

	if (nullptr != CachedModularFeature)
	{
		return CachedModularFeature->ConstructBackgroundRequest();
	}
	else
	{
		return FGenericPlatformBackgroundHttp::ConstructBackgroundRequest();
	}
}

FBackgroundHttpResponsePtr FModularFeaturePlatformBackgroundHttp::ConstructBackgroundResponse(int32 ResponseCode, const FString& TempFilePath)
{
	CacheModularFeature();

	if (nullptr != CachedModularFeature)
	{
		return CachedModularFeature->ConstructBackgroundResponse(ResponseCode, TempFilePath);
	}
	else
	{
		return FGenericPlatformBackgroundHttp::ConstructBackgroundResponse(ResponseCode, TempFilePath);
	}
}

FName FModularFeaturePlatformBackgroundHttp::GetModularFeatureName()
{
	static FName ModularFeatureName = FName(TEXT("BackgroundHttpModularFeature"));
	return ModularFeatureName;
}

void FModularFeaturePlatformBackgroundHttp::CacheModularFeature()
{
	if (!bHasCheckedForModularFeature)
	{
		bHasCheckedForModularFeature = true;

		FString CommandLineOverrideName;
		const bool bDidCommandLineOverride = FParse::Value(FCommandLine::Get(), TEXT("-BackgroundHttpModularFeatureNameOverride="), CommandLineOverrideName);
		
		//First get name of plugin from the .ini
		FString ModuleName;
		if (bDidCommandLineOverride)
		{
			ModuleName = CommandLineOverrideName;
		}
		else if (!GEngineIni.IsEmpty())
		{
			GConfig->GetString(TEXT("BackgroundHttp"), TEXT("PlatformModularFeatureName"), ModuleName, GEngineIni);
		}

		FModuleManager& ModuleManager = FModuleManager::Get();
		if (ModuleManager.ModuleExists(*ModuleName) && (false == ModuleManager.IsModuleLoaded(*ModuleName)))
		{
			ModuleManager.LoadModule(*ModuleName);
		}

		const bool bIsModularFeatureAvailable = (IModularFeatures::Get().IsModularFeatureAvailable(GetModularFeatureName()));
		if (bIsModularFeatureAvailable)
		{
			CachedModularFeature = &(IModularFeatures::Get().GetModularFeature<IBackgroundHttpModularFeature>(GetModularFeatureName()));
		}
		else
		{
			ensureAlwaysMsgf((ModuleName.IsEmpty()), TEXT("Unable to load expected Module %s! BackgroundHttp will fallback to generic implementation!"));
		}
	}
}