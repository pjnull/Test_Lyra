// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Landscape.cpp: Terrain rendering
=============================================================================*/

#include "Landscape.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "UObject/RenderingObjectVersion.h"
#include "UObject/UObjectIterator.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/LinkerLoad.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeInfo.h"
#include "LightMap.h"
#include "Engine/MapBuildDataRegistry.h"
#include "ShadowMap.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeInfoMap.h"
#include "EditorSupportDelegates.h"
#include "LandscapeMeshProxyComponent.h"
#include "LandscapeRender.h"
#include "LandscapeRenderMobile.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "DerivedDataCacheInterface.h"
#include "Interfaces/ITargetPlatform.h"
#include "LandscapeMeshCollisionComponent.h"
#include "Materials/Material.h"
#include "LandscapeMaterialInstanceConstant.h"
#include "Engine/CollisionProfile.h"
#include "LandscapeMeshProxyActor.h"
#include "Materials/MaterialExpressionLandscapeLayerWeight.h"
#include "Materials/MaterialExpressionLandscapeLayerSwitch.h"
#include "Materials/MaterialExpressionLandscapeLayerSample.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProfilingDebugging/CookStats.h"
#include "LandscapeSplinesComponent.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "ComponentRecreateRenderStateContext.h"

#if WITH_EDITOR
#include "MaterialUtilities.h"
#endif
#include "LandscapeVersion.h"

/** Landscape stats */

DEFINE_STAT(STAT_LandscapeDynamicDrawTime);
DEFINE_STAT(STAT_LandscapeStaticDrawLODTime);
DEFINE_STAT(STAT_LandscapeVFDrawTimeVS);
DEFINE_STAT(STAT_LandscapeInitViewCustomData);
DEFINE_STAT(STAT_LandscapePostInitViewCustomData);
DEFINE_STAT(STAT_LandscapeComputeCustomMeshBatchLOD);
DEFINE_STAT(STAT_LandscapeComputeCustomShadowMeshBatchLOD);
DEFINE_STAT(STAT_LandscapeVFDrawTimePS);
DEFINE_STAT(STAT_LandscapeComponentRenderPasses);
DEFINE_STAT(STAT_LandscapeTessellatedShadowCascade);
DEFINE_STAT(STAT_LandscapeTessellatedComponents);
DEFINE_STAT(STAT_LandscapeComponentUsingSubSectionDrawCalls);
DEFINE_STAT(STAT_LandscapeDrawCalls);
DEFINE_STAT(STAT_LandscapeTriangles);

DEFINE_STAT(STAT_LandscapeVertexMem);
DEFINE_STAT(STAT_LandscapeOccluderMem);
DEFINE_STAT(STAT_LandscapeComponentMem);

#if ENABLE_COOK_STATS
namespace LandscapeCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("Landscape.Usage"), TEXT(""));
	});
}
#endif

// Set this to 0 to disable landscape cooking and thus disable it on device.
#define ENABLE_LANDSCAPE_COOKING 1

// Increment this to regenerate mobile landscape data
#define LANDSCAPE_MOBILE_COOK_VERSION 2

#define LOCTEXT_NAMESPACE "Landscape"

static void PrintNumLandscapeShadows()
{
	int32 NumComponents = 0;
	int32 NumShadowCasters = 0;
	for (TObjectIterator<ULandscapeComponent> It; It; ++It)
	{
		ULandscapeComponent* LC = *It;
		NumComponents++;
		if (LC->CastShadow && LC->bCastDynamicShadow)
		{
			NumShadowCasters++;
		}
	}
	UE_LOG(LogConsoleResponse, Display, TEXT("%d/%d landscape components cast shadows"), NumShadowCasters, NumComponents);
}

FAutoConsoleCommand CmdPrintNumLandscapeShadows(
	TEXT("ls.PrintNumLandscapeShadows"),
	TEXT("Prints the number of landscape components that cast shadows."),
	FConsoleCommandDelegate::CreateStatic(PrintNumLandscapeShadows)
	);

ULandscapeComponent::ULandscapeComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, GrassData(MakeShareable(new FLandscapeComponentGrassData()))
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	CastShadow = true;
	// by default we want to see the Landscape shadows even in the far shadow cascades
	bCastFarShadow = true;
	bAffectDistanceFieldLighting = true;
	bUseAsOccluder = true;
	bAllowCullDistanceVolume = false;
	CollisionMipLevel = 0;
	StaticLightingResolution = 0.f; // Default value 0 means no overriding

	MaterialInstances.AddDefaulted(); // make sure we always have a MaterialInstances[0]	

	HeightmapScaleBias = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	WeightmapScaleBias = FVector4(0.0f, 0.0f, 0.0f, 1.0f);

	bBoundsChangeTriggersStreamingDataRebuild = true;
	ForcedLOD = -1;
	LODBias = 0;
#if WITH_EDITORONLY_DATA
	LightingLODBias = -1; // -1 Means automatic LOD calculation based on ForcedLOD + LODBias
#endif

	Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	EditToolRenderData = FLandscapeEditToolRenderData();
#endif

	LpvBiasMultiplier = 0.0f; // Bias is 0 for landscape, since it's single sided

	// We don't want to load this on the server, this component is for graphical purposes only
	AlwaysLoadOnServer = false;
}

UMaterialInstance* ULandscapeComponent::GetMaterialInstance(int32 InIndex, bool InDynamic) const
{
	ALandscapeProxy* Actor = GetLandscapeProxy();

	if (Actor != nullptr && Actor->bUseDynamicMaterialInstance && InDynamic)
	{
		check(MaterialInstancesDynamic.IsValidIndex(InIndex));
		return MaterialInstancesDynamic[InIndex];
	}

	check(MaterialInstances.IsValidIndex(InIndex));
	return MaterialInstances[InIndex];
}

UMaterialInstanceDynamic* ULandscapeComponent::GetMaterialInstanceDynamic(int32 InIndex) const
{
	ALandscapeProxy* Actor = GetLandscapeProxy();

	if (Actor != nullptr && Actor->bUseDynamicMaterialInstance)
	{
		if (MaterialInstancesDynamic.IsValidIndex(InIndex))
		{
			return MaterialInstancesDynamic[InIndex];
		}
	}

	return nullptr;
}

void ULandscapeComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ULandscapeComponent* This = CastChecked<ULandscapeComponent>(InThis);
	Super::AddReferencedObjects(This, Collector);
}

#if WITH_EDITOR
void ULandscapeComponent::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);

	if (TargetPlatform->SupportsFeature(ETargetPlatformFeatures::MobileRendering) && !HasAnyFlags(RF_ClassDefaultObject))
	{
		CheckGenerateLandscapePlatformData(true, TargetPlatform);
	}
}

void ALandscapeProxy::CheckGenerateLandscapePlatformData(bool bIsCooking, const ITargetPlatform* TargetPlatform)
{
	for (ULandscapeComponent* Component : LandscapeComponents)
	{
		Component->CheckGenerateLandscapePlatformData(bIsCooking, TargetPlatform);
	}
}

void ULandscapeComponent::CheckGenerateLandscapePlatformData(bool bIsCooking, const ITargetPlatform* TargetPlatform)
{
#if ENABLE_LANDSCAPE_COOKING
	
	// Regenerate platform data only when it's missing or there is a valid hash-mismatch.

	FBufferArchive ComponentStateAr;
	SerializeStateHashes(ComponentStateAr);

	// Serialize the version number as part of the hash so we can invalidate DDC data if needed
	int32 Version = LANDSCAPE_MOBILE_COOK_VERSION;
	ComponentStateAr << Version;
	
	uint32 Hash[5];
	FSHA1::HashBuffer(ComponentStateAr.GetData(), ComponentStateAr.Num(), (uint8*)Hash);
	FGuid NewSourceHash = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);

	bool bHashMismatch = MobileDataSourceHash != NewSourceHash;
	bool bMissingVertexData = !PlatformData.HasValidPlatformData();
	bool bMissingPixelData = MobileMaterialInterface == nullptr || MobileWeightmapTextures.Num() == 0;
	
	bool bRegenerateVertexData = bMissingVertexData || bMissingPixelData || bHashMismatch;
	
	if (bRegenerateVertexData)
	{
		if (bIsCooking)
		{
			// The DDC is only useful when cooking (see else).

			COOK_STAT(auto Timer = LandscapeCookStats::UsageStats.TimeSyncWork());
			if (PlatformData.LoadFromDDC(NewSourceHash))
			{
				COOK_STAT(Timer.AddHit(PlatformData.GetPlatformDataSize()));
			}
			else
			{
				GeneratePlatformVertexData(TargetPlatform);
				PlatformData.SaveToDDC(NewSourceHash);
				COOK_STAT(Timer.AddMiss(PlatformData.GetPlatformDataSize()));
			}
		}
		else
		{
			// When not cooking (e.g. mobile preview) DDC data isn't sufficient to 
			// display correctly, so the platform vertex data must be regenerated.

			GeneratePlatformVertexData(TargetPlatform);
		}
	}

	bool bRegeneratePixelData = bMissingPixelData || bHashMismatch;

	if (bRegeneratePixelData)
	{
		GeneratePlatformPixelData();
	}

	MobileDataSourceHash = NewSourceHash;

#endif
}
#endif

void ULandscapeComponent::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

#if WITH_EDITOR
	if (Ar.IsCooking() && !HasAnyFlags(RF_ClassDefaultObject) && Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::MobileRendering))
	{
		// for -oldcook:
		// the old cooker calls BeginCacheForCookedPlatformData after the package export set is tagged, so the mobile material doesn't get saved, so we have to do CheckGenerateLandscapePlatformData in serialize
		// the new cooker clears the texture source data before calling serialize, causing GeneratePlatformVertexData to crash, so we have to do CheckGenerateLandscapePlatformData in BeginCacheForCookedPlatformData
		CheckGenerateLandscapePlatformData(true, Ar.CookingTarget());
	}

	if (Ar.IsCooking() && !HasAnyFlags(RF_ClassDefaultObject) && !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::DeferredRendering))
	{
		// These properties are only used for SM4+ so we back them up and clear them before serializing them.
		UTexture2D* BackupHeightmapTexture = nullptr;
		UTexture2D* BackupXYOffsetmapTexture = nullptr;
		TArray<UMaterialInstanceConstant*> BackupMaterialInstances;
		TArray<UTexture2D*> BackupWeightmapTextures;

		Exchange(HeightmapTexture, BackupHeightmapTexture);
		Exchange(BackupXYOffsetmapTexture, XYOffsetmapTexture);
		Exchange(BackupMaterialInstances, MaterialInstances);
		Exchange(BackupWeightmapTextures, WeightmapTextures);

		Super::Serialize(Ar);

		Exchange(HeightmapTexture, BackupHeightmapTexture);
		Exchange(BackupXYOffsetmapTexture, XYOffsetmapTexture);
		Exchange(BackupMaterialInstances, MaterialInstances);
		Exchange(BackupWeightmapTextures, WeightmapTextures);
	}
	else
	if (Ar.IsCooking() && !HasAnyFlags(RF_ClassDefaultObject) && !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::MobileRendering))
	{
		// These properties are only used for mobile so we back them up and clear them before serializing them.
		UMaterialInterface* BackupMobileMaterialInterface = nullptr;
		TArray<UTexture2D*> BackupMobileWeightmapTextures;

		Exchange(MobileMaterialInterface, BackupMobileMaterialInterface);
		Exchange(MobileWeightmapTextures, BackupMobileWeightmapTextures);

		Super::Serialize(Ar);

		Exchange(MobileMaterialInterface, BackupMobileMaterialInterface);
		Exchange(MobileWeightmapTextures, BackupMobileWeightmapTextures);
	}
	else
#endif
	{
		Super::Serialize(Ar);
	}

	if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::MapBuildDataSeparatePackage)
	{
		FMeshMapBuildData* LegacyMapBuildData = new FMeshMapBuildData();
		Ar << LegacyMapBuildData->LightMap;
		Ar << LegacyMapBuildData->ShadowMap;
		LegacyMapBuildData->IrrelevantLights = IrrelevantLights_DEPRECATED;

		FMeshMapBuildLegacyData LegacyComponentData;
		LegacyComponentData.Data.Emplace(MapBuildDataId, LegacyMapBuildData);
		GComponentsWithLegacyLightmaps.AddAnnotation(this, LegacyComponentData);
	}

	if (Ar.UE4Ver() >= VER_UE4_SERIALIZE_LANDSCAPE_GRASS_DATA)
	{
		// Share the shared ref so PIE can share this data
		if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
		{
			if (Ar.IsSaving())
			{
				PTRINT GrassDataPointer = (PTRINT)&GrassData;
				Ar << GrassDataPointer;
			}
			else
			{
				PTRINT GrassDataPointer;
				Ar << GrassDataPointer;
				// Duplicate shared reference
				GrassData = *(TSharedRef<FLandscapeComponentGrassData, ESPMode::ThreadSafe>*)GrassDataPointer;
			}
		}
		else
		{
			Ar << GrassData.Get();
		}
	}

