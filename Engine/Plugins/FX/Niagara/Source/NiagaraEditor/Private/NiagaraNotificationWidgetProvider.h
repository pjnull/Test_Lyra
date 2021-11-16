// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Widgets/Notifications/INotificationWidget.h"

#pragma once

class FNiagaraParameterNotificationWidgetProvider : public INotificationWidget
{
public:
	FNiagaraParameterNotificationWidgetProvider(TSharedRef<SWidget> WidgetToDisplay) : NotificationWidget(WidgetToDisplay) {}

	virtual ~FNiagaraParameterNotificationWidgetProvider() {}

	virtual void OnSetCompletionState(SNotificationItem::ECompletionState State) override {}
	virtual TSharedRef<SWidget> AsWidget() override { return NotificationWidget; }

private:
	TSharedRef<SWidget> NotificationWidget;
};