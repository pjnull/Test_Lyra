// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NodePrivate.h"
#include "NodeSurfaceNew.h"
#include "NodePatchImagePrivate.h"

#include "NodeMesh.h"
#include "NodePatchMesh.h"
#include "NodeImage.h"
#include "NodeScalar.h"
#include "NodeString.h"
#include "NodeColour.h"

#include "MemoryPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeSurfaceNew::Private : public Node::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		string m_name;
        uint32_t m_customID=0;

		struct MESH
		{
			string m_name;
			NodeMeshPtr m_pMesh;

			//!
			void Serialise( OutputArchive& arch ) const
			{
				arch << m_name;
				arch << m_pMesh;
			}

			void Unserialise( InputArchive& arch )
			{
				arch >> m_name;
				arch >> m_pMesh;
			}
		};

		vector<MESH> m_meshes;

		struct IMAGE
		{
			string m_name;
			string m_materialName;
			string m_materialParameterName;
			NodeImagePtr m_pImage;

			// It could be negative, to indicate no layout.
            int8_t m_layoutIndex = 0;

            //!
			void Serialise(OutputArchive& arch) const
			{
				arch << m_name;
				arch << m_materialName;
				arch << m_materialParameterName;
				arch << m_pImage;
                arch << m_layoutIndex;
            }

			void Unserialise(InputArchive& arch)
			{
				arch >> m_name;
				arch >> m_materialName;
				arch >> m_materialParameterName;
				arch >> m_pImage;
                arch >> m_layoutIndex;
            }
        };

		vector<IMAGE> m_images;

		struct VECTOR
		{
			string m_name;
			NodeColourPtr m_pVector;

			//!
			void Serialise(OutputArchive& arch) const
			{
				arch << m_name;
				arch << m_pVector;
			}

			void Unserialise(InputArchive& arch)
			{
				arch >> m_name;
				arch >> m_pVector;
			}
		};

		vector<VECTOR> m_vectors;

        struct SCALAR
        {
            string m_name;
            NodeScalarPtr m_pScalar;

            //!
            void Serialise(OutputArchive& arch) const
            {
                arch << m_name;
                arch << m_pScalar;
            }

            void Unserialise(InputArchive& arch)
            {
                arch >> m_name;
                arch >> m_pScalar;
            }
        };

        vector<SCALAR> m_scalars;

        struct STRING
        {
            string m_name;
            NodeStringPtr m_pString;

            //!
            void Serialise( OutputArchive& arch ) const
            {
                arch << m_name;
                arch << m_pString;
            }

            void Unserialise( InputArchive& arch )
            {
                arch >> m_name;
                arch >> m_pString;
            }
        };

        vector<STRING> m_strings;

        //! Tags in this surface
		vector<mu::string> m_tags;

		//! Find an image node index by name or return -1
		int FindImage( const char* strName ) const;

		//! Find a mesh node index by name or return -1
		int FindMesh(const char* strName) const;

        //! Find a vector node index by name or return -1
        int FindVector(const char* strName) const;

        //! Find a scalar node index by name or return -1
        int FindScalar( const char* strName ) const;

        //! Find a string node index by name or return -1
        int FindString( const char* strName ) const;

        //!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 7;
			arch << ver;

			arch << m_name;
            arch << m_customID;

			arch << m_meshes;
			arch << m_images;
			arch << m_tags;
            arch << m_vectors;
            arch << m_scalars;
            arch << m_strings;
        }

        //!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==7);

			arch >> m_name;
            arch >> m_customID;

			arch >> m_meshes;
			arch >> m_images;
            arch >> m_tags;
            arch >> m_vectors;
            arch >> m_scalars;
            arch >> m_strings;
        }
    };




}
