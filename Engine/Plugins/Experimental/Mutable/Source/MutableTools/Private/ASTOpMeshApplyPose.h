// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AST.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshApplyPose : public ASTOp
	{
	public:

		ASTChild base;
		ASTChild pose;

	public:

		ASTOpMeshApplyPose();
		ASTOpMeshApplyPose(const ASTOpMeshApplyPose&) = delete;
		~ASTOpMeshApplyPose();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_APPLYPOSE; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFunc& mapChild) const override;
		void ForEachChild(const std::function<void(ASTChild&)>&) override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
	};


}
