// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/TargetPlatformBase.h"
#include "HAL/IConsoleManager.h"
#include "DeviceBrowserDefaultPlatformWidgetCreator.h"
#include "Interfaces/IProjectBuildMutatorFeature.h"
#include "Features/IModularFeatures.h"
#include "Interfaces/IProjectManager.h"
#include "Interfaces/IPluginManager.h"
#include "ProjectDescriptor.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "TargetPlatform"

bool FTargetPlatformBase::UsesForwardShading() const
{
	static IConsoleVariable* CVarForwardShading = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForwardShading"));
	return CVarForwardShading ? (CVarForwardShading->GetInt() != 0) : false;
}

bool FTargetPlatformBase::UsesDBuffer() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DBuffer"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

bool FTargetPlatformBase::UsesBasePassVelocity() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BasePassOutputsVelocity"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

bool FTargetPlatformBase::VelocityEncodeDepth() const
{
	return true;
}

bool FTargetPlatformBase::UsesSelectiveBasePassOutputs() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SelectiveBasePassOutputs"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

bool FTargetPlatformBase::UsesDistanceFields() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFields"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

bool FTargetPlatformBase::UsesRayTracing() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

bool FTargetPlatformBase::ForcesSimpleSkyDiffuse() const
{
	return false;
}

float FTargetPlatformBase::GetDownSampleMeshDistanceFieldDivider() const
{
	return 1.0f;
}

int32 FTargetPlatformBase::GetHeightFogModeForOpaque() const
{
	// Don't override the project setting by default
	// Platforms wish to support override need to implement the logic in their own target platform classes
	return 0;
}

bool FTargetPlatformBase::UsesMobileAmbientOcclusion() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.AmbientOcclusion"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

static bool IsPluginEnabledForTarget(const IPlugin& Plugin, const FProjectDescriptor* Project, const FString& Platform, EBuildConfiguration Configuration, EBuildTargetType TargetType)
{
	if (!Plugin.GetDescriptor().SupportsTargetPlatform(Platform))
	{
		return false;
	}

	// TODO: Support transitive calculation of per-platform disabling for plugins.
	// Plugins can reference other plugins, and it would be nice to be able to automatically disable for platform X
	// plugins that are only referenced through another plugin that is disabled for platform X.
	// For the time-being, to disable a transitively referenced plugin per-platform, the project has to 
	// directly include the plugin.

	if (Project != nullptr)
	{
		const FString& PluginName = Plugin.GetName();
		const FPluginReferenceDescriptor* PluginReference = Project->Plugins.FindByPredicate(
			[&PluginName](const FPluginReferenceDescriptor& ExistingReference)
			{
				return ExistingReference.Name == PluginName;
			});
		if (PluginReference)
		{
			bool bEnabledForProject = PluginReference->IsEnabledForPlatform(Platform) &&
				(Configuration == EBuildConfiguration::Unknown || PluginReference->IsEnabledForTargetConfiguration(Configuration)) &&
				PluginReference->IsEnabledForTarget(TargetType);
			if (!bEnabledForProject)
			{
				return false;
			}
		}
	}
	return true;
}

bool FTargetPlatformBase::RequiresTempTarget(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FText& OutReason) const
{
	// check to see if we already have a Target.cs file
	if (bProjectHasCode)
	{
		return false;
	}

	// check if asset nativization is enabled
	if (bRequiresAssetNativization)
    {
		OutReason = LOCTEXT("TempTarget_Nativization", "asset nativization is enabled");
        return true;
    }

	// check to see if any projectmutator modular features are available
	for (IProjectBuildMutatorFeature* Feature : IModularFeatures::Get().GetModularFeatureImplementations<IProjectBuildMutatorFeature>(PROJECT_BUILD_MUTATOR_FEATURE))
	{
		if (Feature->RequiresProjectBuild(PlatformInfo->Name, OutReason))
		{
			return true;
		}
	}

	// check the target platforms for any differences in build settings or additional plugins
	const FProjectDescriptor* Project = IProjectManager::Get().GetCurrentProject();
	if (!FApp::IsEngineInstalled() && !HasDefaultBuildSettings())
	{
		OutReason = LOCTEXT("TempTarget_NonDefaultBuildConfig", "project has non-default build configuration");
		return true;
	}

	// check if there's a non-default plugin change
	FText Reason;
	if (IPluginManager::Get().RequiresTempTargetForCodePlugin(Project, GetPlatformInfo().UBTPlatformString, Configuration, PlatformInfo->PlatformType, Reason))
	{
		OutReason = Reason;
		return true;
	}

	return false;
}

