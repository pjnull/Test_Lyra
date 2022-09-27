// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeScalarEnumParameter.h"
#include "NodeScalarEnumParameterPrivate.h"

#include "MutableMath.h"
#include "MemoryPrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeScalarEnumParameter::Private::s_type =
			NODE_TYPE( "ScalarEnumParameter", NodeScalar::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeScalarEnumParameter, EType::EnumParameter, Node, Node::EType::Scalar)


	//---------------------------------------------------------------------------------------------
	int NodeScalarEnumParameter::GetInputCount() const
	{
        return int( m_pD->m_ranges.size() );
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeScalarEnumParameter::GetInputNode( int i ) const
	{
        check( i<GetInputCount() );
        if (i<GetInputCount())
        {
            return m_pD->m_ranges[i].get();
        }
        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarEnumParameter::SetInputNode( int i, NodePtr n )
	{
        check( i<GetInputCount() );
        if (i<GetInputCount())
        {
            m_pD->m_ranges[i] = dynamic_cast<NodeRange*>(n.get());
        }
	}


	//---------------------------------------------------------------------------------------------
	const char* NodeScalarEnumParameter::GetName() const
	{
		return m_pD->m_name.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarEnumParameter::SetName( const char* strName )
	{
		if ( strName )
		{
			m_pD->m_name = strName;
		}
		else
		{
			m_pD->m_name = "";
		}
	}


	const char* NodeScalarEnumParameter::GetUid() const
	{
		return m_pD->m_uid.c_str();
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarEnumParameter::SetUid( const char* strUid )
	{
		if ( strUid )
		{
			m_pD->m_uid = strUid;
		}
		else
		{
			m_pD->m_uid = "";
		}
	}


	//---------------------------------------------------------------------------------------------
	int NodeScalarEnumParameter::GetDefaultValueIndex() const
	{
		return m_pD->m_defaultValue;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarEnumParameter::SetDefaultValueIndex( int i )
	{
		m_pD->m_defaultValue = i;
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarEnumParameter::SetValueCount( int i )
	{
		m_pD->m_options.resize(i);
	}


	//---------------------------------------------------------------------------------------------
	int NodeScalarEnumParameter::GetValueCount() const
	{
		return (int)m_pD->m_options.size();
	}


	//---------------------------------------------------------------------------------------------
	void NodeScalarEnumParameter::SetValue( int i, float value, const char* strName )
	{
		check( i>=0 && i<(int)m_pD->m_options.size() );
		m_pD->m_options[i].name = strName;
		m_pD->m_options[i].value = value;
	}


    //---------------------------------------------------------------------------------------------
    void NodeScalarEnumParameter::SetRangeCount( int i )
    {
        check(i>=0);
        m_pD->m_ranges.resize(i);
    }


    //---------------------------------------------------------------------------------------------
    void NodeScalarEnumParameter::SetRange( int i, NodeRangePtr pRange )
    {
        check( i>=0 && i<int(m_pD->m_ranges.size()) );
        if ( i>=0 && i<int(m_pD->m_ranges.size()) )
        {
            m_pD->m_ranges[i] = pRange;
        }
    }

}

