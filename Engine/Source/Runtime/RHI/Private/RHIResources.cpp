// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIResources.h"

void FRHIResource::Destroy() const
{
	if (!AtomicFlags.MarkForDelete(std::memory_order_release))
	{
		while (true)
		{
			auto HP = MakeHazardPointer(PendingDeletes, PendingDeletesHPC);
			TClosableMpscQueue<FRHIResource*>* PendingDeletesPtr = HP.Get();
			if (PendingDeletesPtr->Enqueue(const_cast<FRHIResource*>(this)))
			{
				break;
			}
		}
	}
}