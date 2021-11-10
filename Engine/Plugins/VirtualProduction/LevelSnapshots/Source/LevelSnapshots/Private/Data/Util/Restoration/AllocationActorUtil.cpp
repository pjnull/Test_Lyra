// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/Util/Restoration/ActorUtil.h"

#include "Data/ActorSnapshotData.h"
#include "Data/WorldSnapshotData.h"
#include "Data/SnapshotCustomVersion.h"
#include "CustomSerialization/CustomObjectSerializationWrapper.h"
#include "Data/Util/SnapshotObjectUtil.h"
#include "LevelSnapshotsLog.h"
#include "LevelSnapshotsModule.h"
#include "LoadSnapshotObjectArchive.h"
#include "Selection/PropertySelectionMap.h"
#include "Util/EquivalenceUtil.h"
#include "Util/Component/SnapshotComponentUtil.h"
#include "WorldDataUtil.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Templates/NonNullPointer.h"
#include "UObject/Package.h"
#include "UObject/Script.h"

#if USE_STABLE_LOCALIZATION_KEYS
#include "Internationalization/TextPackageNamespaceUtil.h"
#endif

namespace UE::LevelSnapshots::Private::Internal
{
	static void PostSerializeSnapshotActor(AActor* SnapshotActor, FWorldSnapshotData& WorldData, const FSoftObjectPath& OriginalActorPath, UPackage* InLocalisationSnapshotPackage)
	{
		struct Local
		{
			static void RecreateRootComponentIfInstanced(AActor* OriginalActor, AActor* SnapshotActor)
			{
				USceneComponent* OriginalRoot = OriginalActor->GetRootComponent();
				
				const bool bSnapshotIsMissingRoot = SnapshotActor->GetRootComponent() == nullptr;
				// Some actors, e.g. exact instances of AActor, have no components and the editor creates an instanced root component
				const bool bSnapshotWasSavedWithoutRootComponentData = OriginalRoot && OriginalRoot->CreationMethod == EComponentCreationMethod::Instance;
				if (bSnapshotIsMissingRoot && bSnapshotWasSavedWithoutRootComponentData)
				{
					USceneComponent* SnapshotRoot = DuplicateObject<USceneComponent>(OriginalRoot, SnapshotActor, OriginalRoot->GetFName());
					UE_CLOG(SnapshotRoot == nullptr, LogLevelSnapshots, Warning, TEXT("Failed to recreate instanced root component for %s's snapshot counterpart"), *OriginalActor->GetPathName());
					if (SnapshotRoot)
					{
						SnapshotActor->SetRootComponent(
							SnapshotRoot
						);
					}
				}
			}
			
			static void HandleAttachParentNotSaved(AActor* OriginalActor, AActor* SnapshotActor, FWorldSnapshotData& WorldData, UPackage* InLocalisationSnapshotPackage)
			{
				USceneComponent* RootComponent = OriginalActor->GetRootComponent();
				// WorldSettings does not have a root component...
				if (!RootComponent)
				{
					return;
				}
				
				USceneComponent* WorldAttachParent = RootComponent->GetAttachParent();
				if (!WorldAttachParent)
				{
					return;
				}
				const TOptional<TNonNullPtr<AActor>> SnapshotAttachParent = UE::LevelSnapshots::Private::GetDeserializedActor(WorldAttachParent->GetOwner(), WorldData, InLocalisationSnapshotPackage);
				if (!SnapshotAttachParent)
				{
					return;
				}

				const TInlineComponentArray<USceneComponent*> ComponentArray(SnapshotAttachParent.GetValue());
				USceneComponent* const* SnapshotAttachComponent = ComponentArray.FindByPredicate([WorldAttachParent](USceneComponent* Comp)
				{
					return Comp->GetFName() == WorldAttachParent->GetFName();
				});

				if (SnapshotAttachComponent)
				{
					SnapshotActor->AttachToComponent(
						*SnapshotAttachComponent,
						// Do not change property values: anything other than KeepRelativeTransform will recompute
						FAttachmentTransformRules::KeepRelativeTransform,
						RootComponent->GetAttachSocketName()
						);
				}
				else
				{
					UE_LOG(LogLevelSnapshots, Warning, TEXT("Failed to fix up AttachParent for %s's snapshot counterpart"), *OriginalActor->GetPathName());
				}
			}
		};
		
		const int32 SavedVersion = WorldData.SnapshotVersionInfo.GetSnapshotCustomVersion();
		AActor* OriginalActor = Cast<AActor>(OriginalActorPath.ResolveObject());

		// USceneComponent::AttachParent was not yet saved.
		if (SavedVersion < FSnapshotCustomVersion::SubobjectSupport
			&& OriginalActor)
		{
			Local::RecreateRootComponentIfInstanced(OriginalActor, SnapshotActor);
			// To avoid lots of actors being unparented by accident, we set the snapshot actor's attach parent to the equivalent of the editor world
			Local::HandleAttachParentNotSaved(OriginalActor, SnapshotActor, WorldData, InLocalisationSnapshotPackage);
		}
	}

