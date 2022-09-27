// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MutableTools/Private/AST.h"


namespace mu
{

	class ASTOpImageCompose : public ASTOp
	{
	public:

		ASTChild Layout;
		ASTChild Base;
		ASTChild BlockImage;
		ASTChild Mask;
		uint32 BlockIndex=0;

	public:

		ASTOpImageCompose();
		ASTOpImageCompose(const ASTOpImageCompose&) = delete;
		~ASTOpImageCompose();

		OP_TYPE GetOpType() const override { return OP_TYPE::IM_COMPOSE; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFunc& mapChild) const override;
		void ForEachChild(const std::function<void(ASTChild&)>&) override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
		//TODO: Ptr<ASTOp> OptimiseSink(const MODEL_OPTIMIZATION_OPTIONS& options, OPTIMIZE_SINK_CONTEXT& context) const override;
		Ptr<ASTOp> OptimiseSemantic(const MODEL_OPTIMIZATION_OPTIONS& options) const;
		FImageDesc GetImageDesc(bool returnBestOption, GetImageDescContext* context) override;
		void GetLayoutBlockSize(int* pBlockX, int* pBlockY) override;
		bool IsImagePlainConstant(vec4<float>& colour) const override;
		Ptr<ImageSizeExpression> GetImageSizeExpression() const override;

	};

}
