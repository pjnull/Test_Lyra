// Copyright Epic Games, Inc. All Rights Reserved.

#include "Types/TrackedMetaData.h"
#include "Widgets/Accessibility/SlateWidgetTracker.h"

FTrackedMetaData::FTrackedMetaData(const SWidget* InTrackedWidget, TArray<FName> InTags)
{
#if WITH_SLATE_WIDGET_TRACKING
	if (ensure(InTrackedWidget != nullptr && InTags.Num() > 0))
	{
		TrackedWidget = InTrackedWidget;
		Tags = MoveTemp(InTags);
		FSlateWidgetTracker::Get().AddTrackedWidget(TrackedWidget, Tags);
	}
#endif //WITH_SLATE_WIDGET_TRACKING
}

FTrackedMetaData::FTrackedMetaData(const SWidget* InTrackedWidget, FName InTags)
{
#if WITH_SLATE_WIDGET_TRACKING
	if (ensure(InTrackedWidget != nullptr && !InTags.IsNone()))
	{
		TrackedWidget = InTrackedWidget;
		Tags.Emplace(MoveTemp(InTags));
		FSlateWidgetTracker::Get().AddTrackedWidget(TrackedWidget, Tags);
	}
#endif //WITH_SLATE_WIDGET_TRACKING
}

FTrackedMetaData::~FTrackedMetaData()
{
#if WITH_SLATE_WIDGET_TRACKING
	if (TrackedWidget != nullptr)
	{
		FSlateWidgetTracker::Get().RemoveTrackedWidget(TrackedWidget);
	}
#endif //WITH_SLATE_WIDGET_TRACKING
}

