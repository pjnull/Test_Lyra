// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileBasePassRendering.cpp: Base pass rendering implementation.
=============================================================================*/

#include "MobileBasePassRendering.h"
#include "TranslucentRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "StaticMeshDrawList.h"
#include "ScenePrivate.h"
#include "ShaderPlatformQualitySettings.h"
#include "MaterialShaderQualitySettings.h"
#include "PrimitiveSceneInfo.h"
#include "MeshPassProcessor.inl"

template <ELightMapPolicyType Policy, int32 NumMovablePointLights>
void GetUniformMobileBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	bool bEnableSkyLight,
	TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>*& VertexShader,
	TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>*& PixelShader
	)
{
	if (IsMobileHDR())
	{
		VertexShader = (TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>*)Material.GetShader<TMobileBasePassVS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64> >(VertexFactoryType);

		if (bEnableSkyLight)
		{
			PixelShader = (TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>*)Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64, true, NumMovablePointLights> >(VertexFactoryType);
		}
		else
		{
			PixelShader = (TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>*)Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64, false, NumMovablePointLights> >(VertexFactoryType);
		}	
	}
	else
	{
		VertexShader = (TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>*)Material.GetShader<TMobileBasePassVS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32> >(VertexFactoryType);

		if (bEnableSkyLight)
		{
			PixelShader = (TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>*)Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32, true, NumMovablePointLights> >(VertexFactoryType);
		}
		else
		{
			PixelShader = (TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>*)Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32, false, NumMovablePointLights> >(VertexFactoryType);
		}			
	}
}

template <int32 NumMovablePointLights>
void GetMobileBasePassShaders(
	ELightMapPolicyType LightMapPolicyType, 
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	bool bEnableSkyLight,
	TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>*& VertexShader,
	TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>*& PixelShader
	)
{
	switch (LightMapPolicyType)
	{
	case LMP_LQ_LIGHTMAP:
		GetUniformMobileBasePassShaders<LMP_LQ_LIGHTMAP, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP:
		GetUniformMobileBasePassShaders<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM:
		GetUniformMobileBasePassShaders<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
		GetUniformMobileBasePassShaders<LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
		GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
		GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
		GetUniformMobileBasePassShaders<LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT:
		GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM:
		GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP:
		GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP:
		GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_NO_LIGHTMAP:
		GetUniformMobileBasePassShaders<LMP_NO_LIGHTMAP, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	default:										
		check(false);
	}
}

void MobileBasePass::GetShaders(
	ELightMapPolicyType LightMapPolicyType,
	int32 NumMovablePointLights, 
	const FMaterial& MaterialResource,
	FVertexFactoryType* VertexFactoryType,
	bool bEnableSkyLight, 
	TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>*& VertexShader,
	TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>*& PixelShader)
{
	bool bIsLit = (MaterialResource.GetShadingModel() != MSM_Unlit);
	if (bIsLit && !UseSkylightPermutation(bEnableSkyLight, FReadOnlyCVARCache::Get().MobileSkyLightPermutation))	
	{
		bEnableSkyLight = !bEnableSkyLight;
	}
		
	switch (NumMovablePointLights)
	{
	case INT32_MAX:
		GetMobileBasePassShaders<INT32_MAX>(
			LightMapPolicyType,
			MaterialResource,
			VertexFactoryType,
			bEnableSkyLight,
			VertexShader,
			PixelShader
			);
		break;
	case 1:
		GetMobileBasePassShaders<1>(
			LightMapPolicyType, 
			MaterialResource, 
			VertexFactoryType, 
			bEnableSkyLight, 
			VertexShader,
			PixelShader
			);
		break;
	case 2:
		GetMobileBasePassShaders<2>(
			LightMapPolicyType,
			MaterialResource,
			VertexFactoryType,
			bEnableSkyLight,
			VertexShader,
			PixelShader
			);
		break;
	case 3:
		GetMobileBasePassShaders<3>(
			LightMapPolicyType,
			MaterialResource,
			VertexFactoryType,
			bEnableSkyLight,
			VertexShader,
			PixelShader
			);
		break;
	case 4:
		GetMobileBasePassShaders<4>(
			LightMapPolicyType,
			MaterialResource,
			VertexFactoryType,
			bEnableSkyLight,
			VertexShader,
			PixelShader
			);
		break;
	case 0:
	default:
		GetMobileBasePassShaders<0>(
			LightMapPolicyType,
			MaterialResource,
			VertexFactoryType,
			bEnableSkyLight,
			VertexShader,
			PixelShader
			);
		break;
	}
}

