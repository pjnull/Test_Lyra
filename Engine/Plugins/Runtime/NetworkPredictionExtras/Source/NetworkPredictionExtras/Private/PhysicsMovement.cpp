// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMovement.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "ChaosSolversModule.h"
#include "RewindData.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Chaos/SimCallbackObject.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "TimerManager.h"
#include "NetworkPredictionDebug.h"
#include "NetworkPredictionCVars.h"
#include "Async/NetworkPredictionAsyncModelDef.h"
#include "Async/NetworkPredictionAsyncWorldManager.h"
#include "Async/NetworkPredictionAsyncID.h"
#include "Async/NetworkPredictionAsyncModelDefRegistry.h"
#include "Async/NetworkPredictionAsyncProxyImpl.h"

namespace UE_NP
{
	NETSIM_DEVCVAR_SHIPCONST_INT(bEnableMock3, 1, "np2.Mock3.Enable", "Enable Mock2 implementation");
}

struct FPhysicsMovementSimulation
{
	static void Tick_Internal(FNetworkPredictionAsyncTickContext& Context, UE_NP::FNetworkPredictionAsyncID ID, FPhysicsInputCmd& InputCmd, FPhysicsMovementLocalState& LocalState, FPhysicsMovementNetState& NetState)
	{
		UWorld* World = Context.World;
		const float DeltaSeconds = Context.DeltaTimeSeconds;
		const int32 SimulationFrame = Context.SimulationFrame;

		if (!ensureAlways(LocalState.Proxy))
		{
			return;
		}

		if (!InputCmd.bLegit)
		{
  			UE_LOG(LogTemp, Warning, TEXT("Illegitimate InputCmd has seeped in!!! SimFrame: %d. LocalFrame: %d"), SimulationFrame, Context.LocalStorageFrame);
		}

		if (auto* PT = LocalState.Proxy->GetPhysicsThreadAPI())
		{
			UE_NETWORK_PHYSICS::ConditionalFrameEnsure();
			if (UE_NETWORK_PHYSICS::ConditionalFrameBreakpoint())
			{
				UE_LOG(LogTemp, Warning, TEXT("[%d][%d] Location: %s"), UE_NETWORK_PHYSICS::DebugSimulationFrame(), UE_NETWORK_PHYSICS::DebugServer(), *PT->X().ToString());
			}

			//UE_LOG(LogTemp, Warning, TEXT("[%d] 0x%X <%s> TICK_INTERNAL"), SimulationFrame, (int64)LocalState.Proxy->GetHandleUnsafe(), *PT->X().ToString());

			FVector TracePosition = PT->X();
			FVector EndPosition = TracePosition + FVector(0.f, 0.f, -100.f);
			FCollisionShape Shape = FCollisionShape::MakeSphere(250.f);
			ECollisionChannel CollisionChannel = ECollisionChannel::ECC_WorldStatic; 
			FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;

			FCollisionResponseParams ResponseParams = FCollisionResponseParams::DefaultResponseParam;
			FCollisionObjectQueryParams ObjectParams(ECollisionChannel::ECC_PhysicsBody);

			
			FHitResult OutHit;
			const bool bInAir = !UE_NETWORK_PHYSICS::JumpHack() && !World->LineTraceSingleByChannel(OutHit, TracePosition, EndPosition, ECollisionChannel::ECC_WorldStatic, QueryParams, ResponseParams);
			const float UpDot = FVector::DotProduct(PT->R().GetUpVector(), FVector::UpVector);

			if (UE_NETWORK_PHYSICS::JumpMisPredict())
			{
				if (InputCmd.bJumpedPressed && NetState.JumpStartFrame + 10 < SimulationFrame)
				{
					NetState.JumpStartFrame = SimulationFrame;
					PT->SetLinearImpulse(FVector(0.f, 0.f, 60000.f + (FMath::FRand() * 50000.f)));
				}
				return;
			}

			if (UE_NETWORK_PHYSICS::MockImpulse() && NetState.KickFrame + 10 < SimulationFrame)
			{
				/*
				for (FSingleParticlePhysicsProxy* BallProxy : BallProxies)
				{					
				if (auto* BallPT = BallProxy->GetPhysicsThreadAPI())
				{
				const FVector BallLocation = BallPT->X();
				const float BallRadius = BallPT->Geometry()->BoundingBox().OriginRadius(); //GetRadius();

				if (BallRadius > 0.f && (FVector::DistSquared(PT->X(), BallLocation) < BallRadius * BallRadius))
				{
				FVector Impulse = BallPT->X() - PT->X();
				Impulse.Z =0.f;
				Impulse.Normalize();
				Impulse *= UE_NETWORK_PHYSICS::MockImpulseX;
				Impulse.Z = UE_NETWORK_PHYSICS::MockImpulseZ();

				//UE_LOG(LogTemp, Warning, TEXT("Applied Force. %s"), *GetNameSafe(HitPrimitive->GetOwner()));
				BallPT->SetLinearImpulse( Impulse, false );
				SimObject->KickFrame = SimulationFrame;
				}
				}
				}
				*/
			}

			// ---------------------------------------------------------------------------------------------

			if (!bInAir)
			{
				if (NetState.InAirFrame != 0)
				{
					NetState.InAirFrame = 0;
					//SimObject->JumpStartFrame = 0;
				}

				// Check for recovery start
				if (NetState.RecoveryFrame == 0)
				{
					if (UpDot < 0.2f)
					{
						NetState.RecoveryFrame = SimulationFrame;
					}
				}
			}
			else
			{
				if (NetState.InAirFrame == 0)
				{
					NetState.InAirFrame = SimulationFrame;
				}
			}

			//UE_LOG(LogNetworkPhysics, Log, TEXT("[%d] AirFrame: %d. JumpFrame: %d"), SimulationFrame, SimObject->InAirFrame, SimObject->JumpStartFrame);
			if (InputCmd.bJumpedPressed)
			{
				if (NetState.InAirFrame == 0 || (NetState.InAirFrame + UE_NETWORK_PHYSICS::JumpFudgeFrames() > SimulationFrame))
				{
					if (NetState.JumpStartFrame == 0)
					{
						NetState.JumpStartFrame = SimulationFrame;
					}

					if (NetState.JumpStartFrame + UE_NETWORK_PHYSICS::JumpFrameDuration() > SimulationFrame)
					{
						PT->AddForce( Chaos::FVec3(0.f, 0.f, UE_NETWORK_PHYSICS::JumpForce()) );

						//UE_LOG(LogTemp, Warning, TEXT("[%d] Jumped [JumpStart: %d. InAir: %d]"), SimulationFrame, SimObject->JumpStartFrame, SimObject->InAirFrame);
						NetState.JumpCooldownMS = 1000;
					}
				}
			}
			else
			{
				if (NetState.InAirFrame == 0 && (NetState.JumpStartFrame + UE_NETWORK_PHYSICS::JumpFrameDuration() < SimulationFrame))
				{
					NetState.JumpStartFrame = 0;
				}
			}

			if (NetState.RecoveryFrame != 0)
			{
				if (UpDot > 0.7f)
				{
					// Recovered
					NetState.RecoveryFrame = 0;
				}
				else
				{
					// Doing it per-axis like this is probably wrong
					FRotator Rot = PT->R().Rotator();
					const float DeltaRoll = FRotator::NormalizeAxis( -1.f * (Rot.Roll + (PT->W().X * UE_NETWORK_PHYSICS::TurnDampK())));
					const float DeltaPitch = FRotator::NormalizeAxis( -1.f * (Rot.Pitch + (PT->W().Y * UE_NETWORK_PHYSICS::TurnDampK())));

					PT->AddTorque(FVector(DeltaRoll, DeltaPitch, 0.f) * UE_NETWORK_PHYSICS::TurnK() * 1.5f);
					PT->AddForce(FVector(0.f, 0.f, 600.f));
				}
			}
			else if (InputCmd.bBrakesPressed)
			{
				FVector NewV = PT->V();
				if (NewV.SizeSquared2D() < 1.f)
				{
					PT->SetV( Chaos::FVec3(0.f, 0.f, NewV.Z));
				}
				else
				{
					PT->SetV( Chaos::FVec3(NewV.X * 0.8f, NewV.Y * 0.8f, NewV.Z));
				}
			}
			else
			{
				// Movement
				if (InputCmd.Force.SizeSquared() > 0.001f)
				{
					const FVector MovementForce = InputCmd.Force * NetState.ForceMultiplier * UE_NETWORK_PHYSICS::MovementK();
					npEnsure(MovementForce.ContainsNaN() == false);
					PT->AddForce(MovementForce);

					// Auto Turn
					const float CurrentYaw = PT->R().Rotator().Yaw + (PT->W().Z * UE_NETWORK_PHYSICS::TurnDampK());
					const float DesiredYaw = InputCmd.Force.Rotation().Yaw;
					const float DeltaYaw = FRotator::NormalizeAxis( DesiredYaw - CurrentYaw );

					ensure(!FMath::IsNaN(DesiredYaw));
					PT->AddTorque(FVector(0.f, 0.f, DeltaYaw * UE_NETWORK_PHYSICS::TurnK()));
				}
			}

			// Drag force
			FVector V = PT->V();
			V.Z = 0.f;
			if (V.SizeSquared() > 0.1f)
			{
				FVector Drag = -1.f * V * UE_NETWORK_PHYSICS::DragK();
				ensure(Drag.ContainsNaN() == false);
				PT->AddForce(Drag);
			}

			NetState.JumpCooldownMS = FMath::Max( NetState.JumpCooldownMS - (int32)(DeltaSeconds* 1000.f), 0);
			if (NetState.JumpCooldownMS != 0)
			{
				UE_CLOG(UE_NETWORK_PHYSICS::MockDebug(), LogNetworkPhysics, Log, TEXT("[%d/%d] JumpCount: %d. JumpCooldown: %d"), SimulationFrame, Context.LocalStorageFrame, NetState.JumpCount, NetState.JumpCooldownMS);
			}

			if (InputCmd.bJumpedPressed)
			{
				// Note this is really just for debugging. "How many times was the button pressed"
				NetState.JumpCount++;
				UE_CLOG(UE_NETWORK_PHYSICS::MockDebug(), LogNetworkPhysics, Log, TEXT("[%d/%d] bJumpedPressed: %d. Count: %d"), SimulationFrame, Context.LocalStorageFrame, InputCmd.bJumpedPressed, NetState.JumpCount);
				UE_LOG(LogNetworkPhysics, Log, TEXT("[%d/%d] bJumpedPressed: %d. Count: %d"), SimulationFrame, Context.LocalStorageFrame, InputCmd.bJumpedPressed, NetState.JumpCount);
			}
		}
	}
};


