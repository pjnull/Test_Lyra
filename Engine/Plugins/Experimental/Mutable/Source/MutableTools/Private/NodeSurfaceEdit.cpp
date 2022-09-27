// Copyright Epic Games, Inc. All Rights Reserved.


#include "MutableTools/Public/NodeSurfaceEdit.h"
#include "MutableTools/Private/NodeSurfaceEditPrivate.h"

#include "MutableTools/Public/NodeMesh.h"
#include "MutableTools/Public/NodeImage.h"
#include "MutableTools/Public/NodeScalar.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeSurfaceEdit::Private::s_type =
            NODE_TYPE( "EditSurface", NodeSurface::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeSurfaceEdit, EType::Edit, Node, Node::EType::Surface)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeSurfaceEdit::GetInputCount() const
	{
		return
			3 	// Extend mesh, morph, factor
			+
			(int)m_pD->m_textures.size()*2
			;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeSurfaceEdit::GetInputNode( int i ) const
	{
		check( i >=0 && i < GetInputCount() );

		Node* pResult = 0;

		if ( i==0 )
		{
			pResult = m_pD->m_pMesh.get();
		}
		else if ( i==1 )
		{
			pResult = m_pD->m_pMorph.get();
		}
		else if (i == 2)
		{
			pResult = m_pD->m_pFactor.get();
		}
		else
		{
			int idx = i - 3;

			if (idx%2 == 0)
			{
				pResult = m_pD->m_textures[idx/2].m_pExtend.get();
			}
			else
			{
				pResult = m_pD->m_textures[idx/2].m_pPatch.get();
			}
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetInputNode( int i, NodePtr pNode )
	{
		check( i >=0 && i < GetInputCount());

		if ( i==0 )
		{
			m_pD->m_pMesh = dynamic_cast<NodePatchMesh*>( pNode.get() );
		}
		else if ( i==1 )
		{
			m_pD->m_pMorph = dynamic_cast<NodeMesh*>( pNode.get() );
		}
		else if ( i==2 )
		{
			m_pD->m_pFactor = dynamic_cast<NodeScalar*>( pNode.get() );
		}
		else
		{
			int idx = i - 3;
			if (idx % 2==0)
			{
				m_pD->m_textures[idx/2].m_pExtend = dynamic_cast<NodeImage*>( pNode.get() );
			}
			else
			{
				m_pD->m_textures[idx/2].m_pPatch = dynamic_cast<NodePatchImage*>( pNode.get() );
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetParent( NodeSurface* p )
	{
		m_pD->m_pParent = p;
	}


	//---------------------------------------------------------------------------------------------
    NodeSurface* NodeSurfaceEdit::GetParent() const
	{
        return m_pD->m_pParent.get();
	}


    //---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::AddTag(const char* tagName)
    {
        m_pD->m_tags.push_back(tagName);
    }


    //---------------------------------------------------------------------------------------------
    int NodeSurfaceEdit::GetTagCount() const
    {
        return int( m_pD->m_tags.size());
    }


    //---------------------------------------------------------------------------------------------
    const char* NodeSurfaceEdit::GetTag( int i ) const
    {
        if (i>=0 && i<GetTagCount())
        {
            return m_pD->m_tags[i].c_str();
        }
        return nullptr;
    }


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetMesh(NodePatchMesh* pNode )
	{
		m_pD->m_pMesh = pNode;
	}


	//---------------------------------------------------------------------------------------------
    NodePatchMesh* NodeSurfaceEdit::GetMesh() const
	{
		return m_pD->m_pMesh.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetMorph( NodeMeshPtr pNode )
	{
		m_pD->m_pMorph = pNode;
	}


	//---------------------------------------------------------------------------------------------
    NodeMeshPtr NodeSurfaceEdit::GetMorph() const
	{
		return m_pD->m_pMorph.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeSurfaceEdit::SetFactor(NodeScalarPtr pNode)
	{
		m_pD->m_pFactor = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeSurfaceEdit::GetFactor() const
	{
		return m_pD->m_pFactor.get();
	}


	//---------------------------------------------------------------------------------------------
    int NodeSurfaceEdit::GetImageCount() const
	{
		return (int)m_pD->m_textures.size();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetImageCount( int num )
	{
		check( num >=0 );
		m_pD->m_textures.resize( num );
	}


	//---------------------------------------------------------------------------------------------
    NodeImagePtr NodeSurfaceEdit::GetImage( int index ) const
	{
		check( index >=0 && index < (int)m_pD->m_textures.size() );

		return m_pD->m_textures[ index ].m_pExtend.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetImage( int index, NodeImagePtr pNode )
	{
		check( index >=0 && index < (int)m_pD->m_textures.size() );

		m_pD->m_textures[ index ].m_pExtend = pNode;
	}


	//---------------------------------------------------------------------------------------------
    NodePatchImagePtr NodeSurfaceEdit::GetPatch( int index ) const
	{
		check( index >=0 && index < (int)m_pD->m_textures.size() );

		return m_pD->m_textures[ index ].m_pPatch.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeSurfaceEdit::SetPatch( int index, NodePatchImagePtr pNode )
	{
		check( index >=0 && index < (int)m_pD->m_textures.size() );

		m_pD->m_textures[ index ].m_pPatch = pNode;
	}

}

