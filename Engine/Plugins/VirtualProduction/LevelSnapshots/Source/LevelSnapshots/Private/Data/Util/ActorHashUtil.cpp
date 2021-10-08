// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/Util/ActorHashUtil.h"

#include "LevelSnapshotsLog.h"
#include "Data/ActorSnapshotHash.h"
#include "Util/HashArchive.h"
#include "Util/SerializeObjectState.h"

#include "Serialization/BufferArchive.h"
#include "Stats/StatsMisc.h"

FHashSettings SnapshotUtil::GHashSettings;

void SnapshotUtil::PopulateActorHash(FActorSnapshotHash& ActorData, AActor* WorldActor)
{
	SCOPED_SNAPSHOT_CORE_TRACE(PopulateActorHash);

	// We compute both CRC32 and MD5 because:
		// 1. Future proofing: if it turns out one has too many collisions, we can switch to the other without migration
		// 2. Sometimes one is faster than the other. When loading, we use the one that took the least time.
		// 3. The overhead is ok. Both together take 1 ms on average.

	if (GHashSettings.bCanUseCRC)
	{
		const double StartTime = FPlatformTime::Seconds();

		// FArchiveComputeFullCrc32 is faster than FArchiveComputeIncrementalCrc32 for actors
		FArchiveComputeFullCrc32 CrcArchive;
		SerializeObjectState::SerializeWithoutObjectName(CrcArchive, WorldActor);
		ActorData.Crc32DataLength = CrcArchive.GetSerializedData().Num();
		ActorData.Crc32 = CrcArchive.GetCrc32();
		ActorData.MicroSecondsForCrc = FPlatformTime::Seconds() - StartTime;
	}
	
	if (GHashSettings.bCanUseMD5)
	{
		const double StartTime = FPlatformTime::Seconds();
		
		// FArchiveComputeIncrementalMD5 is faster than FArchiveComputeFullMD5 for actors
		FArchiveComputeIncrementalMD5 MD5Archive;
		SerializeObjectState::SerializeWithoutObjectName(MD5Archive, WorldActor);
		ActorData.MD5 = MD5Archive.GetMD5();
		ActorData.MicroSecondsForMD5 = FPlatformTime::Seconds() - StartTime;
	}
}

bool SnapshotUtil::HasMatchingHash(const FActorSnapshotHash& ActorData, AActor* WorldActor)
{
	SCOPED_SNAPSHOT_CORE_TRACE(HasMatchingHash);

	const bool bCrcIsFaster = ActorData.MicroSecondsForCrc < ActorData.MicroSecondsForMD5;
	if (bCrcIsFaster && GHashSettings.bCanUseCRC)
	{
		FArchiveComputeFullCrc32 Archive;
		SerializeObjectState::SerializeWithoutObjectName(Archive, WorldActor);
		return ActorData.MicroSecondsForCrc < GHashSettings.HashCutoffSeconds && ActorData.Crc32DataLength == Archive.GetSerializedData().Num() && ActorData.Crc32 == Archive.GetCrc32();
	}

	if (GHashSettings.bCanUseMD5)
	{
		FArchiveComputeIncrementalMD5 Archive;
		SerializeObjectState::SerializeWithoutObjectName(Archive, WorldActor);
		return ActorData.MicroSecondsForMD5 < GHashSettings.HashCutoffSeconds && ActorData.MD5 == Archive.GetMD5();
	}

	return false;
}