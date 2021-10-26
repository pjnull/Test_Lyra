// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/KahnTopologicalSort.h"

namespace Algo
{
	/**
	 * Sorts the given range in root to leaf order.
	 *
	 * @param UniqueRange A range with element type T
	 * @param GetElementDependencies A callable with prototype that is one of
	 *            RangeType<T> GetElementDependencies(const T& Element)
	 *            const RangeType<T>& GetElementDependencies(const T& Element)
	 *        It should return the leaf-ward vertices of directed edges from the root-wards Element.
	 */
	template <typename RangeType, typename GetElementDependenciesType>
	FORCEINLINE bool TopologicalSort(RangeType& UniqueRange, GetElementDependenciesType GetElementDependencies)
	{
		return KahnTopologicalSort(UniqueRange, GetElementDependencies);
	}
}