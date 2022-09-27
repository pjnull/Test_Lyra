// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeMeshSubtract.h"
#include "NodeMeshSubtractPrivate.h"
#include "MeshPrivate.h"


#define NODE_INPUT_COUNT 	2


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshSubtract::Private::s_type =
			NODE_TYPE( "MeshSubtract", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshSubtract, EType::Subtract, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshSubtract::GetInputCount() const
	{
		return NODE_INPUT_COUNT;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshSubtract::GetInputNode( int i ) const
	{
		check( i>=0 && i<NODE_INPUT_COUNT );
		return i == 0 ? m_pD->m_pA.get() : m_pD->m_pB.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshSubtract::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<NODE_INPUT_COUNT );
		if (i==0)
		{
			m_pD->m_pA = dynamic_cast<NodeMesh*>( pNode.get() );
		}
		else
		{
			m_pD->m_pB = dynamic_cast<NodeMesh*>( pNode.get() );
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeMeshPtr NodeMeshSubtract::GetA() const
	{
		return m_pD->m_pA;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshSubtract::SetA( NodeMesh* p )
	{
		m_pD->m_pA = p;
	}


	//---------------------------------------------------------------------------------------------
	NodeMeshPtr NodeMeshSubtract::GetB() const
	{
		return m_pD->m_pB;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshSubtract::SetB( NodeMesh* p )
	{
		m_pD->m_pB = p;
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshSubtract::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		// TODO: Substract layouts too? Usually they are ignored.
		if ( m_pA )
		{
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>( m_pA->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}

#undef NODE_INPUT_COUNT