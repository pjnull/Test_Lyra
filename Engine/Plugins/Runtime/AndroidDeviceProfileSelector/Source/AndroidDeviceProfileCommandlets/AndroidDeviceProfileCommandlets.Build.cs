// Copyright Epic Games, Inc. All Rights Reserved.
using Tools.DotNETCommon;
using System.IO;
using UnrealBuildTool;

public class AndroidDeviceProfileCommandlets : ModuleRules
{
	public AndroidDeviceProfileCommandlets(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities",
				"AndroidDeviceProfileSelector",
				"AndroidDeviceDetection",
				"PIEPreviewDeviceSpecification"
			}
			);
	}
}