#if WITH_EDITOR
	if (Ar.IsTransacting())
	{
		Ar << EditToolRenderData.SelectedType;
	}
#endif

	bool bCooked = false;

	if (Ar.UE4Ver() >= VER_UE4_LANDSCAPE_PLATFORMDATA_COOKING && !HasAnyFlags(RF_ClassDefaultObject))
	{
		bCooked = Ar.IsCooking() || (FPlatformProperties::RequiresCookedData() && Ar.IsSaving());
		// This is needed when loading cooked data, to know to serialize differently
		Ar << bCooked;
	}

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogLandscape, Fatal, TEXT("This platform requires cooked packages, and this landscape does not contain cooked data %s."), *GetName());
	}

#if ENABLE_LANDSCAPE_COOKING
	if (bCooked)
	{
		bool bCookedMobileData = Ar.IsCooking() && Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::MobileRendering);
		Ar << bCookedMobileData;

		// Saving for cooking path
		if (bCookedMobileData)
		{
			if (Ar.IsCooking())
			{
				check(PlatformData.HasValidPlatformData());
			}
			Ar << PlatformData;
		}
	}
#endif

#if WITH_EDITOR
	if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
	{
		Ar << PlatformData;
	}
#endif
}

void ULandscapeComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GrassData->GetAllocatedSize());
}

#if WITH_EDITOR
UMaterialInterface* ULandscapeComponent::GetLandscapeMaterial() const
	{		
	if (OverrideMaterial)
	{
		return OverrideMaterial;
	}
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		return Proxy->GetLandscapeMaterial();
	}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}

UMaterialInterface* ULandscapeComponent::GetLandscapeHoleMaterial() const
{
	if (OverrideHoleMaterial)
	{
		return OverrideHoleMaterial;
	}
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		return Proxy->GetLandscapeHoleMaterial();
	}
	return nullptr;
}

bool ULandscapeComponent::ComponentHasVisibilityPainted() const
{
	for (const FWeightmapLayerAllocationInfo& Allocation : WeightmapLayerAllocations)
	{
		if (Allocation.LayerInfo == ALandscapeProxy::VisibilityLayer)
		{
			return true;
		}
	}

	return false;
}

FString ULandscapeComponent::GetLayerAllocationKey(const TArray<FWeightmapLayerAllocationInfo>& Allocations, UMaterialInterface* LandscapeMaterial, bool bMobile /*= false*/)
{
	if (!LandscapeMaterial)
	{
		return FString();
	}

	FString Result = LandscapeMaterial->GetPathName();

	// Sort the allocations
	TArray<FString> LayerStrings;
	for (int32 LayerIdx = 0; LayerIdx < Allocations.Num(); LayerIdx++)
	{
		LayerStrings.Add(FString::Printf(TEXT("_%s_%d"), *Allocations[LayerIdx].GetLayerName().ToString(), Allocations[LayerIdx].WeightmapTextureIndex));
	}
	/**
	 * Generate a key for this component's layer allocations to use with MaterialInstanceConstantMap.
	 */
	LayerStrings.Sort(TGreater<FString>());

	for (int32 LayerIdx = 0; LayerIdx < LayerStrings.Num(); LayerIdx++)
	{
		Result += LayerStrings[LayerIdx];
	}

	if (bMobile)
	{
		Result += TEXT("M");
	}

	return Result;
}

void ULandscapeComponent::GetLayerDebugColorKey(int32& R, int32& G, int32& B) const
{
	ULandscapeInfo* Info = GetLandscapeInfo();
	if (Info != nullptr)
	{
		R = INDEX_NONE, G = INDEX_NONE, B = INDEX_NONE;

		for (auto It = Info->Layers.CreateConstIterator(); It; It++)
		{
			const FLandscapeInfoLayerSettings& LayerStruct = *It;
			if (LayerStruct.DebugColorChannel > 0
				&& LayerStruct.LayerInfoObj)
			{
				for (int32 LayerIdx = 0; LayerIdx < WeightmapLayerAllocations.Num(); LayerIdx++)
				{
					if (WeightmapLayerAllocations[LayerIdx].LayerInfo == LayerStruct.LayerInfoObj)
					{
						if (LayerStruct.DebugColorChannel & 1) // R
						{
							R = (WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex * 4 + WeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel);
						}
						if (LayerStruct.DebugColorChannel & 2) // G
						{
							G = (WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex * 4 + WeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel);
						}
						if (LayerStruct.DebugColorChannel & 4) // B
						{
							B = (WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex * 4 + WeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel);
						}
						break;
					}
				}
			}
		}
	}
}
#endif	//WITH_EDITOR

ULandscapeMeshCollisionComponent::ULandscapeMeshCollisionComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// make landscape always create? 
	bAlwaysCreatePhysicsState = true;
}

ULandscapeInfo::ULandscapeInfo(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void ULandscapeInfo::UpdateDebugColorMaterial()
{
	FlushRenderingCommands();
	//GWarn->BeginSlowTask( *FString::Printf(TEXT("Compiling layer color combinations for %s"), *GetName()), true);

	for (auto It = XYtoComponentMap.CreateIterator(); It; ++It)
	{
		ULandscapeComponent* Comp = It.Value();
		if (Comp)
		{
			Comp->EditToolRenderData.UpdateDebugColorMaterial(Comp);
			Comp->UpdateEditToolRenderData();
		}
	}
	FlushRenderingCommands();
	//GWarn->EndSlowTask();
}

void ULandscapeComponent::UpdatedSharedPropertiesFromActor()
{
	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();

	bCastStaticShadow = LandscapeProxy->bCastStaticShadow;
	bCastShadowAsTwoSided = LandscapeProxy->bCastShadowAsTwoSided;
	bCastFarShadow = LandscapeProxy->bCastFarShadow;
	bAffectDistanceFieldLighting = LandscapeProxy->bAffectDistanceFieldLighting;
	bRenderCustomDepth = LandscapeProxy->bRenderCustomDepth;
	LDMaxDrawDistance = LandscapeProxy->LDMaxDrawDistance;
	CustomDepthStencilValue = LandscapeProxy->CustomDepthStencilValue;
	LightingChannels = LandscapeProxy->LightingChannels;
}

void ULandscapeComponent::PostLoad()
{
	Super::PostLoad();

	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
	if (ensure(LandscapeProxy))
	{
		// Ensure that the component's lighting settings matches the actor's.
		UpdatedSharedPropertiesFromActor();	

		// check SectionBaseX/Y are correct
		int32 CheckSectionBaseX = FMath::RoundToInt(RelativeLocation.X) + LandscapeProxy->LandscapeSectionOffset.X;
		int32 CheckSectionBaseY = FMath::RoundToInt(RelativeLocation.Y) + LandscapeProxy->LandscapeSectionOffset.Y;
		if (CheckSectionBaseX != SectionBaseX ||
			CheckSectionBaseY != SectionBaseY)
		{
			UE_LOG(LogLandscape, Warning, TEXT("LandscapeComponent SectionBaseX disagrees with its location, attempted automated fix: '%s', %d,%d vs %d,%d."),
				*GetFullName(), SectionBaseX, SectionBaseY, CheckSectionBaseX, CheckSectionBaseY);
			SectionBaseX = CheckSectionBaseX;
			SectionBaseY = CheckSectionBaseY;
		}
	}

#if WITH_EDITOR
	if (GIsEditor && !HasAnyFlags(RF_ClassDefaultObject))
	{
		// This is to ensure that component relative location is exact section base offset value
		float CheckRelativeLocationX = float(SectionBaseX - LandscapeProxy->LandscapeSectionOffset.X);
		float CheckRelativeLocationY = float(SectionBaseY - LandscapeProxy->LandscapeSectionOffset.Y);
		if (CheckRelativeLocationX != RelativeLocation.X || 
			CheckRelativeLocationY != RelativeLocation.Y)
		{
			UE_LOG(LogLandscape, Warning, TEXT("LandscapeComponent RelativeLocation disagrees with its section base, attempted automated fix: '%s', %f,%f vs %f,%f."),
				*GetFullName(), RelativeLocation.X, RelativeLocation.Y, CheckRelativeLocationX, CheckRelativeLocationY);
			RelativeLocation.X = CheckRelativeLocationX;
			RelativeLocation.Y = CheckRelativeLocationY;
		}

		// Remove standalone flags from data textures to ensure data is unloaded in the editor when reverting an unsaved level.
		// Previous version of landscape set these flags on creation.
		if (HeightmapTexture && HeightmapTexture->HasAnyFlags(RF_Standalone))
		{
			HeightmapTexture->ClearFlags(RF_Standalone);
		}
		for (int32 Idx = 0; Idx < WeightmapTextures.Num(); Idx++)
		{
			if (WeightmapTextures[Idx] && WeightmapTextures[Idx]->HasAnyFlags(RF_Standalone))
			{
				WeightmapTextures[Idx]->ClearFlags(RF_Standalone);
			}
		}

		if (GIBakedBaseColorTexture)
		{
			if (GIBakedBaseColorTexture->GetOutermost() != GetOutermost())
			{
				// The GIBakedBaseColorTexture property was never intended to be reassigned, but it was previously editable so we need to null any invalid values
				// it will get recreated by ALandscapeProxy::UpdateBakedTextures()
				GIBakedBaseColorTexture = nullptr;
				BakedTextureMaterialGuid = FGuid();
			}
			else
			{
				// Remove public flag from GI textures to stop them being visible in the content browser.
				// Previous version of landscape set these flags on creation.
				if (GIBakedBaseColorTexture->HasAnyFlags(RF_Public))
				{
					GIBakedBaseColorTexture->ClearFlags(RF_Public);
				}
			}
		}
	}
#endif

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// If we're loading on a platform that doesn't require cooked data, but *only* supports OpenGL ES, generate or preload data from the DDC
		if (!FPlatformProperties::RequiresCookedData() && GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1)
		{
			CheckGenerateLandscapePlatformData(false, nullptr);
		}
	}

#if WITH_EDITORONLY_DATA
	// Handle old MaterialInstance
	if (MaterialInstance_DEPRECATED)
	{
		MaterialInstances.Empty(1);
		MaterialInstances.Add(MaterialInstance_DEPRECATED);
		MaterialInstance_DEPRECATED = nullptr;

#if WITH_EDITOR
		if (GIsEditor && MaterialInstances.Num() > 0 && MaterialInstances[0] != nullptr)
		{
			MaterialInstances[0]->ConditionalPostLoad();
			UpdateMaterialInstances();
		}
#endif // WITH_EDITOR
	}
#endif

#if !UE_BUILD_SHIPPING
	// Be sure we have the appropriate material count
	const bool bTessellationEnabled = (MaterialInstances.Num() > 0 && MaterialInstances[0] != nullptr && MaterialInstances[0]->Parent != nullptr && MaterialInstances[0]->Parent->GetMaterial()->D3D11TessellationMode != EMaterialTessellationMode::MTM_NoTessellation);

	if (bTessellationEnabled && MaterialInstances.Num() < 2)
	{
		if (GIsEditor)
		{
			UpdateMaterialInstances();
		}
		else
		{
			UE_LOG(LogLandscape, Error, TEXT("Landscape component (%d, %d) have a material with Tessellation enabled but we do not have the right amount of MaterialInstances. To correct this issue, open the map in the editor and resave the map."),	SectionBaseX, SectionBaseY);
		}
	}
#endif // UE_BUILD_SHIPPING

#if WITH_EDITOR
	if (GIsEditor && !HasAnyFlags(RF_ClassDefaultObject))
	{
		// Move the MICs and Textures back to the Package if they're currently in the level
		// Moving them into the level caused them to be duplicated when running PIE, which is *very very slow*, so we've reverted that change
		// Also clear the public flag to avoid various issues, e.g. generating and saving thumbnails that can never be seen
		ULevel* Level = GetLevel();
		if (ensure(Level))
		{
			TArray<UObject*> ObjectsToMoveFromLevelToPackage;
			GetGeneratedTexturesAndMaterialInstances(ObjectsToMoveFromLevelToPackage);

			UPackage* MyPackage = GetOutermost();
			for (auto* Obj : ObjectsToMoveFromLevelToPackage)
			{
				Obj->ClearFlags(RF_Public);
				if (Obj->GetOuter() == Level)
				{
					Obj->Rename(nullptr, MyPackage, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
				}
			}
		}
	}
#endif

#if !UE_BUILD_SHIPPING
	if (MobileCombinationMaterialInstance == nullptr)
	{
		if (GIsEditor)
		{
			UpdateMaterialInstances();
		}
		else
		{
			if(GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1)
			{
				UE_LOG(LogLandscape, Error, TEXT("Landscape component (%d, %d) Does not have a valid mobile combination material. To correct this issue, open the map in the editor and resave the map."), SectionBaseX, SectionBaseY);
			}
		}
	}
#endif // UE_BUILD_SHIPPING

	GrassData->ConditionalDiscardDataOnLoad();
}

#endif // WITH_EDITOR

ALandscapeProxy::ALandscapeProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, TargetDisplayOrder(ELandscapeLayerDisplayMode::Default)
#endif // WITH_EDITORONLY_DATA
	, bHasLandscapeGrass(true)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bAllowTickBeforeBeginPlay = true;

	bReplicates = false;
	NetUpdateFrequency = 10.0f;
	bHidden = false;
	bReplicateMovement = false;
	bCanBeDamaged = false;
	// by default we want to see the Landscape shadows even in the far shadow cascades
	bCastFarShadow = true;
	bAffectDistanceFieldLighting = true;

	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent0"));
	RootComponent = SceneComponent;
	RootComponent->RelativeScale3D = FVector(128.0f, 128.0f, 256.0f); // Old default scale, preserved for compatibility. See ULandscapeEditorObject::NewLandscape_Scale
	RootComponent->Mobility = EComponentMobility::Static;
	LandscapeSectionOffset = FIntPoint::ZeroValue;

	StaticLightingResolution = 1.0f;
	StreamingDistanceMultiplier = 1.0f;
	MaxLODLevel = -1;
	bUseDynamicMaterialInstance = false;
	OccluderGeometryLOD = 1; // 1 - usually is a good default
#if WITH_EDITORONLY_DATA
	bLockLocation = true;
	bIsMovingToLevel = false;
#endif // WITH_EDITORONLY_DATA
	TessellationComponentScreenSize = 0.8f;
	ComponentScreenSizeToUseSubSections = 0.65f;
	UseTessellationComponentScreenSizeFalloff = true;
	TessellationComponentScreenSizeFalloff = 0.75f;
	LOD0DistributionSetting = 1.75f;
	LODDistributionSetting = 2.0f;
	bCastStaticShadow = true;
	bCastShadowAsTwoSided = false;
	bUsedForNavigation = true;
	CollisionThickness = 16;
	BodyInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	bGenerateOverlapEvents = false;
#if WITH_EDITORONLY_DATA
	MaxPaintedLayersPerComponent = 0;
#endif

#if WITH_EDITOR
	if (VisibilityLayer == nullptr)
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<ULandscapeLayerInfoObject> DataLayer;
			FConstructorStatics()
				: DataLayer(TEXT("LandscapeLayerInfoObject'/Engine/EditorLandscapeResources/DataLayer.DataLayer'"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		VisibilityLayer = ConstructorStatics.DataLayer.Get();
		check(VisibilityLayer);
	#if WITH_EDITORONLY_DATA
		// This layer should be no weight blending
		VisibilityLayer->bNoWeightBlend = true;
	#endif
		VisibilityLayer->LayerUsageDebugColor = FLinearColor(0, 0, 0, 0);
		VisibilityLayer->AddToRoot();
	}
#endif

	static uint32 FrameOffsetForTickIntervalInc = 0;
	FrameOffsetForTickInterval = FrameOffsetForTickIntervalInc++;
}

ALandscape::ALandscape(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bLockLocation = false;
#endif // WITH_EDITORONLY_DATA
}

ALandscapeStreamingProxy::ALandscapeStreamingProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bLockLocation = true;
#endif // WITH_EDITORONLY_DATA
}

