// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVirtualAssetsStatistics.h"
#include "VirtualizationManager.h"
#include "Math/UnitConversion.h"
#include "Math/BasicMathExpressionEvaluator.h"
#include "Misc/ExpressionParser.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Internationalization/FastDecimalFormat.h"

#define LOCTEXT_NAMESPACE "VirtualAssets"

extern FString SingleDecimalFormat(double Value);

using namespace UE::Virtualization;

void SVirtualAssetsStatisticsDialog::Construct(const FArguments& InArgs)
{
	const float RowMargin = 0.0f;
	const float TitleMargin = 10.0f;
	const float ColumnMargin = 10.0f;
	const FSlateColor TitleColour = FStyleColors::AccentWhite;
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 5, 0, 0)
		.Expose(GridSlot)
		[
			GetGridPanel()
		]
	];

	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SVirtualAssetsStatisticsDialog::UpdateGridPanels));
}

EActiveTimerReturnType SVirtualAssetsStatisticsDialog::UpdateGridPanels(double InCurrentTime, float InDeltaTime)
{
	(*GridSlot)
	[
		GetGridPanel()
	];

	SlatePrepass(GetPrepassLayoutScaleMultiplier());

	return EActiveTimerReturnType::Continue;
}

TSharedRef<SWidget> SVirtualAssetsStatisticsDialog::GetGridPanel()
{
	const float RowMargin = 0.0f;
	const float TitleMargin = 10.0f;
	const float ColumnMargin = 10.0f;
	const FSlateColor TitleColor = FStyleColors::AccentWhite;
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

	TSharedRef<SGridPanel> Panel = SNew(SGridPanel);

	IVirtualizationSystem& System = IVirtualizationSystem::Get();

	FPayloadActivityInfo AcitvityInfo = System.GetPayloadActivityInfo();
	
	int32 Row = 0;

	Panel->AddSlot(1, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin, 0.0f, TitleMargin))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Justification(ETextJustify::Left)
		.Text(LOCTEXT("Payloads", "Payloads"))
	];

	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin, 0.0f, TitleMargin))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Justification(ETextJustify::Left)
		.Text(LOCTEXT("TotalSize", "TotalSize"))
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin, 0.0f, TitleMargin))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Justification(ETextJustify::Left)
		.Text(LOCTEXT("TotalSize", "Cycles"))
	];

	Row++;

	Panel->AddSlot(0, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text(LOCTEXT("Pull", "Pull"))
	];

	Panel->AddSlot(1, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat(AcitvityInfo.Pull.PayloadCount)); })
	];


	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat(AcitvityInfo.Pull.TotalBytes)); })
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat(AcitvityInfo.Pull.CyclesSpent)); })
	];

	Row++;

	Panel->AddSlot(0, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text(LOCTEXT("Push", "Push"))
	];

	Panel->AddSlot(1, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat(AcitvityInfo.Push.PayloadCount)); })
	];

	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat(AcitvityInfo.Push.TotalBytes)); })
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat(AcitvityInfo.Push.CyclesSpent)); })
	];

	Row++;

	Panel->AddSlot(0, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text(LOCTEXT("Cache", "Cache"))
	];

	Panel->AddSlot(1, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat(AcitvityInfo.Cache.PayloadCount)); })
	];

	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat(AcitvityInfo.Cache.TotalBytes)); })
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat(AcitvityInfo.Push.CyclesSpent)); })
	];

	
	return Panel;
}

#undef LOCTEXT_NAMESPACE
