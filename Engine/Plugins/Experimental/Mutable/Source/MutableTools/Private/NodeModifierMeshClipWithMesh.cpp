// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeModifierMeshClipWithMesh.h"
#include "NodeModifierMeshClipWithMeshPrivate.h"
#include "MeshPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeModifierMeshClipWithMesh::Private::s_type =
            NODE_TYPE( "NodeModifierMeshClipWithMesh", NodeModifier::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeModifierMeshClipWithMesh, EType::MeshClipWithMesh, Node, Node::EType::Modifier)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeModifierMeshClipWithMesh::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeModifierMeshClipWithMesh::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());
        (void)i;
        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    void NodeModifierMeshClipWithMesh::SetInputNode( int i, NodePtr )
	{
		check( i>=0 && i< GetInputCount());
        (void)i;
    }


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipWithMesh::SetClipMesh(NodeMesh* clipMesh)
	{
		m_pD->m_clipMesh = clipMesh;
	}

}