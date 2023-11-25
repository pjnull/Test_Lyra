#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CommonSessionSubSystem.generated.h"

class APlayerController;

UCLASS(BlueprintType)
class COMMONUSER_API UCommonSession_HostSessionRequest : public UObject
{
	GENERATED_BODY()
public:
	FString ConstructTravelURL()const;
	FString GetMapName()const;

	UPROPERTY(BlueprintReadWrite, Category = Session)
	FPrimaryAssetId MapID;

	UPROPERTY(BlueprintReadWrite, Category = Session)
	TMap<FString, FString>ExtraArgs;
};

UCLASS()
class COMMONUSER_API UCommonSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UCommonSessionSubsystem();

	UFUNCTION(BlueprintCallable, Category =Session)
	void HostSession(APlayerController* HostingPlayer,UCommonSession_HostSessionRequest* Request);
	
	FString PendingTravelURL;
};