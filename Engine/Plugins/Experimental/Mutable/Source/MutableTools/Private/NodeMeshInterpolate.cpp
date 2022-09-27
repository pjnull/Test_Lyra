// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeMeshInterpolate.h"
#include "NodeMeshInterpolatePrivate.h"

#include "NodeScalar.h"

#include "MeshPrivate.h"


#define NODE_INPUT_COUNT 	4


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeMeshInterpolate::Private::s_type =
			NODE_TYPE( "MeshInterpolate", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeMeshInterpolate, EType::Interpolate, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeMeshInterpolate::GetInputCount() const
	{
		return 1 + (int)m_pD->m_targets.size();
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeMeshInterpolate::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

		switch (i)
		{
		case 0:
			pResult = m_pD->m_pFactor.get();
			break;

		default:
			pResult = m_pD->m_targets[i-1].get();
			break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->m_pFactor = dynamic_cast<NodeScalar*>(pNode.get());
			break;

		default:
			m_pD->m_targets[i-1] = dynamic_cast<NodeMesh*>(pNode.get());
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeMeshInterpolate::GetFactor() const
	{
		return m_pD->m_pFactor.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetFactor( NodeScalarPtr pNode )
	{
		m_pD->m_pFactor = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeMeshPtr NodeMeshInterpolate::GetTarget( int t ) const
	{
		check( t>=0 && t<(int)m_pD->m_targets.size() );

		return m_pD->m_targets[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetTarget( int t, NodeMeshPtr pNode )
	{
		check( t>=0 && t<(int)m_pD->m_targets.size() );
		m_pD->m_targets[t] = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetTargetCount( int t )
	{
		m_pD->m_targets.resize(t);
	}


	//---------------------------------------------------------------------------------------------
	int NodeMeshInterpolate::GetTargetCount() const
	{
		return (int)m_pD->m_targets.size();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetChannelCount( int t )
	{
		m_pD->m_channels.resize(t);
	}


	//---------------------------------------------------------------------------------------------
	void NodeMeshInterpolate::SetChannel( int i, MESH_BUFFER_SEMANTIC semantic, int semanticIndex )
	{
		check( i>=0 && i<(int)m_pD->m_channels.size() );

		if ( i>=0 && i<(int)m_pD->m_channels.size() )
		{
			m_pD->m_channels[i].semantic = semantic;
			m_pD->m_channels[i].semanticIndex = semanticIndex;
		}
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeMeshInterpolate::Private::GetLayout( int index ) const
	{
		NodeLayoutPtr pResult;

		// TODO: Substract layouts too? Usually they are ignored.
		if ( m_targets.size()>0 && m_targets[0] )
		{
			NodeMesh::Private* pPrivate =
					dynamic_cast<NodeMesh::Private*>( m_targets[0]->GetBasePrivate() );

			pResult = pPrivate->GetLayout( index );
		}

		return pResult;
	}


}

#undef NODE_INPUT_COUNT