// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightMapRendering.h: Light map rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "DrawingPolicy.h"
#include "ShadowRendering.h"
#include "LightMap.h"

class FPrimitiveSceneProxy;

extern ENGINE_API bool GShowDebugSelectedLightmap;
extern ENGINE_API class FLightMap2D* GDebugSelectedLightmap;
extern bool GVisualizeMipLevels;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FIndirectLightingCacheUniformParameters, )
	SHADER_PARAMETER(FVector, IndirectLightingCachePrimitiveAdd) // FCachedVolumeIndirectLightingPolicy
	SHADER_PARAMETER(FVector, IndirectLightingCachePrimitiveScale) // FCachedVolumeIndirectLightingPolicy
	SHADER_PARAMETER(FVector, IndirectLightingCacheMinUV) // FCachedVolumeIndirectLightingPolicy
	SHADER_PARAMETER(FVector, IndirectLightingCacheMaxUV) // FCachedVolumeIndirectLightingPolicy
	SHADER_PARAMETER(FVector4, PointSkyBentNormal) // FCachedPointIndirectLightingPolicy
	SHADER_PARAMETER_EX(float, DirectionalLightShadowing, EShaderPrecisionModifier::Half) // FCachedPointIndirectLightingPolicy
	SHADER_PARAMETER_ARRAY(FVector4, IndirectLightingSHCoefficients0, [3]) // FCachedPointIndirectLightingPolicy
	SHADER_PARAMETER_ARRAY(FVector4, IndirectLightingSHCoefficients1, [3]) // FCachedPointIndirectLightingPolicy
	SHADER_PARAMETER(FVector4,	IndirectLightingSHCoefficients2) // FCachedPointIndirectLightingPolicy
	SHADER_PARAMETER_EX(FVector4, IndirectLightingSHSingleCoefficient, EShaderPrecisionModifier::Half) // FCachedPointIndirectLightingPolicy used in forward Translucent
	SHADER_PARAMETER_TEXTURE(Texture3D, IndirectLightingCacheTexture0) // FCachedVolumeIndirectLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, IndirectLightingCacheTexture1) // FCachedVolumeIndirectLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, IndirectLightingCacheTexture2) // FCachedVolumeIndirectLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, IndirectLightingCacheTextureSampler0) // FCachedVolumeIndirectLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, IndirectLightingCacheTextureSampler1) // FCachedVolumeIndirectLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, IndirectLightingCacheTextureSampler2) // FCachedVolumeIndirectLightingPolicy
END_GLOBAL_SHADER_PARAMETER_STRUCT()

/**
 * Default precomputed lighting data. Used for fully dynamic lightmap policies.
 */
class FEmptyPrecomputedLightingUniformBuffer : public TUniformBuffer< FPrecomputedLightingUniformParameters >
{
	typedef TUniformBuffer< FPrecomputedLightingUniformParameters > Super;
public:
	virtual void InitDynamicRHI() override;
};

/** Global uniform buffer containing the default precomputed lighting data. */
extern TGlobalResource< FEmptyPrecomputedLightingUniformBuffer > GEmptyPrecomputedLightingUniformBuffer;

void GetIndirectLightingCacheParameters(
	ERHIFeatureLevel::Type FeatureLevel,
	FIndirectLightingCacheUniformParameters& Parameters,
	const class FIndirectLightingCache* LightingCache,
	const class FIndirectLightingCacheAllocation* LightingAllocation,
	FVector VolumetricLightmapLookupPosition,
	uint32 SceneFrameNumber,
	class FVolumetricLightmapSceneData* VolumetricLightmapSceneData);

/**
 * Default precomputed lighting data. Used for fully dynamic lightmap policies.
 */
class FEmptyIndirectLightingCacheUniformBuffer : public TUniformBuffer< FIndirectLightingCacheUniformParameters >
{
	typedef TUniformBuffer< FIndirectLightingCacheUniformParameters > Super;
public:
	virtual void InitDynamicRHI() override;
};

