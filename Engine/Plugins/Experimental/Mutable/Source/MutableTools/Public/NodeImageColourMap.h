// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NodeImage.h"


namespace mu
{

	// Forward definitions
	class NodeImageColourMap;
	typedef Ptr<NodeImageColourMap> NodeImageColourMapPtr;
	typedef Ptr<const NodeImageColourMap> NodeImageColourMapPtrConst;


	//! This node changes the colours of a selectd part of the image, applying a colour map from
	//! conteined in another image.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImageColourMap : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageColourMap();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeImageColourMap* pNode, OutputArchive& arch );
		static NodeImageColourMapPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        NodePtr Clone( NodeMapPtr pMap = 0 ) const override;

        const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

        virtual int GetInputCount() const override;
        virtual Node* GetInputNode( int i ) const override;
        void SetInputNode( int i, NodePtr pNode ) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the node generating the base image that will have the other one blended on top.
		NodeImagePtr GetBase() const;
		void SetBase( NodeImagePtr );

		//! Get the node generating the mask image controlling the weight of the blend effect.
		//! \todo: make it optional
		NodeImagePtr GetMask() const;
		void SetMask( NodeImagePtr );

		//! Get the node generating the image to blend on the base.
		NodeImagePtr GetMap() const;
		void SetMap( NodeImagePtr );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageColourMap();

	private:

		Private* m_pD;

	};


}