#pragma once

#include "LyraClone_MappableConfigPair.generated.h"

class UPlayerMappableInputConfig;



USTRUCT()
struct FLyraClone_MappableConfigPair
{
	GENERATED_BODY()
public:
	FLyraClone_MappableConfigPair()=default;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UPlayerMappableInputConfig>Config;

	UPROPERTY(EditAnywhere)
	bool bShouldActivateAutomatically = true;

};