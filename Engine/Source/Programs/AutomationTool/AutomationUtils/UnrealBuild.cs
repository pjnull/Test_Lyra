// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using UnrealBuildTool;
using EpicGames.Core;
using System.Linq;
using UnrealBuildBase;

namespace AutomationTool
{
	class UnrealBuildException : AutomationException
	{
		public UnrealBuildException(string Message)
			: base("BUILD FAILED: " + Message)
		{
			OutputFormat = AutomationExceptionOutputFormat.Minimal;
		}

		public UnrealBuildException(string Format, params object[] Args)
			: this(String.Format(Format, Args))
		{
		}
	}

	[Help("ForceMonolithic", "Toggle to combined the result into one executable")]
	[Help("ForceDebugInfo", "Forces debug info even in development builds")]
	[Help("NoXGE", "Toggle to disable the distributed build process")]
	[Help("ForceNonUnity", "Toggle to disable the unity build system")]
	[Help("ForceUnity", "Toggle to force enable the unity build system")]
	[Help("Licensee", "If set, this build is being compiled by a licensee")]
	public class UnrealBuild
	{
		private BuildCommand OwnerCommand;

		/// <summary>
		/// If true we will let UBT build UHT
		/// </summary>
		public bool AlwaysBuildUHT { get; set; }

		public void AddBuildProduct(string File)
		{
			string Path = CommandUtils.CombinePaths(File);
			if (!CommandUtils.FileExists(File) && !CommandUtils.DirectoryExists(Path))
			{
				throw new UnrealBuildException("Specified file to AddBuildProduct {0} does not exist.", Path);
			}
			BuildProductFiles.Add(Path);
		}

		static readonly string[] BuildReceiptExtensions = { ".version", ".target", ".modules", ".buildid.txt" };
		static bool IsBuildReceipt(string FileName) => BuildReceiptExtensions.Any(FileName.EndsWith);

		BuildManifest AddBuildProductsFromManifest(FileReference ManifestFile)
		{
			if (!FileReference.Exists(ManifestFile))
			{
				throw new UnrealBuildException("UBT Manifest {0} does not exist.", ManifestFile);
			}

			BuildManifest Manifest = CommandUtils.ReadManifest(ManifestFile);
			foreach (string Item in Manifest.BuildProducts)
			{
				if (!CommandUtils.FileExists_NoExceptions(Item) && !CommandUtils.DirectoryExists_NoExceptions(Item))
				{
					throw new UnrealBuildException("{0} was in manifest but was not produced.", Item);
				}
				AddBuildProduct(Item);
			}
			return Manifest;
		}
		
		private static readonly string[] DotNetProductExtensions =  
			{
				".dll",
				".pdb",
				".exe.config",
				".exe",
				"exe.mdb"
			};

		private static readonly string[] SwarmBuildProducts = 
            {
                "AgentInterface",
                "SwarmAgent",
                "SwarmCoordinator",
                "SwarmCoordinatorInterface",
                "SwarmInterface",
				"SwarmCommonUtils"
            };

		void AddBuildProductsForCSharpProj(string CsProj)
		{
			string BaseOutput =	$@"{CommandUtils.CmdEnv.LocalRoot}/Engine/Binaries/DotNET/{Path.GetFileNameWithoutExtension(CsProj)}";
			foreach (string Ext in DotNetProductExtensions)
			{
				if (CommandUtils.FileExists($"{BaseOutput}{Ext}"))
				{
					AddBuildProduct($"{BaseOutput}{Ext}");
				}
			}
		}

		void AddSwarmBuildProducts()
		{
			foreach (string SwarmProduct in SwarmBuildProducts)
			{
				string DotNETOutput = $@"{CommandUtils.CmdEnv.LocalRoot}/Engine/Binaries/DotNET/{SwarmProduct}";
				string Win64Output = $@"{CommandUtils.CmdEnv.LocalRoot}/Engine/Binaries/Win64/{SwarmProduct}";
				foreach (string Ext in DotNetProductExtensions)
				{
					if (CommandUtils.FileExists($"{DotNETOutput}{Ext}"))
					{
						AddBuildProduct($"{DotNETOutput}{Ext}");
					}
				}
				foreach (string Ext in DotNetProductExtensions)
				{
					if (CommandUtils.FileExists($"{Win64Output}{Ext}"))
					{
						AddBuildProduct($"{Win64Output}{Ext}");
					}
				}
			}
		}