ALandscape* ALandscape::GetLandscapeActor()
{
	return this;
}

ALandscape* ALandscapeStreamingProxy::GetLandscapeActor()
{
	return LandscapeActor.Get();
}

#if WITH_EDITOR
ULandscapeInfo* ALandscapeProxy::CreateLandscapeInfo()
{
	ULandscapeInfo* LandscapeInfo = nullptr;

	check(GIsEditor);
	check(LandscapeGuid.IsValid());
	UWorld* OwningWorld = GetWorld();
	check(OwningWorld);
	check(!OwningWorld->IsGameWorld());
	
	auto& LandscapeInfoMap = ULandscapeInfoMap::GetLandscapeInfoMap(OwningWorld);
	LandscapeInfo = LandscapeInfoMap.Map.FindRef(LandscapeGuid);

	if (!LandscapeInfo)
	{
		check(!HasAnyFlags(RF_BeginDestroyed));
		LandscapeInfo = NewObject<ULandscapeInfo>(GetTransientPackage(), NAME_None, RF_Transactional | RF_Transient);
		LandscapeInfoMap.Modify(false);
		LandscapeInfoMap.Map.Add(LandscapeGuid, LandscapeInfo);
	}
	check(LandscapeInfo);
	LandscapeInfo->RegisterActor(this);

	return LandscapeInfo;
}

ULandscapeInfo* ALandscapeProxy::GetLandscapeInfo() const
{
	ULandscapeInfo* LandscapeInfo = nullptr;

	check(GIsEditor);
	check(LandscapeGuid.IsValid());
	UWorld* OwningWorld = GetWorld();

	if (OwningWorld != nullptr && !OwningWorld->IsGameWorld())
	{
		auto& LandscapeInfoMap = ULandscapeInfoMap::GetLandscapeInfoMap(OwningWorld);
		LandscapeInfo = LandscapeInfoMap.Map.FindRef(LandscapeGuid);
	}
	return LandscapeInfo;
}
#endif

ALandscape* ULandscapeComponent::GetLandscapeActor() const
{
	ALandscapeProxy* Landscape = GetLandscapeProxy();
	if (Landscape)
	{
		return Landscape->GetLandscapeActor();
	}
	return nullptr;
}

ULevel* ULandscapeComponent::GetLevel() const
{
	AActor* MyOwner = GetOwner();
	return MyOwner ? MyOwner->GetLevel() : nullptr;
}

#if WITH_EDITOR
void ULandscapeComponent::GetGeneratedTexturesAndMaterialInstances(TArray<UObject*>& OutTexturesAndMaterials) const
{
	if (HeightmapTexture)
	{
		OutTexturesAndMaterials.Add(HeightmapTexture);
	}

	for (auto* Tex : WeightmapTextures)
	{
		OutTexturesAndMaterials.Add(Tex);
	}

	if (XYOffsetmapTexture)
	{
		OutTexturesAndMaterials.Add(XYOffsetmapTexture);
	}

	for (UMaterialInstance* MaterialInstance : MaterialInstances)
	{
		for (ULandscapeMaterialInstanceConstant* CurrentMIC = Cast<ULandscapeMaterialInstanceConstant>(MaterialInstance); CurrentMIC; CurrentMIC = Cast<ULandscapeMaterialInstanceConstant>(CurrentMIC->Parent))
		{
			OutTexturesAndMaterials.Add(CurrentMIC);

			// Sometimes weight map is not registered in the WeightmapTextures, so
			// we need to get it from here.
			auto* WeightmapPtr = CurrentMIC->TextureParameterValues.FindByPredicate(
				[](const FTextureParameterValue& ParamValue)
			{
				static const FName WeightmapParamName("Weightmap0");
				return ParamValue.ParameterInfo.Name == WeightmapParamName;
			});

			if (WeightmapPtr != nullptr &&
				!OutTexturesAndMaterials.Contains(WeightmapPtr->ParameterValue))
			{
				OutTexturesAndMaterials.Add(WeightmapPtr->ParameterValue);
			}
		}
	}
}
#endif

ALandscapeProxy* ULandscapeComponent::GetLandscapeProxy() const
{
	return CastChecked<ALandscapeProxy>(GetOuter());
}

FIntPoint ULandscapeComponent::GetSectionBase() const
{
	return FIntPoint(SectionBaseX, SectionBaseY);
}

void ULandscapeComponent::SetSectionBase(FIntPoint InSectionBase)
{
	SectionBaseX = InSectionBase.X;
	SectionBaseY = InSectionBase.Y;
}

const FMeshMapBuildData* ULandscapeComponent::GetMeshMapBuildData() const
{
	AActor* Owner = GetOwner();

	if (Owner)
	{
		ULevel* OwnerLevel = Owner->GetLevel();

		if (OwnerLevel && OwnerLevel->OwningWorld)
		{
			ULevel* ActiveLightingScenario = OwnerLevel->OwningWorld->GetActiveLightingScenario();
			UMapBuildDataRegistry* MapBuildData = NULL;

			if (ActiveLightingScenario && ActiveLightingScenario->MapBuildData)
			{
				MapBuildData = ActiveLightingScenario->MapBuildData;
			}
			else if (OwnerLevel->MapBuildData)
			{
				MapBuildData = OwnerLevel->MapBuildData;
			}

			if (MapBuildData)
			{
				return MapBuildData->GetMeshBuildData(MapBuildDataId);
			}
		}
	}
	
	return NULL;
}

bool ULandscapeComponent::IsPrecomputedLightingValid() const
{
	return GetMeshMapBuildData() != NULL;
}

void ULandscapeComponent::PropagateLightingScenarioChange()
{
	FComponentRecreateRenderStateContext Context(this);
}

#if WITH_EDITOR
ULandscapeInfo* ULandscapeComponent::GetLandscapeInfo() const
{
	return GetLandscapeProxy()->GetLandscapeInfo();
}
#endif

void ULandscapeComponent::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	// Ask render thread to destroy EditToolRenderData
	EditToolRenderData = FLandscapeEditToolRenderData();
	UpdateEditToolRenderData();

	if (GIsEditor && !HasAnyFlags(RF_ClassDefaultObject))
	{
		ALandscapeProxy* Proxy = GetLandscapeProxy();

		// Remove any weightmap allocations from the Landscape Actor's map
		for (int32 LayerIdx = 0; LayerIdx < WeightmapLayerAllocations.Num(); LayerIdx++)
		{
			int32 WeightmapIndex = WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex;
			if (WeightmapTextures.IsValidIndex(WeightmapIndex))
			{
				UTexture2D* WeightmapTexture = WeightmapTextures[WeightmapIndex];
				FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(WeightmapTexture);
				if (Usage != nullptr)
				{
					Usage->ChannelUsage[WeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel] = nullptr;

					if (Usage->FreeChannelCount() == 4)
					{
						Proxy->WeightmapUsageMap.Remove(WeightmapTexture);
					}
				}
			}
		}
	}
#endif
}

FPrimitiveSceneProxy* ULandscapeComponent::CreateSceneProxy()
{
	const auto FeatureLevel = GetWorld()->FeatureLevel;
	FPrimitiveSceneProxy* Proxy = nullptr;
	if (FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		Proxy = new FLandscapeComponentSceneProxy(this);
	}
	else // i.e. (FeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		if (PlatformData.HasValidRuntimeData())
		{
			Proxy = new FLandscapeComponentSceneProxyMobile(this);
		}
	}

	return Proxy;
}

void ULandscapeComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		Proxy->LandscapeComponents.Remove(this);
	}

	Super::DestroyComponent(bPromoteChildren);
}

FBoxSphereBounds ULandscapeComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox MyBounds = CachedLocalBox.TransformBy(LocalToWorld);
	MyBounds = MyBounds.ExpandBy({0, 0, NegativeZBoundsExtension}, {0, 0, PositiveZBoundsExtension});

	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		MyBounds = MyBounds.ExpandBy({0, 0, Proxy->NegativeZBoundsExtension}, {0, 0, Proxy->PositiveZBoundsExtension});
	}

	return FBoxSphereBounds(MyBounds);
}