/** Global uniform buffer containing the default precomputed lighting data. */
extern TGlobalResource< FEmptyIndirectLightingCacheUniformBuffer > GEmptyIndirectLightingCacheUniformBuffer;


/**
 * A policy for shaders without a light-map.
 */
struct FNoLightMapPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{}

	static bool RequiresSkylight()
	{
		return false;
	}
};

enum ELightmapQuality
{
	LQ_LIGHTMAP,
	HQ_LIGHTMAP,
};

// One of these per lightmap quality
extern const TCHAR* GLightmapDefineName[2];
extern int32 GNumLightmapCoefficients[2];

/**
 * Base policy for shaders with lightmaps.
 */
template< ELightmapQuality LightmapQuality >
struct TLightMapPolicy
{
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(GLightmapDefineName[LightmapQuality],TEXT("1"));
		OutEnvironment.SetDefine(TEXT("NUM_LIGHTMAP_COEFFICIENTS"), GNumLightmapCoefficients[LightmapQuality]);

		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTexturedLightmaps"));
		OutEnvironment.SetDefine(TEXT("LIGHTMAP_VT_ENABLED"), CVar->GetValueOnAnyThread() != 0);
		OutEnvironment.SetDefine(TEXT("LIGHTMAP_VT_16BIT"), LIGHTMAP_VT_16BIT);
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		static const auto CVarProjectCanHaveLowQualityLightmaps = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
		static const auto CVarSupportAllShadersPermutations = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAllShaderPermutations"));
		const bool bForceAllPermutations = CVarSupportAllShadersPermutations && CVarSupportAllShadersPermutations->GetValueOnAnyThread() != 0;

		// if GEngine doesn't exist yet to have the project flag then we should be conservative and cache the LQ lightmap policy
		const bool bProjectCanHaveLowQualityLightmaps = bForceAllPermutations || (!CVarProjectCanHaveLowQualityLightmaps) || (CVarProjectCanHaveLowQualityLightmaps->GetValueOnAnyThread() != 0);

		const bool bShouldCacheQuality = (LightmapQuality != ELightmapQuality::LQ_LIGHTMAP) || bProjectCanHaveLowQualityLightmaps;

		// GetValueOnAnyThread() as it's possible that ShouldCache is called from rendering thread. That is to output some error message.
		return (Material->GetShadingModel() != MSM_Unlit)
			&& bShouldCacheQuality
			&& VertexFactoryType->SupportsStaticLighting() 
			&& (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnAnyThread() != 0)
			&& (Material->IsUsedWithStaticLighting() || Material->IsSpecialEngineMaterial());		
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

// A light map policy for computing up to 4 signed distance field shadow factors in the base pass.
template< ELightmapQuality LightmapQuality >
struct TDistanceFieldShadowsAndLightMapPolicy : public TLightMapPolicy< LightmapQuality >
{
	typedef TLightMapPolicy< LightmapQuality >	Super;

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("STATICLIGHTING_TEXTUREMASK"), 1);
		OutEnvironment.SetDefine(TEXT("STATICLIGHTING_SIGNEDDISTANCEFIELD"), 1);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

/**
 * Policy for 'fake' texture lightmaps, such as the LightMap density rendering mode
 */
struct FDummyLightMapPolicy : public TLightMapPolicy< HQ_LIGHTMAP >
{
public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetShadingModel() != MSM_Unlit && VertexFactoryType->SupportsStaticLighting();
	}
};

/**
 * Policy for self shadowing translucency from a directional light
 */
class FSelfShadowedTranslucencyPolicy
{
public:

	typedef const FUniformBufferRHIParamRef ElementDataType;

	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap) {}
		void Serialize(FArchive& Ar) {}
	};

	class PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			TranslucentSelfShadowBufferParameter.Bind(ParameterMap, TEXT("TranslucentSelfShadow"));
		}

		void Serialize(FArchive& Ar)
		{
			Ar << TranslucentSelfShadowBufferParameter;
		}

		FShaderUniformBufferParameter TranslucentSelfShadowBufferParameter;
	};

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetShadingModel() != MSM_Unlit && IsTranslucentBlendMode(Material->GetBlendMode()) && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("TRANSLUCENT_SELF_SHADOWING"),TEXT("1"));
	}

	static bool RequiresSkylight()
	{
		return false;
	}

	/** Initialization constructor. */
	FSelfShadowedTranslucencyPolicy() {}
	
	static void GetVertexShaderBindings(
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const ElementDataType& ShaderElementData,
		const VertexParametersType* VertexShaderParameters,
		FMeshDrawSingleShaderBindings& ShaderBindings) {}

	static void GetPixelShaderBindings(
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const ElementDataType& ShaderElementData,
		const PixelParametersType* PixelShaderParameters,
		FMeshDrawSingleShaderBindings& ShaderBindings)
	{
		ShaderBindings.Add(PixelShaderParameters->TranslucentSelfShadowBufferParameter, ShaderElementData);
	}

	friend bool operator==(const FSelfShadowedTranslucencyPolicy A,const FSelfShadowedTranslucencyPolicy B)
	{
		return true;
	}

	friend int32 CompareDrawingPolicy(const FSelfShadowedTranslucencyPolicy&,const FSelfShadowedTranslucencyPolicy&)
	{
		return 0;
	}

};

/**
 * Allows precomputed irradiance lookups at any point in space.
 */
struct FPrecomputedVolumetricLightmapLightingPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	
		return Material->GetShadingModel() != MSM_Unlit
			&& (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnAnyThread() != 0);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("PRECOMPUTED_IRRADIANCE_VOLUME_LIGHTING"),TEXT("1"));
	}
};

/**
 * Allows a dynamic object to access indirect lighting through a per-object allocation in a volume texture atlas
 */
struct FCachedVolumeIndirectLightingPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));

		return Material->GetShadingModel() != MSM_Unlit 
			&& !IsTranslucentBlendMode(Material->GetBlendMode()) 
			&& (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnAnyThread() != 0)
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("CACHED_VOLUME_INDIRECT_LIGHTING"),TEXT("1"));
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};


/**
 * Allows a dynamic object to access indirect lighting through a per-object lighting sample
 */
struct FCachedPointIndirectLightingPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	
		return Material->GetShadingModel() != MSM_Unlit
			&& (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnAnyThread() != 0);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);

	static bool RequiresSkylight()
	{
		return false;
	}
};

/**
 * Renders the base pass without outputting to GBuffers, used to support low end hardware where deferred shading is too expensive.
 */
struct FSimpleNoLightmapLightingPolicy : public FNoLightMapPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return PlatformSupportsSimpleForwardShading(Platform)
			&& FNoLightMapPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SIMPLE_FORWARD_SHADING"),TEXT("1"));
		FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return true;
	}
};

/**
 * Renders lightmaps without outputting to GBuffers, used to support low end hardware where deferred shading is too expensive.
 */
struct FSimpleLightmapOnlyLightingPolicy : public TLightMapPolicy<HQ_LIGHTMAP>
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));

		return AllowStaticLightingVar->GetValueOnAnyThread() != 0
			&& PlatformSupportsSimpleForwardShading(Platform)
			&& TLightMapPolicy<HQ_LIGHTMAP>::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SIMPLE_FORWARD_SHADING"),TEXT("1"));
		TLightMapPolicy<HQ_LIGHTMAP>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return true;
	}
};

/**
 * Renders an unshadowed directional light in the base pass, used to support low end hardware where deferred shading is too expensive.
 */
struct FSimpleDirectionalLightLightingPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return PlatformSupportsSimpleForwardShading(Platform)
			&& Material->GetShadingModel() != MSM_Unlit;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SIMPLE_FORWARD_SHADING"),TEXT("1"));
		OutEnvironment.SetDefine(TEXT("SIMPLE_FORWARD_DIRECTIONAL_LIGHT"),TEXT("1"));
	}

	static bool RequiresSkylight()
	{
		return true;
	}
};

/**
 * Renders a stationary directional light in the base pass with distance field precomputed shadows without outputting to GBuffers, used to support low end hardware where deferred shading is too expensive.
 */
