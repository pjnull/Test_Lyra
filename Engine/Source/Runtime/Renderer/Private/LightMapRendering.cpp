// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LightMapRendering.cpp: Light map rendering implementations.
=============================================================================*/

#include "LightMapRendering.h"
#include "ScenePrivate.h"
#include "PrecomputedVolumetricLightmap.h"

#include "Runtime/Engine/Classes/VT/VirtualTexture.h"
#include "Runtime/Engine/Classes/VT/VirtualTextureSpace.h"
#include "VT/VirtualTextureSpace.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FIndirectLightingCacheUniformParameters, "IndirectLightingCache");

const TCHAR* GLightmapDefineName[2] =
{
	TEXT("LQ_TEXTURE_LIGHTMAP"),
	TEXT("HQ_TEXTURE_LIGHTMAP")
};

int32 GNumLightmapCoefficients[2] = 
{
	NUM_LQ_LIGHTMAP_COEF,
	NUM_HQ_LIGHTMAP_COEF
};

void FCachedPointIndirectLightingPolicy::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("CACHED_POINT_INDIRECT_LIGHTING"),TEXT("1"));	
}

void FSelfShadowedCachedPointIndirectLightingPolicy::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("CACHED_POINT_INDIRECT_LIGHTING"),TEXT("1"));	
	FSelfShadowedTranslucencyPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
}

void SetupLCIUniformBuffers(const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FLightCacheInterface* LCI, FUniformBufferRHIParamRef& PrecomputedLightingBuffer, FUniformBufferRHIParamRef& LightmapResourceClusterBuffer, FUniformBufferRHIParamRef& IndirectLightingCacheBuffer)
{
	if (LCI)
	{
		PrecomputedLightingBuffer = LCI->GetPrecomputedLightingBuffer();
		LightmapResourceClusterBuffer = LCI->GetResourceCluster() ? LCI->GetResourceCluster()->UniformBuffer : nullptr;
	}

	if (!PrecomputedLightingBuffer)
	{
		PrecomputedLightingBuffer = GEmptyPrecomputedLightingUniformBuffer.GetUniformBufferRHI();
	}

	if (!LightmapResourceClusterBuffer)
	{
		LightmapResourceClusterBuffer = GDefaultLightmapResourceClusterUniformBuffer.GetUniformBufferRHI();
	}

	if (PrimitiveSceneProxy && PrimitiveSceneProxy->GetPrimitiveSceneInfo())
	{
		IndirectLightingCacheBuffer = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheUniformBuffer;
	}

	if (!IndirectLightingCacheBuffer)
	{
		IndirectLightingCacheBuffer = GEmptyIndirectLightingCacheUniformBuffer.GetUniformBufferRHI();
	}
}

void FSelfShadowedCachedPointIndirectLightingPolicy::GetPixelShaderBindings(
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const ElementDataType& ShaderElementData,
	const PixelParametersType* PixelShaderParameters,
	FMeshDrawSingleShaderBindings& ShaderBindings)
{
	FUniformBufferRHIParamRef PrecomputedLightingBuffer = nullptr;
	FUniformBufferRHIParamRef LightmapResourceClusterBuffer = nullptr;
	FUniformBufferRHIParamRef IndirectLightingCacheBuffer = nullptr;

	SetupLCIUniformBuffers(PrimitiveSceneProxy, ShaderElementData.LCI, PrecomputedLightingBuffer, LightmapResourceClusterBuffer, IndirectLightingCacheBuffer);

	ShaderBindings.Add(PixelShaderParameters->PrecomputedLightingBufferParameter, PrecomputedLightingBuffer);
	ShaderBindings.Add(PixelShaderParameters->IndirectLightingCacheParameter, IndirectLightingCacheBuffer);
	ShaderBindings.Add(PixelShaderParameters->LightmapResourceCluster, LightmapResourceClusterBuffer);

	FSelfShadowedTranslucencyPolicy::GetPixelShaderBindings(PrimitiveSceneProxy, ShaderElementData.SelfShadowTranslucencyUniformBuffer, PixelShaderParameters, ShaderBindings);
}

