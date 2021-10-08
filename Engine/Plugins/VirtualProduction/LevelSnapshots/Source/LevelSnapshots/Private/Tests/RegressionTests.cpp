// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertySelectionMap.h"
#include "SnapshotTestRunner.h"
#include "SnapshotTestActor.h"
#include "Types/ActorWithReferencesInCDO.h"

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

// Bug fixes should generally be tested. Put tests for bug fixes here.

/**
 * FTakeClassDefaultObjectSnapshotArchive used to crash when a class CDO contained a collection of object references. Make sure it does not crash and restores.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContainersWithObjectReferencesInCDO, "VirtualProduction.LevelSnapshots.Snapshot.Regression.ContainersWithObjectReferencesInCDO", (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter));
bool FContainersWithObjectReferencesInCDO::RunTest(const FString& Parameters)
{
	AActorWithReferencesInCDO* Actor = nullptr;

	FSnapshotTestRunner()
		.ModifyWorld([&](UWorld* World)
		{
			Actor = World->SpawnActor<AActorWithReferencesInCDO>();
		})
		.TakeSnapshot()
		.ModifyWorld([&](UWorld* World)
		{
			Actor->SetAllPropertiesTo(Actor->CylinderMesh);
		})
		.ApplySnapshot()
		.RunTest([&]()
		{
			TestTrue(TEXT("Object properties restored correctly"), Actor->DoAllPropertiesPointTo(Actor->CubeMesh));
		});
	
	return true;
}

namespace
{
	bool AllCollisionResponseEqual(ASnapshotTestActor* Instance, ECollisionResponse Response)
	{
		const FCollisionResponse& Responses = Instance->StaticMeshComponent->BodyInstance.GetCollisionResponse();
		constexpr int32 NumChannels = 32;
		for (int32 i = 0; i < NumChannels; ++i)
		{
			if (Responses.GetResponse(static_cast<ECollisionChannel>(i)) != Response)
			{
				return false;
			}
		}
		return true;
	}
}

/**
 * Multiple bugs:
 *	- FBodyInstance::ObjectType, FBodyInstance::CollisionEnabled and FBodyInstance::CollisionResponses should not be diffed when UStaticMeshComponent::bUseDefaultCollision == true
 *	- Other FBodyInstance properties should still diff and restore correctly
 *	- After restoration, transient property FCollisionResponse::ResponseToChannels should contain the correct values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRestoreCollision, "VirtualProduction.LevelSnapshots.Snapshot.Regression.RestoreCollision", (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter));
bool FRestoreCollision::RunTest(const FString& Parameters)
{
	ASnapshotTestActor* CustomBlockAllToOverlapAll = nullptr;
	ASnapshotTestActor* DefaultWithChangedIgnoredProperties = nullptr;
	ASnapshotTestActor* DefaultToCustom = nullptr;
	ASnapshotTestActor* CustomToDefault = nullptr;
	ASnapshotTestActor* ChangeCollisionProfile = nullptr;

	FSnapshotTestRunner()
		.ModifyWorld([&](UWorld* World)
		{
			CustomBlockAllToOverlapAll = World->SpawnActor<ASnapshotTestActor>();
			DefaultWithChangedIgnoredProperties = World->SpawnActor<ASnapshotTestActor>();
			DefaultToCustom = World->SpawnActor<ASnapshotTestActor>();
			CustomToDefault = World->SpawnActor<ASnapshotTestActor>();
			ChangeCollisionProfile = World->SpawnActor<ASnapshotTestActor>();

			CustomBlockAllToOverlapAll->StaticMeshComponent->bUseDefaultCollision = false;
			CustomBlockAllToOverlapAll->StaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
			
			DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetObjectType(ECC_WorldStatic);
			DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetResponseToAllChannels(ECR_Ignore);
			DefaultWithChangedIgnoredProperties->StaticMeshComponent->bUseDefaultCollision = true;
			
			DefaultToCustom->StaticMeshComponent->bUseDefaultCollision = true;
			
			CustomToDefault->StaticMeshComponent->bUseDefaultCollision = false;
			CustomToDefault->StaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

			ChangeCollisionProfile->StaticMeshComponent->SetCollisionProfileName(FName("OverlapAll"));
		})
		.TakeSnapshot()
		.ModifyWorld([&](UWorld* World)
		{
			CustomBlockAllToOverlapAll->StaticMeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);

			DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
			DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetObjectType(ECC_WorldDynamic);
			DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetResponseToAllChannels(ECR_Overlap);

			DefaultToCustom->StaticMeshComponent->SetCollisionProfileName(FName("OverlapAll"));

			CustomToDefault->StaticMeshComponent->bUseDefaultCollision = true;

			ChangeCollisionProfile->StaticMeshComponent->SetCollisionProfileName(FName("BlockAll"));
		})

		.FilterProperties(DefaultWithChangedIgnoredProperties, [&](const FPropertySelectionMap& SelectionMap)
		{
			TestTrue(TEXT("Collision properties ignored when default collision is used"), !SelectionMap.HasChanges(DefaultWithChangedIgnoredProperties));
		})
		.ApplySnapshot()
		.RunTest([&]()
		{
			TestTrue(TEXT("Collision response restored"), AllCollisionResponseEqual(CustomBlockAllToOverlapAll, ECR_Block));
			
			TestTrue(TEXT("Default collision restored"), DefaultToCustom->StaticMeshComponent->bUseDefaultCollision);
			
			TestTrue(TEXT("Collision responses correct"), AllCollisionResponseEqual(CustomToDefault, ECR_Ignore));
			
			TestEqual(TEXT("Profile name restored"), ChangeCollisionProfile->StaticMeshComponent->GetCollisionProfileName(), FName("OverlapAll"));
			TestTrue(TEXT("Collision responses correct"), AllCollisionResponseEqual(ChangeCollisionProfile, ECR_Overlap));
		});
	
	return true;
}