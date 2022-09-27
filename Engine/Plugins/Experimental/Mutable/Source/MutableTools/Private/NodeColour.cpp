// Copyright Epic Games, Inc. All Rights Reserved.


#include "NodeColour.h"
#include "NodePrivate.h"

#include "NodeColourConstant.h"
#include "NodeColourParameter.h"
#include "NodeColourSampleImage.h"
#include "NodeColourTable.h"
#include "NodeColourFromScalars.h"
#include "NodeColourArithmeticOperation.h"
#include "NodeColourSwitch.h"
#include "NodeColourVariation.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static NODE_TYPE s_nodeColourType = NODE_TYPE( "NodeColour", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	void NodeColour::Serialise( const NodeColour* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

		arch << uint32_t(p->Type);
		p->SerialiseWrapper(arch);
    }

        
	//---------------------------------------------------------------------------------------------
	NodeColourPtr NodeColour::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeColourConstant::StaticUnserialise( arch ); break;
		case 1 :  return NodeColourParameter::StaticUnserialise( arch ); break;
		case 2 :  return NodeColourSampleImage::StaticUnserialise( arch ); break;
		case 3 :  return NodeColourTable::StaticUnserialise( arch ); break;
		//case 4 :  return NodeColourImageSize::StaticUnserialise( arch ); break; // deprecated
		case 5 :  return NodeColourFromScalars::StaticUnserialise( arch ); break;
        case 6 :  return NodeColourArithmeticOperation::StaticUnserialise( arch ); break;
        case 7 :  return NodeColourSwitch::StaticUnserialise( arch ); break;
        case 8 :  return NodeColourVariation::StaticUnserialise( arch ); break;
        default : check(false);
		}

		return nullptr;
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeColour::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeColour::GetStaticType()
	{
		return &s_nodeColourType;
	}


}