void FSelfShadowedVolumetricLightmapPolicy::GetPixelShaderBindings(
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const ElementDataType& ShaderElementData,
	const PixelParametersType* PixelShaderParameters,
	FMeshDrawSingleShaderBindings& ShaderBindings)
{
	FUniformBufferRHIParamRef PrecomputedLightingBuffer = nullptr;
	FUniformBufferRHIParamRef LightmapResourceClusterBuffer = nullptr;
	FUniformBufferRHIParamRef IndirectLightingCacheBuffer = nullptr;

	SetupLCIUniformBuffers(PrimitiveSceneProxy, ShaderElementData.LCI, PrecomputedLightingBuffer, LightmapResourceClusterBuffer, IndirectLightingCacheBuffer);

	ShaderBindings.Add(PixelShaderParameters->PrecomputedLightingBufferParameter, PrecomputedLightingBuffer);
	ShaderBindings.Add(PixelShaderParameters->IndirectLightingCacheParameter, IndirectLightingCacheBuffer);
	ShaderBindings.Add(PixelShaderParameters->LightmapResourceCluster, LightmapResourceClusterBuffer);

	FSelfShadowedTranslucencyPolicy::GetPixelShaderBindings(PrimitiveSceneProxy, ShaderElementData.SelfShadowTranslucencyUniformBuffer, PixelShaderParameters, ShaderBindings);
}

void FUniformLightMapPolicy::SetMesh(
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
	) const
{
	FUniformBufferRHIParamRef PrecomputedLightingBuffer = nullptr;
	FUniformBufferRHIParamRef LightmapResourceClusterBuffer = nullptr;
	FUniformBufferRHIParamRef IndirectLightingCacheBuffer = nullptr;

	SetupLCIUniformBuffers(PrimitiveSceneProxy, LCI, PrecomputedLightingBuffer, LightmapResourceClusterBuffer, IndirectLightingCacheBuffer);

	if (VertexShaderParameters)
	{
		SetUniformBufferParameter(RHICmdList, VertexShader->GetVertexShader(), VertexShaderParameters->PrecomputedLightingBufferParameter, PrecomputedLightingBuffer);
		SetUniformBufferParameter(RHICmdList, VertexShader->GetVertexShader(), VertexShaderParameters->IndirectLightingCacheParameter, IndirectLightingCacheBuffer);
		SetUniformBufferParameter(RHICmdList, VertexShader->GetVertexShader(), VertexShaderParameters->LightmapResourceCluster, LightmapResourceClusterBuffer);
	}
	if (PixelShaderParameters)
	{
		SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), PixelShaderParameters->PrecomputedLightingBufferParameter, PrecomputedLightingBuffer);
		SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), PixelShaderParameters->IndirectLightingCacheParameter, IndirectLightingCacheBuffer);
		SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), PixelShaderParameters->LightmapResourceCluster, LightmapResourceClusterBuffer);
	}
}

void FUniformLightMapPolicy::GetVertexShaderBindings(
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const ElementDataType& ShaderElementData,
	const VertexParametersType* VertexShaderParameters,
	FMeshDrawSingleShaderBindings& ShaderBindings) 
{
	FUniformBufferRHIParamRef PrecomputedLightingBuffer = nullptr;
	FUniformBufferRHIParamRef LightmapResourceClusterBuffer = nullptr;
	FUniformBufferRHIParamRef IndirectLightingCacheBuffer = nullptr;

	SetupLCIUniformBuffers(PrimitiveSceneProxy, ShaderElementData, PrecomputedLightingBuffer, LightmapResourceClusterBuffer, IndirectLightingCacheBuffer);

	ShaderBindings.Add(VertexShaderParameters->PrecomputedLightingBufferParameter, PrecomputedLightingBuffer);
	ShaderBindings.Add(VertexShaderParameters->IndirectLightingCacheParameter, IndirectLightingCacheBuffer);
	ShaderBindings.Add(VertexShaderParameters->LightmapResourceCluster, LightmapResourceClusterBuffer);
}

