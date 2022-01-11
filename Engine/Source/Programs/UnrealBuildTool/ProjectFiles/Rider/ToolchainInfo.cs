// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace UnrealBuildTool
{
	sealed class ToolchainInfo : IEquatable<ToolchainInfo>
	{
		public CppStandardVersion CppStandard;
		public bool bUseRTTI;
		public bool bEnableExceptions;
		public bool bIsBuildingLibrary;
		public bool bIsBuildingDLL;
		public string? Architecture;
		public string? Configuration;
		public bool bOptimizeCode;
		public bool bUseInlining;
		public bool bUseUnity;
		public bool bCreateDebugInfo;
		public bool bUseAVX;
		public bool bUseDebugCRT;
		public bool bUseStaticCRT;
		public string? PrecompiledHeaderAction;
		public string? PrecompiledHeaderFile;
		public List<string>? ForceIncludeFiles;
		public string? Compiler;
		public bool bStrictConformanceMode = false;

		public IEnumerable<Tuple<string, object?>> GetDiff(ToolchainInfo Other)
		{
			foreach (FieldInfo FieldInfo in typeof(ToolchainInfo).GetFields())
			{
				if(FieldInfo.GetValue(this) == null) continue;
				if (typeof(List<string>).IsAssignableFrom(FieldInfo.FieldType))
				{
					List<string> LocalField = (List<string>) FieldInfo.GetValue(this)!;
					HashSet<string> OtherField = new HashSet<string>((List<string>)FieldInfo.GetValue(Other)!);
					IEnumerable<string> Result = LocalField.Where(Item => OtherField.Contains(Item));
					if(Result.Any()) yield return new Tuple<string, object?>(FieldInfo.Name, Result);
				}
				else if (!FieldInfo.GetValue(this)!.Equals(FieldInfo.GetValue(Other)))
					yield return new Tuple<string, object?>(FieldInfo.Name, FieldInfo.GetValue(this));
			}
		}

		public IEnumerable<Tuple<string, object?>> GetFields()
		{
			foreach (FieldInfo FieldInfo in typeof(ToolchainInfo).GetFields())
			{
				yield return new Tuple<string, object?>(FieldInfo.Name, FieldInfo.GetValue(this));
			}
		}

		public bool Equals(ToolchainInfo? Other)
		{
			if (Other is null) return false;
			if (ReferenceEquals(this, Other)) return true;
			return PrecompiledHeaderAction == Other.PrecompiledHeaderAction && CppStandard == Other.CppStandard && bUseRTTI == Other.bUseRTTI && bEnableExceptions == Other.bEnableExceptions && bIsBuildingLibrary == Other.bIsBuildingLibrary && bIsBuildingDLL == Other.bIsBuildingDLL && Architecture == Other.Architecture && Configuration == Other.Configuration && bOptimizeCode == Other.bOptimizeCode && bUseInlining == Other.bUseInlining && bUseUnity == Other.bUseUnity && bCreateDebugInfo == Other.bCreateDebugInfo && bUseAVX == Other.bUseAVX && bUseDebugCRT == Other.bUseDebugCRT && bUseStaticCRT == Other.bUseStaticCRT && PrecompiledHeaderFile == Other.PrecompiledHeaderFile && Equals(ForceIncludeFiles, Other.ForceIncludeFiles) && Compiler == Other.Compiler && bStrictConformanceMode == Other.bStrictConformanceMode;
		}

		public override bool Equals(object? Obj)
		{
			if (Obj is null) return false;
			if (ReferenceEquals(this, Obj)) return true;
			return Obj is ToolchainInfo && Equals((ToolchainInfo) Obj);
		}

		public override int GetHashCode()
		{
			HashCode hash = new HashCode();
			hash.Add(CppStandard);
			hash.Add(bUseRTTI);
			hash.Add(bEnableExceptions);
			hash.Add(bIsBuildingLibrary);
			hash.Add(bIsBuildingDLL);
			hash.Add(Architecture);
			hash.Add(Configuration);
			hash.Add(bOptimizeCode);
			hash.Add(bUseInlining);
			hash.Add(bUseUnity);
			hash.Add(bCreateDebugInfo);
			hash.Add(bUseAVX);
			hash.Add(bUseDebugCRT);
			hash.Add(bUseStaticCRT);
			hash.Add(PrecompiledHeaderAction);
			hash.Add(PrecompiledHeaderFile);
			hash.Add(ForceIncludeFiles);
			hash.Add(Compiler);
			hash.Add(bStrictConformanceMode);
			return hash.ToHashCode();
		}
	}
}