bool MobileBasePass::UseSkyReflectionCapture(const FScene* RenderScene)
{
	return RenderScene
		&& RenderScene->ReflectionSceneData.RegisteredReflectionCapturePositions.Num() == 0
		&& RenderScene->SkyLight
		&& RenderScene->SkyLight->ProcessedTexture->TextureRHI;
}

void MobileBasePass::GetSkyTextureParams(const FScene* Scene, float& AverageBrightnessOUT, FTexture*& ReflectionTextureOUT, float& OutSkyMaxMipIndex)
{
	if (Scene && Scene->SkyLight && Scene->SkyLight->ProcessedTexture->TextureRHI)
	{
		AverageBrightnessOUT = Scene->SkyLight->AverageBrightness;
		ReflectionTextureOUT = Scene->SkyLight->ProcessedTexture;
		OutSkyMaxMipIndex = FMath::Log2(ReflectionTextureOUT->GetSizeX());
	}
}

const FLightSceneInfo* MobileBasePass::GetDirectionalLightInfo(const FScene* Scene, const FPrimitiveSceneProxy* PrimitiveSceneProxy)
{
	const FLightSceneInfo* MobileDirectionalLight = nullptr;
	if (PrimitiveSceneProxy && Scene)
	{
		const int32 LightChannel = GetFirstLightingChannelFromMask(PrimitiveSceneProxy->GetLightingChannelMask());
		MobileDirectionalLight = LightChannel >= 0 ? Scene->MobileDirectionalLights[LightChannel] : nullptr;
	}
	return MobileDirectionalLight;
}

int32 MobileBasePass::CalcNumMovablePointLights(const FMaterial& InMaterial, const FPrimitiveSceneProxy* InPrimitiveSceneProxy)
{
	const FReadOnlyCVARCache& ReadOnlyCVARCache = FReadOnlyCVARCache::Get();
	const bool bIsUnlit = InMaterial.GetShadingModel() == MSM_Unlit;
	int32 OutNumMovablePointLights = (InPrimitiveSceneProxy && !bIsUnlit) ? FMath::Min<int32>(InPrimitiveSceneProxy->GetPrimitiveSceneInfo()->NumMobileMovablePointLights, ReadOnlyCVARCache.NumMobileMovablePointLights) : 0;
	if (OutNumMovablePointLights > 0 && ReadOnlyCVARCache.bMobileMovablePointLightsUseStaticBranch)
	{
		OutNumMovablePointLights = INT32_MAX;
	}
	return OutNumMovablePointLights;
}

bool MobileBasePass::StaticCanReceiveCSM(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy)
{
	// For movable directional lights, when CSM culling is disabled the default behavior is to receive CSM.
	static auto* CVarMobileEnableMovableLightCSMShaderCulling = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableMovableLightCSMShaderCulling"));
	if (LightSceneInfo && LightSceneInfo->Proxy->IsMovable() && CVarMobileEnableMovableLightCSMShaderCulling->GetValueOnRenderThread() == 0)
	{		
		return true;
	}

	// If culling is enabled then CSM receiving is determined during InitDynamicShadows.
	// If culling is disabled then stationary directional lights default to no CSM. 
	return false; 
}