void ULandscapeComponent::OnRegister()
{
	Super::OnRegister();

	if (GetLandscapeProxy())
	{
		// Generate MID representing the MIC
		if (GetLandscapeProxy()->bUseDynamicMaterialInstance)
		{
			MaterialInstancesDynamic.Reserve(MaterialInstances.Num());

			for (int32 i = 0; i < MaterialInstances.Num(); ++i)
			{
				MaterialInstancesDynamic.Add(UMaterialInstanceDynamic::Create(MaterialInstances[i], this));
			}
		}

#if WITH_EDITOR
		// AActor::GetWorld checks for Unreachable and BeginDestroyed
		UWorld* World = GetLandscapeProxy()->GetWorld();
		if (World && !World->IsGameWorld())
		{
			ULandscapeInfo* Info = GetLandscapeInfo();
			if (Info)
			{
				Info->RegisterActorComponent(this);
			}
		}
#endif
	}
}

void ULandscapeComponent::OnUnregister()
{
	Super::OnUnregister();

#if WITH_EDITOR
	if (GetLandscapeProxy())
	{
		// AActor::GetWorld checks for Unreachable and BeginDestroyed
		UWorld* World = GetLandscapeProxy()->GetWorld();

		// Game worlds don't have landscape infos
		if (World && !World->IsGameWorld())
		{
			ULandscapeInfo* Info = GetLandscapeInfo();
			if (Info)
			{
				Info->UnregisterActorComponent(this);
			}
		}
	}
#endif
}

void ALandscapeProxy::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITOR
	// Game worlds don't have landscape infos
	if (!GetWorld()->IsGameWorld())
	{
		// Duplicated Landscapes don't have a valid guid until PostEditImport is called, we'll register then
		if (LandscapeGuid.IsValid())
		{
			ULandscapeInfo* LandscapeInfo = CreateLandscapeInfo();

			LandscapeInfo->FixupProxiesTransform();
		}
	}
#endif
}

void ALandscapeProxy::UnregisterAllComponents(const bool bForReregister)
{
#if WITH_EDITOR
	// Game worlds don't have landscape infos
	if (GetWorld() && !GetWorld()->IsGameWorld()
		// On shutdown the world will be unreachable
		&& !GetWorld()->IsPendingKillOrUnreachable() &&
		// When redoing the creation of a landscape we may get UnregisterAllComponents called when
		// we are in a "pre-initialized" state (empty guid, etc)
		LandscapeGuid.IsValid())
	{
		ULandscapeInfo* LandscapeInfo = GetLandscapeInfo();
		if (LandscapeInfo)
		{
			LandscapeInfo->UnregisterActor(this);
		}
	}
#endif

	Super::UnregisterAllComponents(bForReregister);
}

// FLandscapeWeightmapUsage serializer
FArchive& operator<<(FArchive& Ar, FLandscapeWeightmapUsage& U)
{
	return Ar << U.ChannelUsage[0] << U.ChannelUsage[1] << U.ChannelUsage[2] << U.ChannelUsage[3];
}

#if WITH_EDITORONLY_DATA
FArchive& operator<<(FArchive& Ar, FLandscapeAddCollision& U)
{
	return Ar << U.Corners[0] << U.Corners[1] << U.Corners[2] << U.Corners[3];
}
#endif // WITH_EDITORONLY_DATA

FArchive& operator<<(FArchive& Ar, FLandscapeLayerStruct*& L)
{
	if (L)
	{
		Ar << L->LayerInfoObj;
#if WITH_EDITORONLY_DATA
		return Ar << L->ThumbnailMIC;
#else
		return Ar;
#endif // WITH_EDITORONLY_DATA
	}
	return Ar;
}

void ULandscapeInfo::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsTransacting())
	{
		Ar << XYtoComponentMap;
#if WITH_EDITORONLY_DATA
		Ar << XYtoAddCollisionMap;
#endif
		Ar << SelectedComponents;
		Ar << SelectedRegion;
		Ar << SelectedRegionComponents;
	}
}

void ALandscape::PostLoad()
{
	if (!LandscapeGuid.IsValid())
	{
		LandscapeGuid = FGuid::NewGuid();
	}
	else
	{
#if WITH_EDITOR
		UWorld* CurrentWorld = GetWorld();
		for (ALandscape* Landscape : TObjectRange<ALandscape>(RF_ClassDefaultObject | RF_BeginDestroyed))
		{
			if (Landscape && Landscape != this && Landscape->LandscapeGuid == LandscapeGuid && Landscape->GetWorld() == CurrentWorld)
			{
				// Duplicated landscape level, need to generate new GUID
				Modify();
				LandscapeGuid = FGuid::NewGuid();


				// Show MapCheck window

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("ProxyName1"), FText::FromString(Landscape->GetName()));
				Arguments.Add(TEXT("LevelName1"), FText::FromString(Landscape->GetLevel()->GetOutermost()->GetName()));
				Arguments.Add(TEXT("ProxyName2"), FText::FromString(this->GetName()));
				Arguments.Add(TEXT("LevelName2"), FText::FromString(this->GetLevel()->GetOutermost()->GetName()));
				FMessageLog("LoadErrors").Warning()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(FText::Format(LOCTEXT("LoadError_DuplicateLandscapeGuid", "Landscape {ProxyName1} of {LevelName1} has the same guid as {ProxyName2} of {LevelName2}. {LevelName2}.{ProxyName2} has had its guid automatically changed, please save {LevelName2}!"), Arguments)));

				// Show MapCheck window
				FMessageLog("LoadErrors").Open();
				break;
			}
		}
#endif
}

	Super::PostLoad();
}

void ALandscapeProxy::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);

#if WITH_EDITOR
	// Work out whether we have grass or not for the next game run
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		bHasLandscapeGrass = LandscapeComponents.ContainsByPredicate([](ULandscapeComponent* Component) { return Component->MaterialHasGrass(); });
	}
#endif
}

void ALandscapeProxy::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FLandscapeCustomVersion::GUID);

	if (Ar.IsLoading() && Ar.CustomVer(FLandscapeCustomVersion::GUID) < FLandscapeCustomVersion::MigrateOldPropertiesToNewRenderingProperties)
	{
		if (LODDistanceFactor_DEPRECATED > 0)
		{
			const float LOD0LinearDistributionSettingMigrationTable[11] = { 1.75f, 1.75f, 1.75f, 1.75f, 1.75f, 1.68f, 1.55f, 1.4f, 1.25f, 1.25f, 1.25f };
			const float LODDLinearDistributionSettingMigrationTable[11] = { 2.0f, 2.0f, 2.0f, 1.65f, 1.35f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f };
			const float LOD0SquareRootDistributionSettingMigrationTable[11] = { 1.75f, 1.6f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f };
			const float LODDSquareRootDistributionSettingMigrationTable[11] = { 2.0f, 1.8f, 1.55f, 1.3f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.25f };

			if (LODFalloff_DEPRECATED == ELandscapeLODFalloff::Type::Linear)
			{
				LOD0DistributionSetting = LOD0LinearDistributionSettingMigrationTable[FMath::RoundToInt(LODDistanceFactor_DEPRECATED)];
				LODDistributionSetting = LODDLinearDistributionSettingMigrationTable[FMath::RoundToInt(LODDistanceFactor_DEPRECATED)];
			}
			else if (LODFalloff_DEPRECATED == ELandscapeLODFalloff::Type::SquareRoot)
			{
				LOD0DistributionSetting = LOD0SquareRootDistributionSettingMigrationTable[FMath::RoundToInt(LODDistanceFactor_DEPRECATED)];
				LODDistributionSetting = LODDSquareRootDistributionSettingMigrationTable[FMath::RoundToInt(LODDistanceFactor_DEPRECATED)];
			}
		}
	}

#if WITH_EDITOR
	if (Ar.IsTransacting())
	{
		Ar << WeightmapUsageMap;
	}
#endif
}

void ALandscapeProxy::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ALandscapeProxy* This = CastChecked<ALandscapeProxy>(InThis);

	Super::AddReferencedObjects(InThis, Collector);

#if WITH_EDITORONLY_DATA
	Collector.AddReferencedObjects(This->MaterialInstanceConstantMap, This);
#endif

	for (auto It = This->WeightmapUsageMap.CreateIterator(); It; ++It)
	{
		Collector.AddReferencedObject(It.Key(), This);
		Collector.AddReferencedObject(It.Value().ChannelUsage[0], This);
		Collector.AddReferencedObject(It.Value().ChannelUsage[1], This);
		Collector.AddReferencedObject(It.Value().ChannelUsage[2], This);
		Collector.AddReferencedObject(It.Value().ChannelUsage[3], This);
	}
}

#if WITH_EDITOR
FName FLandscapeInfoLayerSettings::GetLayerName() const
{
	checkSlow(LayerInfoObj == nullptr || LayerInfoObj->LayerName == LayerName);

	return LayerName;
}

FLandscapeEditorLayerSettings& FLandscapeInfoLayerSettings::GetEditorSettings() const
{
	check(LayerInfoObj);

	ULandscapeInfo* LandscapeInfo = Owner->GetLandscapeInfo();
	return LandscapeInfo->GetLayerEditorSettings(LayerInfoObj);
}

FLandscapeEditorLayerSettings& ULandscapeInfo::GetLayerEditorSettings(ULandscapeLayerInfoObject* LayerInfo) const
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	FLandscapeEditorLayerSettings* EditorLayerSettings = Proxy->EditorLayerSettings.FindByKey(LayerInfo);
	if (EditorLayerSettings)
	{
		return *EditorLayerSettings;
	}
	else
	{
		int32 Index = Proxy->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(LayerInfo));
		return Proxy->EditorLayerSettings[Index];
	}
}

void ULandscapeInfo::CreateLayerEditorSettingsFor(ULandscapeLayerInfoObject* LayerInfo)
{
	ForAllLandscapeProxies([LayerInfo](ALandscapeProxy* Proxy)
		{
		FLandscapeEditorLayerSettings* EditorLayerSettings = Proxy->EditorLayerSettings.FindByKey(LayerInfo);
		if (!EditorLayerSettings)
		{
			Proxy->Modify();
			Proxy->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(LayerInfo));
	}
	});
}

ULandscapeLayerInfoObject* ULandscapeInfo::GetLayerInfoByName(FName LayerName, ALandscapeProxy* Owner /*= nullptr*/) const
{
	ULandscapeLayerInfoObject* LayerInfo = nullptr;
	for (int32 j = 0; j < Layers.Num(); j++)
	{
		if (Layers[j].LayerInfoObj && Layers[j].LayerInfoObj->LayerName == LayerName
			&& (Owner == nullptr || Layers[j].Owner == Owner))
		{
			LayerInfo = Layers[j].LayerInfoObj;
		}
	}
	return LayerInfo;
}

int32 ULandscapeInfo::GetLayerInfoIndex(ULandscapeLayerInfoObject* LayerInfo, ALandscapeProxy* Owner /*= nullptr*/) const
{
	for (int32 j = 0; j < Layers.Num(); j++)
	{
		if (Layers[j].LayerInfoObj && Layers[j].LayerInfoObj == LayerInfo
			&& (Owner == nullptr || Layers[j].Owner == Owner))
		{
			return j;
		}
	}

	return INDEX_NONE;
}

int32 ULandscapeInfo::GetLayerInfoIndex(FName LayerName, ALandscapeProxy* Owner /*= nullptr*/) const
{
	for (int32 j = 0; j < Layers.Num(); j++)
	{
		if (Layers[j].GetLayerName() == LayerName
			&& (Owner == nullptr || Layers[j].Owner == Owner))
		{
			return j;
		}
	}

	return INDEX_NONE;
}


