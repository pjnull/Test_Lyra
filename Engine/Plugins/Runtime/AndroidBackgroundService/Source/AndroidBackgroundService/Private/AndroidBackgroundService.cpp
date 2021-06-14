// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidBackgroundService.h"

#include "Async/TaskGraphInterfaces.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ConfigCacheIni.h"

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Android/AndroidEventManager.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "HAL/PlatformFile.h"

DEFINE_LOG_CATEGORY(LogAndroidBackgroundService);

void FAndroidBackgroundServiceModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	UE_LOG(LogAndroidBackgroundService, Error, TEXT("STARTED UP!"));
}


void FAndroidBackgroundServiceModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

IMPLEMENT_MODULE(FAndroidBackgroundServiceModule, AndroidBackgroundService);