struct FSimpleStationaryLightPrecomputedShadowsLightingPolicy : public TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP>
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));

		return AllowStaticLightingVar->GetValueOnAnyThread() != 0
			&& PlatformSupportsSimpleForwardShading(Platform)
			&& TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP>::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SIMPLE_FORWARD_SHADING"),TEXT("1"));
		OutEnvironment.SetDefine(TEXT("SIMPLE_FORWARD_DIRECTIONAL_LIGHT"),TEXT("1"));
		TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return true;
	}
};

/**
 * Renders a stationary directional light in the base pass with single sample shadows without outputting to GBuffers, used to support low end hardware where deferred shading is too expensive.
 */
struct FSimpleStationaryLightSingleSampleShadowsLightingPolicy : public FCachedPointIndirectLightingPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));

		return AllowStaticLightingVar->GetValueOnAnyThread() != 0
			&& PlatformSupportsSimpleForwardShading(Platform)
			&& FCachedPointIndirectLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SIMPLE_FORWARD_SHADING"),TEXT("1"));
		OutEnvironment.SetDefine(TEXT("SIMPLE_FORWARD_DIRECTIONAL_LIGHT"),TEXT("1"));
		FCachedPointIndirectLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return true;
	}
};

struct FSimpleStationaryLightVolumetricLightmapShadowsLightingPolicy : public FPrecomputedVolumetricLightmapLightingPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));

		return AllowStaticLightingVar->GetValueOnAnyThread() != 0
			&& PlatformSupportsSimpleForwardShading(Platform)
			&& FPrecomputedVolumetricLightmapLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SIMPLE_FORWARD_SHADING"),TEXT("1"));
		OutEnvironment.SetDefine(TEXT("SIMPLE_FORWARD_DIRECTIONAL_LIGHT"),TEXT("1"));
		FPrecomputedVolumetricLightmapLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

/** Mobile Specific: Combines a distance field shadow with LQ lightmaps. */
class FMobileDistanceFieldShadowsAndLQLightMapPolicy : public TDistanceFieldShadowsAndLightMapPolicy<LQ_LIGHTMAP>
{
	typedef TDistanceFieldShadowsAndLightMapPolicy<LQ_LIGHTMAP>	Super;
public:
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		static auto* CVarMobileAllowDistanceFieldShadows = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowDistanceFieldShadows"));
		const bool bMobileAllowDistanceFieldShadows = CVarMobileAllowDistanceFieldShadows->GetValueOnAnyThread() == 1;
		return bMobileAllowDistanceFieldShadows && Super::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

FORCEINLINE_DEBUGGABLE int32 GetMobileMaxShadowCascades()
{
	static const auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.CSM.MaxMobileCascades"));
	return CVar ? FMath::Clamp(CVar->GetValueOnAnyThread(), 0, MAX_MOBILE_SHADOWCASCADES) : MAX_MOBILE_SHADOWCASCADES;
}

/** Mobile Specific: Combines an distance field shadow with LQ lightmaps and CSM. */
class FMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy : public FMobileDistanceFieldShadowsAndLQLightMapPolicy
{
	typedef FMobileDistanceFieldShadowsAndLQLightMapPolicy Super;

public:
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		static auto* CVarMobileEnableStaticAndCSMShadowReceivers = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableStaticAndCSMShadowReceivers"));
		const bool bMobileEnableStaticAndCSMShadowReceivers = CVarMobileEnableStaticAndCSMShadowReceivers->GetValueOnAnyThread() == 1;
		return bMobileEnableStaticAndCSMShadowReceivers && (Material->GetShadingModel() != MSM_Unlit) && Super::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DIRECTIONAL_LIGHT_CSM"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_MOBILE_SHADOWCASCADES)), GetMobileMaxShadowCascades());

		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

/** Mobile Specific: Combines an unshadowed directional light with indirect lighting from a single SH sample. */
struct FMobileDirectionalLightAndSHIndirectPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static auto* CVarAllowStaticLighting = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		const bool bAllowStaticLighting = CVarAllowStaticLighting->GetValueOnAnyThread() != 0;