ELightMapPolicyType MobileBasePass::SelectMeshLightmapPolicy(
	const FScene* Scene, 
	const FMeshBatch& Mesh, 
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FLightSceneInfo* MobileDirectionalLight,
	EMaterialShadingModel ShadingModel, 
	bool bPrimReceivesCSM, 
	ERHIFeatureLevel::Type FeatureLevel)
{
	// Unlit uses NoLightmapPolicy with 0 point lights
	ELightMapPolicyType SelectedLightmapPolicy = LMP_NO_LIGHTMAP;

	// Check for a cached light-map.
	const bool bIsLitMaterial = ShadingModel != MSM_Unlit;
	if (bIsLitMaterial)
	{
		const FLightMapInteraction LightMapInteraction = (Mesh.LCI && bIsLitMaterial)
			? Mesh.LCI->GetLightMapInteraction(FeatureLevel)
			: FLightMapInteraction();

		const FReadOnlyCVARCache& ReadOnlyCVARCache = FReadOnlyCVARCache::Get();
		const bool bUseMovableLight = MobileDirectionalLight && !MobileDirectionalLight->Proxy->HasStaticShadowing() && ReadOnlyCVARCache.bMobileAllowMovableDirectionalLights;
		const bool bUseStaticAndCSM = MobileDirectionalLight && MobileDirectionalLight->Proxy->UseCSMForDynamicObjects()
			&& bPrimReceivesCSM
			&& ReadOnlyCVARCache.bMobileEnableStaticAndCSMShadowReceivers;

		const bool bMovableWithCSM = bUseMovableLight && MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows() && bPrimReceivesCSM;

		if (LightMapInteraction.GetType() == LMIT_Texture && ReadOnlyCVARCache.bAllowStaticLighting && ReadOnlyCVARCache.bEnableLowQualityLightmaps)
		{
			// Lightmap path
			const FShadowMapInteraction ShadowMapInteraction = (Mesh.LCI && bIsLitMaterial)
				? Mesh.LCI->GetShadowMapInteraction()
				: FShadowMapInteraction();

			if (bUseMovableLight)
			{
				// final determination of whether CSMs are rendered can be view dependent, thus we always need to clear the CSMs even if we're not going to render to them based on the condition below.
				if (MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows() && bMovableWithCSM)
				{
					SelectedLightmapPolicy = LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP;
				}
				else
				{
					SelectedLightmapPolicy = LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP;
				}
			}
			else if (bUseStaticAndCSM)
			{
				if (ShadowMapInteraction.GetType() == SMIT_Texture && MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows() && ReadOnlyCVARCache.bMobileAllowDistanceFieldShadows)
				{
					SelectedLightmapPolicy = LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM;
				}
				else
				{
					SelectedLightmapPolicy = LMP_LQ_LIGHTMAP;
				}
			}
			else
			{
				if (ShadowMapInteraction.GetType() == SMIT_Texture && ReadOnlyCVARCache.bMobileAllowDistanceFieldShadows)
				{
					SelectedLightmapPolicy = LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP;
				}
				else
				{
					SelectedLightmapPolicy = LMP_LQ_LIGHTMAP;
				}
			}
		}
		else if (IsIndirectLightingCacheAllowed(FeatureLevel) /* implies bAllowStaticLighting*/
			&& PrimitiveSceneProxy
			// Movable objects need to get their GI from the indirect lighting cache
			&& PrimitiveSceneProxy->IsMovable())
		{
			if (bUseMovableLight)
			{
				if (MobileDirectionalLight && MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows() && bMovableWithCSM)
				{
					SelectedLightmapPolicy = LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT;
				}
				else
				{
					SelectedLightmapPolicy = LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT;
				}
			}
			else
			{
				if (bUseStaticAndCSM)
				{
					SelectedLightmapPolicy = LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT;
				}
				else
				{
					SelectedLightmapPolicy = LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT;
				}
			}
		}
		else if (bUseMovableLight)
		{
			// final determination of whether CSMs are rendered can be view dependent, thus we always need to clear the CSMs even if we're not going to render to them based on the condition below.
			if (MobileDirectionalLight && bMovableWithCSM)
			{
				SelectedLightmapPolicy = LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM;
			}
			else
			{
				SelectedLightmapPolicy = LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT;
			}
		}
	}
		
	return SelectedLightmapPolicy;
}

