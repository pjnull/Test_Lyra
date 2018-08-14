// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Unix/SocketSubsystemUnix.h"
#include "Misc/CommandLine.h"
#include "SocketSubsystemModule.h"
#include "BSDSockets/IPAddressBSD.h"

#include <ifaddrs.h>
#include <net/if.h>

FSocketSubsystemUnix* FSocketSubsystemUnix::SocketSingleton = NULL;

FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	FName SubsystemName(TEXT("UNIX"));
	// Create and register our singleton factor with the main online subsystem for easy access
	FSocketSubsystemUnix* SocketSubsystem = FSocketSubsystemUnix::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSocketSubsystemUnix::Destroy();
		return NAME_None;
	}
}

void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("UNIX")));
	FSocketSubsystemUnix::Destroy();
}

/** 
 * Singleton interface for the Android socket subsystem 
 * @return the only instance of the Android socket subsystem
 */
FSocketSubsystemUnix* FSocketSubsystemUnix::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemUnix();
	}

	return SocketSingleton;
}

/** 
 * Destroy the singleton Android socket subsystem
 */
void FSocketSubsystemUnix::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = NULL;
	}
}

/**
 * Does Unix platform initialization of the sockets library
 *
 * @param Error a string that is filled with error information
 *
 * @return TRUE if initialized ok, FALSE otherwise
 */
bool FSocketSubsystemUnix::Init(FString& Error)
{
	return true;
}

/**
 * Performs Android specific socket clean up
 */
void FSocketSubsystemUnix::Shutdown(void)
{
}


/**
 * @return Whether the device has a properly configured network device or not
 */
bool FSocketSubsystemUnix::HasNetworkDevice()
{
	// @TODO: implement
	return true;
}

FSocket* FSocketSubsystemUnix::CreateSocket(const FName& SocketType, const FString& SocketDescription, ESocketProtocolFamily ProtocolType, bool bForceUDP)
{
	FSocketBSD* NewSocket = (FSocketBSD*)FSocketSubsystemBSD::CreateSocket(SocketType, SocketDescription, ProtocolType, bForceUDP);

	if (NewSocket != nullptr)
	{
		NewSocket->SetIPv6Only(false);
	}
	else
	{
		UE_LOG(LogSockets, Warning, TEXT("Failed to create socket %s [%s]"), *SocketType.ToString(), *SocketDescription);
	}

	return NewSocket;
}

TSharedRef<FInternetAddr> FSocketSubsystemUnix::GetLocalHostAddr(FOutputDevice& Out, bool& bCanBindAll)
{
	// get parent address first
	TSharedRef<FInternetAddr> Addr = FSocketSubsystemBSD::GetLocalHostAddr(Out, bCanBindAll);

	// If the address is not a loopback one (or none), return it.
	uint32 ParentIp = 0;
	Addr->GetIp(ParentIp); // will return in host order
	if (ParentIp != 0 && (ParentIp & 0xff000000) != 0x7f000000)
	{
		return Addr;
	}

	TArray<TSharedPtr<FInternetAddr>> ResultArray;
	if (GetLocalAdapterAddresses(ResultArray))
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("PRIMARYNET")))
		{
			bCanBindAll = false;
		}

		UE_LOG(LogSockets, Verbose, TEXT("Local address is %s"), *(ResultArray[0]->ToString(false)));
		return ResultArray[0]->Clone();
	}
	else
	{
		UE_LOG(LogSockets, Warning, TEXT("GetLocalAdapterAddresses had no results!"));
	}

	// Fall back to this.
	Addr->SetAnyAddress();
	return Addr;
}

bool FSocketSubsystemUnix::GetLocalAdapterAddresses(TArray<TSharedPtr<FInternetAddr> >& OutAddresses)
{
	TSharedRef<FInternetAddr> MultihomeAddress = FSocketSubsystemBSD::CreateInternetAddr();
	bool bHasMultihome = GetMultihomeAddress(MultihomeAddress);

	// Multihome addresses should always be the first in the array.
	if (bHasMultihome)
	{
		OutAddresses.Add(MultihomeAddress);
	}

	ifaddrs* Interfaces = NULL;
	int InterfaceQueryRet = getifaddrs(&Interfaces);
	if (InterfaceQueryRet == 0)
	{
		// Loop through linked list of interfaces
		for (ifaddrs* Travel = Interfaces; Travel != NULL; Travel = Travel->ifa_next)
		{
			// Skip over empty data sets.
			if (Travel->ifa_addr == NULL)
			{
				continue;
			}

			uint16 AddrFamily = Travel->ifa_addr->sa_family;
			// Find any up and non-loopback addresses
			if ((Travel->ifa_flags & IFF_UP) != 0 && 
				(Travel->ifa_flags & IFF_LOOPBACK) == 0 && 
				(AddrFamily == AF_INET || AddrFamily == AF_INET6))
			{
				TSharedRef<FInternetAddrBSD> NewAddress = MakeShareable(new FInternetAddrBSD);
				NewAddress->SetIp(*((sockaddr_storage*)Travel->ifa_addr));

				// Write the scope id if what we found was the multihome address.
				// Don't write it to our list again though.
				if (bHasMultihome && NewAddress == MultihomeAddress)
				{
					static_cast<FInternetAddrBSD&>(MultihomeAddress.Get()).SetScopeId(ntohl(if_nametoindex(Travel->ifa_name)));
				}
				else
				{
					NewAddress->SetScopeId(ntohl(if_nametoindex(Travel->ifa_name)));
					OutAddresses.Add(NewAddress);
				}
			}
		}

		freeifaddrs(Interfaces);
	}
	else
	{
		UE_LOG(LogSockets, Warning, TEXT("getifaddrs returned result %d"), InterfaceQueryRet);
		return bHasMultihome; // if getifaddrs somehow doesn't work but we have multihome, then it's fine.
	}

	return (OutAddresses.Num() > 0);
}
