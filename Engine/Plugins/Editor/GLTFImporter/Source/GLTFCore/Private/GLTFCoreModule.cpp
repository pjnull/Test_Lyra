// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFCoreModule.h"


/**
 * glTF Core module implementation (private)
 */
class FGLTFCoreModule : public IGLTFCoreModule
{

public:
	virtual void StartupModule() override {}

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FGLTFCoreModule, GLTFCore);
