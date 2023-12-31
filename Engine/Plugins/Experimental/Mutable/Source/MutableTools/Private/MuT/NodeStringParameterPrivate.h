// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeStringPrivate.h"

#include "MuT/NodeStringParameter.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeRange.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{


	class NodeStringParameter::Private : public NodeString::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		string m_defaultValue;
		string m_name;
		string m_uid;
		PARAMETER_DETAILED_TYPE m_detailedType = PARAMETER_DETAILED_TYPE::UNKNOWN;

        TArray<Ptr<NodeImage>> m_additionalImages;

		TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 4;
			arch << ver;

			arch << m_defaultValue;
			arch << m_name;
			arch << m_uid;
			arch << m_additionalImages;
			arch << m_detailedType;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==4);

			arch >> m_defaultValue;
			arch >> m_name;
            arch >> m_uid;
			arch >> m_additionalImages;
            arch >> m_detailedType;
            arch >> m_ranges;
        }
	};

}
