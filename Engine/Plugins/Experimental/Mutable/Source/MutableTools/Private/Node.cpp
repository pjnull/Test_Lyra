// Copyright Epic Games, Inc. All Rights Reserved.


#include "Node.h"
#include "NodePrivate.h"

#include "NodeColour.h"
#include "NodeComponent.h"
#include "NodeImage.h"
#include "NodeLayout.h"
#include "NodeLOD.h"
#include "NodeMesh.h"
#include "NodeObject.h"
#include "NodePatchImage.h"
#include "NodePatchMesh.h"
#include "NodeScalar.h"
#include "NodeString.h"
#include "NodeProjector.h"
#include "NodeSurface.h"
#include "NodeModifier.h"
#include "NodeRange.h"
#include "NodeBool.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static NODE_TYPE s_nodeType = 		NODE_TYPE( "Node", 0 );


	//---------------------------------------------------------------------------------------------
	NODE_TYPE::NODE_TYPE()
	{
		m_strName = "";
		m_pParent = 0;
	}

	//---------------------------------------------------------------------------------------------
	NODE_TYPE::NODE_TYPE( const char* strName, const NODE_TYPE* pParent )
	{
        m_strName = strName;
		m_pParent = pParent;
	}



	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	void Node::Serialise( const Node* pNode, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

		// Warning: cannot be replaced with the wrapping virtual call because some subclasses are abstract
		arch << uint32_t(pNode->Type);

		#define SERIALISE_CHILDREN( C, ID ) C::Serialise(static_cast<const C*>(pNode), arch );

		switch (pNode->Type)
		{
		case EType::Colour:		SERIALISE_CHILDREN(NodeColour,arch); break;
		case EType::Component:  SERIALISE_CHILDREN(NodeComponent,arch); break;
		case EType::Image:		SERIALISE_CHILDREN(NodeImage,arch); break;
		case EType::Layout:		SERIALISE_CHILDREN(NodeLayout,arch); break;
		case EType::LOD:		SERIALISE_CHILDREN(NodeLOD,arch); break;
		case EType::Mesh:		SERIALISE_CHILDREN(NodeMesh,arch); break;
		case EType::Object:		SERIALISE_CHILDREN(NodeObject,arch); break;
		case EType::PatchImage: SERIALISE_CHILDREN(NodePatchImage,arch); break;
		case EType::Scalar:		SERIALISE_CHILDREN(NodeScalar,arch); break;
		case EType::PatchMesh:	SERIALISE_CHILDREN(NodePatchMesh, arch); break;
		case EType::Projector:	SERIALISE_CHILDREN(NodeProjector,arch); break;
		case EType::Surface:	SERIALISE_CHILDREN(NodeSurface,arch); break;
		case EType::Modifier:	SERIALISE_CHILDREN(NodeModifier,arch); break;
		case EType::Range:		SERIALISE_CHILDREN(NodeRange,arch); break;
		case EType::String:		SERIALISE_CHILDREN(NodeString, arch); break;
		case EType::Bool:		SERIALISE_CHILDREN(NodeBool, arch); break;
		default: check(false);
		}

		#undef SERIALISE_CHILDREN
	}


	//---------------------------------------------------------------------------------------------
	NodePtr Node::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (EType(id))
		{
		case EType::Colour:		return NodeColour::StaticUnserialise( arch ); break;
		case EType::Component:  return NodeComponent::StaticUnserialise( arch ); break;
		case EType::Image:		return NodeImage::StaticUnserialise( arch ); break;
		case EType::Layout:		return NodeLayout::StaticUnserialise( arch ); break;
		case EType::LOD:		return NodeLOD::StaticUnserialise( arch ); break;
		case EType::Mesh:		return NodeMesh::StaticUnserialise( arch ); break;
		case EType::Object:		return NodeObject::StaticUnserialise( arch ); break;
		case EType::PatchImage: return NodePatchImage::StaticUnserialise( arch ); break;
		case EType::Scalar:		return NodeScalar::StaticUnserialise( arch ); break;
		case EType::PatchMesh:	return NodePatchMesh::StaticUnserialise( arch ); break;
		case EType::Projector:	return NodeProjector::StaticUnserialise( arch ); break;
		case EType::Surface:	return NodeSurface::StaticUnserialise( arch ); break;
		case EType::Modifier:	return NodeModifier::StaticUnserialise( arch ); break;
		case EType::Range:		return NodeRange::StaticUnserialise( arch ); break;
		case EType::String:		return NodeString::StaticUnserialise( arch ); break;
		case EType::Bool:		return NodeBool::StaticUnserialise( arch ); break;
		default : check(false);
		}

		return 0;
	}



	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* Node::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* Node::GetStaticType()
	{
		return &s_nodeType;
	}


	//---------------------------------------------------------------------------------------------
	void Node::SetMessageContext( const void* context )
	{
		GetBasePrivate()->m_errorContext = context;
	}


	//---------------------------------------------------------------------------------------------
	void Node::Private::CloneChildren( const Node* pThis, NodePtr pTarget, NodeMapPtr pMap )
	{
		// Clone the children
		for ( int i=0; i<pThis->GetInputCount(); ++i )
		{
			NodePtr pSource = pThis->GetInputNode(i);
			NodePtr pCloned;
			if (pSource)
			{
				pCloned = pMap->Get( pSource.get() );
				if (!pCloned)
				{
					pCloned = pSource->Clone( pMap );
				}
			}

			pTarget->SetInputNode( i, pCloned.get() );
		}
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	NodeMap::NodeMap()
	{
		m_pD = new Private();
	}


	//---------------------------------------------------------------------------------------------
	NodeMap::~NodeMap()
	{
        check( m_pD );
		delete m_pD;
		m_pD = 0;
	}


	//---------------------------------------------------------------------------------------------
	NodeMap::Private* NodeMap::GetPrivate() const
	{
		return m_pD;
	}


	//---------------------------------------------------------------------------------------------
	int NodeMap::GetSize( ) const
	{
		return (int)m_pD->m_map.size();
	}


	//---------------------------------------------------------------------------------------------
	void NodeMap::Add( const void* key, NodePtr value )
	{
		m_pD->m_map[ key ] = value;
	}


	//---------------------------------------------------------------------------------------------
	NodePtr NodeMap::Get( const void* key ) const
	{
		NodePtr pResult;

		map<const void*,NodePtr>::const_iterator it = m_pD->m_map.find(key);
		if ( it != m_pD->m_map.end() )
		{
			pResult = it->second;
		}

		return pResult;
	}

}

