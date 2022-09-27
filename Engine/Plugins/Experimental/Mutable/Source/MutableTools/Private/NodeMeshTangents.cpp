// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeMeshTangents.h"
#include "NodeMeshTangentsPrivate.h"
#include "MeshPrivate.h"


#define NODE_INPUT_COUNT 	1


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshTangents::Private::s_type =
			NODE_TYPE( "MeshTangents", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshTangents, EType::Tangents, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshTangents::GetInputCount() const
	{
		return NODE_INPUT_COUNT;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshTangents::GetInputNode( int i ) const
	{
		check( i>=0 && i<NODE_INPUT_COUNT );
        (void)i;
		return m_pD->m_pSource.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTangents::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<NODE_INPUT_COUNT );
        (void)i;
        if (i==0)
		{
			m_pD->m_pSource = dynamic_cast<NodeMesh*>( pNode.get() );
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeMeshPtr NodeMeshTangents::GetSource() const
	{
		return m_pD->m_pSource;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshTangents::SetSource( NodeMesh* p )
	{
		m_pD->m_pSource = p;
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshTangents::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		if ( m_pSource )
		{
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>( m_pSource->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}

#undef NODE_INPUT_COUNT