		return bAllowStaticLighting && Material->GetShadingModel() != MSM_Unlit && FCachedPointIndirectLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FCachedPointIndirectLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

/** Mobile Specific: Combines a movable directional light with indirect lighting from a single SH sample. */
struct FMobileMovableDirectionalLightAndSHIndirectPolicy : public FMobileDirectionalLightAndSHIndirectPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		static auto* CVarMobileAllowMovableDirectionalLights = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowMovableDirectionalLights"));
		const bool bMobileAllowMovableDirectionalLights = CVarMobileAllowMovableDirectionalLights->GetValueOnAnyThread() != 0;

		return bMobileAllowMovableDirectionalLights && FMobileDirectionalLightAndSHIndirectPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_MOBILE_SHADOWCASCADES)), GetMobileMaxShadowCascades());
		FMobileDirectionalLightAndSHIndirectPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

/** Mobile Specific: Combines a movable directional light with CSM with indirect lighting from a single SH sample. */
struct FMobileMovableDirectionalLightCSMAndSHIndirectPolicy : public FMobileMovableDirectionalLightAndSHIndirectPolicy
{
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DIRECTIONAL_LIGHT_CSM"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_MOBILE_SHADOWCASCADES)), GetMobileMaxShadowCascades());
		FMobileMovableDirectionalLightAndSHIndirectPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

/** Mobile Specific: Combines a directional light with CSM with indirect lighting from a single SH sample. */
class FMobileDirectionalLightCSMAndSHIndirectPolicy : public FMobileDirectionalLightAndSHIndirectPolicy
{
public:
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DIRECTIONAL_LIGHT_CSM"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_MOBILE_SHADOWCASCADES)), GetMobileMaxShadowCascades());
		FMobileDirectionalLightAndSHIndirectPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

/** Mobile Specific */
struct FMobileMovableDirectionalLightLightingPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static auto* CVarMobileAllowMovableDirectionalLights = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowMovableDirectionalLights"));
		const bool bMobileAllowMovableDirectionalLights = CVarMobileAllowMovableDirectionalLights->GetValueOnAnyThread() != 0;

		return bMobileAllowMovableDirectionalLights && Material->GetShadingModel() != MSM_Unlit;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT"),TEXT("1"));
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

/** Mobile Specific */
struct FMobileMovableDirectionalLightCSMLightingPolicy
{
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		static auto* CVarMobileAllowMovableDirectionalLights = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowMovableDirectionalLights"));
		const bool bMobileAllowMovableDirectionalLights = CVarMobileAllowMovableDirectionalLights->GetValueOnAnyThread() != 0;

		return bMobileAllowMovableDirectionalLights && Material->GetShadingModel() != MSM_Unlit;
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("DIRECTIONAL_LIGHT_CSM"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_MOBILE_SHADOWCASCADES)), GetMobileMaxShadowCascades());

		FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

/** Mobile Specific */
struct FMobileMovableDirectionalLightWithLightmapPolicy : public TLightMapPolicy<LQ_LIGHTMAP>
{
	typedef TLightMapPolicy<LQ_LIGHTMAP> Super;

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		static auto* CVarMobileAllowMovableDirectionalLights = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowMovableDirectionalLights"));
		const bool bMobileAllowMovableDirectionalLights = CVarMobileAllowMovableDirectionalLights->GetValueOnAnyThread() != 0;

		static auto* CVarAllowStaticLighting = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		const bool bAllowStaticLighting = (!CVarAllowStaticLighting || CVarAllowStaticLighting->GetValueOnAnyThread() != 0);

		return bAllowStaticLighting && bMobileAllowMovableDirectionalLights && (Material->GetShadingModel() != MSM_Unlit) && Super::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_MOBILE_SHADOWCASCADES)), GetMobileMaxShadowCascades());

		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