void FUniformLightMapPolicy::GetPixelShaderBindings(
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const ElementDataType& ShaderElementData,
	const PixelParametersType* PixelShaderParameters,
	FMeshDrawSingleShaderBindings& ShaderBindings)
{
	FUniformBufferRHIParamRef PrecomputedLightingBuffer = nullptr;
	FUniformBufferRHIParamRef LightmapResourceClusterBuffer = nullptr;
	FUniformBufferRHIParamRef IndirectLightingCacheBuffer = nullptr;

	SetupLCIUniformBuffers(PrimitiveSceneProxy, ShaderElementData, PrecomputedLightingBuffer, LightmapResourceClusterBuffer, IndirectLightingCacheBuffer);

	ShaderBindings.Add(PixelShaderParameters->PrecomputedLightingBufferParameter, PrecomputedLightingBuffer);
	ShaderBindings.Add(PixelShaderParameters->IndirectLightingCacheParameter, IndirectLightingCacheBuffer);
	ShaderBindings.Add(PixelShaderParameters->LightmapResourceCluster, LightmapResourceClusterBuffer);
}

#if RHI_RAYTRACING
void FUniformLightMapPolicy::GetRayHitGroupShaderBindings(
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FLightCacheInterface* LCI,
	const RayHitGroupParametersType* RayHitGroupShaderParameters,
	FMeshDrawSingleShaderBindings& RayHitGroupBindings
) const
{
	FUniformBufferRHIParamRef PrecomputedLightingBuffer = nullptr;
	FUniformBufferRHIParamRef LightmapResourceClusterBuffer = nullptr;
	FUniformBufferRHIParamRef IndirectLightingCacheBuffer = nullptr;

	SetupLCIUniformBuffers(PrimitiveSceneProxy, LCI, PrecomputedLightingBuffer, LightmapResourceClusterBuffer, IndirectLightingCacheBuffer);

	RayHitGroupBindings.Add(RayHitGroupShaderParameters->PrecomputedLightingBufferParameter, PrecomputedLightingBuffer);
	RayHitGroupBindings.Add(RayHitGroupShaderParameters->IndirectLightingCacheParameter, IndirectLightingCacheBuffer);
	RayHitGroupBindings.Add(RayHitGroupShaderParameters->LightmapResourceCluster, LightmapResourceClusterBuffer);
}
#endif

