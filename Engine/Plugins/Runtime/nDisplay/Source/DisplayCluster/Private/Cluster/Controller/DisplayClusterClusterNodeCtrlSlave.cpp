// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cluster/Controller/DisplayClusterClusterNodeCtrlSlave.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Failover/IDisplayClusterFailoverNodeController.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Network/Service/ClusterEventsJson/DisplayClusterClusterEventsJsonClient.h"
#include "Network/Service/ClusterEventsBinary/DisplayClusterClusterEventsBinaryClient.h"
#include "Network/Service/ClusterSync/DisplayClusterClusterSyncClient.h"
#include "Network/Service/RenderSync/DisplayClusterRenderSyncClient.h"

#include "DisplayClusterConfigurationTypes.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterClusterNodeCtrlSlave::FDisplayClusterClusterNodeCtrlSlave(const FString& CtrlName, const FString& NodeName) :
	FDisplayClusterClusterNodeCtrlBase(CtrlName, NodeName)
{
}

FDisplayClusterClusterNodeCtrlSlave::~FDisplayClusterClusterNodeCtrlSlave()
{
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Releasing %s cluster controller..."), *GetControllerName());

	Shutdown();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolClusterSync
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSlave::WaitForGameStart()
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->WaitForGameStart();
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSlave::WaitForFrameStart()
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->WaitForFrameStart();
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSlave::WaitForFrameEnd()
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->WaitForFrameEnd();
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSlave::GetTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime)
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->GetTimeData(OutDeltaTime, OutGameTime, OutFrameTime);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSlave::GetObjectsData(const EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData)
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->GetObjectsData(InSyncGroup, OutObjectsData);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSlave::GetEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents)
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->GetEventsData(OutJsonEvents, OutBinaryEvents);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSlave::GetNativeInputData(TMap<FString, FString>& OutNativeInputData)
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->GetNativeInputData(OutNativeInputData);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolRenderSync
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSlave::WaitForSwapSync()
{
	checkSlow(RenderSyncClient.IsValid());
	const EDisplayClusterCommResult CommResult = RenderSyncClient->WaitForSwapSync();
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolEventsJson
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSlave::EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event)
{
	checkSlow(ClusterEventsJsonClient);
	const EDisplayClusterCommResult CommResult = ClusterEventsJsonClient->EmitClusterEventJson(Event);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolEventsBinary
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSlave::EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event)
{
	checkSlow(ClusterEventsBinaryClient);
	const EDisplayClusterCommResult CommResult = ClusterEventsBinaryClient->EmitClusterEventBinary(Event);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterNodeCtrlBase
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterNodeCtrlSlave::InitializeServers()
{
	return FDisplayClusterClusterNodeCtrlBase::InitializeServers();
}

bool FDisplayClusterClusterNodeCtrlSlave::StartServers()
{
	return FDisplayClusterClusterNodeCtrlBase::StartServers();
}

void FDisplayClusterClusterNodeCtrlSlave::StopServers()
{
	FDisplayClusterClusterNodeCtrlBase::StopServers();
}

bool FDisplayClusterClusterNodeCtrlSlave::InitializeClients()
{
	if (!FDisplayClusterClusterNodeCtrlBase::InitializeClients())
	{
		return false;
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - initializing slave clients..."), *GetControllerName());

	// Instantiate local clients
	ClusterSyncClient         = MakeUnique<FDisplayClusterClusterSyncClient>();
	RenderSyncClient          = MakeUnique<FDisplayClusterRenderSyncClient>();
	ClusterEventsJsonClient   = MakeUnique<FDisplayClusterClusterEventsJsonClient>();
	ClusterEventsBinaryClient = MakeUnique<FDisplayClusterClusterEventsBinaryClient>();

	return ClusterSyncClient && RenderSyncClient && ClusterEventsJsonClient && ClusterEventsBinaryClient;
}

bool FDisplayClusterClusterNodeCtrlSlave::StartClients()
{
	if (!FDisplayClusterClusterNodeCtrlBase::StartClients())
	{
		return false;
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - initializing slave clients..."), *GetControllerName());

	// Master config
	const UDisplayClusterConfigurationClusterNode* CfgMaster = GDisplayCluster->GetPrivateConfigMgr()->GetMasterNode();
	if (!CfgMaster)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("No master node configuration data found"));
		return false;
	}

	// Get configuration data
	const UDisplayClusterConfigurationData* ConfigData = GDisplayCluster->GetPrivateConfigMgr()->GetConfig();
	if (!ConfigData)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't get configuration data"));
		return false;
	}

	// Allow children to override master's address
	FString HostToUse = CfgMaster->Host;
	OverrideMasterAddr(HostToUse);

	// Start the clients
	return StartClientWithLogs(ClusterSyncClient.Get(),         HostToUse, ConfigData->Cluster->MasterNode.Ports.ClusterSync,         ConfigData->Cluster->Network.ConnectRetriesAmount, ConfigData->Cluster->Network.ConnectRetryDelay)
		&& StartClientWithLogs(RenderSyncClient.Get(),          HostToUse, ConfigData->Cluster->MasterNode.Ports.ClusterSync,         ConfigData->Cluster->Network.ConnectRetriesAmount, ConfigData->Cluster->Network.ConnectRetryDelay)
		&& StartClientWithLogs(ClusterEventsJsonClient.Get(),   HostToUse, ConfigData->Cluster->MasterNode.Ports.ClusterEventsJson,   ConfigData->Cluster->Network.ConnectRetriesAmount, ConfigData->Cluster->Network.ConnectRetryDelay)
		&& StartClientWithLogs(ClusterEventsBinaryClient.Get(), HostToUse, ConfigData->Cluster->MasterNode.Ports.ClusterEventsBinary, ConfigData->Cluster->Network.ConnectRetriesAmount, ConfigData->Cluster->Network.ConnectRetryDelay);
}

void FDisplayClusterClusterNodeCtrlSlave::StopClients()
{
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Cluster controller %s is shutting down the clients..."), *GetControllerName());

	ClusterEventsJsonClient->Disconnect();
	ClusterEventsBinaryClient->Disconnect();
	ClusterSyncClient->Disconnect();
	RenderSyncClient->Disconnect();

	FDisplayClusterClusterNodeCtrlBase::StopClients();
}
