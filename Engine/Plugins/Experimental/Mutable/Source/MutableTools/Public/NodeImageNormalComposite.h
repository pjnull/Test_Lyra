// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NodeImage.h"
#include "Image.h"

namespace mu
{

	// Forward definitions
	class NodeImageNormalComposite;
	typedef Ptr<NodeImageNormalComposite> NodeImageNormalCompositePtr;
	typedef Ptr<const NodeImageNormalComposite> NodeImageNormalCompositePtrConst;

	//! Node that inverts the colors of an image, channel by channel
	//! \ingroup model

	class MUTABLETOOLS_API NodeImageNormalComposite : public NodeImage
	{
	public:

		//MUTABLE_DEFINE_CONST_VISITABLE()
		
		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageNormalComposite();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise(const NodeImageNormalComposite* pNode, OutputArchive& arch);
		static NodeImageNormalCompositePtr StaticUnserialise(InputArchive& arch);


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		NodePtr Clone(NodeMapPtr pMap = 0) const override;

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		virtual int GetInputCount() const override;
		virtual Node* GetInputNode(int i)const override;
		void SetInputNode(int i, NodePtr pNode) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		NodeImagePtr GetBase() const;
		void SetBase(NodeImagePtr);

        NodeImagePtr GetNormal() const;
        void SetNormal(NodeImagePtr);

		float GetPower() const;
		void SetPower(float); 

        ECompositeImageMode GetMode() const;
        void SetMode(ECompositeImageMode);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
		Node::Private*GetBasePrivate()const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template
		~NodeImageNormalComposite();

	private:

		Private* m_pD;
	};

}