		/// <summary>
		/// Updates the engine version files
		/// </summary>
		public List<FileReference> UpdateVersionFiles(bool ActuallyUpdateVersionFiles = true, int? ChangelistNumberOverride = null, int? CompatibleChangelistNumberOverride = null, string Build = null, bool? IsPromotedOverride = null)
		{
			bool bIsLicenseeVersion = ParseParam("Licensee") || !FileReference.Exists(FileReference.Combine(Unreal.EngineDirectory, "Restricted", "NotForLicensees", "Build", "EpicInternal.txt"));
			bool bIsPromotedBuild = IsPromotedOverride ?? (ParseParamInt("Promoted", 1) != 0);
			bool bDoUpdateVersionFiles = CommandUtils.P4Enabled && ActuallyUpdateVersionFiles;
			
			int ChangelistNumber = 0;
			int CompatibleChangelistNumber = 0;
			
			if (bDoUpdateVersionFiles)
			{
				ChangelistNumber = ChangelistNumberOverride ?? CommandUtils.P4Env.Changelist;
				CompatibleChangelistNumber = CompatibleChangelistNumberOverride ?? 0;
			}

			string Branch = OwnerCommand.ParseParamValue("Branch");
			if (String.IsNullOrEmpty(Branch))
			{
				Branch = CommandUtils.P4Enabled ? CommandUtils.EscapePath(CommandUtils.P4Env.Branch) : "";
			}

			return StaticUpdateVersionFiles(ChangelistNumber, CompatibleChangelistNumber, Branch, Build, bIsLicenseeVersion, bIsPromotedBuild, bDoUpdateVersionFiles);
		}

		public static List<FileReference> StaticUpdateVersionFiles(int ChangelistNumber, int CompatibleChangelistNumber, string Branch, string Build, bool bIsLicenseeVersion, bool bIsPromotedBuild, bool bDoUpdateVersionFiles)
		{
			FileReference BuildVersionFile = BuildVersion.GetDefaultFileName();

			string ChangelistNumberString = ChangelistNumber.ToString();
			
			// Get the revision to sync files to before 
			if(CommandUtils.P4Enabled && ChangelistNumber > 0)
			{
				CommandUtils.P4.Sync($"-f \"{BuildVersionFile}@{ChangelistNumberString}\"", false, false);
			}

			BuildVersion Version;
			if(!BuildVersion.TryRead(BuildVersionFile, out Version))
			{
				Version = new BuildVersion();
			}

			List<FileReference> Result = new List<FileReference>(1);
			{
				if (bDoUpdateVersionFiles)
				{
					CommandUtils.LogLog($"Updating {BuildVersionFile} with:");
					CommandUtils.LogLog($"  Changelist={ChangelistNumberString}");
					CommandUtils.LogLog($"  CompatibleChangelist={CompatibleChangelistNumber.ToString()}");
					CommandUtils.LogLog($"  IsLicenseeVersion={(bIsLicenseeVersion? "1" : "0")}");
					CommandUtils.LogLog($"  IsPromotedBuild={(bIsPromotedBuild? "1" : "0")}");
					CommandUtils.LogLog($"  BranchName={Branch}");
					CommandUtils.LogLog($"  BuildVersion={Build}");

					Version.Changelist = ChangelistNumber;
					if(CompatibleChangelistNumber > 0)
					{
						Version.CompatibleChangelist = CompatibleChangelistNumber;
					}
					else if(bIsLicenseeVersion != Version.IsLicenseeVersion)
					{
						Version.CompatibleChangelist = 0; // Clear out the compatible changelist number; it corresponds to a different P4 server.
					}
					Version.IsLicenseeVersion = bIsLicenseeVersion;
					Version.IsPromotedBuild = bIsPromotedBuild;
					Version.BranchName = Branch;
					Version.BuildVersionString = Build;

					if (File.Exists(BuildVersionFile.FullName))
					{
						VersionFileUpdater.MakeFileWriteable(BuildVersionFile.FullName);
					}

					Version.Write(BuildVersionFile);
				}
				else
				{
					CommandUtils.LogVerbose($"{BuildVersionFile} will not be updated because P4 is not enabled.");
				}
				Result.Add(BuildVersionFile);
			}

			return Result;
		}

