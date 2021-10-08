// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HashSettings.generated.h"

USTRUCT()
struct LEVELSNAPSHOTS_API FHashSettings
{
	GENERATED_BODY()

	/**
	 * Performance trade-off. Used when filtering a snapshot.
	 * 
	 * For filtering, we need to load every actor into memory. Loading actors takes a long time.
	 * Instead when a snapshot is taken, we compute its hash. When filtering, we can recompute the hash using the actor
	 * in the editor world. If they match, we can skip loading the saved actor data.
	 *
	 * For most actors, it takes about 600 micro seconds to compute a hash. However, there are outliers which can take
	 * more. For such actors, it can be faster to just load the saved actor data into memory.
	 *
	 * Actors for which hashing took more than this configured variable, we skip hashing altogether and immediately load
	 * the actor data. 
	 */
	UPROPERTY(EditAnywhere, Category = "Level Snapshots")
	double HashCutoffSeconds = 5;

	/** Whether Level Snapshots will compute CRC32 hashes (when taking and analysing snapshots) */
	UPROPERTY(EditAnywhere, Category = "Level Snapshots")
	bool bCanUseCRC = true;

	/** Whether Level Snapshots will compute MD5 hashes (when taking and analysing snapshots) */
	UPROPERTY(EditAnywhere, Category = "Level Snapshots")
	bool bCanUseMD5 = true;
};