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
	const double BytesToMegaBytes = 1.0 / (1024.0 * 1024.0);

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
		.Text(LOCTEXT("Count", "Count"))
	];

	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin, 0.0f, TitleMargin))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Justification(ETextJustify::Left)
		.Text(LOCTEXT("Size", "Size"))
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin, 0.0f, TitleMargin))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Justification(ETextJustify::Left)
		.Text(LOCTEXT("Time", "Time"))
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
		.Text_Lambda([AcitvityInfo] { return FText::FromString(FString::Printf(TEXT("%u"),AcitvityInfo.Pull.PayloadCount)); })
	];


	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo, BytesToMegaBytes] { return FText::FromString(SingleDecimalFormat((double)AcitvityInfo.Pull.TotalBytes*BytesToMegaBytes) + TEXT(" MB")); })
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat((double)AcitvityInfo.Pull.CyclesSpent*FPlatformTime::GetSecondsPerCycle()) + TEXT(" s")); })
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
		.Text_Lambda([AcitvityInfo] { return FText::FromString(FString::Printf(TEXT("%u"),AcitvityInfo.Push.PayloadCount)); })
	];

	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo,BytesToMegaBytes] { return FText::FromString(SingleDecimalFormat((double)AcitvityInfo.Push.TotalBytes*BytesToMegaBytes) + TEXT(" MB")); })
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat((double)AcitvityInfo.Push.CyclesSpent * FPlatformTime::GetSecondsPerCycle()) + TEXT(" s")); })
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
		.Text_Lambda([AcitvityInfo] { return FText::FromString(FString::Printf(TEXT("%u"),AcitvityInfo.Cache.PayloadCount)); })
	];

	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo, BytesToMegaBytes] { return FText::FromString(SingleDecimalFormat((double)AcitvityInfo.Cache.TotalBytes * BytesToMegaBytes) + TEXT(" MB")); })
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([AcitvityInfo] { return FText::FromString(SingleDecimalFormat((double)AcitvityInfo.Cache.CyclesSpent * FPlatformTime::GetSecondsPerCycle()) + TEXT(" s")); })
	];

	
	return Panel;
}

#undef LOCTEXT_NAMESPACE
