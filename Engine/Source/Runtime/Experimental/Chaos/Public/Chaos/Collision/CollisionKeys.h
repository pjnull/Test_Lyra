// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Chaos/Core.h"
#include "Chaos/ParticleHandle.h"

namespace Chaos
{
	inline uint32 OrderIndependentHashCombine(const uint32 A, const uint32 B)
	{
		if (A < B)
		{
			return ::HashCombine(A, B);
		}
		else
		{
			return ::HashCombine(B, A);
		}
	}

	/**
	 * @brief Order particles in a consistent way for use by Broadphase and Resim
	*/
	inline bool ShouldSwapParticleOrder(const FGeometryParticleHandle* Particle0, const FGeometryParticleHandle* Particle1)
	{
		const bool bIsParticle1Preferred = (Particle1->ParticleID() < Particle0->ParticleID());
		const bool bSwapOrder = !FConstGenericParticleHandle(Particle0)->IsDynamic() || !bIsParticle1Preferred;
		return bSwapOrder;
	}

	/**
	 * @brief A key which uniquely identifes a particle pair for use by the collision detection system
	 * This key will be the same if particles order is reversed.
	*/
	class FCollisionParticlePairKey
	{
	public:
		FCollisionParticlePairKey()
			: Key(0)
		{
		}

		FCollisionParticlePairKey(const FGeometryParticleHandle* Particle0, const FGeometryParticleHandle* Particle1)
			: Key(GenerateHash(Particle0, Particle1))
		{
		}

		uint32 GetKey() const
		{
			return Key;
		}

	private:
		uint32 GenerateHash(const FGeometryParticleHandle* Particle0, const FGeometryParticleHandle* Particle1)
		{
			uint32 Hash0 = HashCombine(::GetTypeHash(Particle0->ParticleID().GlobalID), ::GetTypeHash(Particle0->ParticleID().LocalID));
			uint32 Hash1 = HashCombine(::GetTypeHash(Particle1->ParticleID().GlobalID), ::GetTypeHash(Particle1->ParticleID().LocalID));
			return OrderIndependentHashCombine(Hash1, Hash0);
		}

		uint32 Key;
	};

	/**
	 * @brief A key which uniquely identifes a collision constraint for use by the collision detection system
	 * This key will be the same if the particle order is reversed.
	*/
	class FCollisionConstraintKey
	{
	public:
		FCollisionConstraintKey()
			: Key(0)
		{
		}

		FCollisionConstraintKey(const FGeometryParticleHandle* Particle0, const FImplicitObject* Implicit0, const FBVHParticles* Simplicial0, const FGeometryParticleHandle* Particle1, const FImplicitObject* Implicit1, const FBVHParticles* Simplicial1)
			: Key(0)
		{
			GenerateHash(Particle0, Implicit0, Simplicial0, Particle1, Implicit1, Simplicial1);
		}

		uint32 GetKey() const
		{
			return Key;
		}

		friend bool operator==(const FCollisionConstraintKey& L, const FCollisionConstraintKey& R)
		{
			return L.Key == R.Key;
		}

		friend bool operator!=(const FCollisionConstraintKey& L, const FCollisionConstraintKey& R)
		{
			return !(L == R);
		}

		friend bool operator<(const FCollisionConstraintKey& L, const FCollisionConstraintKey& R)
		{
			return L.Key < R.Key;
		}

	private:
		void GenerateHash(const FGeometryParticleHandle* Particle0, const FImplicitObject* Implicit0, const FBVHParticles* Simplicial0, const FGeometryParticleHandle* Particle1, const FImplicitObject* Implicit1, const FBVHParticles* Simplicial1)
		{
			// @todo(chaos): We should use ShapeIndex rather than Implicit/Simplicial pointers in the hash
			const uint32 Particle0Hash = HashCombine(::GetTypeHash(Particle0->ParticleID().GlobalID), ::GetTypeHash(Particle0->ParticleID().LocalID));
			const uint32 Implicit0Hash = ::GetTypeHash(Implicit0);
			const uint32 Simplicial0Hash = ::GetTypeHash(Simplicial0);
			const uint32 Hash0 = ::HashCombine(Particle0Hash, ::HashCombine(Implicit0Hash, Simplicial0Hash));

			const uint32 Particle1Hash = HashCombine(::GetTypeHash(Particle1->ParticleID().GlobalID), ::GetTypeHash(Particle1->ParticleID().LocalID));
			const uint32 Implicit1Hash = ::GetTypeHash(Implicit1);
			const uint32 Simplicial1Hash = ::GetTypeHash(Simplicial1);
			const uint32 Hash1 = ::HashCombine(Particle1Hash, ::HashCombine(Implicit1Hash, Simplicial1Hash));

			Key = OrderIndependentHashCombine(Hash0, Hash1);
		}

		uint32 Key;
	};
}