bool FTargetPlatformBase::IsEnabledForPlugin(const IPlugin& Plugin) const
{
	const FProjectDescriptor* Project = IProjectManager::Get().GetCurrentProject();
	return IsPluginEnabledForTarget(Plugin, Project, GetPlatformInfo().UBTPlatformString, EBuildConfiguration::Unknown,
		PlatformInfo->PlatformType);
}

TSharedPtr<IDeviceManagerCustomPlatformWidgetCreator> FTargetPlatformBase::GetCustomWidgetCreator() const
{
	static TSharedPtr<FDeviceBrowserDefaultPlatformWidgetCreator> DefaultWidgetCreator = MakeShared<FDeviceBrowserDefaultPlatformWidgetCreator>();
	return DefaultWidgetCreator;
}

bool FTargetPlatformBase::HasDefaultBuildSettings() const
{
	// first check default build settings for all platforms
	TArray<FString> BoolKeys, IntKeys, StringKeys, BuildKeys;
	BuildKeys.Add(TEXT("bCompileApex")); 
	BuildKeys.Add(TEXT("bCompileICU"));
	BuildKeys.Add(TEXT("bCompileSimplygon")); 
	BuildKeys.Add(TEXT("bCompileSimplygonSSF"));
	BuildKeys.Add(TEXT("bCompileRecast")); 
	BuildKeys.Add(TEXT("bCompileSpeedTree"));
	BuildKeys.Add(TEXT("bCompileWithPluginSupport")); 
	BuildKeys.Add(TEXT("bCompilePhysXVehicle")); 
	BuildKeys.Add(TEXT("bCompileFreeType"));
	BuildKeys.Add(TEXT("bCompileForSize"));	
	BuildKeys.Add(TEXT("bCompileCEF3")); 
	BuildKeys.Add(TEXT("bCompileCustomSQLitePlatform"));

	if (!DoProjectSettingsMatchDefault(IniPlatformName(), TEXT("/Script/BuildSettings.BuildSettings"), &BuildKeys, nullptr, nullptr))
	{
		return false;
	}

	FString PlatformSection;
	GetBuildProjectSettingKeys(PlatformSection, BoolKeys, IntKeys, StringKeys);

	if(!DoProjectSettingsMatchDefault(IniPlatformName(), PlatformSection, &BoolKeys, &IntKeys, &StringKeys))
	{
		return false;
	}

	return true;
}

bool FTargetPlatformBase::DoProjectSettingsMatchDefault(const FString& InPlatformName, const FString& InSection, const TArray<FString>* InBoolKeys, const TArray<FString>* InIntKeys, const TArray<FString>* InStringKeys)
{
	FConfigFile ProjIni;
	FConfigFile DefaultIni;
	FConfigCacheIni::LoadLocalIniFile(ProjIni, TEXT("Engine"), true, *InPlatformName, true);
	FConfigCacheIni::LoadExternalIniFile(DefaultIni, TEXT("Engine"), *FPaths::EngineConfigDir(), *FPaths::EngineConfigDir(), true, NULL, true);

	if (InBoolKeys != NULL)
	{
		for (int Index = 0; Index < InBoolKeys->Num(); ++Index)
		{
			FString Default(TEXT("False")), Project(TEXT("False"));
			DefaultIni.GetString(*InSection, *((*InBoolKeys)[Index]), Default);
			ProjIni.GetString(*InSection, *((*InBoolKeys)[Index]), Project);
			if (Default.Compare(Project, ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
	}

	if (InIntKeys != NULL)
	{
		for (int Index = 0; Index < InIntKeys->Num(); ++Index)
		{
			int64 Default(0), Project(0);
			DefaultIni.GetInt64(*InSection, *((*InIntKeys)[Index]), Default);
			ProjIni.GetInt64(*InSection, *((*InIntKeys)[Index]), Project);
			if (Default != Project)
			{
				return false;
			}
		}
	}

	if (InStringKeys != NULL)
	{
		for (int Index = 0; Index < InStringKeys->Num(); ++Index)
		{
			FString Default(TEXT("False")), Project(TEXT("False"));
			DefaultIni.GetString(*InSection, *((*InStringKeys)[Index]), Default);
			ProjIni.GetString(*InSection, *((*InStringKeys)[Index]), Project);
			if (Default.Compare(Project, ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
