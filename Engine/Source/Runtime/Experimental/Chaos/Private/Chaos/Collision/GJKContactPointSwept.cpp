// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Collision/GJKContactPointSwept.h"

#include "Chaos/CastingUtilities.h"
#include "Chaos/Collision/ContactPoint.h"
#include "Chaos/Collision/ContactPointsMiscShapes.h"
#include "Chaos/GJK.h"

namespace Chaos
{
	template <typename GeometryA, typename GeometryB>
	FContactPoint GJKContactPointSwept(const GeometryA& A, const FRigidTransform3& ATM, const GeometryB& B, const FRigidTransform3& BTM, const FVec3& Dir, const FReal Length, FReal& TOI)
	{
		FContactPoint Contact;
		const FRigidTransform3 AToBTM = ATM.GetRelativeTransform(BTM);
		const FVec3 LocalDir = BTM.InverseTransformVectorNoScale(Dir);

		FReal OutTime;
		FVec3 Location, Normal;
		if (GJKRaycast2(B, A, AToBTM, LocalDir, Length, OutTime, Location, Normal, (FReal)0, true))
		{
			// GJK output is all in the local space of B. We need to transform the B-relative position and the normal in to B-space
			const FVec3 WorldLocation = BTM.TransformPosition(Location);
			const FVec3 WorldNormal = BTM.TransformVectorNoScale(Normal);
			const FRigidTransform3 ATMAtHit = FRigidTransform3(ATM.GetTranslation() + Dir * OutTime, ATM.GetRotation());	// OutTime is a distance :|
			Contact.ShapeMargins[0] = 0.0f;
			Contact.ShapeMargins[1] = 0.0f;
			Contact.ShapeContactPoints[0] = ATMAtHit.InverseTransformPosition(WorldLocation);
			Contact.ShapeContactPoints[1] = Location;
			Contact.ShapeContactNormal = Normal;
			Contact.Location = WorldLocation;
			Contact.Normal = WorldNormal;
			ComputeSweptContactPhiAndTOIHelper(Contact.Normal, Dir, Length, OutTime, TOI, Contact.Phi);
		}

		return Contact;
	}

	FContactPoint GenericConvexConvexContactPointSwept(const FImplicitObject& A, const FRigidTransform3& ATM, const FImplicitObject& B, const FRigidTransform3& BTM, const FVec3& Dir, const FReal Length, FReal& TOI)
	{
		// This expands to a switch of switches that calls the inner function with the appropriate concrete implicit types
		return Utilities::CastHelperNoUnwrap(A, ATM, [&](const auto& ADowncast, const FRigidTransform3& AFullTM)
			{
				return Utilities::CastHelperNoUnwrap(B, BTM, [&](const auto& BDowncast, const FRigidTransform3& BFullTM)
					{
						return GJKContactPointSwept(ADowncast, AFullTM, BDowncast, BFullTM, Dir, Length, TOI);
					});
			});
	}

}