// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class UEOpenExr : ModuleRules
{
	private enum EXRVersion
	{
		EXR_2_3_0,
		EXR_3_1_1
	}
	public UEOpenExr(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

		string DeploymentDirectory = Path.Combine(ModuleDirectory, "Deploy", "openexr-3.1.1");

		// Set which version to use.
		// XXX: Soon to be switched to 3.1.1, and 2.3.0 (and the enum) removed...
		EXRVersion Version = EXRVersion.EXR_2_3_0;

		if (Version >= EXRVersion.EXR_3_1_1)
		{
			PublicDependencyModuleNames.Add("Imath");

			PublicIncludePaths.Add(Path.Combine(DeploymentDirectory, "include"));

			// XXX: OpenEXR includes some of its own headers without the
			// leading "OpenEXR/..."
			PublicIncludePaths.Add(Path.Combine(DeploymentDirectory, "include", "OpenEXR"));

			string LibPostfix = bDebug ? "_d" : "";

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
			{
				string LibDirectory = Path.Combine(
					DeploymentDirectory,
					"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(),
					Target.WindowsPlatform.GetArchitectureSubpath(),
					"lib");

				PublicAdditionalLibraries.AddRange(
					new string[] {
						Path.Combine(LibDirectory, "Iex-3_1" + LibPostfix + ".lib"),
						Path.Combine(LibDirectory, "IlmThread-3_1" + LibPostfix + ".lib"),
						Path.Combine(LibDirectory, "OpenEXR-3_1" + LibPostfix + ".lib"),
						Path.Combine(LibDirectory, "OpenEXRCore-3_1" + LibPostfix + ".lib"),
						Path.Combine(LibDirectory, "OpenEXRUtil-3_1" + LibPostfix + ".lib")
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				string LibDirectory = Path.Combine(
					DeploymentDirectory,
					"Mac",
					"lib");

				PublicAdditionalLibraries.AddRange(
					new string[] {
						Path.Combine(LibDirectory, "libIex-3_1" + LibPostfix + ".a"),
						Path.Combine(LibDirectory, "libIlmThread-3_1" + LibPostfix + ".a"),
						Path.Combine(LibDirectory, "libOpenEXR-3_1" + LibPostfix + ".a"),
						Path.Combine(LibDirectory, "libOpenEXRCore-3_1" + LibPostfix + ".a"),
						Path.Combine(LibDirectory, "libOpenEXRUtil-3_1" + LibPostfix + ".a")
					}
				);
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				string LibDirectory = Path.Combine(
					DeploymentDirectory,
					"Unix",
					Target.Architecture,
					"lib");

				PublicAdditionalLibraries.AddRange(
					new string[] {
						Path.Combine(LibDirectory, "libIex-3_1" + LibPostfix + ".a"),
						Path.Combine(LibDirectory, "libIlmThread-3_1" + LibPostfix + ".a"),
						Path.Combine(LibDirectory, "libOpenEXR-3_1" + LibPostfix + ".a"),
						Path.Combine(LibDirectory, "libOpenEXRCore-3_1" + LibPostfix + ".a"),
						Path.Combine(LibDirectory, "libOpenEXRUtil-3_1" + LibPostfix + ".a")
					}
				);
			}

			return;
		}

		// Legacy OpenEXR version below, soon to be removed.

		string DeployDir = Path.Combine("openexr", "Deploy", "OpenEXR-2.3.0");

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) || Target.Platform == UnrealTargetPlatform.Mac || Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibDir = Target.UEThirdPartySourceDirectory + DeployDir + "/OpenEXR//lib/";
			string Platform = "";
			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				Platform = "x64";
				LibDir += "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				Platform = "Mac";
				bDebug = false;
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				Platform = "Unix";
				bDebug = false;
			}
			LibDir = LibDir + "/" + Platform;
			LibDir = LibDir + "/Static" + (bDebug ? "Debug" : "Release");

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				PublicAdditionalLibraries.AddRange(
					new string[] {
						LibDir + "/Half.lib",
						LibDir + "/Iex.lib",
						LibDir + "/IexMath.lib",
						LibDir + "/IlmImf.lib",
						LibDir + "/IlmImfUtil.lib",
						LibDir + "/IlmThread.lib",
						LibDir + "/Imath.lib",
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicAdditionalLibraries.AddRange(
					new string[] {
						LibDir + "/libHalf.a",
						LibDir + "/libIex.a",
						LibDir + "/libIexMath.a",
						LibDir + "/libIlmImf.a",
						LibDir + "/libIlmImfUtil.a",
						LibDir + "/libIlmThread.a",
						LibDir + "/libImath.a",
					}
				);
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture.StartsWith("x86_64"))
			{
				string LibArchDir = LibDir + "/" + Target.Architecture;
				PublicAdditionalLibraries.AddRange(
					new string[] {
						LibArchDir + "/libHalf.a",
						LibArchDir + "/libIex.a",
						LibArchDir + "/libIexMath.a",
						LibArchDir + "/libIlmImf.a",
						LibArchDir + "/libIlmImfUtil.a",
						LibArchDir + "/libIlmThread.a",
						LibArchDir + "/libImath.a",
					}
				);
			}

			PublicSystemIncludePaths.AddRange(
				new string[] {
					Target.UEThirdPartySourceDirectory + DeployDir + "/OpenEXR/include",
					Target.UEThirdPartySourceDirectory + DeployDir + "/OpenEXR/include/openexr", // Alembic SDK is non-standard, doesn't prefix its include correctly
				}
			);
		}
	}
}