		[DebuggerDisplay("{TargetName} {Platform} {Config}")]
		public class BuildTarget
		{
			/// <summary>
			/// Name of the target
			/// </summary>
			public string TargetName;

			/// <summary>
			/// For code-based projects with a .uproject, the TargetName isn't enough for UBT to find the target, this will point UBT to the target
			/// </summary>
			public FileReference UprojectPath;

			/// <summary>
			/// Platform to build
			/// </summary>
			public UnrealTargetPlatform Platform;

			/// <summary>
			/// Configuration to build
			/// </summary>
			public UnrealTargetConfiguration Config;

			/// <summary>
			/// Extra UBT args
			/// </summary>
			public string UBTArgs;

			/// <summary>
			/// Whether to clean this target. If not specified, the target will be cleaned if -Clean is on the command line.
			/// </summary>
			public bool? Clean;

			/// <summary>
			/// Format as string
			/// </summary>
			/// <returns></returns>
			public override string ToString()
			{
				return $"{TargetName} {Platform.ToString()} {Config.ToString()}";
			}
		}


		public class BuildAgenda
		{
			/// <summary>
			/// .NET .csproj files that will be compiled and included in the build.  Currently we assume the output
			/// binary file names match the solution file base name, but with various different binary file extensions.
			/// </summary>
			public List<string> DotNetProjects = new List<string>();

			public string SwarmAgentProject = "";
			public string SwarmCoordinatorProject = "";

			/// <summary>
			/// List of targets to build.  These can be various Unreal projects, programs or libraries in various configurations
			/// </summary>
			public List<BuildTarget> Targets = new List<BuildTarget>();

			/// <summary>
			/// Adds a target with the specified configuration.
			/// </summary>
			/// <param name="TargetName">Name of the target</param>
			/// <param name="TargetPlatform">Platform</param>
			/// <param name="Configuration">Configuration</param>
			/// <param name="UprojectPath">Path to optional uproject file</param>
			/// <param name="AddArgs">Specifies additional arguments for UBT</param>
			public void AddTarget(string TargetName, UnrealTargetPlatform TargetPlatform, UnrealTargetConfiguration Configuration, FileReference UprojectPath = null, string AddArgs = "")
			{
				// Is this platform a compilable target?
				if (!Platform.GetPlatform(TargetPlatform).CanBeCompiled())
				{
					return;
				}

				Targets.Add(new BuildTarget()
				{
					TargetName = TargetName,
					Platform = TargetPlatform,
					Config = Configuration,
					UprojectPath = UprojectPath,
					UBTArgs = AddArgs,
				});
			}

			/// <summary>
			/// Adds multiple targets with the specified configuration.
			/// </summary>
			/// <param name="TargetNames">List of targets.</param>
			/// <param name="TargetPlatform">Platform</param>
			/// <param name="Configuration">Configuration</param>
			/// <param name="UprojectPath">Path to optional uproject file</param>
			/// <param name="AddArgs">Specifies additional arguments for UBT</param>
			public void AddTargets(string[] TargetNames, UnrealTargetPlatform TargetPlatform, UnrealTargetConfiguration Configuration, FileReference UprojectPath = null, string AddArgs = "")
			{
				// Is this platform a compilable target?
				if (!Platform.GetPlatform(TargetPlatform).CanBeCompiled())
				{
					return;
				}

				foreach (string Target in TargetNames)
				{
					Targets.Add(new BuildTarget()
					{
						TargetName = Target,
						Platform = TargetPlatform,
						Config = Configuration,
						UprojectPath = UprojectPath,
						UBTArgs = AddArgs,
					});
				}
			}
		}