	using FHandleFoundComponent = TFunctionRef<void(FSubobjectSnapshotData& SerializedCompData, const FComponentSnapshotData& ComponentMetaData, UActorComponent* ActorComp, const FSoftObjectPath& OriginalComponentPath, FWorldSnapshotData& SharedData)>;

	static void DeserializeComponents(
		const FActorSnapshotData& ActorData,
		AActor* IntoActor,
		FWorldSnapshotData& WorldData,
		FHandleFoundComponent HandleComponent
	)
	{
		for (auto CompIt = ActorData.ComponentData.CreateConstIterator(); CompIt; ++CompIt)
		{
			const EComponentCreationMethod CreationMethod = CompIt->Value.CreationMethod;
			if (CreationMethod == EComponentCreationMethod::UserConstructionScript)	// Construction script components are not supported 
			{
				continue;
			}

			const int32 SubobjectIndex = CompIt->Key;
			const FSoftObjectPath& OriginalComponentPath = WorldData.SerializedObjectReferences[SubobjectIndex];
			if (UActorComponent* ComponentToRestore = FindMatchingComponent(IntoActor, OriginalComponentPath))
			{
				FSubobjectSnapshotData& SnapshotData = WorldData.Subobjects[SubobjectIndex];
				HandleComponent(SnapshotData, CompIt->Value, ComponentToRestore, OriginalComponentPath, WorldData);
			}
		}
	}

	static void DeserializeSubobjectsForSnapshotActor(const FActorSnapshotData& ActorData, FWorldSnapshotData& WorldData, const FProcessObjectDependency& ProcessObjectDependency, UPackage* InLocalisationSnapshotPackage)
	{
		for (const int32 SubobjectIndex : ActorData.OwnedSubobjects)
		{
			check(!ActorData.ComponentData.Contains(SubobjectIndex));

			FString LocalisationNamespace;
#if USE_STABLE_LOCALIZATION_KEYS
			LocalisationNamespace = TextNamespaceUtil::EnsurePackageNamespace(InLocalisationSnapshotPackage);
#endif

			// Ensures the object is allocated and serialized into. Return value not needed.
			ResolveObjectDependencyForSnapshotWorld(
				WorldData,
				SubobjectIndex,
				ProcessObjectDependency,
				LocalisationNamespace
			);
		}
	}

	static void DeserializeIntoTransient(FObjectSnapshotData& SerializedComponentData, UActorComponent* ComponentToDeserializeInto, FWorldSnapshotData& WorldData, const FProcessObjectDependency& ProcessObjectDependency, UPackage* InLocalisationSnapshotPackage)
	{
		if (ComponentToDeserializeInto->CreationMethod == EComponentCreationMethod::UserConstructionScript)
		{
			UE_LOG(LogLevelSnapshots, Warning, TEXT("Components created dynamically in the construction script are not supported (%s). Skipping..."), *ComponentToDeserializeInto->GetPathName());
			return;
		}
		
		// We cannot supply a template object for components whose CreationMethod is not Instance.
		// Yet every actor class, native and Blueprint, has their own archetype for each component, i.e. in the details panel the property has a yellow "Reset to Default" icon next to it if the value is different from the Blueprint's value.
		// We apply the CDO data we saved before we apply the component's serialized data.
			// Scenario: We have a custom Blueprint StaticMeshBP that has a UStaticMeshComponent. Suppose we change some default property value, like ComponentTags.
			// 1st case: We change no default values, then take a snapshot.
			//   - After the snapshot was taken, we change a default value.
			//   - The value of the property was not saved in the component's serialized data because the value was equal to the CDO's value.
			//   - When applying the snapshot, we will override the new default value because we serialize the CDO here. Good.
			// 2nd case: We change some default value, then take a snapshot
			//   - The value was saved into the component's serialized data because it was different from the CDO's default value at the time of taking the snapshot
			//   - We apply the CDO and afterwards we override it with the serialized data. Good.
		SerializeClassDefaultsInto(WorldData, ComponentToDeserializeInto);
		
		FLoadSnapshotObjectArchive::ApplyToSnapshotWorldObject(SerializedComponentData, WorldData, ComponentToDeserializeInto, ProcessObjectDependency, InLocalisationSnapshotPackage);
	}

