// Copyright Epic Games, Inc. All Rights Reserved.

#include "Archive/LoadSnapshotObjectArchive.h"

#include "LevelSnapshotsLog.h"
#include "LevelSnapshotsModule.h"
#include "WorldSnapshotData.h"
#include "Data/Util/SnapshotObjectUtil.h"

#include "Serialization/ObjectWriter.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"

void UE::LevelSnapshots::Private::FLoadSnapshotObjectArchive::ApplyToSnapshotWorldObject(FObjectSnapshotData& InObjectData, FWorldSnapshotData& InSharedData, UObject* InObjectToRestore, FProcessObjectDependency ProcessObjectDependency, UPackage* InLocalisationSnapshotPackage)
{
	ApplyToSnapshotWorldObject(
		InObjectData,
		InSharedData,
		InObjectToRestore,
		MoveTemp(ProcessObjectDependency),
#if USE_STABLE_LOCALIZATION_KEYS
		TextNamespaceUtil::EnsurePackageNamespace(InLocalisationSnapshotPackage)
#else
		FString()
#endif
		);
}

void UE::LevelSnapshots::Private::FLoadSnapshotObjectArchive::ApplyToSnapshotWorldObject(FObjectSnapshotData& InObjectData, FWorldSnapshotData& InSharedData, UObject* InObjectToRestore, FProcessObjectDependency ProcessObjectDependency, const FString& InLocalisationNamespace)
{
	UE_LOG(LogLevelSnapshots, Verbose, TEXT("Loading snapshot object %s (class %s)"), *InObjectToRestore->GetPathName(), *InObjectToRestore->GetClass()->GetPathName());
	
	FLoadSnapshotObjectArchive Archive(InObjectData, InSharedData, InObjectToRestore, MoveTemp(ProcessObjectDependency));
#if USE_STABLE_LOCALIZATION_KEYS
	Archive.SetLocalizationNamespace(InLocalisationNamespace);
#endif

	InObjectToRestore->Serialize(Archive);
	FLevelSnapshotsModule::GetInternalModuleInstance().OnPostLoadSnapshotObject({ InObjectToRestore, InSharedData });
}

UObject* UE::LevelSnapshots::Private::FLoadSnapshotObjectArchive::ResolveObjectDependency(int32 ObjectIndex) const
{
	FString LocalizationNamespace;
#if USE_STABLE_LOCALIZATION_KEYS
	LocalizationNamespace = GetLocalizationNamespace();
#endif

	ProcessObjectDependency(ObjectIndex);
	return ResolveObjectDependencyForSnapshotWorld(GetSharedData(), ObjectIndex, ProcessObjectDependency, LocalizationNamespace);
}

UE::LevelSnapshots::Private::FLoadSnapshotObjectArchive::FLoadSnapshotObjectArchive(FObjectSnapshotData& InObjectData, FWorldSnapshotData& InSharedData, UObject* InSerializedObject, FProcessObjectDependency ProcessObjectDependency)
	:
	Super(InObjectData, InSharedData, true, InSerializedObject),
	ProcessObjectDependency(ProcessObjectDependency)
{}
