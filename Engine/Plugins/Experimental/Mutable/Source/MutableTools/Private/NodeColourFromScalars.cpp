// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeColourFromScalars.h"
#include "NodeColourFromScalarsPrivate.h"

#include "NodeScalar.h"

#include "ImagePrivate.h"


#define NODE_INPUT_COUNT 	4


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeColourFromScalars::Private::s_type =
			NODE_TYPE( "ColourFromScalars", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourFromScalars, EType::FromScalars, Node, Node::EType::Colour)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeColourFromScalars::GetInputCount() const
	{
		return NODE_INPUT_COUNT;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeColourFromScalars::GetInputNode( int i ) const
	{
		check( i>=0 && i<NODE_INPUT_COUNT );

		Node* pResult = 0;

		switch (i)
		{
		case 0: pResult = m_pD->m_pX.get(); break;
		case 1: pResult = m_pD->m_pY.get(); break;
		case 2: pResult = m_pD->m_pZ.get(); break;
		case 3: pResult = m_pD->m_pW.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourFromScalars::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<NODE_INPUT_COUNT );

		switch (i)
		{
		case 0: m_pD->m_pX = dynamic_cast<NodeScalar*>(pNode.get()); break;
		case 1: m_pD->m_pY = dynamic_cast<NodeScalar*>(pNode.get()); break;
		case 2: m_pD->m_pZ = dynamic_cast<NodeScalar*>(pNode.get()); break;
		case 3: m_pD->m_pW = dynamic_cast<NodeScalar*>(pNode.get()); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeColourFromScalars::GetX() const
	{
		return m_pD->m_pX.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourFromScalars::SetX( NodeScalarPtr pNode )
	{
		m_pD->m_pX = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeColourFromScalars::GetY() const
	{
		return m_pD->m_pY.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourFromScalars::SetY( NodeScalarPtr pNode )
	{
		m_pD->m_pY = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeColourFromScalars::GetZ() const
	{
		return m_pD->m_pZ.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourFromScalars::SetZ( NodeScalarPtr pNode )
	{
		m_pD->m_pZ = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeColourFromScalars::GetW() const
	{
		return m_pD->m_pW.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourFromScalars::SetW( NodeScalarPtr pNode )
	{
		m_pD->m_pW = pNode;
	}

}

#undef NODE_INPUT_COUNT