	static TOptional<TNonNullPtr<AActor>> GetDeserialized(FActorSnapshotData& ActorData, FWorldSnapshotData& WorldData, const FSoftObjectPath& OriginalActorPath, UWorld* SnapshotWorld, UPackage* InLocalisationSnapshotPackage)
	{
		SCOPED_SNAPSHOT_CORE_TRACE(GetDeserialized);
		if (ActorData.bReceivedSerialisation && ActorData.CachedSnapshotActor.IsValid())
		{
			return { ActorData.CachedSnapshotActor.Get() };
		}
		UE_LOG(LogLevelSnapshots, Verbose, TEXT("========== Get Deserialized %s =========="), *OriginalActorPath.ToString());

		const TOptional<TNonNullPtr<AActor>> Preallocated = UE::LevelSnapshots::Private::GetPreallocated(ActorData, WorldData, SnapshotWorld);
		if (!Preallocated)
		{
			return {};
		}
		ActorData.bReceivedSerialisation = true;

		const auto ProcessObjectDependency = [&ActorData](int32 OriginalObjectDependency)
		{
			ActorData.ObjectDependencies.Add(OriginalObjectDependency);
		};
		AActor* PreallocatedActor = Preallocated.GetValue();
		{
			const FRestoreObjectScope FinishRestore = PreActorRestore_SnapshotWorld(PreallocatedActor, ActorData.CustomActorSerializationData, WorldData, ProcessObjectDependency, InLocalisationSnapshotPackage);
			FLoadSnapshotObjectArchive::ApplyToSnapshotWorldObject(ActorData.SerializedActorData, WorldData, PreallocatedActor, ProcessObjectDependency, InLocalisationSnapshotPackage);
#if WITH_EDITOR
			UE_LOG(LogLevelSnapshots, Verbose, TEXT("ActorLabel is \"%s\" for \"%s\" (editor object path \"%s\")"), *PreallocatedActor->GetActorLabel(), *PreallocatedActor->GetPathName(), *OriginalActorPath.ToString());
#endif
		}

		DeserializeComponents(ActorData, PreallocatedActor, WorldData,
			[&WorldData, &ProcessObjectDependency, InLocalisationSnapshotPackage](
				FSubobjectSnapshotData& SerializedCompData,
				const FComponentSnapshotData& CompData,
				UActorComponent* Comp,
				const FSoftObjectPath& OriginalComponentPath,
				FWorldSnapshotData& SharedData)
			{
				SerializedCompData.SnapshotObject = Comp;
				
				const FRestoreObjectScope FinishRestore = PreSubobjectRestore_SnapshotWorld(Comp, OriginalComponentPath, WorldData, ProcessObjectDependency, InLocalisationSnapshotPackage);
				DeserializeIntoTransient(SerializedCompData, Comp, SharedData, ProcessObjectDependency, InLocalisationSnapshotPackage);
			}
		);

		DeserializeSubobjectsForSnapshotActor(ActorData, WorldData, ProcessObjectDependency, InLocalisationSnapshotPackage);
#if WITH_EDITOR
		// Hide this actor so external systems can see that this components should not render, i.e. make USceneComponent::ShouldRender return false
		if (!ensureMsgf(PreallocatedActor->IsTemporarilyHiddenInEditor(), TEXT("Transient property bHiddenEdTemporary was set to false by serializer. This should not happen. Investigate.")))
		{
			ActorData.CachedSnapshotActor->SetIsTemporarilyHiddenInEditor(true);
		}
#endif

		PostSerializeSnapshotActor(PreallocatedActor, WorldData, OriginalActorPath, InLocalisationSnapshotPackage);
		PreallocatedActor->UpdateComponentTransforms();
		{
			// GAllowActorScriptExecutionInEditor must be temporarily true so call to UserConstructionScript isn't skipped
			FEditorScriptExecutionGuard AllowConstructionScript;
			PreallocatedActor->UserConstructionScript();
		}
		
		return Preallocated;
	}
	