bool ULandscapeInfo::UpdateLayerInfoMap(ALandscapeProxy* Proxy /*= nullptr*/, bool bInvalidate /*= false*/)
{
	bool bHasCollision = false;
	if (GIsEditor)
	{
		if (Proxy)
		{
			if (bInvalidate)
			{
				// this is a horribly dangerous combination of parameters...

				for (int32 i = 0; i < Layers.Num(); i++)
				{
					if (Layers[i].Owner == Proxy)
					{
						Layers.RemoveAt(i--);
					}
				}
			}
			else // Proxy && !bInvalidate
			{
				TArray<FName> LayerNames = Proxy->GetLayersFromMaterial();

				// Validate any existing layer infos owned by this proxy
				for (int32 i = 0; i < Layers.Num(); i++)
				{
					if (Layers[i].Owner == Proxy)
					{
						Layers[i].bValid = LayerNames.Contains(Layers[i].GetLayerName());
					}
				}

				// Add placeholders for any unused material layers
				for (int32 i = 0; i < LayerNames.Num(); i++)
				{
					int32 LayerInfoIndex = GetLayerInfoIndex(LayerNames[i]);
					if (LayerInfoIndex == INDEX_NONE)
					{
						FLandscapeInfoLayerSettings LayerSettings(LayerNames[i], Proxy);
						LayerSettings.bValid = true;
						Layers.Add(LayerSettings);
					}
				}

				// Populate from layers used in components
				for (int32 ComponentIndex = 0; ComponentIndex < Proxy->LandscapeComponents.Num(); ComponentIndex++)
				{
					ULandscapeComponent* Component = Proxy->LandscapeComponents[ComponentIndex];

					// Add layers from per-component override materials
					if (Component->OverrideMaterial != nullptr)
					{
						TArray<FName> ComponentLayerNames = Proxy->GetLayersFromMaterial(Component->OverrideMaterial);
						for (int32 i = 0; i < ComponentLayerNames.Num(); i++)
						{
							int32 LayerInfoIndex = GetLayerInfoIndex(ComponentLayerNames[i]);
							if (LayerInfoIndex == INDEX_NONE)
							{
								FLandscapeInfoLayerSettings LayerSettings(ComponentLayerNames[i], Proxy);
								LayerSettings.bValid = true;
								Layers.Add(LayerSettings);
							}
						}
					}

					for (int32 AllocationIndex = 0; AllocationIndex < Component->WeightmapLayerAllocations.Num(); AllocationIndex++)
					{
						ULandscapeLayerInfoObject* LayerInfo = Component->WeightmapLayerAllocations[AllocationIndex].LayerInfo;
						if (LayerInfo)
						{
							int32 LayerInfoIndex = GetLayerInfoIndex(LayerInfo);
							bool bValid = LayerNames.Contains(LayerInfo->LayerName);

							#if WITH_EDITORONLY_DATA
							if (bValid)
							{
								//LayerInfo->IsReferencedFromLoadedData = true;
							}
							#endif

							if (LayerInfoIndex != INDEX_NONE)
							{
								FLandscapeInfoLayerSettings& LayerSettings = Layers[LayerInfoIndex];

								// Valid layer infos take precedence over invalid ones
								// Landscape Actors take precedence over Proxies
								if ((bValid && !LayerSettings.bValid)
									|| (bValid == LayerSettings.bValid && Proxy->IsA<ALandscape>()))
								{
									LayerSettings.Owner = Proxy;
									LayerSettings.bValid = bValid;
									LayerSettings.ThumbnailMIC = nullptr;
								}
							}
							else
							{
								// handle existing placeholder layers
								LayerInfoIndex = GetLayerInfoIndex(LayerInfo->LayerName);
								if (LayerInfoIndex != INDEX_NONE)
								{
									FLandscapeInfoLayerSettings& LayerSettings = Layers[LayerInfoIndex];

									//if (LayerSettings.Owner == Proxy)
									{
										LayerSettings.Owner = Proxy;
										LayerSettings.LayerInfoObj = LayerInfo;
										LayerSettings.bValid = bValid;
										LayerSettings.ThumbnailMIC = nullptr;
									}
								}
								else
								{
									FLandscapeInfoLayerSettings LayerSettings(LayerInfo, Proxy);
									LayerSettings.bValid = bValid;
									Layers.Add(LayerSettings);
								}
							}
						}
					}
				}

				// Add any layer infos cached in the actor
				Proxy->EditorLayerSettings.RemoveAll([](const FLandscapeEditorLayerSettings& Settings) { return Settings.LayerInfoObj == nullptr; });
				for (int32 i = 0; i < Proxy->EditorLayerSettings.Num(); i++)
				{
					FLandscapeEditorLayerSettings& EditorLayerSettings = Proxy->EditorLayerSettings[i];
					if (LayerNames.Contains(EditorLayerSettings.LayerInfoObj->LayerName))
					{
						// intentionally using the layer name here so we don't add layer infos from
						// the cache that have the same name as an actual assignment from a component above
						int32 LayerInfoIndex = GetLayerInfoIndex(EditorLayerSettings.LayerInfoObj->LayerName);
						if (LayerInfoIndex != INDEX_NONE)
						{
							FLandscapeInfoLayerSettings& LayerSettings = Layers[LayerInfoIndex];
							if (LayerSettings.LayerInfoObj == nullptr)
							{
								LayerSettings.Owner = Proxy;
								LayerSettings.LayerInfoObj = EditorLayerSettings.LayerInfoObj;
								LayerSettings.bValid = true;
							}
						}
					}
					else
					{
						Proxy->Modify();
						Proxy->EditorLayerSettings.RemoveAt(i--);
					}
				}
			}
		}
		else // !Proxy
		{
			Layers.Empty();

			if (!bInvalidate)
			{
				ForAllLandscapeProxies([this](ALandscapeProxy* EachProxy)
				{
					if (!EachProxy->IsPendingKillPending())
				{
						checkSlow(EachProxy->GetLandscapeInfo() == this);
						UpdateLayerInfoMap(EachProxy, false);
				}
				});
			}
		}

		//if (GCallbackEvent)
		//{
		//	GCallbackEvent->Send( CALLBACK_EditorPostModal );
		//}
	}
	return bHasCollision;
}
#endif

void ALandscapeProxy::PostLoad()
{
	Super::PostLoad();

	// disable ticking if we have no grass to tick
	if (!GIsEditor && !bHasLandscapeGrass)
	{
		SetActorTickEnabled(false);
		PrimaryActorTick.bCanEverTick = false;
	}

	// Temporary
	if (ComponentSizeQuads == 0 && LandscapeComponents.Num() > 0)
	{
		ULandscapeComponent* Comp = LandscapeComponents[0];
		if (Comp)
		{
			ComponentSizeQuads = Comp->ComponentSizeQuads;
			SubsectionSizeQuads = Comp->SubsectionSizeQuads;
			NumSubsections = Comp->NumSubsections;
		}
	}

	if (IsTemplate() == false)
	{
		BodyInstance.FixupData(this);
	}

#if WITH_EDITOR
	if (GIsEditor && !GetWorld()->IsGameWorld())
	{
		if ((GetLinker() && (GetLinker()->UE4Ver() < VER_UE4_LANDSCAPE_COMPONENT_LAZY_REFERENCES)) ||
			LandscapeComponents.Num() != CollisionComponents.Num() ||
			LandscapeComponents.ContainsByPredicate([](ULandscapeComponent* Comp) { return ((Comp != nullptr) && !Comp->CollisionComponent.IsValid()); }))
		{
			// Need to clean up invalid collision components
			CreateLandscapeInfo();
			RecreateCollisionComponents();
		}
	}

	EditorLayerSettings.RemoveAll([](const FLandscapeEditorLayerSettings& Settings) { return Settings.LayerInfoObj == nullptr; });

	if (EditorCachedLayerInfos_DEPRECATED.Num() > 0)
	{
		for (int32 i = 0; i < EditorCachedLayerInfos_DEPRECATED.Num(); i++)
		{
			EditorLayerSettings.Add(FLandscapeEditorLayerSettings(EditorCachedLayerInfos_DEPRECATED[i]));
		}
		EditorCachedLayerInfos_DEPRECATED.Empty();
	}

	if (GIsEditor && !GetWorld()->IsGameWorld())
	{
		ULandscapeInfo* LandscapeInfo = CreateLandscapeInfo();
		LandscapeInfo->RegisterActor(this, true);

		FixupWeightmaps();
	}

	// track feature level change to flush grass cache
	FOnFeatureLevelChanged::FDelegate FeatureLevelChangedDelegate = FOnFeatureLevelChanged::FDelegate::CreateUObject(this, &ALandscapeProxy::OnFeatureLevelChanged);
	FeatureLevelChangedDelegateHandle = GetWorld()->AddOnFeatureLevelChangedHandler(FeatureLevelChangedDelegate);

#endif
}

#if WITH_EDITOR
void ALandscapeProxy::Destroyed()
{
	Super::Destroyed();

	UWorld* World = GetWorld();

	if (GIsEditor && !World->IsGameWorld())
	{
		ULandscapeInfo::RecreateLandscapeInfo(World, false);

		if (SplineComponent)
		{
			SplineComponent->ModifySplines();
		}

		TotalComponentsNeedingGrassMapRender -= NumComponentsNeedingGrassMapRender;
		NumComponentsNeedingGrassMapRender = 0;
		TotalTexturesToStreamForVisibleGrassMapRender -= NumTexturesToStreamForVisibleGrassMapRender;
		NumTexturesToStreamForVisibleGrassMapRender = 0;
	}

	// unregister feature level changed handler for grass
	if (FeatureLevelChangedDelegateHandle.IsValid())
	{
		World->RemoveOnFeatureLevelChangedHandler(FeatureLevelChangedDelegateHandle);
		FeatureLevelChangedDelegateHandle.Reset();
	}
}

void ALandscapeProxy::GetSharedProperties(ALandscapeProxy* Landscape)
{
	if (GIsEditor && Landscape)
	{
		Modify();

		LandscapeGuid = Landscape->LandscapeGuid;

		//@todo UE4 merge, landscape, this needs work
		RootComponent->SetRelativeScale3D(Landscape->GetRootComponent()->GetComponentToWorld().GetScale3D());

		//PrePivot = Landscape->PrePivot;
		StaticLightingResolution = Landscape->StaticLightingResolution;
		bCastStaticShadow = Landscape->bCastStaticShadow;
		bCastShadowAsTwoSided = Landscape->bCastShadowAsTwoSided;
		LightingChannels = Landscape->LightingChannels;
		bRenderCustomDepth = Landscape->bRenderCustomDepth;
		LDMaxDrawDistance = Landscape->LDMaxDrawDistance;		
		CustomDepthStencilValue = Landscape->CustomDepthStencilValue;
		ComponentSizeQuads = Landscape->ComponentSizeQuads;
		NumSubsections = Landscape->NumSubsections;
		SubsectionSizeQuads = Landscape->SubsectionSizeQuads;
		MaxLODLevel = Landscape->MaxLODLevel;
		LODDistanceFactor_DEPRECATED = Landscape->LODDistanceFactor_DEPRECATED;
		LODFalloff_DEPRECATED = Landscape->LODFalloff_DEPRECATED;
		TessellationComponentScreenSize = Landscape->TessellationComponentScreenSize;
		ComponentScreenSizeToUseSubSections = Landscape->ComponentScreenSizeToUseSubSections;
		UseTessellationComponentScreenSizeFalloff = Landscape->UseTessellationComponentScreenSizeFalloff;
		TessellationComponentScreenSizeFalloff = Landscape->TessellationComponentScreenSizeFalloff;
		LODDistributionSetting = Landscape->LODDistributionSetting;
		LOD0DistributionSetting = Landscape->LOD0DistributionSetting;
		OccluderGeometryLOD = Landscape->OccluderGeometryLOD;
		NegativeZBoundsExtension = Landscape->NegativeZBoundsExtension;
		PositiveZBoundsExtension = Landscape->PositiveZBoundsExtension;
		CollisionMipLevel = Landscape->CollisionMipLevel;
		bBakeMaterialPositionOffsetIntoCollision = Landscape->bBakeMaterialPositionOffsetIntoCollision;
		if (!LandscapeMaterial)
		{
			LandscapeMaterial = Landscape->LandscapeMaterial;
		}
		if (!LandscapeHoleMaterial)
		{
			LandscapeHoleMaterial = Landscape->LandscapeHoleMaterial;
		}
		if (LandscapeMaterial == Landscape->LandscapeMaterial)
		{
			EditorLayerSettings = Landscape->EditorLayerSettings;
		}
		if (!DefaultPhysMaterial)
		{
			DefaultPhysMaterial = Landscape->DefaultPhysMaterial;
		}
		LightmassSettings = Landscape->LightmassSettings;
	}
}