		public UnrealBuild(BuildCommand Command)
		{
			OwnerCommand = Command;
			BuildProductFiles.Clear();
		}

		private bool ParseParam(string Name)
		{
			return OwnerCommand != null && OwnerCommand.ParseParam(Name);
		}

		private int ParseParamInt(string Name, int Default = 0)
		{
			return OwnerCommand?.ParseParamInt(Name, Default) ?? Default;
		}

		/// <summary>
		/// Executes a build.
		/// </summary>
		/// <param name="Agenda">Build agenda.</param>
		/// <param name="OverrideCleanBehavior">if specified, determines if the build products will be deleted before building. If not specified -clean parameter will be used,</param>
		/// <param name="bUpdateVersionFiles">True if the version files are to be updated </param>
		/// <param name="bForceNoXGE">If true will force XGE off</param>
		public void Build(BuildAgenda Agenda, bool? OverrideCleanBehavior = null, bool bUpdateVersionFiles = true, 
			bool bForceNoXGE = false, int? ChangelistNumberOverride = null, 
			Dictionary<BuildTarget, BuildManifest> TargetToManifest = null)
		{
			if (!CommandUtils.CmdEnv.HasCapabilityToCompile)
			{
				throw new UnrealBuildException("No supported compiler detected");
			}
			
			if (bUpdateVersionFiles)
			{
				UpdateVersionFiles(ChangelistNumberOverride: ChangelistNumberOverride);
			}

			if (Agenda.SwarmAgentProject != "")
			{
				string SwarmAgentSolution = Path.Combine(CommandUtils.CmdEnv.LocalRoot, Agenda.SwarmAgentProject);
				CommandUtils.BuildSolution(CommandUtils.CmdEnv, SwarmAgentSolution, "Development", "Mixed Platforms");
				AddSwarmBuildProducts();
			}

			if (Agenda.SwarmCoordinatorProject != "")
			{
				string SwarmCoordinatorSolution = Path.Combine(CommandUtils.CmdEnv.LocalRoot, Agenda.SwarmCoordinatorProject);
				CommandUtils.BuildSolution(CommandUtils.CmdEnv, SwarmCoordinatorSolution, "Development", "Mixed Platforms");
				AddSwarmBuildProducts();
			}
				
			foreach (string DotNetProject in Agenda.DotNetProjects)
			{
				string CsProj = Path.Combine(CommandUtils.CmdEnv.LocalRoot, DotNetProject);
				CommandUtils.BuildCSharpProject(CommandUtils.CmdEnv, CsProj);
				AddBuildProductsForCSharpProj(CsProj);
			}

			BuildWithUBT(Agenda.Targets, 
				bClean: OverrideCleanBehavior ?? ParseParam("Clean"), 
				bDisableXGE: bForceNoXGE || ParseParam("NoXGE"), 
				TargetToManifest);
		}

		[Obsolete("Parameters and parameter names deprecated in 5.0; update call sites to call non-obsolete Build()")]
		public void Build(BuildAgenda Agenda, bool? InDeleteBuildProducts = null, bool InUpdateVersionFiles = true, 
			bool InForceNoXGE = false, bool InUseParallelExecutor = false, bool InShowProgress = false, 
			int? InChangelistNumberOverride = null, Dictionary<BuildTarget, BuildManifest> InTargetToManifest = null)
		{
			Build(Agenda, OverrideCleanBehavior: InDeleteBuildProducts, bUpdateVersionFiles: InUpdateVersionFiles,
				bForceNoXGE: InForceNoXGE, ChangelistNumberOverride: InChangelistNumberOverride,
				TargetToManifest: InTargetToManifest);
		}
		/// <summary>
		/// Checks to make sure there was at least one build product, and that all files exist.  Also, logs them all out.
		/// </summary>
		/// <param name="BuildProductFiles">List of files</param>
		public static void CheckBuildProducts(HashSet<string> BuildProductFiles)
		{
			// Check build products
			{
				CommandUtils.LogLog("Build products *******");
				if (BuildProductFiles.Count < 1)
				{
					CommandUtils.LogInformation("No build products were made");
				}
				else
				{
					foreach (string Product in BuildProductFiles)
					{
						if (!CommandUtils.FileExists(Product) && !CommandUtils.DirectoryExists(Product))
						{
							throw new UnrealBuildException("{0} was a build product but no longer exists", Product);
						}
						CommandUtils.LogLog(Product);
					}
				}
				CommandUtils.LogLog("End Build products *******");
			}
		}


