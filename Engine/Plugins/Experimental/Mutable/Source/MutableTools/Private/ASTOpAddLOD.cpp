// Copyright Epic Games, Inc. All Rights Reserved.

#include "ASTOpAddLOD.h"
#include "ErrorLogPrivate.h"
#include "CodeOptimiser.h"
#include "StreamsPrivate.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	ASTOpAddLOD::ASTOpAddLOD()
	{
	}


	ASTOpAddLOD::~ASTOpAddLOD()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpAddLOD::IsEqual(const ASTOp& otherUntyped) const
	{
		if (auto other = dynamic_cast<const ASTOpAddLOD*>(&otherUntyped))
		{
			return lods == other->lods;
		}
		return false;
	}


	uint64 ASTOpAddLOD::Hash() const
	{
		uint64 res = std::hash<uint64>()(uint64(OP_TYPE::IN_ADDLOD));
		for (auto& c : lods)
		{
			hash_combine(res, c.child().get());
		}
		return res;
	}


	mu::Ptr<ASTOp> ASTOpAddLOD::Clone(MapChildFunc& mapChild) const
	{
		Ptr<ASTOpAddLOD> n = new ASTOpAddLOD();
		for (auto& c : lods)
		{
			n->lods.emplace_back(n, mapChild(c.child()));
		}
		return n;
	}


	void ASTOpAddLOD::ForEachChild(const std::function<void(ASTChild&)>& f)
	{
		for (auto& l : lods)
		{
			f(l);
		}
	}


	void ASTOpAddLOD::Link(PROGRAM& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::InstanceAddLODArgs args;
			memset(&args, 0, sizeof(args));

			int i = 0;
			for (auto& l : lods)
			{
				if (l)
				{
					args.lod[i] = l->linkedAddress;
					++i;
					if (i >= MUTABLE_OP_MAX_ADD_COUNT)
					{
						break;
					}
				}
			}

			linkedAddress = (OP::ADDRESS)program.m_opAddress.size();
			program.m_opAddress.push_back((uint32_t)program.m_byteCode.size());
			AppendCode(program.m_byteCode, OP_TYPE::IN_ADDLOD);
			AppendCode(program.m_byteCode, args);
		}

	}

}