// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeImageSwizzle.h"
#include "NodeImageSwizzlePrivate.h"


#define NODE_INPUT_COUNT 	4


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageSwizzle::Private::s_type =
			NODE_TYPE( "ImageSwizzle", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageSwizzle, EType::Swizzle, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageSwizzle::GetInputCount() const
	{
		return (int)m_pD->m_sources.size();
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageSwizzle::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

		pResult = m_pD->m_sources[i].get();

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwizzle::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		m_pD->m_sources[i] = dynamic_cast<NodeImage*>(pNode.get());
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	EImageFormat NodeImageSwizzle::GetFormat() const
	{
		return m_pD->m_format;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwizzle::SetFormat(EImageFormat format )
	{
		m_pD->m_format = format;

		int channelCount = GetImageFormatData( format ).m_channels;
		m_pD->m_sources.resize( channelCount );
		m_pD->m_sourceChannels.resize( channelCount );
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageSwizzle::GetSource( int t ) const
	{
		check( t>=0 && t<(int)m_pD->m_sources.size() );
		return m_pD->m_sources[t].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwizzle::SetSource( int t, NodeImagePtr pNode )
	{
		check( t>=0 && t<(int)m_pD->m_sources.size() );
		m_pD->m_sources[t] = pNode;
	}


	//---------------------------------------------------------------------------------------------
	int NodeImageSwizzle::GetSourceChannel( int t ) const
	{
		check( t>=0 && t<(int)m_pD->m_sourceChannels.size() );
		return m_pD->m_sourceChannels[t];
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageSwizzle::SetSourceChannel( int t, int channel )
	{
		check( t>=0 && t<(int)m_pD->m_sourceChannels.size() );
		m_pD->m_sourceChannels[t] = channel;
	}



}

#undef NODE_INPUT_COUNT