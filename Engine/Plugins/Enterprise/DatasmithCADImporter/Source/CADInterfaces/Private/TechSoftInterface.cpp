// Copyright Epic Games, Inc. All Rights Reserved.

#define INITIALIZE_A3D_API

#include "TechSoftInterface.h"

#include "CADInterfacesModule.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "TUniqueTechSoftObj.h"


namespace CADLibrary
{
	FTechSoftInterface& GetTechSoftInterface()
	{
		static FTechSoftInterface TechSoftInterface;
		return TechSoftInterface;
	}

	bool TECHSOFT_InitializeKernel(const TCHAR* InEnginePluginsPath)
	{
		return GetTechSoftInterface().InitializeKernel(InEnginePluginsPath);
	}

	bool FTechSoftInterface::InitializeKernel(const TCHAR* InEnginePluginsPath)
	{
#ifdef USE_TECHSOFT_SDK
		if (bIsInitialize)
		{
			return true;
		}

		FString EnginePluginsPath(InEnginePluginsPath);
		if (EnginePluginsPath.IsEmpty())
		{
			EnginePluginsPath = FPaths::EnginePluginsDir();
		}

		FString TechSoftDllPath = FPaths::Combine(EnginePluginsPath, TEXT("Enterprise/DatasmithCADImporter"), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory(), "TechSoft");
		TechSoftDllPath = FPaths::ConvertRelativePathToFull(TechSoftDllPath);
		ExchangeLoader = MakeUnique<A3DSDKHOOPSExchangeLoader>(*TechSoftDllPath);

		const A3DStatus IRet = ExchangeLoader->m_eSDKStatus;
		if (IRet != A3D_SUCCESS)
		{
			UE_LOG(CADInterfaces, Warning, TEXT("Failed to load required library in %s. Plug-in will not be functional."), *TechSoftDllPath);
		}
		else
		{
			bIsInitialize = true;
		}
		return bIsInitialize;
#else
		return false;
#endif
	}

#ifdef USE_TECHSOFT_SDK

	A3DStatus FTechSoftInterface::Import(const A3DImport& Import)
	{
		return ExchangeLoader->Import(Import);
	}

	A3DAsmModelFile* FTechSoftInterface::GetModelFile()
	{
		return ExchangeLoader->m_psModelFile;
	}


