// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeImageLuminance.h"
#include "NodeImageLuminancePrivate.h"

#include "NodeScalar.h"

#include "ImagePrivate.h"


#define NODE_INPUT_COUNT 	1


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageLuminance::Private::s_type =
			NODE_TYPE( "ImageLuminance", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageLuminance, EType::Luminance, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageLuminance::GetInputCount() const
	{
		return NODE_INPUT_COUNT;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageLuminance::GetInputNode( int i ) const
	{
		check( i>=0 && i<NODE_INPUT_COUNT );

		Node* pResult = 0;

		switch (i)
		{
        case 0: pResult = m_pD->m_pSource.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageLuminance::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<NODE_INPUT_COUNT );

		switch (i)
		{
        case 0: m_pD->m_pSource = dynamic_cast<NodeImage*>(pNode.get()); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageLuminance::GetSource() const
	{
		return m_pD->m_pSource.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageLuminance::SetSource( NodeImagePtr pNode )
	{
		m_pD->m_pSource = pNode;
	}



}

#undef NODE_INPUT_COUNT