void MobileBasePass::SetOpaqueRenderState(FDrawingPolicyRenderState& DrawRenderState, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMaterial& Material, bool bEnableReceiveDecalOutput)
{
	bool bEncodedHDR = GetMobileHDRMode() == EMobileHDRMode::EnabledRGBE && Material.GetMaterialDomain() != MD_UI;
	if (bEncodedHDR)
	{
		DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
	}
		
	if (bEnableReceiveDecalOutput)
	{
		const uint8 StencilValue = (PrimitiveSceneProxy && !PrimitiveSceneProxy->ReceivesDecals() ? 0x01 : 0x00);
		
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
				true, CF_DepthNearOrEqual,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				0x00, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1)>::GetRHI());
		
		DrawRenderState.SetStencilRef(GET_STENCIL_BIT_MASK(RECEIVE_DECAL, StencilValue)); // we hash the stencil group because we only have 6 bits.
	}
	else
	{
		// default depth state should be already set
	}
}

void MobileBasePass::SetTranslucentRenderState(FDrawingPolicyRenderState& DrawRenderState, const FMaterial& Material)
{
	bool bEncodedHDR = GetMobileHDRMode() == EMobileHDRMode::EnabledRGBE && Material.GetMaterialDomain() != MD_UI;
	static const auto CVarMonoscopicFarField = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MonoscopicFarField"));
	const bool bIsMobileMonoscopic = CVarMonoscopicFarField && (CVarMonoscopicFarField->GetValueOnRenderThread() != 0);

	if (bEncodedHDR == false)
	{
		switch (Material.GetBlendMode())
		{
		case BLEND_Translucent:
			if (Material.ShouldWriteOnlyAlpha())
			{
				DrawRenderState.SetBlendState(TStaticBlendState<CW_ALPHA, BO_Add, BF_Zero, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
			} 
			else if (bIsMobileMonoscopic)
			{
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_One>::GetRHI());
			}
			else
			{
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
			}
			break;
		case BLEND_Additive:
			// Add to the existing scene color
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
			break;
		case BLEND_Modulate:
			// Modulate with the existing scene color
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
			break;
		case BLEND_AlphaComposite:
			// Blend with existing scene color. New color is already pre-multiplied by alpha.
			DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
			break;
		default:
			check(0);
		};
	}
	else
	{
		DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
	}

	if (Material.ShouldDisableDepthTest())
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	}
}

static FMeshDrawCommandSortKey GetBasePassStaticSortKey(EBlendMode BlendMode)
{
	FMeshDrawCommandSortKey SortKey;
	SortKey.PackedData = (BlendMode == EBlendMode::BLEND_Masked ? 1 : 0);
	return SortKey;
}

static uint64 GetSortKey(bool bMasked, bool bBackground, int32 PipelineId, int32 StateBucketId, float PipelineDistance, float PrimitiveDistance)
{
	const float PrimitiveDepthQuantization = ((1 << 14) - 1);
	
	union
	{
		uint64 PackedInt;
		struct
		{
			uint64 DepthBits : 14;			// Order by primitive depth
			uint64 StateBucketId : 20; 		// Order by state bucket
			uint64 PipelineId : 20;			// Order by PSO
			uint64 PipelineDepthBits : 8;	// Order PSOs front to back
			uint64 Background : 1;			// Non-background meshes first 
			uint64 Masked : 1;				// Non-masked first
		} Fields;
	} Key;

	union FFloatToInt { float F; uint32 I; };
	FFloatToInt F2I;
	
	Key.PackedInt = 0;
	Key.Fields.Masked = bMasked;
	Key.Fields.Background = bBackground;
	F2I.F = PipelineDistance/HALF_WORLD_MAX;
	Key.Fields.PipelineDepthBits = (F2I.I >> 23) & 0xff; // 8 bit exponent
	Key.Fields.PipelineId = PipelineId;
	Key.Fields.StateBucketId = StateBucketId;
	Key.Fields.DepthBits = int32((FMath::Min<float>(PrimitiveDistance, HALF_WORLD_MAX) / HALF_WORLD_MAX) * PrimitiveDepthQuantization);
	
	return Key.PackedInt;
}