/** Mobile Specific */
struct FMobileMovableDirectionalLightCSMWithLightmapPolicy : public FMobileMovableDirectionalLightWithLightmapPolicy
{
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DIRECTIONAL_LIGHT_CSM"), TEXT("1"));

		FMobileMovableDirectionalLightWithLightmapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	static bool RequiresSkylight()
	{
		return false;
	}
};

enum ELightMapPolicyType
{
	LMP_NO_LIGHTMAP,
	LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING,
	LMP_CACHED_VOLUME_INDIRECT_LIGHTING,
	LMP_CACHED_POINT_INDIRECT_LIGHTING,
	LMP_SIMPLE_NO_LIGHTMAP,
	LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING,
	LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING,
	LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING,
	LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING,
	LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING,
	LMP_LQ_LIGHTMAP,
	LMP_HQ_LIGHTMAP,
	LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP,
	// Mobile specific
	LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP,
	LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM,
	LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT,
	LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT,
	LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT,
	LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT,
	LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT,
	LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM,
	LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP,
	LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP,
	// LightMapDensity
	LMP_DUMMY
};

class FUniformLightMapPolicyShaderParametersType
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		PrecomputedLightingBufferParameter.Bind(ParameterMap, TEXT("PrecomputedLightingBuffer"));
		IndirectLightingCacheParameter.Bind(ParameterMap, TEXT("IndirectLightingCache"));
		LightmapResourceCluster.Bind(ParameterMap, TEXT("LightmapResourceCluster"));
	}

	void Serialize(FArchive& Ar)
	{
		Ar << PrecomputedLightingBufferParameter;
		Ar << IndirectLightingCacheParameter;
		Ar << LightmapResourceCluster;
	}

	FShaderUniformBufferParameter PrecomputedLightingBufferParameter;
	FShaderUniformBufferParameter IndirectLightingCacheParameter;
	FShaderUniformBufferParameter LightmapResourceCluster;
};

class FUniformLightMapPolicy
{
public:

	typedef const FLightCacheInterface* ElementDataType;

	typedef FUniformLightMapPolicyShaderParametersType PixelParametersType;
	typedef FUniformLightMapPolicyShaderParametersType VertexParametersType;
#if RHI_RAYTRACING
	typedef FUniformLightMapPolicyShaderParametersType RayHitGroupParametersType;
#endif

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return false; // This one does not compile shaders since we can't tell which policy to use.
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment) 
	{}

	FUniformLightMapPolicy(ELightMapPolicyType InIndirectPolicy) : IndirectPolicy(InIndirectPolicy) {}

	void SetMesh(
		FRHICommandList& RHICmdList,
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FLightCacheInterface* LCI
		) const;

	static void GetVertexShaderBindings(
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const ElementDataType& ShaderElementData,
		const VertexParametersType* VertexShaderParameters,
		FMeshDrawSingleShaderBindings& ShaderBindings);

	static void GetPixelShaderBindings(
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const ElementDataType& ShaderElementData,
		const PixelParametersType* PixelShaderParameters,
		FMeshDrawSingleShaderBindings& ShaderBindings);

#if RHI_RAYTRACING
	void GetRayHitGroupShaderBindings(
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FLightCacheInterface* ElementData,
		const RayHitGroupParametersType* RayHitGroupShaderParameters,
		FMeshDrawSingleShaderBindings& RayHitGroupBindings
	) const;
#endif // RHI_RAYTRACING

	friend bool operator==(const FUniformLightMapPolicy A,const FUniformLightMapPolicy B)
	{
		return A.IndirectPolicy == B.IndirectPolicy;
	}

	friend int32 CompareDrawingPolicy(const FUniformLightMapPolicy& A,const FUniformLightMapPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(IndirectPolicy);
		return  0;
	}

	ELightMapPolicyType GetIndirectPolicy() const { return IndirectPolicy; }

private:

	ELightMapPolicyType IndirectPolicy;
};

template <ELightMapPolicyType Policy>
class TUniformLightMapPolicy : public FUniformLightMapPolicy
{
public:

	TUniformLightMapPolicy() : FUniformLightMapPolicy(Policy) {}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		CA_SUPPRESS(6326);
		switch (Policy)
		{
		case LMP_NO_LIGHTMAP:
			return FNoLightMapPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING:
			return FPrecomputedVolumetricLightmapLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_CACHED_VOLUME_INDIRECT_LIGHTING:
			return FCachedVolumeIndirectLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_CACHED_POINT_INDIRECT_LIGHTING:
			return FCachedPointIndirectLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_SIMPLE_NO_LIGHTMAP:
			return FSimpleNoLightmapLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING:
			return FSimpleLightmapOnlyLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING:
			return FSimpleDirectionalLightLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING:
			return FSimpleStationaryLightPrecomputedShadowsLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING:
			return FSimpleStationaryLightSingleSampleShadowsLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING:
			return FSimpleStationaryLightVolumetricLightmapShadowsLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_LQ_LIGHTMAP:
			return TLightMapPolicy<LQ_LIGHTMAP>::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_HQ_LIGHTMAP:
			return TLightMapPolicy<HQ_LIGHTMAP>::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP:
			return TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP>::ShouldCompilePermutation(Platform, Material, VertexFactoryType);

		// Mobile specific
		
		case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP:
			return FMobileDistanceFieldShadowsAndLQLightMapPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM:
			return FMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
			return FMobileDirectionalLightAndSHIndirectPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
			return FMobileMovableDirectionalLightAndSHIndirectPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
			return FMobileDirectionalLightCSMAndSHIndirectPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
			return FMobileMovableDirectionalLightCSMAndSHIndirectPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT:
			return FMobileMovableDirectionalLightLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM:
			return FMobileMovableDirectionalLightCSMLightingPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP:
			return FMobileMovableDirectionalLightWithLightmapPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP:
			return FMobileMovableDirectionalLightCSMWithLightmapPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);

		// LightMapDensity
	
		case LMP_DUMMY:
			return FDummyLightMapPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);

		default:
			check(false);
			return false;
		};
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment) 
	{
		OutEnvironment.SetDefine(TEXT("MAX_NUM_LIGHTMAP_COEF"), MAX_NUM_LIGHTMAP_COEF);

		CA_SUPPRESS(6326);
		switch (Policy)
		{
		case LMP_NO_LIGHTMAP:							
			FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING:
			FPrecomputedVolumetricLightmapLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_CACHED_VOLUME_INDIRECT_LIGHTING:
			FCachedVolumeIndirectLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_CACHED_POINT_INDIRECT_LIGHTING:
			FCachedPointIndirectLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_SIMPLE_NO_LIGHTMAP:
			return FSimpleNoLightmapLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING:
			return FSimpleLightmapOnlyLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING:
			FSimpleDirectionalLightLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING:
			return FSimpleStationaryLightPrecomputedShadowsLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING:
			return FSimpleStationaryLightSingleSampleShadowsLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING:
			return FSimpleStationaryLightVolumetricLightmapShadowsLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_LQ_LIGHTMAP:
			TLightMapPolicy<LQ_LIGHTMAP>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_HQ_LIGHTMAP:
			TLightMapPolicy<HQ_LIGHTMAP>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP:
			TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;

		// Mobile specific
		case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP:
			FMobileDistanceFieldShadowsAndLQLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM:
			FMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
			FMobileDirectionalLightAndSHIndirectPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
			FMobileMovableDirectionalLightAndSHIndirectPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
			FMobileDirectionalLightCSMAndSHIndirectPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
			FMobileMovableDirectionalLightCSMAndSHIndirectPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT:
			FMobileMovableDirectionalLightLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM:
			FMobileMovableDirectionalLightCSMLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP:
			FMobileMovableDirectionalLightWithLightmapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;
		case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP:
			FMobileMovableDirectionalLightCSMWithLightmapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;

		// LightMapDensity
	
		case LMP_DUMMY:
			FDummyLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
			break;

		default:
			check(false);
			break;
		}
	}

	static bool RequiresSkylight()
	{
		CA_SUPPRESS(6326);
		switch (Policy)
		{
		// Simple forward
		case LMP_SIMPLE_NO_LIGHTMAP:
		case LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING:
		case LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING:
		case LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING:
		case LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING:
			return true;
		default:
			return false;
		};
	}
};

