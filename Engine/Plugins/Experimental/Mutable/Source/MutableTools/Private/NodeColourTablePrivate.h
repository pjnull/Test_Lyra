// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "NodePrivate.h"
#include "NodeColourTable.h"
#include "Table.h"
#include "AST.h"

#include "MemoryPrivate.h"


namespace mu
{


	class NodeColourTable::Private : public Node::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		string m_parameterName;
		TablePtr m_pTable;
		string m_columnName;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_parameterName;
			arch << m_pTable;
			arch << m_columnName;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==0);

			arch >> m_parameterName;
			arch >> m_pTable;
			arch >> m_columnName;
		}

	};

}