void MobileBasePass::ComputeBasePassSortKeys(const FScene& Scene, const FViewInfo& View, FMeshCommandOneFrameArray& VisibleMeshCommands)
{
	FVector ViewLocation = View.ViewMatrices.GetViewOrigin();
	int32 NumCmds = VisibleMeshCommands.Num();

	// pre-compute distance to a group of meshes that share same PSO
	TMap<int32, float> PipelineDistances;
	PipelineDistances.Reserve(256);

	for (int32 CmdIdx = 0; CmdIdx < NumCmds; ++CmdIdx)
	{
		FVisibleMeshDrawCommand& Cmd = VisibleMeshCommands[CmdIdx];

		float PrimitiveDistance = 0;

		if (Cmd.DrawPrimitiveId < Scene.PrimitiveBounds.Num())
		{
			const FPrimitiveBounds& PrimitiveBounds = Scene.PrimitiveBounds[Cmd.DrawPrimitiveId];
			PrimitiveDistance = (PrimitiveBounds.BoxSphereBounds.Origin - ViewLocation).Size();
		}
		
		float& PipelineDistance = PipelineDistances.FindOrAdd(Cmd.MeshDrawCommand->CachedPipelineId.GetId());
		// not sure what could be better: average distance, max or min
		PipelineDistance = FMath::Max(PipelineDistance, PrimitiveDistance);
	}
	
	// compute sort key for each mesh command
	for (int32 CmdIdx = 0; CmdIdx < NumCmds; ++CmdIdx)
	{
		FVisibleMeshDrawCommand& Cmd = VisibleMeshCommands[CmdIdx];
		float PrimitiveDistance = 0;
		bool bMasked = false;
		bool bBackground = false;

		if (Cmd.DrawPrimitiveId < Scene.PrimitiveBounds.Num())
		{
			const FPrimitiveBounds& PrimitiveBounds = Scene.PrimitiveBounds[Cmd.DrawPrimitiveId];
			PrimitiveDistance = (PrimitiveBounds.BoxSphereBounds.Origin - ViewLocation).Size();
			bBackground = (PrimitiveBounds.BoxSphereBounds.SphereRadius > HALF_WORLD_MAX/4.0f); // TODO: per-primitive bBackgound flag
			bMasked = View.PrimitiveViewRelevanceMap[Cmd.DrawPrimitiveId].bMaskedRelevance;
		}

		int32 PipelineId = Cmd.MeshDrawCommand->CachedPipelineId.GetId();
		float PipelineDistance = PipelineDistances.FindRef(PipelineId);
		// poor man StateID, can't use Cmd.StateBucketId as it is unique for each primitive if platform does not support auto-instancing
		int32 StateBucketId = PointerHash(Cmd.MeshDrawCommand->IndexBuffer);
				
		Cmd.SortKey.PackedData = GetSortKey(bMasked, bBackground, PipelineId, StateBucketId, PipelineDistance, PrimitiveDistance);
	}
}