void ALandscapeProxy::ConditionalAssignCommonProperties(ALandscape* Landscape)
{
	if (Landscape == nullptr)
	{
		return;
	}
	
	bool bUpdated = false;
	
	if (MaxLODLevel != Landscape->MaxLODLevel)
	{
		MaxLODLevel = Landscape->MaxLODLevel;
		bUpdated = true;
	}
	
	if (TessellationComponentScreenSize != Landscape->TessellationComponentScreenSize)
	{
		TessellationComponentScreenSize = Landscape->TessellationComponentScreenSize;
		bUpdated = true;
	}

	if (ComponentScreenSizeToUseSubSections != Landscape->ComponentScreenSizeToUseSubSections)
	{
		ComponentScreenSizeToUseSubSections = Landscape->ComponentScreenSizeToUseSubSections;
		bUpdated = true;
	}

	if (UseTessellationComponentScreenSizeFalloff != Landscape->UseTessellationComponentScreenSizeFalloff)
	{
		UseTessellationComponentScreenSizeFalloff = Landscape->UseTessellationComponentScreenSizeFalloff;
		bUpdated = true;
	}

	if (TessellationComponentScreenSizeFalloff != Landscape->TessellationComponentScreenSizeFalloff)
	{
		TessellationComponentScreenSizeFalloff = Landscape->TessellationComponentScreenSizeFalloff;
		bUpdated = true;
	}
	
	if (LODDistributionSetting != Landscape->LODDistributionSetting)
	{
		LODDistributionSetting = Landscape->LODDistributionSetting;
		bUpdated = true;
	}

	if (LOD0DistributionSetting != Landscape->LOD0DistributionSetting)
	{
		LOD0DistributionSetting = Landscape->LOD0DistributionSetting;
		bUpdated = true;
	}

	if (OccluderGeometryLOD != Landscape->OccluderGeometryLOD)
	{
		OccluderGeometryLOD = Landscape->OccluderGeometryLOD;
		bUpdated = true;
	}

	if (TargetDisplayOrder != Landscape->TargetDisplayOrder)
	{
		TargetDisplayOrder = Landscape->TargetDisplayOrder;
		bUpdated = true;
	}

	if (TargetDisplayOrderList != Landscape->TargetDisplayOrderList)
	{
		TargetDisplayOrderList = Landscape->TargetDisplayOrderList;
		bUpdated = true;
	}

	if (bUpdated)
	{
		MarkPackageDirty();
	}
}

FTransform ALandscapeProxy::LandscapeActorToWorld() const
{
	FTransform TM = ActorToWorld();
	// Add this proxy landscape section offset to obtain landscape actor transform
	TM.AddToTranslation(TM.TransformVector(-FVector(LandscapeSectionOffset)));
	return TM;
}

void ALandscapeProxy::SetAbsoluteSectionBase(FIntPoint InSectionBase)
{
	FIntPoint Difference = InSectionBase - LandscapeSectionOffset;
	LandscapeSectionOffset = InSectionBase;

	for (int32 CompIdx = 0; CompIdx < LandscapeComponents.Num(); CompIdx++)
	{
		ULandscapeComponent* Comp = LandscapeComponents[CompIdx];
		if (Comp)
		{
			FIntPoint AbsoluteSectionBase = Comp->GetSectionBase() + Difference;
			Comp->SetSectionBase(AbsoluteSectionBase);
			Comp->RecreateRenderState_Concurrent();
		}
	}

	for (int32 CompIdx = 0; CompIdx < CollisionComponents.Num(); CompIdx++)
	{
		ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents[CompIdx];
		if (Comp)
		{
			FIntPoint AbsoluteSectionBase = Comp->GetSectionBase() + Difference;
			Comp->SetSectionBase(AbsoluteSectionBase);
		}
	}
}

FIntPoint ALandscapeProxy::GetSectionBaseOffset() const
{
	return LandscapeSectionOffset;
}

void ALandscapeProxy::RecreateComponentsState()
{
	for (int32 ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++)
	{
		ULandscapeComponent* Comp = LandscapeComponents[ComponentIndex];
		if (Comp)
		{
			Comp->UpdateComponentToWorld();
			Comp->UpdateCachedBounds();
			Comp->UpdateBounds();
			Comp->RecreateRenderState_Concurrent(); // @todo UE4 jackp just render state needs update?
		}
	}

	for (int32 ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++)
	{
		ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents[ComponentIndex];
		if (Comp)
		{
			Comp->UpdateComponentToWorld();
			Comp->RecreatePhysicsState();
		}
	}
}

UMaterialInterface* ALandscapeProxy::GetLandscapeMaterial() const
{
	if (LandscapeMaterial)
	{
		return LandscapeMaterial;
					}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}

UMaterialInterface* ALandscapeProxy::GetLandscapeHoleMaterial() const
{
	if (LandscapeHoleMaterial)
	{
		return LandscapeHoleMaterial;
	}
	return nullptr;
}

UMaterialInterface* ALandscapeStreamingProxy::GetLandscapeMaterial() const
			{
	if (LandscapeMaterial)
	{
		return LandscapeMaterial;
	}
	else if (LandscapeActor)
	{
		return LandscapeActor->GetLandscapeMaterial();
	}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}

UMaterialInterface* ALandscapeStreamingProxy::GetLandscapeHoleMaterial() const
{
	if (LandscapeHoleMaterial)
	{
		return LandscapeHoleMaterial;
	}
	else if (ALandscape* Landscape = LandscapeActor.Get())
	{
		return Landscape->GetLandscapeHoleMaterial();
	}
	return nullptr;
}

void ALandscape::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
	//ULandscapeInfo* Info = GetLandscapeInfo();
	//if (GIsEditor && Info && !IsRunningCommandlet())
	//{
	//	for (TSet<ALandscapeProxy*>::TIterator It(Info->Proxies); It; ++It)
	//	{
	//		ALandscapeProxy* Proxy = *It;
	//		if (!ensure(Proxy->LandscapeActor == this))
	//		{
	//			Proxy->LandscapeActor = this;
	//			Proxy->GetSharedProperties(this);
	//		}
	//	}
	//}
}

ALandscapeProxy* ULandscapeInfo::GetLandscapeProxyForLevel(ULevel* Level) const
{
	ALandscapeProxy* LandscapeProxy = nullptr;
	ForAllLandscapeProxies([&LandscapeProxy, Level](ALandscapeProxy* Proxy)
	{
		if (Proxy->GetLevel() == Level)
	{
			LandscapeProxy = Proxy;
	}
	});
			return LandscapeProxy;
		}

ALandscapeProxy* ULandscapeInfo::GetCurrentLevelLandscapeProxy(bool bRegistered) const
{
	ALandscapeProxy* LandscapeProxy = nullptr;
	ForAllLandscapeProxies([&LandscapeProxy, bRegistered](ALandscapeProxy* Proxy)
	{
		if (!bRegistered || Proxy->GetRootComponent()->IsRegistered())
		{
			UWorld* ProxyWorld = Proxy->GetWorld();
			if (ProxyWorld &&
				ProxyWorld->GetCurrentLevel() == Proxy->GetOuter())
			{
				LandscapeProxy = Proxy;
			}
		}
	});
	return LandscapeProxy;
}

ALandscapeProxy* ULandscapeInfo::GetLandscapeProxy() const
{
	// Mostly this Proxy used to calculate transformations
	// in Editor all proxies of same landscape actor have root components in same locations
	// so it doesn't really matter which proxy we return here

	// prefer LandscapeActor in case it is loaded
	if (LandscapeActor.IsValid())
	{
		ALandscape* Landscape = LandscapeActor.Get();
		if (Landscape != nullptr &&
			Landscape->GetRootComponent()->IsRegistered())
		{
			return Landscape;
		}
	}

	// prefer current level proxy 
	ALandscapeProxy* Proxy = GetCurrentLevelLandscapeProxy(true);
	if (Proxy != nullptr)
	{
		return Proxy;
	}

	// any proxy in the world
	for (auto It = Proxies.CreateConstIterator(); It; ++It)
	{
		Proxy = (*It);
		if (Proxy != nullptr &&
			Proxy->GetRootComponent()->IsRegistered())
		{
			return Proxy;
		}
	}

	return nullptr;
}

void ULandscapeInfo::ForAllLandscapeProxies(TFunctionRef<void(ALandscapeProxy*)> Fn) const
{
	ALandscape* Landscape = LandscapeActor.Get();
	if (Landscape)
	{
		Fn(Landscape);
	}

	for (ALandscapeProxy* LandscapeProxy : Proxies)
	{
		Fn(LandscapeProxy);
	}
}

void ULandscapeInfo::RegisterActor(ALandscapeProxy* Proxy, bool bMapCheck)
{
	// do not pass here invalid actors
	checkSlow(Proxy);
	check(Proxy->GetLandscapeGuid().IsValid());
	UWorld* OwningWorld = Proxy->GetWorld();

	// in case this Info object is not initialized yet
	// initialized it with properties from passed actor
	if (LandscapeGuid.IsValid() == false ||
		(GetLandscapeProxy() == nullptr && ensure(LandscapeGuid == Proxy->GetLandscapeGuid())))
	{
		LandscapeGuid = Proxy->GetLandscapeGuid();
		ComponentSizeQuads = Proxy->ComponentSizeQuads;
		ComponentNumSubsections = Proxy->NumSubsections;
		SubsectionSizeQuads = Proxy->SubsectionSizeQuads;
		DrawScale = Proxy->HasValidRootComponent() ? Proxy->GetRootComponent()->RelativeScale3D : FVector(100.0f);
	}

	// check that passed actor matches all shared parameters
	check(LandscapeGuid == Proxy->GetLandscapeGuid());
	check(ComponentSizeQuads == Proxy->ComponentSizeQuads);
	check(ComponentNumSubsections == Proxy->NumSubsections);
	check(SubsectionSizeQuads == Proxy->SubsectionSizeQuads);

	if (Proxy->HasValidRootComponent() && !DrawScale.Equals(Proxy->GetRootComponent()->RelativeScale3D))
	{
		UE_LOG(LogLandscape, Warning, TEXT("Landscape proxy (%s) scale (%s) does not match to main actor scale (%s)."),
			*Proxy->GetName(), *Proxy->GetRootComponent()->RelativeScale3D.ToCompactString(), *DrawScale.ToCompactString());
	}

	// register
	if (ALandscape* Landscape = Cast<ALandscape>(Proxy))
	{
		checkf(!LandscapeActor || LandscapeActor == Landscape, TEXT("Multiple landscapes with the same GUID detected: %s vs %s"), *LandscapeActor->GetPathName(), *Landscape->GetPathName());
		LandscapeActor = Landscape;
		// In world composition user is not allowed to move landscape in editor, only through WorldBrowser 
		LandscapeActor->bLockLocation = (OwningWorld->WorldComposition != nullptr);

		// update proxies reference actor
		for (ALandscapeStreamingProxy* StreamingProxy : Proxies)
		{
			StreamingProxy->LandscapeActor = LandscapeActor;
			StreamingProxy->ConditionalAssignCommonProperties(Landscape);
		}
	}
	else
	{
		ALandscapeStreamingProxy* StreamingProxy = CastChecked<ALandscapeStreamingProxy>(Proxy);

		Proxies.Add(StreamingProxy);
		StreamingProxy->LandscapeActor = LandscapeActor;
		StreamingProxy->ConditionalAssignCommonProperties(LandscapeActor.Get());
	}

	UpdateLayerInfoMap(Proxy);
	UpdateAllAddCollisions();

	//
	// add proxy components to the XY map
	//
	for (int32 CompIdx = 0; CompIdx < Proxy->LandscapeComponents.Num(); ++CompIdx)
	{
		RegisterActorComponent(Proxy->LandscapeComponents[CompIdx], bMapCheck);
	}
}

void ULandscapeInfo::UnregisterActor(ALandscapeProxy* Proxy)
{
	if (ALandscape* Landscape = Cast<ALandscape>(Proxy))
	{
		// Note: UnregisterActor sometimes gets triggered twice, e.g. it has been observed to happen during redo
		// Note: In some cases LandscapeActor could be updated to a new landscape actor before the old landscape is unregistered/destroyed
		// e.g. this has been observed when merging levels in the editor

		if (LandscapeActor.Get() == Landscape)
		{
		LandscapeActor = nullptr;
		}

		// update proxies reference to landscape actor
		for (ALandscapeStreamingProxy* StreamingProxy : Proxies)
		{
			StreamingProxy->LandscapeActor = LandscapeActor;
		}
	}
	else
	{
		ALandscapeStreamingProxy* StreamingProxy = CastChecked<ALandscapeStreamingProxy>(Proxy);
		Proxies.Remove(StreamingProxy);
		StreamingProxy->LandscapeActor = nullptr;
	}

	// remove proxy components from the XY map
	for (int32 CompIdx = 0; CompIdx < Proxy->LandscapeComponents.Num(); ++CompIdx)
	{
		ULandscapeComponent* Component = Proxy->LandscapeComponents[CompIdx];
		if (Component) // When a landscape actor is being GC'd it's possible the components were already GC'd and are null
		{
			UnregisterActorComponent(Component);
		}
	}
	XYtoComponentMap.Compact();

	UpdateLayerInfoMap();
	UpdateAllAddCollisions();
}

