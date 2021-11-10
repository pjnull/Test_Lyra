// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionEditorPerProjectUserSettings.h"
#include "Engine/World.h"
#include "WorldPartition/WorldPartition.h"
#include "GameFramework/WorldSettings.h"

#if WITH_EDITOR

void UWorldPartitionEditorPerProjectUserSettings::UpdateEditorGridConfigHash(UWorld* InWorld, FWorldPartitionPerWorldSettings& PerWorldSettings)
{
	const int32 NewEditorGridConfigHash = InWorld->GetWorldPartition()->GetEditorGridConfigHash();
	if (PerWorldSettings.EditorGridConfigHash != NewEditorGridConfigHash)
	{
		// don't reset LoadedEditorGridCells if Hash is default value.
		if (PerWorldSettings.EditorGridConfigHash != 0)
		{
			PerWorldSettings.LoadedEditorGridCells.Empty();
		}
		PerWorldSettings.EditorGridConfigHash = NewEditorGridConfigHash;
	}
}

void UWorldPartitionEditorPerProjectUserSettings::UpdateEditorGridConfigHash(UWorld* InWorld)
{
	if (ShouldSaveSettings(InWorld))
	{
		if (FWorldPartitionPerWorldSettings* PerWorldSettings = PerWorldEditorSettings.Find(TSoftObjectPtr<UWorld>(InWorld)))
		{
			const int32 PreviousEditorGridConfigHash = PerWorldSettings->EditorGridConfigHash;
			UpdateEditorGridConfigHash(InWorld, *PerWorldSettings);
			if (PerWorldSettings->EditorGridConfigHash != PreviousEditorGridConfigHash)
			{
				SaveConfig();
			}
		}
	}
}

void UWorldPartitionEditorPerProjectUserSettings::SetWorldDataLayersNonDefaultEditorLoadStates(UWorld* InWorld, const TArray<FName>& InDataLayersLoadedInEditor, const TArray<FName>& InDataLayersNotLoadedInEditor)
{
	if (ShouldSaveSettings(InWorld))
	{
		FWorldPartitionPerWorldSettings& PerWorldSettings = PerWorldEditorSettings.FindOrAdd(TSoftObjectPtr<UWorld>(InWorld));
		UpdateEditorGridConfigHash(InWorld, PerWorldSettings);
		PerWorldSettings.NotLoadedDataLayers = InDataLayersNotLoadedInEditor;
		PerWorldSettings.LoadedDataLayers = InDataLayersLoadedInEditor;
		
		SaveConfig();
	}
}

void UWorldPartitionEditorPerProjectUserSettings::SetEditorGridLoadedCells(UWorld* InWorld, const TArray<FName>& InEditorGridLoadedCells)
{
	if (ShouldSaveSettings(InWorld))
	{
		FWorldPartitionPerWorldSettings& PerWorldSettings = PerWorldEditorSettings.FindOrAdd(TSoftObjectPtr<UWorld>(InWorld));
		UpdateEditorGridConfigHash(InWorld, PerWorldSettings);
		PerWorldSettings.LoadedEditorGridCells = InEditorGridLoadedCells;
		
		SaveConfig();
	}
}

TArray<FName> UWorldPartitionEditorPerProjectUserSettings::GetEditorGridLoadedCells(UWorld* InWorld) const
{
	if (const FWorldPartitionPerWorldSettings* PerWorldSettings = GetWorldPartitionPerWorldSettings(InWorld))
	{
		return PerWorldSettings->LoadedEditorGridCells;
	}

	return TArray<FName>();
}

TArray<FName> UWorldPartitionEditorPerProjectUserSettings::GetWorldDataLayersNotLoadedInEditor(UWorld* InWorld) const
{
	if (const FWorldPartitionPerWorldSettings* PerWorldSettings = GetWorldPartitionPerWorldSettings(InWorld))
	{
		return PerWorldSettings->NotLoadedDataLayers;
	}

	return TArray<FName>();
}

TArray<FName> UWorldPartitionEditorPerProjectUserSettings::GetWorldDataLayersLoadedInEditor(UWorld* InWorld) const
{
	if (const FWorldPartitionPerWorldSettings* PerWorldSettings = GetWorldPartitionPerWorldSettings(InWorld))
	{
		return PerWorldSettings->LoadedDataLayers;
	}
	
	return TArray<FName>();
}

const FWorldPartitionPerWorldSettings* UWorldPartitionEditorPerProjectUserSettings::GetWorldPartitionPerWorldSettings(UWorld* InWorld) const
{
	if (const FWorldPartitionPerWorldSettings* ExistingPerWorldSettings = PerWorldEditorSettings.Find(TSoftObjectPtr<UWorld>(InWorld)))
	{
		return ExistingPerWorldSettings;
	}
	else if (const FWorldPartitionPerWorldSettings* DefaultPerWorldSettings = InWorld->GetWorldSettings()->GetDefaultWorldPartitionSettings())
	{
		return DefaultPerWorldSettings;
	}

	return nullptr;
}

#endif