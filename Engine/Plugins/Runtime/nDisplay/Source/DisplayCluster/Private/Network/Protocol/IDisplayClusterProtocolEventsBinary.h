// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FDisplayClusterClusterEventBinary;

#include "Network/DisplayClusterNetworkTypes.h"


/**
 * Binary cluster events protocol.
 */
class IDisplayClusterProtocolEventsBinary
{
public:
	virtual ~IDisplayClusterProtocolEventsBinary() = default;

public:
	virtual EDisplayClusterCommResult EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event) = 0;
};