		/// <summary>
		/// Adds or edits existing files at head revision, expecting an exclusive lock, resolving by clobbering any existing version
		/// </summary>
		/// <param name="Files">List of files to check out</param>
		public static void AddBuildProductsToChangelist(int WorkingCL, IReadOnlyCollection<string> Files)
		{
			CommandUtils.LogInformation($"Adding {Files.Count().ToString()} build products to changelist {WorkingCL.ToString()}...");
			foreach (string File in Files)
			{
				CommandUtils.P4.Sync($"-f -k {CommandUtils.MakePathSafeToUseWithCommandLine(File)}#head"); // sync the file without overwriting local one
				if (!CommandUtils.FileExists(File))
				{
					throw new UnrealBuildException("{0} was a build product but no longer exists", File);
				}

				CommandUtils.P4.ReconcileNoDeletes(WorkingCL, CommandUtils.MakePathSafeToUseWithCommandLine(File));

				// Change file type on binary files to be always writeable.
				P4FileStat FileStats = CommandUtils.P4.FStat(File);

                if (CommandUtils.IsProbablyAMacOrIOSExe(File))
                {
                    if (FileStats.Type == P4FileType.Binary && (FileStats.Attributes & (P4FileAttributes.Executable | P4FileAttributes.Writeable)) != (P4FileAttributes.Executable | P4FileAttributes.Writeable))
                    {
                        CommandUtils.P4.ChangeFileType(File, (P4FileAttributes.Executable | P4FileAttributes.Writeable));
                    }
                }
                else
                {
					if (IsBuildProduct(File, FileStats) && (FileStats.Attributes & P4FileAttributes.Writeable) != P4FileAttributes.Writeable)
                    {
                        CommandUtils.P4.ChangeFileType(File, P4FileAttributes.Writeable);
                    }

                }                    
			}
		}

		/// <summary>
		/// Determines if this file is a build product.
		/// </summary>
		/// <param name="File">File path</param>
		/// <param name="FileStats">P4 file stats.</param>
		/// <returns>True if this is a Windows build product. False otherwise.</returns>
		private static bool IsBuildProduct(string File, P4FileStat FileStats)
		{
			if(FileStats.Type == P4FileType.Binary || IsBuildReceipt(File))
			{
				return true;
			}

			return FileStats.Type == P4FileType.Text && File.EndsWith(".exe.config", StringComparison.InvariantCultureIgnoreCase);
		}

		/// <summary>
		/// Add UBT files to build products
		/// </summary>
		public void AddUBTFilesToBuildProducts()
		{
			var UBTLocation = Path.GetDirectoryName(GetUBTExecutable());
			var UBTFiles = new List<string>(new string[] 
					{
						"UnrealBuildTool.exe",
					});

			foreach (var UBTFile in UBTFiles)
			{
				var UBTProduct = CommandUtils.CombinePaths(UBTLocation, UBTFile);
				if (!CommandUtils.FileExists_NoExceptions(UBTProduct))
				{
					throw new UnrealBuildException("Cannot add UBT to the build products because {0} does not exist.", UBTProduct);
				}
				AddBuildProduct(UBTProduct);
			}
		}

		/// <summary>
		/// Copy the UAT files to their precompiled location, and add them as build products
		/// </summary>
		public void AddUATFilesToBuildProducts()
		{
			foreach (FileReference BuildProduct in ScriptManager.BuildProducts)
			{
				string UATScriptFilePath = BuildProduct.FullName;
				if (!CommandUtils.FileExists_NoExceptions(UATScriptFilePath))
				{
					throw new UnrealBuildException("Cannot add UAT to the build products because {0} does not exist.", UATScriptFilePath);
				}
				AddBuildProduct(UATScriptFilePath);
			}
		}