struct FPhysicsMovementAsyncModelDef : public UE_NP::FNetworkPredictionAsyncModelDef
{
	NP_ASYNC_MODEL_BODY()

	using InputCmdType = FPhysicsInputCmd;
	using NetStateType = FPhysicsMovementNetState;
	using LocalStateType = FPhysicsMovementLocalState;
	using SimulationTickType = FPhysicsMovementSimulation;

	static const TCHAR* GetName() { return TEXT("PhysicsMovement"); }
	static constexpr int32 GetSortPriority() { return 0; }
};

NP_ASYNC_MODEL_REGISTER(FPhysicsMovementAsyncModelDef);


UPhysicsMovementComponent::UPhysicsMovementComponent()
{
	bWantsInitializeComponent = true;
	bAutoActivate = true;
	SetIsReplicatedByDefault(true);

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPhysicsMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
#if WITH_CHAOS
	UWorld* World = GetWorld();	
	checkSlow(World);
	UNetworkPhysicsManager* Manager = World->GetSubsystem<UNetworkPhysicsManager>();
	if (!Manager)
	{
		return;
	}

	if (UE_NP::bEnableMock3() > 0)
	{
		FPhysicsMovementLocalState LocalState;
		LocalState.Proxy = UE_NP::FindBestPhysicsProxy(GetOwner(), ManagedComponentTag);
		if (LocalState.Proxy)
		{
			if (ensure(NetworkPredictionProxy.RegisterProxy(GetWorld())))
			{
				NetworkPredictionProxy.RegisterSim<FPhysicsMovementAsyncModelDef>(MoveTemp(LocalState), FPhysicsMovementNetState(), &PendingInputCmd, &MovementState);
			}
		}
		else
		{
			UE_LOG(LogNetworkPhysics, Warning, TEXT("No valid physics body found on %s"), *GetName());
		}
	}
#endif
}

