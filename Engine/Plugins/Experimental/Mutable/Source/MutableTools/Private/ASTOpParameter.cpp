// Copyright Epic Games, Inc. All Rights Reserved.

#include "ASTOpParameter.h"
#include "ErrorLogPrivate.h"
#include "CodeOptimiser.h"
#include "StreamsPrivate.h"


using namespace mu;


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ASTOpParameter::~ASTOpParameter()
{
    // Explicit call needed to avoid recursive destruction
    ASTOp::RemoveChildren();
}


void ASTOpParameter::ForEachChild(const std::function<void(ASTChild&)>& f)
{
    for (auto &c: ranges)
    {
        f(c.rangeSize);
    }
    for (auto &c: additionalImages)
    {
        f(c);
    }
}


bool ASTOpParameter::IsEqual(const ASTOp& otherUntyped) const
{
    if ( auto other = dynamic_cast<const ASTOpParameter*>(&otherUntyped) )
    {
        return type==other->type &&
                parameter==other->parameter &&
                ranges==other->ranges &&
                additionalImages==other->additionalImages;
    }
    return false;
}


mu::Ptr<ASTOp> ASTOpParameter::Clone(MapChildFunc& mapChild) const
{
    Ptr<ASTOpParameter> n = new ASTOpParameter();
    n->type = type;
    n->parameter = parameter;
    for(const auto& c:ranges)
    {
        n->ranges.emplace_back( n.get(), mapChild(c.rangeSize.child()), c.rangeName, c.rangeUID );
    }
    for(const auto& c:additionalImages)
    {
        n->additionalImages.emplace_back(n,mapChild(c.child()));
    }
    return n;
}


uint64 ASTOpParameter::Hash() const
{
	uint64 res = std::hash<uint64>()(uint64(type));
    hash_combine( res, parameter.m_type );
    hash_combine( res, parameter.m_name );
    return res;
}


void ASTOpParameter::Assert()
{
    ASTOp::Assert();
}


void ASTOpParameter::Link( PROGRAM& program, const FLinkerOptions* )
{
    // Already linked?
    if (!linkedAddress)
    {
        OP::ParameterArgs args;
        memset( &args,0, sizeof(args) );

        args.variable = (OP::ADDRESS)program.m_parameters.size();
        program.m_parameters.push_back(parameter);

        for( const auto& d: ranges)
        {
            OP::ADDRESS sizeAt = 0;
            uint16_t rangeId = 0;
            LinkRange( program, d, sizeAt, rangeId );
            program.m_parameters.back().m_ranges.push_back(rangeId);
        }

        for( const auto& d: additionalImages)
        {
            OP::ADDRESS descAt = d ? d->linkedAddress : 0;
            program.m_parameters.back().m_descImages.push_back(descAt);
        }

        linkedAddress = (OP::ADDRESS)program.m_opAddress.size();
        //program.m_code.push_back(op);

        program.m_opAddress.push_back((uint32_t)program.m_byteCode.size());
        AppendCode(program.m_byteCode,type);
        AppendCode(program.m_byteCode,args);
    }
}


int ASTOpParameter::EvaluateInt( ASTOpList& facts, bool &unknown ) const
{
    unknown = true;

    // Check the facts, in case we have the value for our parameter.
    for( const auto& f: facts )
    {
        if (f->GetOpType()==OP_TYPE::BO_EQUAL_INT_CONST)
        {
            const auto typedFact = dynamic_cast<const ASTOpFixed*>(f.get());
            auto value = typedFact->children[typedFact->op.args.BoolEqualScalarConst.value].child();
            if (value.get()==this)
            {
                unknown = false;
                return typedFact->op.args.BoolEqualScalarConst.constant;
            }
            else
            {
                // We could try something more if it was an expression and it had the parameter
                // somewhere in it.
            }
        }
    }

    return 0;
}


ASTOp::BOOL_EVAL_RESULT ASTOpParameter::EvaluateBool( ASTOpList& /*facts*/,
                                                      EVALUATE_BOOL_CACHE* ) const
{
    check(type==OP_TYPE::BO_PARAMETER);
    return BET_UNKNOWN;
}


FImageDesc ASTOpParameter::GetImageDesc(bool, GetImageDescContext*)
{
	check(type == OP_TYPE::IM_PARAMETER);
	return FImageDesc();
}