template<>
void TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>::GetShaderBindings(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material,
	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
	FUniformBufferRHIParamRef PassUniformBufferValue,
	const TMobileBasePassShaderElementData<FUniformLightMapPolicy>& ShaderElementData,
	FMeshDrawSingleShaderBindings& ShaderBindings) const
{
	FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ViewUniformBuffer, PassUniformBufferValue, ShaderElementData, ShaderBindings);

	FUniformLightMapPolicy::GetPixelShaderBindings(
		PrimitiveSceneProxy,
		ShaderElementData.LightMapPolicyElementData,
		this,
		ShaderBindings);

	if (Scene)
	{
		static const int32 MaxNumReflections = FPrimitiveSceneInfo::MaxCachedReflectionCaptureProxies;
		static_assert(MaxNumReflections == 3, "Update reflection array initializations to match MaxCachedReflectionCaptureProxies");
		// set reflection parameters
		const FShaderResourceParameter* ReflectionTextureParameters[MaxNumReflections] = { &ReflectionCubemap, &ReflectionCubemap1, &ReflectionCubemap2 };
		const FShaderResourceParameter* ReflectionSamplerParameters[MaxNumReflections] = { &ReflectionSampler, &ReflectionSampler1, &ReflectionSampler2 };
		FTexture* ReflectionCubemapTextures[MaxNumReflections] = { GBlackTextureCube, GBlackTextureCube, GBlackTextureCube };
		FVector4 CapturePositions[MaxNumReflections] = { FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0) };
		FVector4 ReflectionParams(1.0f, 1.0f, 1.0f, 0.0f);
		
		if (MobileBasePass::UseSkyReflectionCapture(Scene))
		{
			// if > 0 this will disable shader's RGBM decoding and enable sky light tinting of this envmap.
			// ReflectionParams.X == inv average brightness
			// ReflectionParams.W == max sky cube mip
			if (FeatureLevel > ERHIFeatureLevel::ES2) // not-supported on ES2 at the moment
			{
				float AverageBrightness = 1.0f;
				MobileBasePass::GetSkyTextureParams(Scene, AverageBrightness, ReflectionCubemapTextures[0], ReflectionParams.W);
				ReflectionParams.X = 1.0f / AverageBrightness;
			}
		}
		else
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy ? PrimitiveSceneProxy->GetPrimitiveSceneInfo() : nullptr;
			// test for HQ reflection parameter existence
			if (PrimitiveSceneInfo && (ReflectionCubemap1.IsBound() || ReflectionCubemap2.IsBound() || ReflectionPositionsAndRadii.IsBound()))
			{
				for (int32 i = 0; i < MaxNumReflections; i++)
				{
					const FReflectionCaptureProxy* ReflectionProxy = PrimitiveSceneInfo->CachedReflectionCaptureProxies[i];
					if (ReflectionProxy)
					{
						CapturePositions[i] = ReflectionProxy->Position;
						CapturePositions[i].W = ReflectionProxy->InfluenceRadius;
						if (ReflectionProxy->EncodedHDRCubemap && ReflectionProxy->EncodedHDRCubemap->IsInitialized())
						{
							ReflectionCubemapTextures[i] = PrimitiveSceneInfo->CachedReflectionCaptureProxies[i]->EncodedHDRCubemap;
						}
						ReflectionParams[i] = 1.0f / ReflectionProxy->EncodedHDRAverageBrightness;
					}
				}
			}
			else if (ReflectionCubemap.IsBound())
			{
				if (PrimitiveSceneInfo 
					&& PrimitiveSceneInfo->CachedReflectionCaptureProxy
					&& PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap
					&& PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap->IsInitialized())
				{
					ReflectionParams.X = 1.0f / PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRAverageBrightness;
					ReflectionCubemapTextures[0] = PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap;
				}
			}
		}

		for (int32 i = 0; i < MaxNumReflections; i++)
		{
			ShaderBindings.AddTexture(*ReflectionTextureParameters[i], *ReflectionSamplerParameters[i], ReflectionCubemapTextures[i]->SamplerStateRHI, ReflectionCubemapTextures[i]->TextureRHI);
		}
		ShaderBindings.Add(ReflectionPositionsAndRadii, CapturePositions);
		ShaderBindings.Add(MobileReflectionParams, ReflectionParams);

		if (LightPositionAndInvRadiusParameter.IsBound())
		{
			// Set dynamic point lights
			FMobileBasePassMovablePointLightInfo LightInfo(PrimitiveSceneProxy);
			ShaderBindings.Add(NumDynamicPointLightsParameter, LightInfo.NumMovablePointLights);
			ShaderBindings.Add(LightPositionAndInvRadiusParameter, LightInfo.LightPositionAndInvRadius);
			ShaderBindings.Add(LightColorAndFalloffExponentParameter, LightInfo.LightColorAndFalloffExponent);
		}
	}
	else
	{
		ensure(!ReflectionCubemap.IsBound());
	}

	// Set directional light UB
	if (MobileDirectionLightBufferParam.IsBound())
	{
		int32 UniformBufferIndex = PrimitiveSceneProxy ? GetFirstLightingChannelFromMask(PrimitiveSceneProxy->GetLightingChannelMask()) + 1 : 0;
		ShaderBindings.Add(MobileDirectionLightBufferParam, Scene->UniformBuffers.MobileDirectionalLightUniformBuffers[UniformBufferIndex]);
	}

	if (CSMDebugHintParams.IsBound())
	{
		static const auto CVarsCSMDebugHint = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Mobile.Shadow.CSMDebugHint"));
		float CSMDebugValue = CVarsCSMDebugHint->GetValueOnRenderThread();
		ShaderBindings.Add(CSMDebugHintParams, CSMDebugValue);
	}
}

