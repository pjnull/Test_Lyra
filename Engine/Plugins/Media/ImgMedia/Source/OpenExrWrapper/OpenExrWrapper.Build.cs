// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenExrWrapper : ModuleRules
	{
		public OpenExrWrapper(ReadOnlyTargetRules Target) : base(Target)
		{
			bEnableExceptions = true;

			PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;
			PrivatePCHHeaderFile = "Public/OpenExrWrapper.h";

            bUseRTTI = true;

            PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"TimeManagement",
				});

            if ((Target.Platform == UnrealTargetPlatform.Win64) ||
                (Target.Platform == UnrealTargetPlatform.Win32) ||
                (Target.Platform == UnrealTargetPlatform.Linux) ||
                (Target.Platform == UnrealTargetPlatform.Mac))
            {
                AddEngineThirdPartyPrivateStaticDependencies(Target, "UEOpenExr");
                AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
            }
            else
            {
                System.Console.WriteLine("OpenExrWrapper does not supported this platform");
            }

            PrivateIncludePaths.Add("OpenExrWrapper/Private");
        }
	}
}
