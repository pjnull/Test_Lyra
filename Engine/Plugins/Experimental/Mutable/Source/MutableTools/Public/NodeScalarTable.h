// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NodeScalar.h"


namespace mu
{

	// Forward definitions
	class NodeScalarTable;
	typedef Ptr<NodeScalarTable> NodeScalarTablePtr;
	typedef Ptr<const NodeScalarTable> NodeScalarTablePtrConst;

	class Table;
	typedef Ptr<Table> TablePtr;
	typedef Ptr<const Table> TablePtrConst;


	//! This node provides the meshes stored in the column of a table.
	//! \ingroup transform
	class MUTABLETOOLS_API NodeScalarTable : public NodeScalar
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeScalarTable();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeScalarTable* pNode, OutputArchive& arch );
		static NodeScalarTablePtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		NodePtr Clone( NodeMapPtr pMap = 0 ) const override;

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		int GetInputCount() const override;
		Node* GetInputNode( int i ) const override;
		void SetInputNode( int i, NodePtr pNode ) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Set the name of the implicit table parameter.
		void SetParameterName( const char* strName );

		//!
		TablePtr GetTable() const;
		void SetTable( TablePtr );

		//!
		const char* GetColumn() const;
		void SetColumn( const char* strName );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;


	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeScalarTable();

	private:

		Private* m_pD;

	};


}