FMobileBasePassMeshProcessor::FMobileBasePassMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FDrawingPolicyRenderState& InDrawRenderState, FMeshPassDrawListContext& InDrawListContext, bool bInCanReceiveCSM, ETranslucencyPass::Type InTranslucencyPassType)
	: FMeshPassProcessor(Scene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InDrawRenderState)
	, TranslucencyPassType(InTranslucencyPassType)
	, bTranslucentBasePass(InTranslucencyPassType != ETranslucencyPass::TPT_MAX)
	, bCanReceiveCSM(bInCanReceiveCSM)
{}

void FMobileBasePassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (!MeshBatch.bUseForMaterial)
	{
		return;
	}
	
	// Determine the mesh's material and blend mode.
	const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
	const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);
	const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;
	const EBlendMode BlendMode = Material.GetBlendMode();
	const EMaterialShadingModel ShadingModel = Material.GetShadingModel();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
	
	if (bTranslucentBasePass)
	{
		bool bShouldDraw = bIsTranslucent && 
		(TranslucencyPassType == ETranslucencyPass::TPT_AllTranslucency
		|| (TranslucencyPassType == ETranslucencyPass::TPT_StandardTranslucency && !Material.IsMobileSeparateTranslucencyEnabled())
		|| (TranslucencyPassType == ETranslucencyPass::TPT_TranslucencyAfterDOF && Material.IsMobileSeparateTranslucencyEnabled()));
							
		if (bShouldDraw)
		{
			check(bCanReceiveCSM == false);
			const FLightSceneInfo* MobileDirectionalLight = MobileBasePass::GetDirectionalLightInfo(Scene, PrimitiveSceneProxy);
			ELightMapPolicyType LightmapPolicyType = MobileBasePass::SelectMeshLightmapPolicy(Scene, MeshBatch, PrimitiveSceneProxy, MobileDirectionalLight, ShadingModel, bCanReceiveCSM, FeatureLevel);
			Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, BlendMode, ShadingModel, LightmapPolicyType, MeshBatch.LCI);
		}
	}
	else
	{
		// opaque materials.
		if (!bIsTranslucent)
		{
			const FLightSceneInfo* MobileDirectionalLight = MobileBasePass::GetDirectionalLightInfo(Scene, PrimitiveSceneProxy);
			ELightMapPolicyType LightmapPolicyType = MobileBasePass::SelectMeshLightmapPolicy(Scene, MeshBatch, PrimitiveSceneProxy, MobileDirectionalLight, ShadingModel, bCanReceiveCSM, FeatureLevel);
			Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, BlendMode, ShadingModel, LightmapPolicyType, MeshBatch.LCI);
		}
	}
}

void FMobileBasePassMeshProcessor::Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		EBlendMode BlendMode,
		EMaterialShadingModel ShadingModel,
		const ELightMapPolicyType LightMapPolicyType,
		const FUniformLightMapPolicy::ElementDataType& RESTRICT LightMapElementData)
{
	TMeshProcessorShaders<
		TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>,
		FBaseHS,
		FBaseDS,
		TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>> BasePassShaders;
	
	bool bEnableSkyLight = ShadingModel != MSM_Unlit && Scene->ShouldRenderSkylightInBasePass(BlendMode);
	int32 NumMovablePointLights = MobileBasePass::CalcNumMovablePointLights(MaterialResource, PrimitiveSceneProxy);

	MobileBasePass::GetShaders(
		LightMapPolicyType, 
		NumMovablePointLights, 
		MaterialResource, 
		MeshBatch.VertexFactory->GetType(), 
		bEnableSkyLight, 
		BasePassShaders.VertexShader, 
		BasePassShaders.PixelShader);

	FDrawingPolicyRenderState DrawRenderState(PassDrawRenderState);
	if (bTranslucentBasePass)
	{
		MobileBasePass::SetTranslucentRenderState(DrawRenderState, MaterialResource);
	}
	else
	{
		bool bEnableReceiveDecalOutput = IsMobileHDR();
		MobileBasePass::SetOpaqueRenderState(DrawRenderState, PrimitiveSceneProxy, MaterialResource, bEnableReceiveDecalOutput);
	}

	FMeshDrawCommandSortKey SortKey; 
	if (bTranslucentBasePass)
	{
		SortKey = CalculateStaticTranslucentMeshSortKey(PrimitiveSceneProxy, MeshBatch.MeshIdInPrimitive);
	}
	else
	{
		// Default static sort key separates masked and non-masked geometry, generic mesh sorting will also sort by PSO
		// if platform wants front to back sorting, this key will be recomputed in InitViews
		SortKey = GetBasePassStaticSortKey(BlendMode);
	}
	
	ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, MaterialResource);
	ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, MaterialResource);

	TMobileBasePassShaderElementData<FUniformLightMapPolicy> ShaderElementData(LightMapElementData);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		BasePassShaders,
		MeshFillMode,
		MeshCullMode,
		1,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);
}

