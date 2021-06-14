// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class AndroidFetchBackgroundDownload : ModuleRules
	{
		public AndroidFetchBackgroundDownload(ReadOnlyTargetRules Target) : base(Target)
		{
			//Can not depend on BackgroundHTTP module as our Startup needs to run before it's startup.
			//This way we can still include the Android version of these interfaces
			//PublicIncludePaths.AddRange(new string[]
				//{
				//	"Runtime/Online/BackgroundHTTP/Public/",
				//});

			PrivateDependencyModuleNames.AddRange(new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Launch",
					"AndroidBackgroundService",
					"BackgroundHTTP",
				});

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "AndroidFetchBackgroundDownload_UPL.xml"));
			}
		}
	}
}