void ULandscapeInfo::RegisterActorComponent(ULandscapeComponent* Component, bool bMapCheck)
{
	// Do not register components which are not part of the world
	if (Component == nullptr ||
		Component->IsRegistered() == false)
	{
		return;
	}

	check(Component);

	FIntPoint ComponentKey = Component->GetSectionBase() / Component->ComponentSizeQuads;
	auto RegisteredComponent = XYtoComponentMap.FindRef(ComponentKey);

	if (RegisteredComponent != Component)
	{
		if (RegisteredComponent == nullptr)
		{
			XYtoComponentMap.Add(ComponentKey, Component);
		}
		else if (bMapCheck)
		{
			ALandscapeProxy* OurProxy = Component->GetLandscapeProxy();
			ALandscapeProxy* ExistingProxy = RegisteredComponent->GetLandscapeProxy();
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ProxyName1"), FText::FromString(OurProxy->GetName()));
			Arguments.Add(TEXT("LevelName1"), FText::FromString(OurProxy->GetLevel()->GetOutermost()->GetName()));
			Arguments.Add(TEXT("ProxyName2"), FText::FromString(ExistingProxy->GetName()));
			Arguments.Add(TEXT("LevelName2"), FText::FromString(ExistingProxy->GetLevel()->GetOutermost()->GetName()));
			Arguments.Add(TEXT("XLocation"), Component->GetSectionBase().X);
			Arguments.Add(TEXT("YLocation"), Component->GetSectionBase().Y);
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(OurProxy))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_LandscapeComponentPostLoad_Warning", "Landscape {ProxyName1} of {LevelName1} has overlapping render components with {ProxyName2} of {LevelName2} at location ({XLocation}, {YLocation})."), Arguments)))
				->AddToken(FActionToken::Create(LOCTEXT("MapCheck_RemoveDuplicateLandscapeComponent", "Delete Duplicate"), LOCTEXT("MapCheck_RemoveDuplicateLandscapeComponentDesc", "Deletes the duplicate landscape component."), FOnActionTokenExecuted::CreateUObject(OurProxy, &ALandscapeProxy::RemoveOverlappingComponent, Component), true))
				->AddToken(FMapErrorToken::Create(FMapErrors::LandscapeComponentPostLoad_Warning));

			// Show MapCheck window
			FMessageLog("MapCheck").Open(EMessageSeverity::Warning);
		}
	}

	// Update Selected Components/Regions
	if (Component->EditToolRenderData.SelectedType)
	{
		if (Component->EditToolRenderData.SelectedType & FLandscapeEditToolRenderData::ST_COMPONENT)
		{
			SelectedComponents.Add(Component);
		}
		else if (Component->EditToolRenderData.SelectedType & FLandscapeEditToolRenderData::ST_REGION)
		{
			SelectedRegionComponents.Add(Component);
		}
	}
}

void ULandscapeInfo::UnregisterActorComponent(ULandscapeComponent* Component)
{
	if (ensure(Component))
	{
	FIntPoint ComponentKey = Component->GetSectionBase() / Component->ComponentSizeQuads;
	auto RegisteredComponent = XYtoComponentMap.FindRef(ComponentKey);

	if (RegisteredComponent == Component)
	{
		XYtoComponentMap.Remove(ComponentKey);
	}

	SelectedComponents.Remove(Component);
	SelectedRegionComponents.Remove(Component);
}
}

void ULandscapeInfo::Reset()
{
	LandscapeActor.Reset();

	Proxies.Empty();
	XYtoComponentMap.Empty();
	XYtoAddCollisionMap.Empty();

	//SelectedComponents.Empty();
	//SelectedRegionComponents.Empty();
	//SelectedRegion.Empty();
}

void ULandscapeInfo::FixupProxiesTransform()
{
	ALandscape* Landscape = LandscapeActor.Get();

	if (Landscape == nullptr ||
		Landscape->GetRootComponent()->IsRegistered() == false)
	{
		return;
	}

	// Make sure section offset of all proxies is multiple of ALandscapeProxy::ComponentSizeQuads
	for (auto It = Proxies.CreateConstIterator(); It; ++It)
	{
		ALandscapeProxy* Proxy = *It;
		FIntPoint LandscapeSectionOffset = Proxy->LandscapeSectionOffset - Landscape->LandscapeSectionOffset;
		FIntPoint LandscapeSectionOffsetRem(
			LandscapeSectionOffset.X % Proxy->ComponentSizeQuads, 
			LandscapeSectionOffset.Y % Proxy->ComponentSizeQuads);

		if (LandscapeSectionOffsetRem.X != 0 || LandscapeSectionOffsetRem.Y != 0)
		{
			FIntPoint NewLandscapeSectionOffset = Proxy->LandscapeSectionOffset - LandscapeSectionOffsetRem;
			
			UE_LOG(LogLandscape, Warning, TEXT("Landscape section base is not multiple of component size, attempted automated fix: '%s', %d,%d vs %d,%d."),
					*Proxy->GetFullName(), Proxy->LandscapeSectionOffset.X, Proxy->LandscapeSectionOffset.Y, NewLandscapeSectionOffset.X, NewLandscapeSectionOffset.Y);

			Proxy->SetAbsoluteSectionBase(NewLandscapeSectionOffset);
		}
	}

	FTransform LandscapeTM = Landscape->LandscapeActorToWorld();
	// Update transformations of all linked landscape proxies
	for (auto It = Proxies.CreateConstIterator(); It; ++It)
	{
		ALandscapeProxy* Proxy = *It;
		FTransform ProxyRelativeTM(FVector(Proxy->LandscapeSectionOffset));
		FTransform ProxyTransform = ProxyRelativeTM*LandscapeTM;

		if (!Proxy->GetTransform().Equals(ProxyTransform))
		{
			Proxy->SetActorTransform(ProxyTransform);

			// Let other systems know that an actor was moved
			GEngine->BroadcastOnActorMoved(Proxy);
		}
	}
}

void ULandscapeInfo::UpdateComponentLayerWhitelist()
{
	ForAllLandscapeProxies([](ALandscapeProxy* Proxy)
	{
		for (ULandscapeComponent* Comp : Proxy->LandscapeComponents)
		{
			Comp->UpdateLayerWhitelistFromPaintedLayers();
		}
	});
}

void ULandscapeInfo::RecreateLandscapeInfo(UWorld* InWorld, bool bMapCheck)
{
	check(InWorld);

	ULandscapeInfoMap& LandscapeInfoMap = ULandscapeInfoMap::GetLandscapeInfoMap(InWorld);
	LandscapeInfoMap.Modify();

	// reset all LandscapeInfo objects
	for (auto& LandscapeInfoPair : LandscapeInfoMap.Map)
	{
		ULandscapeInfo* LandscapeInfo = LandscapeInfoPair.Value;

		if (LandscapeInfo != nullptr)
		{
			LandscapeInfo->Modify();
			LandscapeInfo->Reset();
		}
	}

	TMap<FGuid, TArray<ALandscapeProxy*>> ValidLandscapesMap;
	// Gather all valid landscapes in the world
	for (ALandscapeProxy* Proxy : TActorRange<ALandscapeProxy>(InWorld))
	{
		if (Proxy->GetLevel() &&
			Proxy->GetLevel()->bIsVisible &&
			!Proxy->HasAnyFlags(RF_BeginDestroyed) &&
			!Proxy->IsPendingKill() &&
			!Proxy->IsPendingKillPending())
		{
			ValidLandscapesMap.FindOrAdd(Proxy->GetLandscapeGuid()).Add(Proxy);
		}
	}

	// Register landscapes in global landscape map
	for (auto& ValidLandscapesPair : ValidLandscapesMap)
	{
		auto& LandscapeList = ValidLandscapesPair.Value;
		for (ALandscapeProxy* Proxy : LandscapeList)
		{
			Proxy->CreateLandscapeInfo()->RegisterActor(Proxy, bMapCheck);
		}
	}

	// Remove empty entries from global LandscapeInfo map
	for (auto It = LandscapeInfoMap.Map.CreateIterator(); It; ++It)
	{
		ULandscapeInfo* Info = It.Value();

		if (Info != nullptr && Info->GetLandscapeProxy() == nullptr)
		{
			Info->MarkPendingKill();
			It.RemoveCurrent();
		}
		else if (Info == nullptr) // remove invalid entry
		{
			It.RemoveCurrent();
		}
	}

	// We need to inform Landscape editor tools about LandscapeInfo updates
	FEditorSupportDelegates::WorldChange.Broadcast();
}


#endif

void ULandscapeComponent::PostInitProperties()
{
	Super::PostInitProperties();

	// Create a new guid in case this is a newly created component
	// If not, this guid will be overwritten when serialized
	FPlatformMisc::CreateGuid(StateId);

	// Initialize MapBuildDataId to something unique, in case this is a new ULandscapeComponent
	MapBuildDataId = FGuid::NewGuid();
}

void ULandscapeComponent::PostDuplicate(bool bDuplicateForPIE)
{
	if (!bDuplicateForPIE)
	{
		// Reset the StateId on duplication since it needs to be unique for each capture.
		// PostDuplicate covers direct calls to StaticDuplicateObject, but not actor duplication (see PostEditImport)
		FPlatformMisc::CreateGuid(StateId);
	}
}

// Generate a new guid to force a recache of all landscape derived data
#define LANDSCAPE_FULL_DERIVEDDATA_VER			TEXT("016D326F3A954BBA9CCDFA00CEFA31E9")

FString FLandscapeComponentDerivedData::GetDDCKeyString(const FGuid& StateId)
{
	return FDerivedDataCacheInterface::BuildCacheKey(TEXT("LS_FULL"), LANDSCAPE_FULL_DERIVEDDATA_VER, *StateId.ToString());
}

void FLandscapeComponentDerivedData::InitializeFromUncompressedData(const TArray<uint8>& UncompressedData)
{
	int32 UncompressedSize = UncompressedData.Num() * UncompressedData.GetTypeSize();

	TArray<uint8> TempCompressedMemory;
	// Compressed can be slightly larger than uncompressed
	TempCompressedMemory.Empty(UncompressedSize * 4 / 3);
	TempCompressedMemory.AddUninitialized(UncompressedSize * 4 / 3);
	int32 CompressedSize = TempCompressedMemory.Num() * TempCompressedMemory.GetTypeSize();

	verify(FCompression::CompressMemory(
		(ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory),
		TempCompressedMemory.GetData(),
		CompressedSize,
		UncompressedData.GetData(),
		UncompressedSize));

	// Note: change LANDSCAPE_FULL_DERIVEDDATA_VER when modifying the serialization layout
	FMemoryWriter FinalArchive(CompressedLandscapeData, true);
	FinalArchive << UncompressedSize;
	FinalArchive << CompressedSize;
	FinalArchive.Serialize(TempCompressedMemory.GetData(), CompressedSize);
}

FArchive& operator<<(FArchive& Ar, FLandscapeComponentDerivedData& Data)
{
	return Ar << Data.CompressedLandscapeData;
}

bool FLandscapeComponentDerivedData::LoadFromDDC(const FGuid& StateId)
{
	return GetDerivedDataCacheRef().GetSynchronous(*GetDDCKeyString(StateId), CompressedLandscapeData);
}

void FLandscapeComponentDerivedData::SaveToDDC(const FGuid& StateId)
{
	check(CompressedLandscapeData.Num() > 0);
	GetDerivedDataCacheRef().Put(*GetDDCKeyString(StateId), CompressedLandscapeData);
}

