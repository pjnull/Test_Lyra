// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace SerializeObjectState
{
	/**
	 * Serializes the object and all its subobjects.
	 */
	void SerializeWithSubobjects(FArchive& Archive, UObject* Root);

	/**
	 * Serializes the object and its subobjects but skips serializing the FName of the currently serialized object.
	 */
	void SerializeWithoutObjectName(FArchive& Archive, UObject* Root);
}