		FileReference GetManifestFile(FileReference ProjectFile, int Index)
		{
			// Can't write to Engine directory on installed builds
			if (Unreal.IsEngineInstalled() && ProjectFile != null)
			{
				return FileReference.Combine(ProjectFile.Directory, "Intermediate", "Build", $"Manifest_{Index.ToString()}.xml");
			}
			else
			{
				return FileReference.Combine(Unreal.EngineDirectory, "Intermediate", "Build", $"Manifest_{Index.ToString()}.xml");
			}
		}

		static string GetUBTExecutable()
		{
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, @"Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool" + (RuntimePlatform.IsWindows ? ".exe" : ""));
		}

		public static readonly string UBTExecutable = GetUBTExecutable();

		void BuildWithUBT(IReadOnlyList<BuildTarget> Targets, bool bClean, bool bDisableXGE,
			Dictionary<BuildTarget, BuildManifest> TargetToManifest)
		{
			List<string> CommandLine = new List<string>(3 + Targets.Count());

			CommandLine.Add("-NoUBTMakefiles"); // BuildConfiguration
			
			if (bDisableXGE)
			{
				CommandLine.Add("-noxge"); // BuildConfiguration
			}
			
			if (!AlwaysBuildUHT &&
				// Prevent UHT from being cleaned if it's not going to be (re)built
			    !Targets.Any(x => String.Equals(x.TargetName, "UnrealHeaderTool")))
			{
				CommandLine.Add("-nobuilduht"); // BuildConfiguration
			}

			List<string> TargetCommandLine = new List<string>(9);
					
			int TargetIndex = 0;
			foreach (BuildTarget Target in Targets)
			{
				TargetCommandLine.Clear();

				TargetCommandLine.Add("-NoHotReload"); // TargetDescriptor

				if (Target.UprojectPath != null)
				{
					TargetCommandLine.Add(
						$"-Project={CommandUtils.MakePathSafeToUseWithCommandLine(Target.UprojectPath.FullName)}"); // TargetDescriptor
				}
				TargetCommandLine.Add(Target.TargetName); // TargetDescriptor
				TargetCommandLine.Add(Target.Platform.ToString()); // TargetDescriptor
				TargetCommandLine.Add(Target.Config.ToString()); // TargetDescriptor
				
				if (Target.Clean ?? bClean)
				{
					TargetCommandLine.Add("-Rebuild"); // TargetDescriptor
				}
				
				// When building a target for Mac or iOS, -flushmac to clean up the remote builder
				if (bClean && (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS))
				{
					TargetCommandLine.Add("-flushmac"); // TargetRules
				}

				if (TargetToManifest != null)
				{
					FileReference ManifestFile = GetManifestFile(Target.UprojectPath, TargetIndex++);
					CommandUtils.DeleteFile(ManifestFile);
					TargetCommandLine.Add(
						$"-Manifest={CommandUtils.MakePathSafeToUseWithCommandLine((ManifestFile.FullName))}"); // TargetRules
				}

				if (!String.IsNullOrEmpty(Target.UBTArgs))
				{
					TargetCommandLine.Add(Target.UBTArgs);
				}

				CommandLine.Add($"\"-Target={String.Join(" ", TargetCommandLine)}\""); // TargetDescriptor
			}

			CommandUtils.RunUBT(CommandUtils.CmdEnv, UBTExecutable, String.Join(" ", CommandLine)); 
			
			if(TargetToManifest != null)
			{
				TargetIndex = 0;
				foreach (BuildTarget Target in Targets)
				{
					FileReference ManifestFile = GetManifestFile(Target.UprojectPath, TargetIndex++);
					TargetToManifest[Target] = AddBuildProductsFromManifest(ManifestFile);
					CommandUtils.DeleteFile(ManifestFile);
				}
			}
		}

		// List of everything we built so far
		public readonly HashSet<string> BuildProductFiles = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);
	}

	[Obsolete("Deprecated in 5.0. Use UnrealBuild")]
	public class UE4Build : UnrealBuild
	{
		public UE4Build(BuildCommand Command) : base(Command)
		{
		}
	}
}