void InterpolateVolumetricLightmap(
	FVector LookupPosition,
	const FVolumetricLightmapSceneData& VolumetricLightmapSceneData,
	FVolumetricLightmapInterpolation& OutInterpolation)
{
	SCOPE_CYCLE_COUNTER(STAT_InterpolateVolumetricLightmapOnCPU);

	checkSlow(VolumetricLightmapSceneData.HasData());
	const FPrecomputedVolumetricLightmapData& VolumetricLightmapData = *VolumetricLightmapSceneData.GetLevelVolumetricLightmap()->Data;
	
	const FVector IndirectionDataSourceCoordinate = ComputeIndirectionCoordinate(LookupPosition, VolumetricLightmapData.GetBounds(), VolumetricLightmapData.IndirectionTextureDimensions);

	check(VolumetricLightmapData.IndirectionTexture.Data.Num() > 0);
	checkSlow(GPixelFormats[VolumetricLightmapData.IndirectionTexture.Format].BlockBytes == sizeof(uint8) * 4);
	const int32 NumIndirectionTexels = VolumetricLightmapData.IndirectionTextureDimensions.X * VolumetricLightmapData.IndirectionTextureDimensions.Y * VolumetricLightmapData.IndirectionTextureDimensions.Z;
	check(VolumetricLightmapData.IndirectionTexture.Data.Num() * VolumetricLightmapData.IndirectionTexture.Data.GetTypeSize() == NumIndirectionTexels * sizeof(uint8) * 4);
	
	FIntVector IndirectionBrickOffset;
	int32 IndirectionBrickSize;
	SampleIndirectionTexture(IndirectionDataSourceCoordinate, VolumetricLightmapData.IndirectionTextureDimensions, VolumetricLightmapData.IndirectionTexture.Data.GetData(), IndirectionBrickOffset, IndirectionBrickSize);

	const FVector BrickTextureCoordinate = ComputeBrickTextureCoordinate(IndirectionDataSourceCoordinate, IndirectionBrickOffset, IndirectionBrickSize, VolumetricLightmapData.BrickSize);

	const FVector AmbientVector = (FVector)FilteredVolumeLookup<FFloat3Packed>(BrickTextureCoordinate, VolumetricLightmapData.BrickDataDimensions, (const FFloat3Packed*)VolumetricLightmapData.BrickData.AmbientVector.Data.GetData());
	
	auto ReadSHCoefficient = [&BrickTextureCoordinate, &VolumetricLightmapData, &AmbientVector](uint32 CoefficientIndex)
	{
		check(CoefficientIndex < ARRAY_COUNT(VolumetricLightmapData.BrickData.SHCoefficients));

		// Undo normalization done in FIrradianceBrickData::SetFromVolumeLightingSample
		const FLinearColor SHDenormalizationScales0(
			0.488603f / 0.282095f,
			0.488603f / 0.282095f,
			0.488603f / 0.282095f,
			1.092548f / 0.282095f);

		const FLinearColor SHDenormalizationScales1(
			1.092548f / 0.282095f,
			4.0f * 0.315392f / 0.282095f,
			1.092548f / 0.282095f,
			2.0f * 0.546274f / 0.282095f);

		FLinearColor SHCoefficientEncoded = FilteredVolumeLookup<FColor>(BrickTextureCoordinate, VolumetricLightmapData.BrickDataDimensions, (const FColor*)VolumetricLightmapData.BrickData.SHCoefficients[CoefficientIndex].Data.GetData());
		const FLinearColor& DenormalizationScales = ((CoefficientIndex & 1) == 0) ? SHDenormalizationScales0 : SHDenormalizationScales1;
		return FVector4((SHCoefficientEncoded * 2.0f - FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)) * AmbientVector[CoefficientIndex / 2] * DenormalizationScales);
	};

	auto GetSHVector3 = [](float Ambient, const FVector4& Coeffs0, const FVector4& Coeffs1 )
	{
		FSHVector3 Result;
		Result.V[0] = Ambient;
		FMemory::Memcpy(&Result.V[1], &Coeffs0, sizeof(Coeffs0));
		FMemory::Memcpy(&Result.V[5], &Coeffs1, sizeof(Coeffs1));
		return Result;
	};

	FSHVectorRGB3 LQSH;
	LQSH.R = GetSHVector3(AmbientVector.X, ReadSHCoefficient(0), ReadSHCoefficient(1));
	LQSH.G = GetSHVector3(AmbientVector.Y, ReadSHCoefficient(2), ReadSHCoefficient(3));
	LQSH.B = GetSHVector3(AmbientVector.Z, ReadSHCoefficient(4), ReadSHCoefficient(5));

	if(VolumetricLightmapData.BrickData.LQLightColor.Data.Num())
	{
		// Add stationary direct lighting to match ILC's LQ behavior.
		FLinearColor LQLightColour = FilteredVolumeLookup<FFloat3Packed>(BrickTextureCoordinate, VolumetricLightmapData.BrickDataDimensions, (const FFloat3Packed*)VolumetricLightmapData.BrickData.LQLightColor.Data.GetData());
		FVector LQLightDirection = (FVector)FilteredVolumeLookup<FColor>(BrickTextureCoordinate, VolumetricLightmapData.BrickDataDimensions, (const FColor*)VolumetricLightmapData.BrickData.LQLightDirection.Data.GetData());
		LQLightDirection = LQLightDirection*2.0f - FVector(1.0f, 1.0f, 1.0f);
		LQLightDirection.Normalize();

		LQSH.AddIncomingRadiance(LQLightColour, 1.0f, LQLightDirection);
	}

	// Pack the 3rd order SH as the shader expects
	OutInterpolation.IndirectLightingSHCoefficients0[0] = FVector4(LQSH.R.V[0], LQSH.R.V[1], LQSH.R.V[2], LQSH.R.V[3]) * INV_PI;
	OutInterpolation.IndirectLightingSHCoefficients0[1] = FVector4(LQSH.G.V[0], LQSH.G.V[1], LQSH.G.V[2], LQSH.G.V[3]) * INV_PI;
	OutInterpolation.IndirectLightingSHCoefficients0[2] = FVector4(LQSH.B.V[0], LQSH.B.V[1], LQSH.B.V[2], LQSH.B.V[3]) * INV_PI;
	OutInterpolation.IndirectLightingSHCoefficients1[0] = FVector4(LQSH.R.V[4], LQSH.R.V[5], LQSH.R.V[6], LQSH.R.V[7]) * INV_PI;
	OutInterpolation.IndirectLightingSHCoefficients1[1] = FVector4(LQSH.G.V[4], LQSH.G.V[5], LQSH.G.V[6], LQSH.G.V[7]) * INV_PI;
	OutInterpolation.IndirectLightingSHCoefficients1[2] = FVector4(LQSH.B.V[4], LQSH.B.V[5], LQSH.B.V[6], LQSH.B.V[7]) * INV_PI;
	OutInterpolation.IndirectLightingSHCoefficients2 = FVector4(LQSH.R.V[8], LQSH.G.V[8], LQSH.B.V[8], 0.0f) * INV_PI;

	OutInterpolation.IndirectLightingSHSingleCoefficient = FVector4(AmbientVector.X, AmbientVector.Y, AmbientVector.Z)
		* FSHVector2::ConstantBasisIntegral * .5f;

	if (VolumetricLightmapData.BrickData.SkyBentNormal.Data.Num() > 0)
	{
		const FLinearColor SkyBentNormalUnpacked = FilteredVolumeLookup<FColor>(BrickTextureCoordinate, VolumetricLightmapData.BrickDataDimensions, (const FColor*)VolumetricLightmapData.BrickData.SkyBentNormal.Data.GetData());
		const FVector SkyBentNormal(SkyBentNormalUnpacked.R, SkyBentNormalUnpacked.G, SkyBentNormalUnpacked.B);
		const float BentNormalLength = SkyBentNormal.Size();
		OutInterpolation.PointSkyBentNormal = FVector4(SkyBentNormal / FMath::Max(BentNormalLength, .0001f), BentNormalLength);
	}
	else
	{
		OutInterpolation.PointSkyBentNormal = FVector4(0, 0, 1, 1);
	}

	const FLinearColor DirectionalLightShadowingUnpacked = FilteredVolumeLookup<uint8>(BrickTextureCoordinate, VolumetricLightmapData.BrickDataDimensions, (const uint8*)VolumetricLightmapData.BrickData.DirectionalLightShadowing.Data.GetData());
	OutInterpolation.DirectionalLightShadowing = DirectionalLightShadowingUnpacked.R;
}

