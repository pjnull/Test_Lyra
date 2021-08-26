// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkPhysics.h"
#include "Async/NetworkPredictionAsyncProxy.h"
#include "PhysicsMovement.generated.h"


USTRUCT(BlueprintType)
struct FPhysicsInputCmd
{
	GENERATED_BODY()

	FPhysicsInputCmd()
		: Force(ForceInitToZero)
	{ }

	// Simple world vector force to be applied
	UPROPERTY(BlueprintReadWrite,Category="Input")
	FVector	Force;

	UPROPERTY(BlueprintReadWrite,Category="Input")
	bool bJumpedPressed = false;

	UPROPERTY(BlueprintReadWrite,Category="Input")
	bool bBrakesPressed = false;

	// For testing: to ensure the system only sees user created InputCmds and not defaults
	bool bLegit = false;

	void NetSerialize(FArchive& Ar)
	{
		if (Ar.IsSaving() && !bLegit)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Sending Bad Input!"));
		}

		Ar << Force;
		Ar << bJumpedPressed;
		Ar << bBrakesPressed;
		Ar << bLegit;

		if (!Ar.IsSaving() && !bLegit)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Received Bad Input!"));
		}
	}

	bool ShouldReconcile(const FPhysicsInputCmd& AuthState) const
	{
		//return FVector::DistSquared(Force, AuthState.Force) > 0.1f || 
		return Force != AuthState.Force ||
			bJumpedPressed != AuthState.bJumpedPressed || 
			bBrakesPressed != AuthState.bBrakesPressed ||
			bLegit != AuthState.bLegit;
	}

	void ApplyCorrection(const FPhysicsInputCmd& AuthState)
	{
		*this = AuthState;
	}
};

USTRUCT(BlueprintType)
struct FPhysicsMovementNetState
{
	GENERATED_BODY()

	// Actually used by AsyncTick to scale force applied
	UPROPERTY(BlueprintReadWrite, Category="Mock Object")
	float ForceMultiplier = 125000.f;

	// Arbitrary data that doesn't affect sim but could still trigger rollback
	UPROPERTY(BlueprintReadWrite, Category="Mock Object")
	int32 RandValue = 0;

	UPROPERTY(BlueprintReadWrite, Category="Mock Object")
	int32 JumpCooldownMS = 0;

	// Number of frames jump has been pressed
	UPROPERTY(BlueprintReadWrite, Category="Mock Object")
	int32 JumpCount = 0;

	UPROPERTY(BlueprintReadWrite, Category="Mock Object")
	int32 CheckSum = 0;

	// Frame we started "in air recovery" on
	UPROPERTY(BlueprintReadWrite, Category="Mock Object")
	int32 RecoveryFrame = 0;

	// Frame we started jumping on
	UPROPERTY(BlueprintReadWrite, Category="Mock Object")
	int32 JumpStartFrame = 0;

	// Frame we started being in the air
	UPROPERTY(BlueprintReadWrite, Category="Mock Object")
	int32 InAirFrame = 0;

	// Frame we last applied a kick impulse
	UPROPERTY(BlueprintReadWrite, Category="Mock Object")
	int32 KickFrame = 0;

	void NetSerialize(FArchive& Ar)
	{
		Ar << ForceMultiplier;
		Ar << RandValue;
		Ar << JumpCooldownMS;
		Ar << JumpCount;
		Ar << CheckSum;	
		Ar << RecoveryFrame;
		Ar << JumpStartFrame;
		Ar << InAirFrame;
		Ar << KickFrame;
	}

	bool ShouldReconcile(const FPhysicsMovementNetState& AuthState) const
	{
		return 
			ForceMultiplier != AuthState.ForceMultiplier || 
			RandValue != AuthState.RandValue ||
			JumpCooldownMS != AuthState.JumpCooldownMS || 
			JumpCount != AuthState.JumpCount ||
			RecoveryFrame != AuthState.RecoveryFrame ||
			JumpStartFrame != AuthState.JumpStartFrame ||
			InAirFrame != AuthState.InAirFrame ||
			KickFrame!= AuthState.KickFrame;
	}
};

USTRUCT(BlueprintType)
struct FPhysicsMovementLocalState
{
	GENERATED_BODY()

	FSingleParticlePhysicsProxy* Proxy = nullptr;
};

UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class NETWORKPREDICTIONEXTRAS_API UPhysicsMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UPhysicsMovementComponent();

	virtual void InitializeComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker);

	UPROPERTY(Replicated)
	FNetworkPredictionAsyncProxy NetworkPredictionProxy;

	// Latest MovementState
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	FPhysicsMovementNetState MovementState;

	// Game code should write to this when locally controlling this object.
	UPROPERTY(BlueprintReadWrite, Category="Input")
	FPhysicsInputCmd PendingInputCmd;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGenerateLocalInputCmd);

	// Event called when local input is needed: fill out PendingInputCmd with latest input data
	UPROPERTY(BlueprintAssignable, Category = "Input")
	FOnGenerateLocalInputCmd OnGenerateLocalInputCmd;

	// Which component's physics body to manage if there are multiple PrimitiveComponent and not the root component.
	UPROPERTY(EditDefaultsOnly, Category = "Physics")
	FName ManagedComponentTag = NAME_None;

	void TestMisprediction();

private:

	APlayerController* GetOwnerPC() const;

	UPROPERTY()
	APlayerController* CachedPC = nullptr;
};