	static void ConditionallyRerunConstructionScript(FWorldSnapshotData& WorldData, AActor* RequiredActor, const TArray<int32>& OriginalObjectDependencies, UPackage* LocalisationSnapshotPackage)
	{
		bool bHadActorDependencies = false;
		for (const int32 OriginalObjectIndex : OriginalObjectDependencies)
		{
			const FSoftObjectPath& ObjectPath = WorldData.SerializedObjectReferences[OriginalObjectIndex];
			if (WorldData.ActorData.Contains(ObjectPath))
			{
				GetDeserializedActor(ObjectPath, WorldData, LocalisationSnapshotPackage);
				bHadActorDependencies = true;
			}
		}
		
		if (bHadActorDependencies && ensure(RequiredActor))
		{
			RequiredActor->RerunConstructionScripts();
		}
	}
}

TOptional<TNonNullPtr<AActor>> UE::LevelSnapshots::Private::GetDeserializedActor(const FSoftObjectPath& OriginalObjectPath, FWorldSnapshotData& WorldData, UPackage* LocalisationSnapshotPackage)
{
	FActorSnapshotData* SerializedActor = WorldData.ActorData.Find(OriginalObjectPath);
	if (SerializedActor)
	{
		const bool bJustReceivedSerialisation = !SerializedActor->bReceivedSerialisation;
		const TOptional<TNonNullPtr<AActor>> Result = Internal::GetDeserialized(*SerializedActor, WorldData, OriginalObjectPath, WorldData.SnapshotWorld->GetWorld(), LocalisationSnapshotPackage);
		if (Result && bJustReceivedSerialisation && ensure(SerializedActor->bReceivedSerialisation))
		{
			Internal::ConditionallyRerunConstructionScript(WorldData, Result.GetValue(), SerializedActor->ObjectDependencies, LocalisationSnapshotPackage);
		}
		return Result;
	}

	UE_LOG(LogLevelSnapshots, Warning, TEXT("No save data found for actor %s"), *OriginalObjectPath.ToString());
	return {};
}

TOptional<TNonNullPtr<AActor>> UE::LevelSnapshots::Private::GetPreallocatedIfValidButDoNotAllocate(const FActorSnapshotData& ActorData)
{
	return ActorData.CachedSnapshotActor.IsValid() ? TOptional<TNonNullPtr<AActor>>(ActorData.CachedSnapshotActor.Get()) : TOptional<TNonNullPtr<AActor>>();
}

TOptional<TNonNullPtr<AActor>> UE::LevelSnapshots::Private::GetPreallocated(const FActorSnapshotData& ActorData, FWorldSnapshotData& WorldData, UWorld* SnapshotWorld)
{
	SCOPED_SNAPSHOT_CORE_TRACE(GetPreallocated);
	
	if (!ActorData.CachedSnapshotActor.IsValid())
	{
		const FSoftClassPath SoftClassPath(ActorData.ActorClass);
		UClass* TargetClass = SoftClassPath.TryLoadClass<AActor>();
		if (!TargetClass)
		{
			UE_LOG(LogLevelSnapshots, Error, TEXT("Unknown class %s. The snapshot is mostly likely referencing a class that was deleted."), *SoftClassPath.ToString());
			return {};
		}
		
		FActorSpawnParameters SpawnParams;
		SpawnParams.Template = Cast<AActor>(GetClassDefault(WorldData, TargetClass));
		SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		if (ensureMsgf(SpawnParams.Template, TEXT("Failed to get class default. This should not happen. Investigate.")))
		{
			UClass* ClassToUse = SpawnParams.Template->GetClass();
			SpawnParams.Name = *FString("SnapshotObjectInstance_").Append(*MakeUniqueObjectName(SnapshotWorld, ClassToUse).ToString());
			ActorData.CachedSnapshotActor = SnapshotWorld->SpawnActor<AActor>(ClassToUse, SpawnParams);
		}
		else
		{
			ActorData.CachedSnapshotActor = SnapshotWorld->SpawnActor<AActor>(TargetClass, SpawnParams);
		}
		
		if (!ensureMsgf(ActorData.CachedSnapshotActor.IsValid(), TEXT("Failed to spawn actor of class '%s'"), *ActorData.ActorClass.ToString()))
		{
			return {};
		}
		
#if WITH_EDITOR
		// Hide this actor so external systems can see that this components should not render, i.e. make USceneComponent::ShouldRender return false
		ActorData.CachedSnapshotActor->SetIsTemporarilyHiddenInEditor(true);
#endif

		AllocateMissingComponentsForSnapshotActor(ActorData.CachedSnapshotActor.Get(), ActorData, WorldData);
	}

	return ActorData.CachedSnapshotActor.IsValid() ? TOptional<TNonNullPtr<AActor>>(ActorData.CachedSnapshotActor.Get()) : TOptional<TNonNullPtr<AActor>>();
}