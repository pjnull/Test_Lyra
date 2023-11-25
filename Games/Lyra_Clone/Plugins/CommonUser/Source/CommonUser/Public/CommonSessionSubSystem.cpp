#include "CommonSessionSubSystem.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonSessionSubSystem)




FString UCommonSession_HostSessionRequest::ConstructTravelURL()const
{
	FString CombinedExtraArgs;

	for (const auto& ExtraArg : ExtraArgs)
	{
		if (ExtraArg.Key.IsEmpty())continue;

		if (ExtraArg.Value.IsEmpty())CombinedExtraArgs += FString::Printf(TEXT("?%s"),*ExtraArg.Key);
		else CombinedExtraArgs += FString::Printf(TEXT("?%s=%s"), *ExtraArg.Key, *ExtraArg.Value);
	}
	return FString::Printf(TEXT("%s%s"),*GetMapName(),*CombinedExtraArgs);
}


FString UCommonSession_HostSessionRequest::GetMapName()const
{
	FAssetData MapAssetData;
	if (UAssetManager::Get().GetPrimaryAssetData(MapID, MapAssetData))
	{
		return MapAssetData.PackageName.ToString();
	}
	return FString();
}

UCommonSessionSubsystem::UCommonSessionSubsystem()
{
}

void UCommonSessionSubsystem::HostSession(APlayerController* HostingPlayer, UCommonSession_HostSessionRequest* Request)
{
	ULocalPlayer* LocalPlayer = (HostingPlayer != nullptr) ? HostingPlayer->GetLocalPlayer() : nullptr;

	if (!LocalPlayer)return;

	GetWorld()->ServerTravel(Request->ConstructTravelURL());
}
