// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_SLATE_WIDGET_TRACKING

#include "Widgets/Accessibility/SlateWidgetTracker.h"
#include "Types/ISlateMetaData.h"
#include "Widgets/SWidget.h"
#include "Misc/MemStack.h"

FSlateWidgetTracker& FSlateWidgetTracker::Get()
{
	static FSlateWidgetTracker Singleton;
	return Singleton;
}

void FSlateWidgetTracker::AddTrackedWidget(const SWidget* WidgetToTrack, const TArray<FName>& Tags)
{
	if (WidgetToTrack != nullptr)
	{
		for (const FName& Tag : Tags)
		{
			AddTrackedWidget(WidgetToTrack, Tag);
		}
	}
}

void FSlateWidgetTracker::AddTrackedWidget(const SWidget* WidgetToTrack, FName Tag)
{
	if (WidgetToTrack != nullptr)
	{
		TrackedWidgets.FindOrAdd(Tag).Add(WidgetToTrack);
		if (FTrackedWidgetsChangedEvent* TrackedWidgetsChangedEvent = TrackedWidgetsChangedEvents.Find(Tag))
		{
			TrackedWidgetsChangedEvent->Broadcast(WidgetToTrack, Tag, ETrackedSlateWidgetOperations::AddedTrackedWidget);
		}
	}
}

void FSlateWidgetTracker::RemoveTrackedWidget(const SWidget* WidgetToStopTracking)
{
	if (WidgetToStopTracking != nullptr)
	{
		FMemMark Mark(FMemStack::Get());
		TArray<FName, TMemStackAllocator<>> AllTags;
		TrackedWidgets.GenerateKeyArray(AllTags);
		for (const FName& Tag : AllTags)
		{
			if (TArray<const SWidget*>* TrackedWidgetsOfTag = TrackedWidgets.Find(Tag))
			{
				TrackedWidgetsOfTag->Remove(WidgetToStopTracking);

				if (TrackedWidgetsOfTag->Num() == 0)
				{
					TrackedWidgets.Remove(Tag);
				}

				if (FTrackedWidgetsChangedEvent* TrackedWidgetsChangedEvent = TrackedWidgetsChangedEvents.Find(Tag))
				{
					TrackedWidgetsChangedEvent->Broadcast(WidgetToStopTracking, Tag, ETrackedSlateWidgetOperations::RemovedTrackedWidget);
				}
			}
		}
	}
}

FTrackedWidgetsChangedEvent& FSlateWidgetTracker::OnTrackedWidgetsChanged(const FName& Tag)
{
	return TrackedWidgetsChangedEvents.FindOrAdd(Tag);
}

#endif //WITH_SLATE_WIDGET_TRACKING
