// Copyright Epic Games, Inc. All Rights Reserved.

#include "ASTOpMeshExtractLayoutBlocks.h"
#include "ErrorLogPrivate.h"
#include "CodeOptimiser.h"
#include "StreamsPrivate.h"


using namespace mu;


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
ASTOpMeshExtractLayoutBlocks::ASTOpMeshExtractLayoutBlocks()
    : source(this)
{
}


ASTOpMeshExtractLayoutBlocks::~ASTOpMeshExtractLayoutBlocks()
{
    // Explicit call needed to avoid recursive destruction
    ASTOp::RemoveChildren();
}


bool ASTOpMeshExtractLayoutBlocks::IsEqual(const ASTOp& otherUntyped) const
{
    if ( auto other = dynamic_cast<const ASTOpMeshExtractLayoutBlocks*>(&otherUntyped) )
    {
        return source==other->source && layout==other->layout && blocks==other->blocks;
    }
    return false;
}


mu::Ptr<ASTOp> ASTOpMeshExtractLayoutBlocks::Clone(MapChildFunc& mapChild) const
{
    Ptr<ASTOpMeshExtractLayoutBlocks> n = new ASTOpMeshExtractLayoutBlocks();
    n->source = mapChild(source.child());
    n->layout = layout;
    n->blocks = blocks;
    return n;
}


void ASTOpMeshExtractLayoutBlocks::Assert()
{
    check( blocks.size()<std::numeric_limits<uint16_t>::max() );
    ASTOp::Assert();
}


void ASTOpMeshExtractLayoutBlocks::ForEachChild(const std::function<void(ASTChild&)>& f )
{
    f(source);
}


uint64 ASTOpMeshExtractLayoutBlocks::Hash() const
{
	uint64 res = std::hash<size_t>()( size_t(source.child().get() ) );
    return res;
}


void ASTOpMeshExtractLayoutBlocks::Link( PROGRAM& program, const FLinkerOptions* )
{
    // Already linked?
    if (!linkedAddress)
    {
        linkedAddress = (OP::ADDRESS)program.m_opAddress.size();

        program.m_opAddress.push_back((uint32_t)program.m_byteCode.size());
        AppendCode(program.m_byteCode, OP_TYPE::ME_EXTRACTLAYOUTBLOCK);
        OP::ADDRESS sourceAt = source ? source->linkedAddress : 0;
        AppendCode(program.m_byteCode, sourceAt );
        AppendCode(program.m_byteCode, (uint16_t)layout );
        AppendCode(program.m_byteCode, (uint16_t)blocks.size() );

        for ( auto b: blocks )
        {
            AppendCode(program.m_byteCode, (uint32_t)b );
        }
    }
}