	void TUniqueTSObj<A3DAsmModelFileData>::InitializeData(A3DAsmModelFileData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DAsmModelFileData, OutData);
	}

	void TUniqueTSObj<A3DAsmPartDefinitionData>::InitializeData(A3DAsmPartDefinitionData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DAsmPartDefinitionData, OutData);
	}

	void TUniqueTSObj<A3DAsmProductOccurrenceData>::InitializeData(A3DAsmProductOccurrenceData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DAsmProductOccurrenceData, OutData);
	}

	void TUniqueTSObj<A3DAsmProductOccurrenceDataCV5>::InitializeData(A3DAsmProductOccurrenceDataCV5& OutData)
	{
		A3D_INITIALIZE_DATA(A3DAsmProductOccurrenceDataCV5, OutData);
	}

	void TUniqueTSObj<A3DAsmProductOccurrenceDataSLW>::InitializeData(A3DAsmProductOccurrenceDataSLW& OutData)
	{
		A3D_INITIALIZE_DATA(A3DAsmProductOccurrenceDataSLW, OutData);
	}

	void TUniqueTSObj<A3DAsmProductOccurrenceDataUg>::InitializeData(A3DAsmProductOccurrenceDataUg& OutData)
	{
		A3D_INITIALIZE_DATA(A3DAsmProductOccurrenceDataUg, OutData);
	}

	void TUniqueTSObj<A3DGraphicsData>::InitializeData(A3DGraphicsData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DGraphicsData, OutData);
	}

	void TUniqueTSObj<A3DMiscAttributeData>::InitializeData(A3DMiscAttributeData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DMiscAttributeData, OutData);
	}

	void TUniqueTSObj<A3DMiscCartesianTransformationData>::InitializeData(A3DMiscCartesianTransformationData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DMiscCartesianTransformationData, OutData);
	}

	void TUniqueTSObj<A3DMiscGeneralTransformationData>::InitializeData(A3DMiscGeneralTransformationData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DMiscGeneralTransformationData, OutData);
	}

	void TUniqueTSObj<A3DMiscMaterialPropertiesData>::InitializeData(A3DMiscMaterialPropertiesData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DMiscMaterialPropertiesData, OutData);
	}

	void TUniqueTSObj<A3DRiBrepModelData>::InitializeData(A3DRiBrepModelData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DRiBrepModelData, OutData);
	}

	void TUniqueTSObj<A3DRiCoordinateSystemData>::InitializeData(A3DRiCoordinateSystemData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DRiCoordinateSystemData, OutData);
	}

	void TUniqueTSObj<A3DRiDirectionData>::InitializeData(A3DRiDirectionData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DRiDirectionData, OutData);
	}

	void TUniqueTSObj<A3DRiPolyBrepModelData>::InitializeData(A3DRiPolyBrepModelData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DRiPolyBrepModelData, OutData);
	}

	void TUniqueTSObj<A3DRiRepresentationItemData>::InitializeData(A3DRiRepresentationItemData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DRiRepresentationItemData, OutData);
	}

	void TUniqueTSObj<A3DRiSetData>::InitializeData(A3DRiSetData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DRiSetData, OutData);
	}

	void TUniqueTSObj<A3DRootBaseData>::InitializeData(A3DRootBaseData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DRootBaseData, OutData);
	}

	void TUniqueTSObj<A3DRootBaseWithGraphicsData>::InitializeData(A3DRootBaseWithGraphicsData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DRootBaseWithGraphicsData, OutData);
	}

	void TUniqueTSObj<A3DTess3DData>::InitializeData(A3DTess3DData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DTess3DData, OutData);
	}

	void TUniqueTSObj<A3DTessBaseData>::InitializeData(A3DTessBaseData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DTessBaseData, OutData);
	}

	void TUniqueTSObj<A3DTopoBrepDataData>::InitializeData(A3DTopoBrepDataData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DTopoBrepDataData, OutData);
	}

	void TUniqueTSObj<A3DTopoEdgeData>::InitializeData(A3DTopoEdgeData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DTopoEdgeData, OutData);
	}

	void TUniqueTSObj<A3DTopoFaceData>::InitializeData(A3DTopoFaceData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DTopoFaceData, OutData);
	}

	void TUniqueTSObj<A3DTopoUniqueVertexData>::InitializeData(A3DTopoUniqueVertexData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DTopoUniqueVertexData, OutData);
	}

	void TUniqueTSObj<A3DTopoWireEdgeData>::InitializeData(A3DTopoWireEdgeData& OutData)
	{
		A3D_INITIALIZE_DATA(A3DTopoWireEdgeData, OutData);
	}

	A3DStatus TUniqueTSObj<A3DAsmModelFileData>::GetData(const A3DAsmModelFile* InAsmModelFilePtr, A3DAsmModelFileData& OutData)
	{
		return A3DAsmModelFileGet(InAsmModelFilePtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DAsmPartDefinitionData>::GetData(const A3DAsmPartDefinition* InAsmPartDefinitionPtr, A3DAsmPartDefinitionData& OutData)
	{
		return A3DAsmPartDefinitionGet(InAsmPartDefinitionPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DAsmProductOccurrenceData>::GetData(const A3DAsmProductOccurrence* InAsmProductOccurrencePtr, A3DAsmProductOccurrenceData& OutData)
	{
		return A3DAsmProductOccurrenceGet(InAsmProductOccurrencePtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DAsmProductOccurrenceDataCV5>::GetData(const A3DAsmProductOccurrence* InAsmProductOccurrencePtr, A3DAsmProductOccurrenceDataCV5& OutData)
	{
		return A3DAsmProductOccurrenceGetCV5(InAsmProductOccurrencePtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DAsmProductOccurrenceDataSLW>::GetData(const A3DAsmProductOccurrence* InAsmProductOccurrencePtr, A3DAsmProductOccurrenceDataSLW& OutData)
	{
		return A3DAsmProductOccurrenceGetSLW(InAsmProductOccurrencePtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DAsmProductOccurrenceDataUg>::GetData(const A3DAsmProductOccurrence* InAsmProductOccurrencePtr, A3DAsmProductOccurrenceDataUg& OutData)
	{
		return A3DAsmProductOccurrenceGetUg(InAsmProductOccurrencePtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DGraphicsData>::GetData(const A3DGraphics* InGraphicsPtr, A3DGraphicsData& OutData)
	{
		return A3DGraphicsGet(InGraphicsPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DMiscAttributeData>::GetData(const A3DMiscAttribute* InMiscAttributePtr, A3DMiscAttributeData& OutData)
	{
		return A3DMiscAttributeGet(InMiscAttributePtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DMiscCartesianTransformationData>::GetData(const A3DMiscCartesianTransformation* InMiscCartesianTransformationPtr, A3DMiscCartesianTransformationData& OutData)
	{
		return A3DMiscCartesianTransformationGet(InMiscCartesianTransformationPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DMiscGeneralTransformationData>::GetData(const A3DMiscGeneralTransformation* InMiscGeneralTransformationPtr, A3DMiscGeneralTransformationData& OutData)
	{
		return A3DMiscGeneralTransformationGet(InMiscGeneralTransformationPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DMiscMaterialPropertiesData>::GetData(const A3DEntity* InMiscMaterialPropertiesPtr, A3DMiscMaterialPropertiesData& OutData)
	{
		return A3DMiscGetMaterialProperties(InMiscMaterialPropertiesPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DRiBrepModelData>::GetData(const A3DRiBrepModel* InRiBrepModelPtr, A3DRiBrepModelData& OutData)
	{
		return A3DRiBrepModelGet(InRiBrepModelPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DRiCoordinateSystemData>::GetData(const A3DRiCoordinateSystem* InRiCoordinateSystemPtr, A3DRiCoordinateSystemData& OutData)
	{
		return A3DRiCoordinateSystemGet(InRiCoordinateSystemPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DRiDirectionData>::GetData(const A3DRiDirection* InRiDirectionPtr, A3DRiDirectionData& OutData)
	{
		return A3DRiDirectionGet(InRiDirectionPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DRiPolyBrepModelData>::GetData(const A3DRiPolyBrepModel* InRiPolyBrepModelPtr, A3DRiPolyBrepModelData& OutData)
	{
		return A3DRiPolyBrepModelGet(InRiPolyBrepModelPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DRiRepresentationItemData>::GetData(const A3DRiRepresentationItem* InRiRepresentationItemPtr, A3DRiRepresentationItemData& OutData)
	{
		return A3DRiRepresentationItemGet(InRiRepresentationItemPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DRiSetData>::GetData(const A3DRiSet* InRiSetPtr, A3DRiSetData& OutData)
	{
		return A3DRiSetGet(InRiSetPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DRootBaseData>::GetData(const A3DRootBase* InRootBasePtr, A3DRootBaseData& OutData)
	{
		return A3DRootBaseGet(InRootBasePtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DRootBaseWithGraphicsData>::GetData(const A3DRootBaseWithGraphics* InRootBaseWithGraphicsPtr, A3DRootBaseWithGraphicsData& OutData)
	{
		return A3DRootBaseWithGraphicsGet(InRootBaseWithGraphicsPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DTess3DData>::GetData(const A3DTess3D* InTess3DPtr, A3DTess3DData& OutData)
	{
		return A3DTess3DGet(InTess3DPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DTessBaseData>::GetData(const A3DTessBase* InTessBasePtr, A3DTessBaseData& OutData)
	{
		return A3DTessBaseGet(InTessBasePtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DTopoBrepDataData>::GetData(const A3DTopoBrepData* InTopoBrepDataPtr, A3DTopoBrepDataData& OutData)
	{
		return A3DTopoBrepDataGet(InTopoBrepDataPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DTopoEdgeData>::GetData(const A3DTopoEdge* InTopoEdgePtr, A3DTopoEdgeData& OutData)
	{
		return A3DTopoEdgeGet(InTopoEdgePtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DTopoFaceData>::GetData(const A3DTopoFace* InTopoFacePtr, A3DTopoFaceData& OutData)
	{
		return A3DTopoFaceGet(InTopoFacePtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DTopoUniqueVertexData>::GetData(const A3DTopoUniqueVertex* InTopoUniqueVertexPtr, A3DTopoUniqueVertexData& OutData)
	{
		return A3DTopoUniqueVertexGet(InTopoUniqueVertexPtr, &OutData);
	}

	A3DStatus TUniqueTSObj<A3DTopoWireEdgeData>::GetData(const A3DTopoWireEdge* InTopoWireEdgePtr, A3DTopoWireEdgeData& OutData)
	{
		return A3DTopoWireEdgeGet(InTopoWireEdgePtr, &OutData);
	}
#endif

}
