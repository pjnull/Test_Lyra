// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "Materials/MaterialExpressionLandscapeLayerWeight.h"
#include "Materials/MaterialExpressionLandscapeLayerSample.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionLandscapeLayerSwitch.h"

#if WITH_EDITOR


// ----------------------------------------------------------------------------------

LANDSCAPE_API FLandscapeImportLayerInfo::FLandscapeImportLayerInfo(const FLandscapeInfoLayerSettings& InLayerSettings)
	: LayerName(InLayerSettings.GetLayerName())
	, LayerInfo(InLayerSettings.LayerInfoObj)
	, SourceFilePath(InLayerSettings.GetEditorSettings().ReimportLayerFilePath)
{
}

// ----------------------------------------------------------------------------------
// 
// Static initialization : 
FGetLayersFromMaterialCache* FGetLayersFromMaterialCache::ActiveCache = nullptr;

TArray<FName> FGetLayersFromMaterialCache::GetLayersFromMaterial(const UMaterialInterface* InMaterialInterface)
{
	// If we have an active cache at the moment, let's benefit from it, otherwise, recompute the list on the spot:
	return (ActiveCache) ? ActiveCache->GetLayersFromMaterialInternal(InMaterialInterface) : ComputeLayersFromMaterial(InMaterialInterface);
}

TArray<FName> FGetLayersFromMaterialCache::GetLayersFromMaterialInternal(const UMaterialInterface* InMaterialInterface)
{
	TArray<FName> Result;
	// Have we computed the layers for this material already? 
	if (TArray<FName>* CachedResult = PerMaterialLayersCache.Find(InMaterialInterface))
	{
		Result = *CachedResult;
	}
	else if (InMaterialInterface != nullptr)
	{
		// Recompute the layers now and add it to our cache : 
		Result = PerMaterialLayersCache.Add(InMaterialInterface, ComputeLayersFromMaterial(InMaterialInterface));
	}

	return Result;
}

TArray<FName> FGetLayersFromMaterialCache::ComputeLayersFromMaterial(const UMaterialInterface* InMaterialInterface)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ALandscapeProxy::ComputeLayersFromMaterial);
	TArray<FName> Result;

	if (InMaterialInterface)
	{
		// TODO - Probably should add landscape layers to FMaterialExpressionCachedData, as this code will not properly discover layer names introduced in material layers on an MI
		if (const UMaterial* Material = InMaterialInterface->GetMaterial())
		{
			for (UMaterialExpression* Expression : Material->Expressions)
			{
				if (Expression)
				{
					Expression->GetLandscapeLayerNames(Result);
				}
			}
		}
	}

	return Result;
}

#endif // WITH_EDITOR
