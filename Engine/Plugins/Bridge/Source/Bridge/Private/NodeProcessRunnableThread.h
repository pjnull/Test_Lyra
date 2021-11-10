// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

class NodeProcessRunnableThread : public FRunnable
{
	public:
		NodeProcessRunnableThread();
		virtual ~NodeProcessRunnableThread();

		// FRunnable functions
		virtual uint32 Run() override;

	protected:
		FRunnableThread* Thread = nullptr;
		bool bStopThread = false;
};
