// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeImageVariation.h"
#include "NodeImageVariationPrivate.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    NODE_TYPE NodeImageVariation::Private::s_type =
        NODE_TYPE( "ImageVariation", NodeImage::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeImageVariation, EType::Variation, Node, Node::EType::Image)


    //---------------------------------------------------------------------------------------------
    int NodeImageVariation::GetInputCount() const { return 1 + int( m_pD->m_variations.size() ); }


    //---------------------------------------------------------------------------------------------
    Node* NodeImageVariation::GetInputNode( int i ) const
    {
        check( i >= 0 && i < GetInputCount() );

        if ( i == 0 )
        {
            return m_pD->m_defaultImage.get();
        }
        i -= 1;

        if ( i < int( m_pD->m_variations.size() ) )
        {
            return m_pD->m_variations[i].m_image.get();
        }
        i -= int( m_pD->m_variations.size() );

        return nullptr;
    }


    //---------------------------------------------------------------------------------------------
    void NodeImageVariation::SetInputNode( int i, NodePtr pNode )
    {
        check( i >= 0 && i < GetInputCount() );

        if ( i == 0 )
        {
            m_pD->m_defaultImage = dynamic_cast<NodeImage*>( pNode.get() );
            return;
        }

        i -= 1;
        if ( i < int( m_pD->m_variations.size() ) )
        {

            m_pD->m_variations[i].m_image = dynamic_cast<NodeImage*>( pNode.get() );
            return;
        }
        i -= (int)m_pD->m_variations.size();
    }


    //---------------------------------------------------------------------------------------------
    // Own Interface
    //---------------------------------------------------------------------------------------------
    void NodeImageVariation::SetDefaultImage( NodeImage* p ) { m_pD->m_defaultImage = p; }


    //---------------------------------------------------------------------------------------------
    int NodeImageVariation::GetVariationCount() const { return int( m_pD->m_variations.size() ); }


    //---------------------------------------------------------------------------------------------
    void NodeImageVariation::SetVariationCount( int num )
    {
        check( num >= 0 );
        m_pD->m_variations.resize( num );
    }

    //---------------------------------------------------------------------------------------------
    void NodeImageVariation::SetVariationTag( int index, const char* strTag )
    {
        check( index >= 0 && index < (int)m_pD->m_variations.size() );
        check( strTag );

        if ( strTag )
        {
            m_pD->m_variations[index].m_tag = strTag;
        }
        else
        {
            m_pD->m_variations[index].m_tag = "";
        }
    }


    //---------------------------------------------------------------------------------------------
    void NodeImageVariation::SetVariationImage( int index, NodeImage* pNode )
    {
        check( index >= 0 && index < (int)m_pD->m_variations.size() );

        m_pD->m_variations[index].m_image = pNode;
    }


} // namespace mu