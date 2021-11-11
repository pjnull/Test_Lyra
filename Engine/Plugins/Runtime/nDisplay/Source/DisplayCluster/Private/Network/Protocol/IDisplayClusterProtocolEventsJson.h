// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FDisplayClusterClusterEventJson;

#include "Network/DisplayClusterNetworkTypes.h"


/**
 * JSON cluster events protocol
 */
class IDisplayClusterProtocolEventsJson
{
public:
	virtual ~IDisplayClusterProtocolEventsJson() = default;

public:
	virtual EDisplayClusterCommResult EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event) = 0;
};