FMeshPassProcessor* CreateMobileBasePassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext& InDrawListContext)
{
	FExclusiveDepthStencil::Type DefaultBasePassDepthStencilAccess = GetDefaultBasePassDepthStencilAccess(EShadingPath::Mobile);
	
	FDrawingPolicyRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.MobileOpaqueBasePassUniformBuffer);
	PassDrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(DefaultBasePassDepthStencilAccess);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());

	return new(FMemStack::Get()) FMobileBasePassMeshProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, false);
}

FMeshPassProcessor* CreateMobileBasePassCSMProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext& InDrawListContext)
{
	FExclusiveDepthStencil::Type DefaultBasePassDepthStencilAccess = GetDefaultBasePassDepthStencilAccess(EShadingPath::Mobile);

	FDrawingPolicyRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.MobileOpaqueBasePassUniformBuffer);
	PassDrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(DefaultBasePassDepthStencilAccess);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());

	return new(FMemStack::Get()) FMobileBasePassMeshProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, true);
}

FMeshPassProcessor* CreateMobileTranslucencyStandardPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext& InDrawListContext)
{
	FDrawingPolicyRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.MobileTranslucentBasePassUniformBuffer);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilRead);

	return new(FMemStack::Get()) FMobileBasePassMeshProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, false, ETranslucencyPass::TPT_StandardTranslucency);
}

FMeshPassProcessor* CreateMobileTranslucencyAfterDOFProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext& InDrawListContext)
{
	FDrawingPolicyRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.MobileTranslucentBasePassUniformBuffer);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilRead);

	return new(FMemStack::Get()) FMobileBasePassMeshProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, false, ETranslucencyPass::TPT_TranslucencyAfterDOF);
}

FMeshPassProcessor* CreateMobileTranslucencyAllPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext& InDrawListContext)
{
	FDrawingPolicyRenderState PassDrawRenderState(Scene->UniformBuffers.ViewUniformBuffer, Scene->UniformBuffers.MobileTranslucentBasePassUniformBuffer);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilRead);

	return new(FMemStack::Get()) FMobileBasePassMeshProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext, false, ETranslucencyPass::TPT_AllTranslucency);
}

FRegisterPassProcessorCreateFunction RegisterMobileBasePass(&CreateMobileBasePassProcessor, EShadingPath::Mobile, EMeshPass::BasePass, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileBasePassCSM(&CreateMobileBasePassCSMProcessor, EShadingPath::Mobile, EMeshPass::MobileBasePassCSM, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileTranslucencyAllPass(&CreateMobileTranslucencyAllPassProcessor, EShadingPath::Mobile, EMeshPass::TranslucencyAll, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileTranslucencyStandardPass(&CreateMobileTranslucencyStandardPassProcessor, EShadingPath::Mobile, EMeshPass::TranslucencyStandard, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileTranslucencyAfterDOFPass(&CreateMobileTranslucencyAfterDOFProcessor, EShadingPath::Mobile, EMeshPass::TranslucencyAfterDOF, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);

