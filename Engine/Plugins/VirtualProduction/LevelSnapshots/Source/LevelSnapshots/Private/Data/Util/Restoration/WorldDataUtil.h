// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/NonNullPointer.h"

class AActor;
class UActorComponent;
class UWorld;

struct FActorSnapshotData;
struct FComponentSnapshotData;
struct FObjectSnapshotData;
struct FPropertySelectionMap;
struct FWorldSnapshotData;

namespace UE::LevelSnapshots::Private
{
	/** Captures the state of the world */
	FWorldSnapshotData SnapshotWorld(UWorld* World);
	
	/* Applies the saved properties to WorldActor */
	void ApplyToWorld(FWorldSnapshotData& WorldData, UWorld* WorldToApplyTo, UPackage* LocalisationSnapshotPackage, const FPropertySelectionMap& PropertiesToSerialize);


	
	/** Sets the world reference */
	void InitWithSnapshotWorld(FWorldSnapshotData& WorldData, UWorld* SnapshotWorld);
	
	/** Clears all references to the snapshot world */
	void ClearSnapshotWorldReferences(FWorldSnapshotData& WorldData);

	

	/** Gets the state of the CDO from when the snapshot was taken. CDO is saved when a snapshot is taken so CDO changes can be detected. */
	TOptional<TNonNullPtr<FObjectSnapshotData>> GetSerializedClassDefaults(FWorldSnapshotData& WorldData, UClass* Class);
	
	/** Adds a class default entry in the world data */
	void AddClassDefault(FWorldSnapshotData& WorldData, UClass* Class);
	
	/** Gets a saved class default object for the given class */
	UObject* GetClassDefault(FWorldSnapshotData& WorldData, UClass* Class);
	
	/**
	 * Gets the Object's class and serializes the saved CDO into it.
	 * This is intended for cases where you cannot specify a template object for new objects. Components are one such use case.
	 */
	void SerializeClassDefaultsInto(FWorldSnapshotData& WorldData, UObject* Object);
}
