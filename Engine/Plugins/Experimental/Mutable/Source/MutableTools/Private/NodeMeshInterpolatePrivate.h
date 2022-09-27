// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NodeMeshInterpolate.h"
#include "NodeScalar.h"
#include "NodeMeshPrivate.h"
#include "AST.h"

#include "MeshPrivate.h"


namespace mu
{


	class NodeMeshInterpolate::Private : public NodeMesh::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeScalarPtr m_pFactor;
		vector<NodeMeshPtr> m_targets;

		//!
		struct CHANNEL
		{
			CHANNEL()
			{
				semantic = MBS_NONE;
				semanticIndex = 0;
			}

			CHANNEL( MESH_BUFFER_SEMANTIC asemantic,
                     int32_t asemanticIndex )
			{
				semantic = asemantic;
				semanticIndex = asemanticIndex;
			}

			MESH_BUFFER_SEMANTIC semantic;
            int32_t semanticIndex;

			//-------------------------------------------------------------------------------------
			void Serialise( OutputArchive& arch ) const
			{
                const int32_t ver = 0;
				arch << ver;

				arch << semantic;
				arch << semanticIndex;
			}

			//-------------------------------------------------------------------------------------
			void Unserialise( InputArchive& arch )
			{
                int32_t ver = 0;
				arch >> ver;
				check(ver == 0);

				arch >> semantic;
				arch >> semanticIndex;
			}
		};

		vector<CHANNEL> m_channels;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 1;
			arch << ver;

			arch << m_pFactor;
			arch << m_targets;
			arch << m_channels;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==1);

			arch >> m_pFactor;
			arch >> m_targets;

			if (ver>=1)
			{
				arch >> m_channels;
			}
		}

		// NodeMesh::Private interface
        NodeLayoutPtr GetLayout( int index ) const override;

	};


}