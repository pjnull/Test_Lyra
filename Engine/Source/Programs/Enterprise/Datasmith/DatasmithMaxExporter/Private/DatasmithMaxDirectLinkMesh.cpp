// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NEW_DIRECTLINK_PLUGIN

#include "DatasmithMaxDirectLink.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithMesh.h"
#include "DatasmithUtils.h"

#include "Logging/LogMacros.h"


#include "Windows/AllowWindowsPlatformTypes.h"
MAX_INCLUDES_START
	#include "max.h"

	#include "MeshNormalSpec.h"
MAX_INCLUDES_END

namespace DatasmithMaxDirectLink
{

class FNullView : public View
{
public:
	FNullView()
	{
		worldToView.IdentityMatrix(); screenW = 640.0f; screenH = 480.0f;
	}

	virtual Point2 ViewToScreen(Point3 p) override
	{
		return Point2(p.x, p.y);
	}
};


Mesh* GetMeshForGeomObject(INode* Node, Object* Obj, bool& bOutNeedsDelete)
{
	// todo: baseline exporter uses GetBaseObject which takes result of EvalWorldState
	// and searched down DerivedObject pipeline(by taking GetObjRef) 
	// This is STRANGE as EvalWorldState shouldn't return DerivedObject in the first place(it should return result of pipeline evaluation)

	GeomObject* GeomObj = dynamic_cast<GeomObject*>(Obj);

	FNullView View;
	TimeValue Time = GetCOREInterface()->GetTime();
	BOOL bNeedsDelete;
	Mesh* Result = GeomObj->GetRenderMesh(Time, Node, View, bNeedsDelete);

	bOutNeedsDelete = bNeedsDelete;

	return Result;
}

bool ConvertGeomNodeToDatasmithMesh(ISceneTracker& Scene, TSharedPtr<IDatasmithMeshElement>& DatasmithMeshElement, FString MeshName, INode* Node, Object* Obj, TSet<uint16>& SupportedChannels)
{
	bool bNeedsDelete;
	Mesh* RenderMesh = GetMeshForGeomObject(Node, Obj, bNeedsDelete);

	if (!RenderMesh)
	{
		return false;
	}

	return ConvertMaxMeshToDatasmith(Scene, DatasmithMeshElement, Node, *MeshName, RenderMesh, SupportedChannels, bNeedsDelete); // export might produce anything(e.g. if mesh is empty)
}




// todo: copied from baseline plugin(it has dependencies on converters that are not static in FDatasmithMaxMeshExporter)
void FillDatasmithMeshFromMaxMesh(FDatasmithMesh& DatasmithMesh, Mesh& MaxMesh, INode* ExportedNode, bool bForceSingleMat, TSet<uint16>& SupportedChannels, const TCHAR* MeshName, FTransform Pivot)
{
	FDatasmithConverter Converter;

	const int NumFaces = MaxMesh.getNumFaces();
	const int NumVerts = MaxMesh.getNumVerts();

	DatasmithMesh.SetVerticesCount(NumVerts);
	DatasmithMesh.SetFacesCount(NumFaces);

	// Vertices
	for (int i = 0; i < NumVerts; i++)
	{
		Point3 Point = MaxMesh.getVert(i);

		FVector Vertex = Converter.toDatasmithVector(Point);
		Vertex = Pivot.TransformPosition(Vertex); // Bake object-offset in the mesh data when possible

		DatasmithMesh.SetVertex(i, Vertex.X, Vertex.Y, Vertex.Z);
	}

	// Vertex Colors
	if (MaxMesh.curVCChan == 0 && MaxMesh.numCVerts > 0)
	{
		// Default vertex color channel
		for (int32 i = 0; i < NumFaces; i++)
		{
			TVFace& Face = MaxMesh.vcFace[i];
			DatasmithMesh.SetVertexColor(i * 3, Converter.toDatasmithColor(MaxMesh.vertCol[Face.t[0]]));
			DatasmithMesh.SetVertexColor(i * 3 + 1, Converter.toDatasmithColor(MaxMesh.vertCol[Face.t[1]]));
			DatasmithMesh.SetVertexColor(i * 3 + 2, Converter.toDatasmithColor(MaxMesh.vertCol[Face.t[2]]));
		}
	}

	// UVs
	TMap<int32, int32> UVChannelsMap;
	TMap<uint32, int32> HashToChannel;
	bool bIsFirstUVChannelValid = true;

	for (int32 i = 1; i <= MaxMesh.getNumMaps(); ++i)
	{
		if (MaxMesh.mapSupport(i) == BOOL(true) && MaxMesh.getNumMapVerts(i) > 0)
		{
			DatasmithMesh.AddUVChannel();
			const int32 UVChannelIndex = DatasmithMesh.GetUVChannelsCount() - 1;
			const int32 UVsCount = MaxMesh.getNumMapVerts(i);

			DatasmithMesh.SetUVCount(UVChannelIndex, UVsCount);

			UVVert* Vertex = MaxMesh.mapVerts(i);

			for (int32 j = 0; j < UVsCount; ++j)
			{
				const UVVert& MaxUV = Vertex[j];
				DatasmithMesh.SetUV(UVChannelIndex, j, MaxUV.x, 1.f - MaxUV.y);
			}

			TVFace* Faces = MaxMesh.mapFaces(i);
			for (int32 j = 0; j < MaxMesh.getNumFaces(); ++j)
			{
				DatasmithMesh.SetFaceUV(j, UVChannelIndex, Faces[j].t[0], Faces[j].t[1], Faces[j].t[2]);
			}

			if (UVChannelIndex == 0)
			{
				//Verifying that the UVs are properly unfolded, which is required to calculate the tangent in unreal.
				bIsFirstUVChannelValid = FDatasmithMeshUtils::IsUVChannelValid(DatasmithMesh, UVChannelIndex);
			}

			uint32 Hash = DatasmithMesh.GetHashForUVChannel(UVChannelIndex);
			int32* PointerToChannel = HashToChannel.Find(Hash);

			if (PointerToChannel)
			{
				// Remove the channel because there is another one that is identical
				DatasmithMesh.RemoveUVChannel();

				// Map the user-specified UV Channel (in 3dsmax) to the actual UV channel that will be exported to Unreal
				UVChannelsMap.Add(i - 1, *PointerToChannel);
			}
			else
			{
				// Map the user-specified UV Channel (in 3dsmax) to the actual UV channel that will be exported to Unreal
				UVChannelsMap.Add(i - 1, UVChannelIndex);
				HashToChannel.Add(Hash, UVChannelIndex);
			}
		}
	}

	if (!bIsFirstUVChannelValid)
	{
		//DatasmithMaxLogger::Get().AddGeneralError(*FString::Printf(TEXT("%s's UV channel #1 contains degenerated triangles, this can cause issues in Unreal. It is recommended to properly unfold and flatten exported UV data.")
		//	, static_cast<const TCHAR*>(ExportedNode->GetName())));
	}

	if (MeshName != nullptr)
	{
		//MeshNamesToUVChannels.Add(MeshName, MoveTemp(UVChannelsMap));
	}

	// Faces
	for (int i = 0; i < NumFaces; i++)
	{
		// Create polygons. Assign texture and texture UV indices.
		// all faces of the cube have the same texture

		Face& MaxFace = MaxMesh.faces[i];
		int MaterialId = bForceSingleMat ? 0 : MaxFace.getMatID();

		SupportedChannels.Add(MaterialId);

		//Max's channel UI is not zero-based, so we register an incremented ChannelID for better visual consistency after importing in Unreal.
		DatasmithMesh.SetFace(i, MaxFace.getVert(0), MaxFace.getVert(1), MaxFace.getVert(2), MaterialId + 1);
		DatasmithMesh.SetFaceSmoothingMask(i, (uint32)MaxFace.getSmGroup());
	}

	//Normals

	MaxMesh.SpecifyNormals();
	MeshNormalSpec* Normal = MaxMesh.GetSpecifiedNormals();
	Normal->MakeNormalsExplicit(false);
	Normal->CheckNormals();

	Matrix3 RotationMatrix;
	RotationMatrix.IdentityMatrix();
	Quat ObjectOffsetRotation = ExportedNode->GetObjOffsetRot();
	RotateMatrix(RotationMatrix, ObjectOffsetRotation);

	Point3 Point;

	for (int i = 0; i < NumFaces; i++)
	{
		Point = Normal->GetNormal(i, 0).Normalize() * RotationMatrix;
		FVector NormalVector = Converter.toDatasmithVector(Point);
		DatasmithMesh.SetNormal(i * 3, NormalVector.X, NormalVector.Y, NormalVector.Z);

		Point = Normal->GetNormal(i, 1).Normalize() * RotationMatrix;
		NormalVector = Converter.toDatasmithVector(Point);
		DatasmithMesh.SetNormal(i * 3 + 1, NormalVector.X, NormalVector.Y, NormalVector.Z);

		Point = Normal->GetNormal(i, 2).Normalize() * RotationMatrix;
		NormalVector = Converter.toDatasmithVector(Point);
		DatasmithMesh.SetNormal(i * 3 + 2, NormalVector.X, NormalVector.Y, NormalVector.Z);
	}
}

// todo: paralelize calls to ExportToUObject 
bool ConvertMaxMeshToDatasmith(ISceneTracker& Scene, TSharedPtr<IDatasmithMeshElement>& DatasmithMeshElement, INode* Node, const TCHAR* MeshName, Mesh* RenderMesh, TSet<uint16>& SupportedChannels, bool bNeedsDelete)
{
	// Reset old mesh
	if (DatasmithMeshElement)
	{
		// todo: potential mesh reuse - when DatasmithMeshElement allows to reset materials(as well as other params)
		Scene.ReleaseMeshElement(DatasmithMeshElement);
	}

	bool bResult = false;
	if (RenderMesh->getNumFaces())
	{
		// Copy mesh to clean it before filling Datasmith mesh from it
		Mesh CachedMesh;
		CachedMesh.DeepCopy(RenderMesh, TOPO_CHANNEL | GEOM_CHANNEL | TEXMAP_CHANNEL | VERTCOLOR_CHANNEL);

		CachedMesh.DeleteIsoVerts();
		CachedMesh.RemoveDegenerateFaces();
		CachedMesh.RemoveIllegalFaces();

		// Need to invalidate/rebuild strips/edges after topology change(removing bad verts/faces)
		CachedMesh.InvalidateStrips();
		CachedMesh.BuildStripsAndEdges();

		if (CachedMesh.getNumFaces() > 0)
		{
			FDatasmithMesh DatasmithMesh;

			// todo: pivot
			FillDatasmithMeshFromMaxMesh(DatasmithMesh, CachedMesh, Node, false, SupportedChannels, MeshName, FTransform::Identity);


			DatasmithMeshElement = FDatasmithSceneFactory::CreateMesh(MeshName);

			Scene.AddMeshElement(DatasmithMeshElement, DatasmithMesh);

			bResult = true; // Set to true, don't care what ExportToUObject does here - we need to move it to a thread anyway

		}
		CachedMesh.FreeAll();
	}

	if (bNeedsDelete)
	{
		RenderMesh->DeleteThis();
	}

	return bResult;
}

}

#include "Windows/HideWindowsPlatformTypes.h"

#endif // NEW_DIRECTLINK_PLUGIN
