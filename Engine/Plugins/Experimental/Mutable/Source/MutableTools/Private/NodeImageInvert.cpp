// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeImageInvert.h"
#include "NodeImageInvertPrivate.h"

#include "ImagePrivate.h"


#define NODE_INPUT_COUNT 1

namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageInvert::Private::s_type =
		NODE_TYPE("ImageInvert", NodeImage::GetStaticType());

	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageInvert, EType::Invert, Node, Node::EType::Image)

	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	void NodeImageInvert::SetInputNode(int i, NodePtr pNode)
	{
		check(i >= 0 && i < NODE_INPUT_COUNT);

		m_pD->m_pBase = dynamic_cast<NodeImage*>(pNode.get());
	}

	//---------------------------------------------------------------------------------------------
	int NodeImageInvert::GetInputCount() const
	{
		return NODE_INPUT_COUNT;
	}

	//---------------------------------------------------------------------------------------------
	Node* NodeImageInvert::GetInputNode(int i)const
	{
		check(i >= 0 && i < NODE_INPUT_COUNT);

		Node* pResult = 0;
		pResult = m_pD->m_pBase.get();

		return pResult;
	}

	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageInvert::GetBase()const
	{
		return m_pD->m_pBase;
	}

	void NodeImageInvert::SetBase(NodeImagePtr pNode)
	{
		m_pD->m_pBase = pNode;
	}
	
}

#undef NODE_INPUT_COUNT