void LandscapeMaterialsParameterValuesGetter(FStaticParameterSet& OutStaticParameterSet, UMaterialInstance* Material)
{
	if (Material->Parent)
	{
		UMaterial* ParentMaterial = Material->Parent->GetMaterial();

		TArray<FMaterialParameterInfo> OutParameterInfo;
		TArray<FGuid> Guids;
		Material->GetAllParameterInfo<UMaterialExpressionLandscapeLayerWeight>(OutParameterInfo, Guids);
		Material->GetAllParameterInfo<UMaterialExpressionLandscapeLayerSwitch>(OutParameterInfo, Guids);
		Material->GetAllParameterInfo<UMaterialExpressionLandscapeLayerSample>(OutParameterInfo, Guids);
		Material->GetAllParameterInfo<UMaterialExpressionLandscapeLayerBlend>(OutParameterInfo, Guids);
		Material->GetAllParameterInfo<UMaterialExpressionLandscapeVisibilityMask>(OutParameterInfo, Guids);

		OutStaticParameterSet.TerrainLayerWeightParameters.AddZeroed(OutParameterInfo.Num());
		for (int32 ParameterIdx = 0; ParameterIdx < OutParameterInfo.Num(); ParameterIdx++)
		{
			FStaticTerrainLayerWeightParameter& ParentParameter = OutStaticParameterSet.TerrainLayerWeightParameters[ParameterIdx];
			const FMaterialParameterInfo& ParameterInfo = OutParameterInfo[ParameterIdx];
			FGuid ExpressionId = Guids[ParameterIdx];
			int32 WeightmapIndex = INDEX_NONE;

			ParentParameter.bOverride = false;
			ParentParameter.ParameterInfo = ParameterInfo;
			// Get the settings from the parent in the MIC chain
			Material->Parent->GetTerrainLayerWeightParameterValue(ParameterInfo, ParentParameter.WeightmapIndex, ExpressionId);
			ParentParameter.ExpressionGUID = ExpressionId;

			// If the SourceInstance is overriding this parameter, use its settings
			for (int32 WeightParamIdx = 0; WeightParamIdx < Material->GetStaticParameters().TerrainLayerWeightParameters.Num(); WeightParamIdx++)
			{
				const FStaticTerrainLayerWeightParameter &TerrainLayerWeightParam = Material->GetStaticParameters().TerrainLayerWeightParameters[WeightParamIdx];

				if (ParameterInfo == TerrainLayerWeightParam.ParameterInfo)
				{
					ParentParameter.bOverride = TerrainLayerWeightParam.bOverride;
					if (TerrainLayerWeightParam.bOverride)
					{
						ParentParameter.WeightmapIndex = TerrainLayerWeightParam.WeightmapIndex;
						ParentParameter.bWeightBasedBlend = TerrainLayerWeightParam.bWeightBasedBlend;
					}
				}
			}
		}
	}
}

bool LandscapeMaterialsParameterSetUpdater(FStaticParameterSet& StaticParameterSet, UMaterial* ParentMaterial)
{
	return UpdateParameterSet<FStaticTerrainLayerWeightParameter, UMaterialExpressionLandscapeLayerWeight>(StaticParameterSet.TerrainLayerWeightParameters, ParentMaterial);
}

bool ALandscapeProxy::ShouldTickIfViewportsOnly() const
{
	return true;
}

void ALandscapeProxy::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
#if WITH_EDITOR
	// editor-only
	UWorld* World = GetWorld();
	if (GIsEditor && World && !World->IsPlayInEditor())
	{
		UpdateBakedTextures();
	}
#endif

	// Tick grass even while paused or in the editor
	if (GIsEditor || bHasLandscapeGrass)
	{
		TickGrass();
	}

	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
}

ALandscapeProxy::~ALandscapeProxy()
{
	for (int32 Index = 0; Index < AsyncFoliageTasks.Num(); Index++)
	{
		FAsyncTask<FAsyncGrassTask>* Task = AsyncFoliageTasks[Index];
		Task->EnsureCompletion(true);
		FAsyncGrassTask& Inner = Task->GetTask();
		delete Task;
	}
	AsyncFoliageTasks.Empty();

#if WITH_EDITOR
	TotalComponentsNeedingGrassMapRender -= NumComponentsNeedingGrassMapRender;
	NumComponentsNeedingGrassMapRender = 0;
	TotalTexturesToStreamForVisibleGrassMapRender -= NumTexturesToStreamForVisibleGrassMapRender;
	NumTexturesToStreamForVisibleGrassMapRender = 0;
#endif
}

//
// ALandscapeMeshProxyActor
//
ALandscapeMeshProxyActor::ALandscapeMeshProxyActor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bCanBeDamaged = false;

	LandscapeMeshProxyComponent = CreateDefaultSubobject<ULandscapeMeshProxyComponent>(TEXT("LandscapeMeshProxyComponent0"));
	LandscapeMeshProxyComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	LandscapeMeshProxyComponent->Mobility = EComponentMobility::Static;
	LandscapeMeshProxyComponent->SetGenerateOverlapEvents(false);

	RootComponent = LandscapeMeshProxyComponent;
}

//
// ULandscapeMeshProxyComponent
//
ULandscapeMeshProxyComponent::ULandscapeMeshProxyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULandscapeMeshProxyComponent::InitializeForLandscape(ALandscapeProxy* Landscape, int8 InProxyLOD)
{
	LandscapeGuid = Landscape->GetLandscapeGuid();

	for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
	{
		if (Component)
		{
			ProxyComponentBases.Add(Component->GetSectionBase() / Component->ComponentSizeQuads);
		}
	}

	if (InProxyLOD != INDEX_NONE)
	{
		ProxyLOD = FMath::Clamp<int32>(InProxyLOD, 0, FMath::CeilLogTwo(Landscape->SubsectionSizeQuads + 1) - 1);
	}
}

#if WITH_EDITOR
void ULandscapeComponent::SerializeStateHashes(FArchive& Ar)
{
	if (GetMaterialInstance(0, false))
	{
		UMaterialInterface::TMicRecursionGuard RecursionGuard;
		FGuid LocalStateId = MaterialInstances[0]->GetMaterial_Concurrent(RecursionGuard)->StateId;
		Ar << LocalStateId;
	}

	FGuid HeightmapGuid = HeightmapTexture->Source.GetId();
	Ar << HeightmapGuid;
	for (auto WeightmapTexture : WeightmapTextures)
	{
		FGuid WeightmapGuid = WeightmapTexture->Source.GetId();
		Ar << WeightmapGuid;
	}

	int32 OccluderGeometryLOD = GetLandscapeProxy()->OccluderGeometryLOD;
	Ar << OccluderGeometryLOD;

	// Take into account the Heightmap offset per component
	Ar << HeightmapScaleBias.Z;
	Ar << HeightmapScaleBias.W;
}

void ALandscapeProxy::UpdateBakedTextures()
{
	// See if we can render
	UWorld* World = GetWorld();
	if (!GIsEditor || GUsingNullRHI || !World || World->IsGameWorld() || World->FeatureLevel < ERHIFeatureLevel::SM4)
	{
		return;
	}

	if (UpdateBakedTexturesCountdown-- > 0)
	{
		return;
	}

	// Check if we can want to generate landscape GI data
	static const auto DistanceFieldCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
	static const auto LandscapeGICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateLandscapeGIData"));
	if (DistanceFieldCVar->GetValueOnGameThread() == 0 || LandscapeGICVar->GetValueOnGameThread() == 0)
	{
		// Clear out any existing GI textures
		for (ULandscapeComponent* Component : LandscapeComponents)
		{
			if (Component != nullptr && Component->GIBakedBaseColorTexture != nullptr)
			{
				Component->BakedTextureMaterialGuid.Invalidate();
				Component->GIBakedBaseColorTexture = nullptr;
				Component->MarkRenderStateDirty();
			}
		}

		// Don't check if we need to update anything for another 60 frames
		UpdateBakedTexturesCountdown = 60;

		return;
	}
	
	// Stores the components and their state hash data for a single atlas
	struct FBakedTextureSourceInfo
	{
		// pointer as FMemoryWriter caches the address of the FBufferArchive, and this struct could be relocated on a realloc.
		TUniquePtr<FBufferArchive> ComponentStateAr;
		TArray<ULandscapeComponent*> Components;

		FBakedTextureSourceInfo()
		{
			ComponentStateAr = MakeUnique<FBufferArchive>();
		}
	};

	// Group components by heightmap texture
	TMap<UTexture2D*, FBakedTextureSourceInfo> ComponentsByHeightmap;
	for (ULandscapeComponent* Component : LandscapeComponents)
	{
		if (Component == nullptr)
		{
			continue;
		}

		FBakedTextureSourceInfo& Info = ComponentsByHeightmap.FindOrAdd(Component->HeightmapTexture);
		Info.Components.Add(Component);
		Component->SerializeStateHashes(*Info.ComponentStateAr);
	}

	TotalComponentsNeedingTextureBaking -= NumComponentsNeedingTextureBaking;
	NumComponentsNeedingTextureBaking = 0;
	int32 NumGenerated = 0;

	for (auto It = ComponentsByHeightmap.CreateConstIterator(); It; ++It)
	{
		const FBakedTextureSourceInfo& Info = It.Value();

		bool bCanBake = true;
		for (ULandscapeComponent* Component : Info.Components)
		{
			// not registered; ignore this component
			if (!Component->SceneProxy)
			{
				continue;
			}

			// Check we can render the material
			UMaterialInstance* MaterialInstance = Component->GetMaterialInstance(0, false);
			if (!MaterialInstance)
			{
				// Cannot render this component yet as it doesn't have a material; abandon the atlas for this heightmap
				bCanBake = false;
				break;
			}

			FMaterialResource* MaterialResource = MaterialInstance->GetMaterialResource(World->FeatureLevel);
			if (!MaterialResource || !MaterialResource->HasValidGameThreadShaderMap())
			{
				// Cannot render this component yet as its shaders aren't compiled; abandon the atlas for this heightmap
				bCanBake = false;
				break;
			}
		}

		if (bCanBake)
		{
			// Calculate a combined Guid-like ID we can use for this component
			uint32 Hash[5];
			FSHA1::HashBuffer(Info.ComponentStateAr->GetData(), Info.ComponentStateAr->Num(), (uint8*)Hash);
			FGuid CombinedStateId = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);

			bool bNeedsBake = false;
			for (ULandscapeComponent* Component : Info.Components)
			{
				if (Component->BakedTextureMaterialGuid != CombinedStateId)
				{
					bNeedsBake = true;
					break;
				}
			}
			
			if (bNeedsBake)
			{
				// We throttle, baking only one atlas per frame
				if (NumGenerated > 0)
				{
					NumComponentsNeedingTextureBaking += Info.Components.Num();
				}
				else
				{
					UTexture2D* HeightmapTexture = It.Key();
					// 1/8 the res of the heightmap
					FIntPoint AtlasSize(HeightmapTexture->GetSizeX() >> 3, HeightmapTexture->GetSizeY() >> 3);

					TArray<FColor> AtlasSamples;
					AtlasSamples.AddZeroed(AtlasSize.X * AtlasSize.Y);

					for (ULandscapeComponent* Component : Info.Components)
					{
						// not registered; ignore this component
						if (!Component->SceneProxy)
						{
							continue;
						}

						int32 ComponentSamples = (SubsectionSizeQuads + 1) * NumSubsections;
						check(FMath::IsPowerOfTwo(ComponentSamples));

						int32 BakeSize = ComponentSamples >> 3;
						TArray<FColor> Samples;
						if (FMaterialUtilities::ExportBaseColor(Component, BakeSize, Samples))
						{
							int32 AtlasOffsetX = FMath::RoundToInt(Component->HeightmapScaleBias.Z * (float)HeightmapTexture->GetSizeX()) >> 3;
							int32 AtlasOffsetY = FMath::RoundToInt(Component->HeightmapScaleBias.W * (float)HeightmapTexture->GetSizeY()) >> 3;
							for (int32 y = 0; y < BakeSize; y++)
							{
								FMemory::Memcpy(&AtlasSamples[(y + AtlasOffsetY)*AtlasSize.X + AtlasOffsetX], &Samples[y*BakeSize], sizeof(FColor)* BakeSize);
							}
							NumGenerated++;
						}
					}
					UTexture2D* AtlasTexture = FMaterialUtilities::CreateTexture(GetOutermost(), HeightmapTexture->GetName() + TEXT("_BaseColor"), AtlasSize, AtlasSamples, TC_Default, TEXTUREGROUP_World, RF_NoFlags, true, CombinedStateId);
					AtlasTexture->MarkPackageDirty();

					for (ULandscapeComponent* Component : Info.Components)
					{
						Component->BakedTextureMaterialGuid = CombinedStateId;
						Component->GIBakedBaseColorTexture = AtlasTexture;
						Component->MarkRenderStateDirty();
					}
				}
			}
		}
	}

	TotalComponentsNeedingTextureBaking += NumComponentsNeedingTextureBaking;

	if (NumGenerated == 0)
	{
		// Don't check if we need to update anything for another 60 frames
		UpdateBakedTexturesCountdown = 60;
	}
}
#endif

void ALandscapeProxy::InvalidateGeneratedComponentData(const TSet<ULandscapeComponent*>& Components)
{
	TMap<ALandscapeProxy*, TSet<ULandscapeComponent*>> ByProxy;
	for (auto Component : Components)
	{
		Component->BakedTextureMaterialGuid.Invalidate();

		ByProxy.FindOrAdd(Component->GetLandscapeProxy()).Add(Component);
	}
	for (auto It = ByProxy.CreateConstIterator(); It; ++It)
	{
		It.Key()->FlushGrassComponents(&It.Value());
	}
}

#undef LOCTEXT_NAMESPACE