struct FSelfShadowLightCacheElementData
{
	const FLightCacheInterface* LCI;
	FUniformBufferRHIParamRef SelfShadowTranslucencyUniformBuffer;
};

/**
 * Self shadowing translucency from a directional light + allows a dynamic object to access indirect lighting through a per-object lighting sample
 */
class FSelfShadowedCachedPointIndirectLightingPolicy : public FSelfShadowedTranslucencyPolicy
{
public:
	typedef const FSelfShadowLightCacheElementData ElementDataType;

	class PixelParametersType : public FUniformLightMapPolicyShaderParametersType, public FSelfShadowedTranslucencyPolicy::PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			FUniformLightMapPolicyShaderParametersType::Bind(ParameterMap);
			FSelfShadowedTranslucencyPolicy::PixelParametersType::Bind(ParameterMap);
		}

		void Serialize(FArchive& Ar)
		{
			FUniformLightMapPolicyShaderParametersType::Serialize(Ar);
			FSelfShadowedTranslucencyPolicy::PixelParametersType::Serialize(Ar);
		}
	};

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static IConsoleVariable* AllowStaticLightingVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowStaticLighting"));

		return Material->GetShadingModel() != MSM_Unlit 
			&& IsTranslucentBlendMode(Material->GetBlendMode()) 
			&& (!AllowStaticLightingVar || AllowStaticLightingVar->GetInt() != 0)
			&& FSelfShadowedTranslucencyPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);

	/** Initialization constructor. */
	FSelfShadowedCachedPointIndirectLightingPolicy() {}

	static void GetVertexShaderBindings(
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const ElementDataType& ShaderElementData,
		const VertexParametersType* VertexShaderParameters,
		FMeshDrawSingleShaderBindings& ShaderBindings) {}

	static void GetPixelShaderBindings(
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const ElementDataType& ShaderElementData,
		const PixelParametersType* PixelShaderParameters,
		FMeshDrawSingleShaderBindings& ShaderBindings);
};

class FSelfShadowedVolumetricLightmapPolicy : public FSelfShadowedTranslucencyPolicy
{
public:
	typedef const FSelfShadowLightCacheElementData ElementDataType;

	class PixelParametersType : public FUniformLightMapPolicyShaderParametersType, public FSelfShadowedTranslucencyPolicy::PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			FUniformLightMapPolicyShaderParametersType::Bind(ParameterMap);
			FSelfShadowedTranslucencyPolicy::PixelParametersType::Bind(ParameterMap);
		}

		void Serialize(FArchive& Ar)
		{
			FUniformLightMapPolicyShaderParametersType::Serialize(Ar);
			FSelfShadowedTranslucencyPolicy::PixelParametersType::Serialize(Ar);
		}
	};

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static IConsoleVariable* AllowStaticLightingVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowStaticLighting"));

		return Material->GetShadingModel() != MSM_Unlit 
			&& IsTranslucentBlendMode(Material->GetBlendMode()) 
			&& (!AllowStaticLightingVar || AllowStaticLightingVar->GetInt() != 0)
			&& FSelfShadowedTranslucencyPolicy::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("PRECOMPUTED_IRRADIANCE_VOLUME_LIGHTING"),TEXT("1"));	
		FSelfShadowedTranslucencyPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	FSelfShadowedVolumetricLightmapPolicy() {}

	static void GetVertexShaderBindings(
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const ElementDataType& ShaderElementData,
		const VertexParametersType* VertexShaderParameters,
		FMeshDrawSingleShaderBindings& ShaderBindings) {}

	static void GetPixelShaderBindings(
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const ElementDataType& ShaderElementData,
		const PixelParametersType* PixelShaderParameters,
		FMeshDrawSingleShaderBindings& ShaderBindings);
};