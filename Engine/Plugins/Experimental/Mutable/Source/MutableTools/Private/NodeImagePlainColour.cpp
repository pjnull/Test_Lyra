// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeImagePlainColour.h"
#include "NodeImagePlainColourPrivate.h"

#include "NodeColour.h"

#include "ImagePrivate.h"


#define NODE_INPUT_COUNT 	1


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImagePlainColour::Private::s_type =
			NODE_TYPE( "ImagePlainColour", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImagePlainColour, EType::PlainColour, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	void NodeImagePlainColour::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<NODE_INPUT_COUNT );

		switch (i)
		{
        case 0: m_pD->m_pColour = dynamic_cast<NodeColour*>(pNode.get()); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	int NodeImagePlainColour::GetInputCount() const
	{
		return NODE_INPUT_COUNT;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImagePlainColour::GetInputNode( int i ) const
	{
		check( i>=0 && i<NODE_INPUT_COUNT );

		Node* pResult = 0;

		switch (i)
		{
        case 0: pResult = m_pD->m_pColour.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeColourPtr NodeImagePlainColour::GetColour() const
	{
		return m_pD->m_pColour;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImagePlainColour::SetColour( NodeColourPtr pNode )
	{
		m_pD->m_pColour = pNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImagePlainColour::SetSize( int x, int y )
	{
		m_pD->m_sizeX = x;
		m_pD->m_sizeY = y;
	}

}

#undef NODE_INPUT_COUNT
