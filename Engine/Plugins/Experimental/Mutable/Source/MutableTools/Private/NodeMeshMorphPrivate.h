// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NodeMeshMorph.h"
#include "NodeScalar.h"
#include "NodeMeshPrivate.h"
#include "AST.h"

namespace mu
{


	class NodeMeshMorph::Private : public NodeMesh::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeScalarPtr m_pFactor;
		NodeMeshPtr m_pBase;
		vector<NodeMeshPtr> m_morphs;
        bool m_vertexIndicesAreRelative = false;
		
		bool m_reshapeSkeleton = false;
		bool m_reshapePhysicsVolumes = false;
		
		bool m_deformAllBones = false;
		bool m_deformAllPhysics = false;
		
		TArray<string> m_bonesToDeform;
		TArray<string> m_physicsToDeform;

        //!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 3;
			arch << ver;

			arch << m_pFactor;
			arch << m_pBase;
			arch << m_morphs;
            arch << m_vertexIndicesAreRelative;

			arch << m_reshapeSkeleton;
			arch << m_reshapePhysicsVolumes;
			arch << m_bonesToDeform;

			arch << m_deformAllBones;
			arch << m_deformAllPhysics;
			
			arch << m_physicsToDeform;

        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver <= 3);

			arch >> m_pFactor;
			arch >> m_pBase;
			arch >> m_morphs;
            arch >> m_vertexIndicesAreRelative;
	
			if (ver >= 2)
			{
				arch >> m_reshapeSkeleton;
				arch >> m_reshapePhysicsVolumes;
	
				// This repetition is needed, there was a bug where m_reshapePhysicsVolumes was serialized twice in ver 2.
				if (ver == 2) 
				{
					arch >> m_reshapePhysicsVolumes;
				}
				arch >> m_bonesToDeform;			
			}
			else
			{
				m_reshapeSkeleton = false;
				m_reshapePhysicsVolumes = false;
				m_bonesToDeform.Empty();
			}

			if (ver >= 3)
			{
				arch >> m_deformAllBones;
				arch >> m_deformAllPhysics;
				arch >> m_physicsToDeform;
			}
			else
			{
				m_deformAllBones = false;
				m_deformAllPhysics = false;
				m_physicsToDeform.Empty();
			}

		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};


}