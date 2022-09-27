// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeRange.h"
#include "Node.h"
#include "NodePrivate.h"
#include "NodeRangeFromScalar.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    static NODE_TYPE s_nodeRangeType = 	NODE_TYPE( "NodeRange", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
    void NodeRange::Serialise( const NodeRange* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

#define SERIALISE_CHILDREN( C, ID ) \
        ( const C* pTyped##ID = dynamic_cast<const C*>(p) )			\
        {                                                           \
            arch << (uint32_t)ID;                                   \
            C::Serialise( pTyped##ID, arch );						\
        }                                                           \

        if SERIALISE_CHILDREN( NodeRangeFromScalar          , 0  )
        else check(false);

#undef SERIALISE_CHILDREN
    }

    //---------------------------------------------------------------------------------------------
    NodeRangePtr NodeRange::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (id)
		{
        case 0 :  return NodeRangeFromScalar::StaticUnserialise( arch ); break;
        default : check(false);
		}

        return nullptr;
	}


	//---------------------------------------------------------------------------------------------
    const NODE_TYPE* NodeRange::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
    const NODE_TYPE* NodeRange::GetStaticType()
	{
        return &s_nodeRangeType;
	}


}

