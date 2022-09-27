// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeMeshTransform.h"
#include "NodeMeshTransformPrivate.h"
#include "MeshPrivate.h"


#define NODE_INPUT_COUNT 	1


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeMeshTransform::Private::s_type =
            NODE_TYPE( "MeshTransform", NodeMesh::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeMeshTransform, EType::Transform, Node, Node::EType::Mesh)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeMeshTransform::GetInputCount() const
	{
		return NODE_INPUT_COUNT;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeMeshTransform::GetInputNode( int i ) const
	{
		check( i>=0 && i<NODE_INPUT_COUNT );
        (void)i;
        return m_pD->m_pSource.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeMeshTransform::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<NODE_INPUT_COUNT );
		if (i==0)
		{
			m_pD->m_pSource = dynamic_cast<NodeMesh*>( pNode.get() );
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeMeshTransform::GetSource() const
	{
		return m_pD->m_pSource;
	}


    //---------------------------------------------------------------------------------------------
    void NodeMeshTransform::SetSource( NodeMesh* p )
    {
        m_pD->m_pSource = p;
    }


    //---------------------------------------------------------------------------------------------
    void NodeMeshTransform::SetTransform( const float* mat )
    {
        memcpy( &m_pD->m_transform[0][0], mat, 16*sizeof(float) );
    }


    //---------------------------------------------------------------------------------------------
    void NodeMeshTransform::GetTransform( float* mat ) const
    {
        memcpy( mat, &m_pD->m_transform[0][0], 16*sizeof(float) );
    }


	//---------------------------------------------------------------------------------------------
    NodeLayoutPtr NodeMeshTransform::Private::GetLayout( int index ) const
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