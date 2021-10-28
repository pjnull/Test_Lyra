// Copyright Epic Games, Inc. All Rights Reserved.

#include "InsightsStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/ToolBarStyle.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FInsightsStyle
////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FInsightsStyle::FStyle> FInsightsStyle::StyleInstance = nullptr;

////////////////////////////////////////////////////////////////////////////////////////////////////

void FInsightsStyle::Initialize()
{
	LLM_SCOPE_BYNAME(TEXT("Insights/Style"));

	// The core style must be initialized before the editor style
	FSlateApplication::InitializeCoreStyle();

	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<FInsightsStyle::FStyle> FInsightsStyle::Create()
{
	TSharedRef<class FInsightsStyle::FStyle> NewStyle = MakeShareable(new FInsightsStyle::FStyle(FInsightsStyle::GetStyleSetName()));
	NewStyle->Initialize();
	return NewStyle;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FInsightsStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const ISlateStyle& FInsightsStyle::Get()
{
	return *StyleInstance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName FInsightsStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("InsightsStyle"));
	return StyleSetName;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FInsightsStyle::FStyle
////////////////////////////////////////////////////////////////////////////////////////////////////

FInsightsStyle::FStyle::FStyle(const FName& InStyleSetName)
	: FSlateStyleSet(InStyleSetName)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FInsightsStyle::FStyle::SyncParentStyles()
{
	const ISlateStyle* ParentStyle = GetParentStyle();

	NormalText = ParentStyle->GetWidgetStyle<FTextBlockStyle>("NormalText");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#define EDITOR_IMAGE_BRUSH(RelativePath, ...) IMAGE_BRUSH("../../Editor/Slate/" RelativePath, __VA_ARGS__)
#define EDITOR_IMAGE_BRUSH_SVG(RelativePath, ...) IMAGE_BRUSH_SVG("../../Editor/Slate/" RelativePath, __VA_ARGS__)
#define TODO_IMAGE_BRUSH(...) EDITOR_IMAGE_BRUSH_SVG("Starship/Common/StaticMesh", __VA_ARGS__)

void FInsightsStyle::FStyle::Initialize()
{
	SetParentStyleName("CoreStyle");

	// Sync styles from the parent style that will be used as templates for styles defined here
	SyncParentStyles();

	SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	const FVector2D Icon12x12(12.0f, 12.0f); // for TreeItem icons
	const FVector2D Icon16x16(16.0f, 16.0f); // for regular icons
	const FVector2D Icon20x20(20.0f, 20.0f); // for ToolBar icons

	Set("AppIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Insights/insights", FVector2D(45.0f, 45.0f)));
	Set("AppIconPadding", FMargin(5.0f, 5.0f, 5.0f, 5.0f));

	Set("AppIcon.Small", new CORE_IMAGE_BRUSH_SVG("Starship/Insights/insights", FVector2D(24.0f, 24.0f)));
	Set("AppIconPadding.Small", FMargin(4.0f, 4.0f, 0.0f, 0.0f));

	//////////////////////////////////////////////////

	Set("WhiteBrush", new FSlateColorBrush(FLinearColor::White));
	Set("SingleBorder", new FSlateBorderBrush(NAME_None, FMargin(1.0f)));
	Set("DoubleBorder", new FSlateBorderBrush(NAME_None, FMargin(2.0f)));

	Set("EventBorder", new FSlateBorderBrush(NAME_None, FMargin(1.0f)));
	Set("HoveredEventBorder", new FSlateBorderBrush(NAME_None, FMargin(2.0f)));
	Set("SelectedEventBorder", new FSlateBorderBrush(NAME_None, FMargin(2.0f)));

	Set("RoundedBackground", new FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), Icon16x16));

	Set("Border.TB", new BOX_BRUSH("Icons/Profiler/Profiler_Border_TB_16x", FMargin(4.0f / 16.0f)));
	Set("Border.L", new BOX_BRUSH("Icons/Profiler/Profiler_Border_L_16x", FMargin(4.0f / 16.0f)));
	Set("Border.R", new BOX_BRUSH("Icons/Profiler/Profiler_Border_R_16x", FMargin(4.0f / 16.0f)));

	Set("Graph.Point", new IMAGE_BRUSH("Old/Graph/ExecutionBubble", Icon16x16));

	//////////////////////////////////////////////////
	// Icons for major components

	Set("Icons.SessionInfo", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Info", Icon16x16));

	//////////////////////////////////////////////////
	// Trace Store, Connection, Launcher

	Set("Icons.TraceStore", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Home", Icon16x16));
	Set("Icons.Connection", new TODO_IMAGE_BRUSH(Icon16x16));
	Set("Icons.Launcher", new TODO_IMAGE_BRUSH(Icon16x16));

	//////////////////////////////////////////////////
	// Timing Insights

	Set("Icons.TimingProfiler", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Recent", Icon16x16));

	Set("Icons.FramesTrack", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Statistics", Icon16x16));
	Set("Icons.FramesTrack.ToolBar", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Statistics", Icon20x20));

	Set("Icons.TimingView", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Timeline", Icon16x16));
	Set("Icons.TimingView.ToolBar", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Timeline", Icon20x20));

	Set("Icons.TimersView", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Realtime", Icon16x16));
	Set("Icons.TimersView.ToolBar", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Realtime", Icon20x20));

	Set("Icons.CountersView", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Profile", Icon16x16));
	Set("Icons.CountersView.ToolBar", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Profile", Icon20x20));

	Set("Icons.CallersView", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Realtime", Icon16x16));
	Set("Icons.CallersView.ToolBar", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Realtime", Icon20x20));

	Set("Icons.CalleesView", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Realtime", Icon16x16));
	Set("Icons.CalleesView.ToolBar", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Realtime", Icon20x20));

	Set("Icons.LogView", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Log", Icon16x16));
	Set("Icons.LogView.ToolBar", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Log", Icon20x20));

	Set("Icons.TableTreeView", new TODO_IMAGE_BRUSH(Icon16x16));
	Set("Icons.TableTreeView.ToolBar", new TODO_IMAGE_BRUSH(Icon20x20));

	Set("Icons.TasksView", new TODO_IMAGE_BRUSH(Icon16x16));

	Set("Icons.AllTracksMenu.ToolBar", new CORE_IMAGE_BRUSH_SVG("Starship/Common/menu", Icon20x20));
	Set("Icons.CpuGpuTracksMenu.ToolBar", new CORE_IMAGE_BRUSH_SVG("Starship/Common/menu", Icon20x20));
	Set("Icons.OtherTracksMenu.ToolBar", new CORE_IMAGE_BRUSH_SVG("Starship/Common/menu", Icon20x20));
	Set("Icons.PluginTracksMenu.ToolBar", new CORE_IMAGE_BRUSH_SVG("Starship/Common/menu", Icon20x20));
	Set("Icons.ViewModeMenu.ToolBar", new CORE_IMAGE_BRUSH_SVG("Starship/Common/menu", Icon20x20));

	//////////////////////////////////////////////////
	// Asset Loading Insights

	Set("Icons.LoadingProfiler", new TODO_IMAGE_BRUSH(Icon16x16));

	//////////////////////////////////////////////////
	// Networking Insights

	Set("Icons.NetworkingProfiler", new TODO_IMAGE_BRUSH(Icon16x16));

	Set("Icons.PacketView", new TODO_IMAGE_BRUSH(Icon16x16));
	Set("Icons.PacketView.ToolBar", new TODO_IMAGE_BRUSH(Icon20x20));

	Set("Icons.PacketContentView", new TODO_IMAGE_BRUSH(Icon16x16));
	Set("Icons.PacketContentView.ToolBar", new TODO_IMAGE_BRUSH(Icon20x20));

	Set("Icons.NetStatsView", new TODO_IMAGE_BRUSH(Icon16x16));
	Set("Icons.NetStatsView.ToolBar", new TODO_IMAGE_BRUSH(Icon20x20));

	//////////////////////////////////////////////////
	// Memory Insights

	Set("Icons.MemoryProfiler", new TODO_IMAGE_BRUSH(Icon16x16));

	Set("Icons.MemTagTreeView", new TODO_IMAGE_BRUSH(Icon16x16));
	Set("Icons.MemTagTreeView.ToolBar", new TODO_IMAGE_BRUSH(Icon20x20));

	Set("Icons.MemInvestigationView", new TODO_IMAGE_BRUSH(Icon16x16));
	Set("Icons.MemInvestigationView.ToolBar", new TODO_IMAGE_BRUSH(Icon20x20));

	Set("Icons.MemAllocTableTreeView", new TODO_IMAGE_BRUSH(Icon16x16));

	Set("Icons.AddAllMemTagGraphs", new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus-circle", Icon16x16));
	Set("Icons.RemoveAllMemTagGraphs", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", Icon16x16));

	//////////////////////////////////////////////////
	// Tasks

	Set("Icons.GoToTask", new IMAGE_BRUSH("Icons/Profiler/profiler_ViewColumn_32x", Icon16x16));
	Set("Icons.ShowTaskCriticalPath", new IMAGE_BRUSH("Icons/Profiler/profiler_HotPath_32x", Icon16x16));
	Set("Icons.ShowTaskDependencies", new IMAGE_BRUSH("Icons/Profiler/profiler_Calls_32x", Icon16x16));
	Set("Icons.ShowTaskPrerequisites", new IMAGE_BRUSH("Icons/Profiler/profiler_Calls_32x", Icon16x16));
	Set("Icons.ShowTaskSubsequents", new IMAGE_BRUSH("Icons/Profiler/profiler_Calls_32x", Icon16x16));
	Set("Icons.ShowNestedTasks", new IMAGE_BRUSH("Icons/Profiler/profiler_Calls_32x", Icon16x16));

	//////////////////////////////////////////////////

	Set("Icons.FindFirst.ToolBar", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-left", Icon20x20));
	Set("Icons.FindPrevious.ToolBar", new EDITOR_IMAGE_BRUSH("Icons/GeneralTools/Previous_40x", Icon20x20));
	Set("Icons.FindNext.ToolBar", new EDITOR_IMAGE_BRUSH("Icons/GeneralTools/Next_40x", Icon20x20));
	Set("Icons.FindLast.ToolBar", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-right", Icon20x20));

	//////////////////////////////////////////////////

	Set("Icons.ResetToDefault", new IMAGE_BRUSH("Icons/Profiler/profiler_ResetToDefault_32x", Icon16x16));
	Set("Icons.DiffersFromDefault", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/ResetToDefault", Icon16x16));

	//Set("Icons.Filter", new CORE_IMAGE_BRUSH_SVG("Starship/Common/filter", Icon16x16));	//-> use FAppStyle "Icons.Filter"
	Set("Icons.Filter.ToolBar", new CORE_IMAGE_BRUSH_SVG("Starship/Common/filter", Icon20x20));
	Set("Icons.Find", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/TraceDataFiltering", Icon16x16));
	Set("Icons.FilterAddGroup", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/WorldOutliner", Icon16x16));

	Set("Icons.FolderExplore", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/ContentBrowser", Icon16x16));
	//Set("Icons.FolderOpen", new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-open", Icon16x16));		//-> use FAppStyle "Icons.FolderOpen"
	//Set("Icons.FolderClosed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-closed", Icon16x16));	//-> use FAppStyle "Icons.FolderClosed"

	Set("Icons.SortBy", new IMAGE_BRUSH("Icons/Profiler/profiler_SortBy_32x", Icon16x16));
	//Set("Icons.SortAscending", new CORE_IMAGE_BRUSH_SVG("Starship/Common/SortUp", Icon16x16));	//-> use FAppStyle "Icons.SortUp"
	//Set("Icons.SortDescending", new CORE_IMAGE_BRUSH_SVG("Starship/Common/SortDown", Icon16x16));	//-> use FAppStyle "Icons.SortDown"

	Set("Icons.ViewColumn", new IMAGE_BRUSH("Icons/Profiler/profiler_ViewColumn_32x", Icon16x16));
	Set("Icons.ResetColumn", new IMAGE_BRUSH("Icons/Profiler/profiler_ResetColumn_32x", Icon16x16));

	Set("Icons.ExpandAll", new IMAGE_BRUSH("Icons/Profiler/profiler_ExpandAll_32x", Icon16x16));
	Set("Icons.CollapseAll", new IMAGE_BRUSH("Icons/Profiler/profiler_CollapseAll_32x", Icon16x16));
	Set("Icons.ExpandSelection", new IMAGE_BRUSH("Icons/Profiler/profiler_ExpandSelection_32x", Icon16x16));
	Set("Icons.CollapseSelection", new IMAGE_BRUSH("Icons/Profiler/profiler_CollapseSelection_32x", Icon16x16));

	Set("Icons.ToggleShowGraphSeries", new IMAGE_BRUSH("Icons/Profiler/profiler_ShowGraphData_32x", Icon16x16));

	Set("Icons.TestAutomation", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/TestAutomation", Icon16x16));
	Set("Icons.Test", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Test", Icon16x16));

	Set("Icons.Debug", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/bug", Icon16x16));
	Set("Icons.Debug.ToolBar", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/bug", Icon20x20));

	Set("Icons.AutoScroll", new EDITOR_IMAGE_BRUSH_SVG("Starship/MainToolbar/play", Icon16x16));

	Set("Icons.ZeroCountFilter", new CORE_IMAGE_BRUSH_SVG("Starship/Common/filter", Icon16x16));

	//////////////////////////////////////////////////

	Set("TreeTable.RowBackground", new IMAGE_BRUSH("Old/White", Icon16x16, FLinearColor(1.0f, 1.0f, 1.0f, 0.25f)));

	Set("Icons.Hint.TreeItem", new IMAGE_BRUSH("Icons/Profiler/Profiler_Custom_Tooltip_12x", Icon12x12));
	Set("Icons.HotPath.TreeItem", new IMAGE_BRUSH("Icons/Profiler/profiler_HotPath_32x", Icon12x12));
	Set("Icons.Group.TreeItem", new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-closed", Icon12x12));
	Set("Icons.Leaf.TreeItem", new CORE_IMAGE_BRUSH_SVG("Starship/Common/file", Icon12x12));
	Set("Icons.GpuTimer.TreeItem", new TODO_IMAGE_BRUSH(Icon12x12));
	Set("Icons.CpuTimer.TreeItem", new TODO_IMAGE_BRUSH(Icon12x12));
	Set("Icons.Counter.TreeItem", new TODO_IMAGE_BRUSH(Icon12x12));
	Set("Icons.StatCounter.TreeItem", new TODO_IMAGE_BRUSH(Icon12x12));
	Set("Icons.DataTypeDouble.TreeItem", new TODO_IMAGE_BRUSH(Icon12x12));
	Set("Icons.DataTypeInt64.TreeItem", new TODO_IMAGE_BRUSH(Icon12x12));
	Set("Icons.NetEvent.TreeItem", new TODO_IMAGE_BRUSH(Icon12x12));
	Set("Icons.MemTag.TreeItem", new TODO_IMAGE_BRUSH(Icon12x12));

	Set("TreeTable.TooltipBold", FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Bold", 8))
		.SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
		.SetShadowOffset(FVector2D(1.0f, 1.0f))
		.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
	);

	Set("TreeTable.Tooltip", FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Regular", 8))
		.SetColorAndOpacity(FLinearColor::White)
		.SetShadowOffset(FVector2D(1.0f, 1.0f))
		.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
	);

	//////////////////////////////////////////////////

	// PrimaryToolbar
	{
		FToolBarStyle PrimaryToolbarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("SlimToolBar");

		Set("PrimaryToolbar", PrimaryToolbarStyle);

		Set("PrimaryToolbar.MinUniformToolbarSize", 40.0f);
		Set("PrimaryToolbar.MaxUniformToolbarSize", 40.0f);
	}

	// SecondaryToolbar
	{
		FToolBarStyle SecondaryToolbarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("SlimToolBar");

		SecondaryToolbarStyle.SetBackgroundPadding(         FMargin(4.0f, 4.0f));
		SecondaryToolbarStyle.SetBlockPadding(              FMargin(2.0f, 0.0f));
		SecondaryToolbarStyle.SetButtonPadding(             FMargin(0.0f, 0.0f));
		SecondaryToolbarStyle.SetCheckBoxPadding(           FMargin(2.0f, 0.0f));
		SecondaryToolbarStyle.SetComboButtonPadding(        FMargin(0.0f, 0.0f));
		SecondaryToolbarStyle.SetIndentedBlockPadding(      FMargin(2.0f, 0.0f));
		SecondaryToolbarStyle.SetLabelPadding(              FMargin(2.0f, 0.0f));
		SecondaryToolbarStyle.SetSeparatorPadding(          FMargin(2.0f, -3.0f));

		SecondaryToolbarStyle.ToggleButton.SetPadding(      FMargin(0.0f, 0.0f));

		SecondaryToolbarStyle.ButtonStyle.SetNormalPadding( FMargin(6.0f, 2.0f, 4.0f, 2.0f));
		SecondaryToolbarStyle.ButtonStyle.SetPressedPadding(FMargin(6.0f, 2.0f, 4.0f, 2.0f));

		//SecondaryToolbarStyle.IconSize.Set(16.0f, 16.0f);

		Set("SecondaryToolbar", SecondaryToolbarStyle);

		Set("SecondaryToolbar.MinUniformToolbarSize", 32.0f);
		Set("SecondaryToolbar.MaxUniformToolbarSize", 32.0f);
	}

	// SecondaryToolbar2 (used by AutoScroll and NetPacketContentView toolbars)
	{
		FToolBarStyle SecondaryToolbarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("SlimToolBar");

		SecondaryToolbarStyle.SetBackgroundPadding(         FMargin(4.0f, 2.0f));
		SecondaryToolbarStyle.SetBlockPadding(              FMargin(2.0f, 2.0f));
		SecondaryToolbarStyle.SetButtonPadding(             FMargin(0.0f, 2.0f));
		SecondaryToolbarStyle.SetCheckBoxPadding(           FMargin(2.0f, 2.0f));
		SecondaryToolbarStyle.SetComboButtonPadding(        FMargin(0.0f, 2.0f));
		SecondaryToolbarStyle.SetIndentedBlockPadding(      FMargin(2.0f, 2.0f));
		SecondaryToolbarStyle.SetLabelPadding(              FMargin(2.0f, 2.0f));
		SecondaryToolbarStyle.SetSeparatorPadding(          FMargin(2.0f, -1.0f));

		SecondaryToolbarStyle.ToggleButton.SetPadding(      FMargin(0.0f, 0.0f));

		SecondaryToolbarStyle.ButtonStyle.SetNormalPadding( FMargin(3.0f, 0.0f, -1.0f, 0.0f));
		SecondaryToolbarStyle.ButtonStyle.SetPressedPadding(FMargin(3.0f, 0.0f, -1.0f, 0.0f));

		//SecondaryToolbarStyle.IconSize.Set(16.0f, 16.0f);

		Set("SecondaryToolbar2", SecondaryToolbarStyle);

		Set("SecondaryToolbar2.MinUniformToolbarSize", 32.0f);
		Set("SecondaryToolbar2.MaxUniformToolbarSize", 32.0f);
	}
}

#undef TODO_IMAGE_BRUSH
#undef EDITOR_IMAGE_BRUSH_SVG
#undef EDITOR_IMAGE_BRUSH

////////////////////////////////////////////////////////////////////////////////////////////////////
