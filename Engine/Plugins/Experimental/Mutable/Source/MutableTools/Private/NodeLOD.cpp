// Copyright Epic Games, Inc. All Rights Reserved.


#include "MutableTools/Public/NodeLOD.h"
#include "MutableTools/Private/NodeLODPrivate.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeLOD::Private::s_type =
			NODE_TYPE( "LOD", NodeLOD::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeLOD, EType::LOD, Node, Node::EType::LOD);


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeLOD::GetInputCount() const
	{
		return int(m_pD->m_components.size()+m_pD->m_modifiers.size());
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeLOD::GetInputNode( int i ) const
	{
        check(i >= 0 && i < GetInputCount());

        if (i < int(m_pD->m_components.size()))
        {
            return m_pD->m_components[i].get();
		}

        i -= int(m_pD->m_components.size());

        if (i < int(m_pD->m_modifiers.size()))
        {
            return m_pD->m_modifiers[i].get();
        }

        return nullptr;
    }


    //---------------------------------------------------------------------------------------------
	void NodeLOD::SetInputNode( int i, NodePtr pNode )
	{
        check(i >= 0 && i < GetInputCount());

        if (i<int(m_pD->m_components.size()))
		{
            m_pD->m_components[i] = dynamic_cast<NodeComponent*>(pNode.get());
            return;
        }

        i -= int(m_pD->m_components.size());
        if (i < int(m_pD->m_modifiers.size()))
        {
            m_pD->m_modifiers[i] = dynamic_cast<NodeModifier*>(pNode.get());
        }
    }


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	int NodeLOD::GetComponentCount() const
	{
		return (int)m_pD->m_components.size();
	}


	//---------------------------------------------------------------------------------------------
	void NodeLOD::SetComponentCount(int num)
	{
		check(num >= 0);
		m_pD->m_components.resize(num);
	}


	//---------------------------------------------------------------------------------------------
	NodeComponentPtr NodeLOD::GetComponent(int index) const
	{
		check(index >= 0 && index < (int)m_pD->m_components.size());

		return m_pD->m_components[index].get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeLOD::SetComponent(int index, NodeComponentPtr pComponent)
	{
		check(index >= 0 && index < (int)m_pD->m_components.size());

		m_pD->m_components[index] = pComponent;
	}


	//---------------------------------------------------------------------------------------------
	int NodeLOD::GetModifierCount() const
	{
		return (int)m_pD->m_modifiers.size();
	}


	//---------------------------------------------------------------------------------------------
	void NodeLOD::SetModifierCount(int num)
	{
		check(num >= 0);
		m_pD->m_modifiers.resize(num);
	}


	//---------------------------------------------------------------------------------------------
	void NodeLOD::SetModifier(int index, NodeModifierPtr pModifier)
	{
		check(index >= 0 && index < (int)m_pD->m_modifiers.size());

		m_pD->m_modifiers[index] = pModifier;
	}


}