void UPhysicsMovementComponent::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
	NetworkPredictionProxy.OnPreReplication();
}

void UPhysicsMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (UE_NP::bEnableMock3() > 0)
	{
		NetworkPredictionProxy.UnregisterProxy();
	}
}

void UPhysicsMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (UE_NP::bEnableMock3() > 0)
	{
		APlayerController* PC = GetOwnerPC();

		if (CachedPC != PC)
		{
			NetworkPredictionProxy.RegisterController(PC);
			CachedPC = PC;
		}

		if (PC && PC->IsLocalController())
		{
			// Broadcast out a delegate. The user will write to PendingInputCmd
			OnGenerateLocalInputCmd.Broadcast();
			PendingInputCmd.bLegit = true;
		}
	}
}

void UPhysicsMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME( UPhysicsMovementComponent, NetworkPredictionProxy);
}

APlayerController* UPhysicsMovementComponent::GetOwnerPC() const
{
	if (APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		return Cast<APlayerController>(PawnOwner->GetController());
	}

	return nullptr;
}

void UPhysicsMovementComponent::TestMisprediction()
{
	const int32 RandValue = FMath::RandHelper(1024);
	UE_LOG(LogNetworkPhysics, Warning, TEXT("Setting NewRand on to %d"), RandValue);
	NetworkPredictionProxy.ModifyNetState<FPhysicsMovementAsyncModelDef>([RandValue](FPhysicsMovementNetState& NetState)
	{
		NetState.RandValue = RandValue;
	});	
}

FAutoConsoleCommandWithWorldAndArgs ForcePhysicsMovementCorrectionCmd(TEXT("np.PhysicsMovement.ForceRandCorrection2"), TEXT(""),
FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray< FString >& Args, UWorld* InWorld) 
	{
		for (TObjectIterator<UWorld> It; It; ++It)
		{
			UWorld* World = *It;
			if (World->GetNetMode() != NM_Client && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game))
			{
				const int32 NewRand = FMath::RandHelper(1024);

				for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
				{
					if (UPhysicsMovementComponent* PhysComp = ActorIt->FindComponentByClass<UPhysicsMovementComponent>())
					{
						PhysComp->TestMisprediction();
					}
				}
			}
		}
	}));