void FEmptyPrecomputedLightingUniformBuffer::InitDynamicRHI()
{
	FPrecomputedLightingUniformParameters Parameters;
	GetPrecomputedLightingParameters(GMaxRHIFeatureLevel, Parameters, NULL);
	SetContentsNoUpdate(Parameters);

	Super::InitDynamicRHI();
}

/** Global uniform buffer containing the default precomputed lighting data. */
TGlobalResource< FEmptyPrecomputedLightingUniformBuffer > GEmptyPrecomputedLightingUniformBuffer;

void GetIndirectLightingCacheParameters(
	ERHIFeatureLevel::Type FeatureLevel,
	FIndirectLightingCacheUniformParameters& Parameters, 
	const FIndirectLightingCache* LightingCache, 
	const FIndirectLightingCacheAllocation* LightingAllocation, 
	FVector VolumetricLightmapLookupPosition,
	uint32 SceneFrameNumber,
	FVolumetricLightmapSceneData* VolumetricLightmapSceneData
	)
{
	// FCachedVolumeIndirectLightingPolicy, FCachedPointIndirectLightingPolicy
	{
		if (VolumetricLightmapSceneData)
		{
			FVolumetricLightmapInterpolation* Interpolation = VolumetricLightmapSceneData->CPUInterpolationCache.Find(VolumetricLightmapLookupPosition);

			if (!Interpolation)
			{
				Interpolation = &VolumetricLightmapSceneData->CPUInterpolationCache.Add(VolumetricLightmapLookupPosition);
				InterpolateVolumetricLightmap(VolumetricLightmapLookupPosition, *VolumetricLightmapSceneData, *Interpolation);
			}

			Interpolation->LastUsedSceneFrameNumber = SceneFrameNumber;
			
			Parameters.PointSkyBentNormal = Interpolation->PointSkyBentNormal;
			Parameters.DirectionalLightShadowing = Interpolation->DirectionalLightShadowing;

			for (int32 i = 0; i < 3; i++)
			{
				Parameters.IndirectLightingSHCoefficients0[i] = Interpolation->IndirectLightingSHCoefficients0[i];
				Parameters.IndirectLightingSHCoefficients1[i] = Interpolation->IndirectLightingSHCoefficients1[i];
			}

			Parameters.IndirectLightingSHCoefficients2 = Interpolation->IndirectLightingSHCoefficients2;
			Parameters.IndirectLightingSHSingleCoefficient = Interpolation->IndirectLightingSHSingleCoefficient;

			// Unused
			Parameters.IndirectLightingCachePrimitiveAdd = FVector(0, 0, 0);
			Parameters.IndirectLightingCachePrimitiveScale = FVector(1, 1, 1);
			Parameters.IndirectLightingCacheMinUV = FVector(0, 0, 0);
			Parameters.IndirectLightingCacheMaxUV = FVector(1, 1, 1);
		}
		else if (LightingAllocation)
		{
			Parameters.IndirectLightingCachePrimitiveAdd = LightingAllocation->Add;
			Parameters.IndirectLightingCachePrimitiveScale = LightingAllocation->Scale;
			Parameters.IndirectLightingCacheMinUV = LightingAllocation->MinUV;
			Parameters.IndirectLightingCacheMaxUV = LightingAllocation->MaxUV;
			Parameters.PointSkyBentNormal = LightingAllocation->CurrentSkyBentNormal;
			Parameters.DirectionalLightShadowing = LightingAllocation->CurrentDirectionalShadowing;

			for (uint32 i = 0; i < 3; ++i) // RGB
			{
				Parameters.IndirectLightingSHCoefficients0[i] = LightingAllocation->SingleSamplePacked0[i];
				Parameters.IndirectLightingSHCoefficients1[i] = LightingAllocation->SingleSamplePacked1[i];
			}
			Parameters.IndirectLightingSHCoefficients2 = LightingAllocation->SingleSamplePacked2;
			Parameters.IndirectLightingSHSingleCoefficient = FVector4(LightingAllocation->SingleSamplePacked0[0].X, LightingAllocation->SingleSamplePacked0[1].X, LightingAllocation->SingleSamplePacked0[2].X)
					* FSHVector2::ConstantBasisIntegral * .5f; //@todo - why is .5f needed to match directional?
		}
		else
		{
			Parameters.IndirectLightingCachePrimitiveAdd = FVector(0, 0, 0);
			Parameters.IndirectLightingCachePrimitiveScale = FVector(1, 1, 1);
			Parameters.IndirectLightingCacheMinUV = FVector(0, 0, 0);
			Parameters.IndirectLightingCacheMaxUV = FVector(1, 1, 1);
			Parameters.PointSkyBentNormal = FVector4(0, 0, 1, 1);
			Parameters.DirectionalLightShadowing = 1;

			for (uint32 i = 0; i < 3; ++i) // RGB
			{
				Parameters.IndirectLightingSHCoefficients0[i] = FVector4(0, 0, 0, 0);
				Parameters.IndirectLightingSHCoefficients1[i] = FVector4(0, 0, 0, 0);
			}
			Parameters.IndirectLightingSHCoefficients2 = FVector4(0, 0, 0, 0);
			Parameters.IndirectLightingSHSingleCoefficient = FVector4(0, 0, 0, 0);
		}

		// If we are using FCachedVolumeIndirectLightingPolicy then InitViews should have updated the lighting cache which would have initialized it
		// However the conditions for updating the lighting cache are complex and fail very occasionally in non-reproducible ways
		// Silently skipping setting the cache texture under failure for now
		if (FeatureLevel >= ERHIFeatureLevel::SM4 && LightingCache && LightingCache->IsInitialized() && GSupportsVolumeTextureRendering)
		{
			Parameters.IndirectLightingCacheTexture0 = const_cast<FIndirectLightingCache*>(LightingCache)->GetTexture0().ShaderResourceTexture;
			Parameters.IndirectLightingCacheTexture1 = const_cast<FIndirectLightingCache*>(LightingCache)->GetTexture1().ShaderResourceTexture;
			Parameters.IndirectLightingCacheTexture2 = const_cast<FIndirectLightingCache*>(LightingCache)->GetTexture2().ShaderResourceTexture;

			Parameters.IndirectLightingCacheTextureSampler0 = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			Parameters.IndirectLightingCacheTextureSampler1 = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			Parameters.IndirectLightingCacheTextureSampler2 = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		}
		else
		if (FeatureLevel >= ERHIFeatureLevel::ES3_1)
		{
			Parameters.IndirectLightingCacheTexture0 = GBlackVolumeTexture->TextureRHI;
			Parameters.IndirectLightingCacheTexture1 = GBlackVolumeTexture->TextureRHI;
			Parameters.IndirectLightingCacheTexture2 = GBlackVolumeTexture->TextureRHI;

			Parameters.IndirectLightingCacheTextureSampler0 = GBlackVolumeTexture->SamplerStateRHI;
			Parameters.IndirectLightingCacheTextureSampler1 = GBlackVolumeTexture->SamplerStateRHI;
			Parameters.IndirectLightingCacheTextureSampler2 = GBlackVolumeTexture->SamplerStateRHI;
		}
		else
		{
			Parameters.IndirectLightingCacheTexture0 = GBlackTexture->TextureRHI;
			Parameters.IndirectLightingCacheTexture1 = GBlackTexture->TextureRHI;
			Parameters.IndirectLightingCacheTexture2 = GBlackTexture->TextureRHI;

			Parameters.IndirectLightingCacheTextureSampler0 = GBlackTexture->SamplerStateRHI;
			Parameters.IndirectLightingCacheTextureSampler1 = GBlackTexture->SamplerStateRHI;
			Parameters.IndirectLightingCacheTextureSampler2 = GBlackTexture->SamplerStateRHI;
		}
	}
}

void FEmptyIndirectLightingCacheUniformBuffer::InitDynamicRHI()
{
	FIndirectLightingCacheUniformParameters Parameters;
	GetIndirectLightingCacheParameters(GMaxRHIFeatureLevel, Parameters, nullptr, nullptr, FVector(0, 0, 0), 0, nullptr);
	SetContentsNoUpdate(Parameters);

	Super::InitDynamicRHI();
}

/** */
TGlobalResource< FEmptyIndirectLightingCacheUniformBuffer > GEmptyIndirectLightingCacheUniformBuffer;