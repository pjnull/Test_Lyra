// Copyright Epic Games, Inc. All Rights Reserved.

#include "FoliageSupport/FoliageRestorationInfo.h"

#include "FoliageSupport/FoliageInfoData.h"
#include "Selection/AddedAndRemovedComponentInfo.h"
#include "Selection/RestorableObjectSelection.h"
#include "Selection/PropertySelectionMap.h"

#include "InstancedFoliageActor.h"
#include "LevelSnapshotsLog.h"

FFoliageRestorationInfo FFoliageRestorationInfo::From(AInstancedFoliageActor* Object, const FPropertySelectionMap& SelectionMap, bool bWasRecreated)
{
	FFoliageRestorationInfo Result;
	Result.bWasRecreated = bWasRecreated;
	
	const UE::LevelSnapshots::FRestorableObjectSelection ObjectSelection = SelectionMap.GetObjectSelection(Object);
	if (const UE::LevelSnapshots::FAddedAndRemovedComponentInfo* ComponentInfo = ObjectSelection.GetComponentSelection())
	{
		Result.EditorWorldComponentsToRemove = ComponentInfo->EditorWorldComponentsToRemove.Array();
		Result.SnapshotComponentsToAdd = ComponentInfo->SnapshotComponentsToAdd.Array();
	}

	for (UHierarchicalInstancedStaticMeshComponent* FoliageComp : TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*>(Object))
	{
		if (SelectionMap.HasChanges(FoliageComp))
		{
			Result.ModifiedComponents.Add(FoliageComp);
		}
	}

	return Result;
}

bool FFoliageRestorationInfo::ShouldSkipFoliageType(const FFoliageInfoData& SavedData) const
{
	return SavedData.GetImplType() != EFoliageImplType::StaticMesh;
}

bool FFoliageRestorationInfo::ShouldSerializeFoliageType(const FFoliageInfoData& SavedData) const
{
	if (bWasRecreated)
	{
		return true;
	}
	
	if (TOptional<FName> CompName = SavedData.GetComponentName(); CompName && SavedData.GetImplType() == EFoliageImplType::StaticMesh)
	{
		const bool bIsAddedComponent = SnapshotComponentsToAdd.ContainsByPredicate(
			[CompName](const TWeakObjectPtr<UActorComponent>& Component)
			{
				return Component.IsValid() && Component->GetFName().IsEqual(*CompName);
			});
		const bool bIsModifiedComponent = ModifiedComponents.ContainsByPredicate(
			[CompName](const UActorComponent* Component)
			{
				return Component->GetFName().IsEqual(*CompName);
			});

		return bIsAddedComponent || bIsModifiedComponent;
	}

	// Actor foliage is currently not supported: just restore everything
	UE_LOG(LogLevelSnapshots, Warning, TEXT("Only static mesh foliage is supported"));
	return false;
}
