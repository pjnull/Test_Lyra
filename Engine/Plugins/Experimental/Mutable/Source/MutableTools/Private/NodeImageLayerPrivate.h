// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NodeImagePrivate.h"
#include "NodeImageLayer.h"
#include "NodeColour.h"
#include "AST.h"

#include "MemoryPrivate.h"


namespace mu
{

	class NodeImageLayer::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeImagePtr m_pBase;
		NodeImagePtr m_pMask;
		NodeImagePtr m_pBlended;
		EBlendType m_type;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pBase;
			arch << m_pMask;
			arch << m_pBlended;
            arch << (uint32_t)m_type;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pBase;
			arch >> m_pMask;
			arch >> m_pBlended;

            uint32_t t;
            arch >> t;
            m_type=(EBlendType)t;
		}
	};

}