 
#pragma once

#include "CoreMinimal.h"
#include "LyraClone_CharacterPartTypes.generated.h"

USTRUCT(BlueprintType)
struct FLyraClone_CharacterPartHandle
{
	GENERATED_BODY()

	void Reset()
	{
		PartHandle = INDEX_NONE;
	}
	bool IsValid()const
	{
		return PartHandle != INDEX_NONE;

	}

	UPROPERTY()
	int32 PartHandle = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FLyraClone_CharacterPart
{//part에 대한 정의,메타데이터,레이아웃형태
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TSubclassOf<AActor>PartClass;
	//파츠로 생성된 Actor의 Class 정보를 가지고 있다.
	//즉, 파츠는 Actor단위이다.
	//Bone에 붙여야되기 때문에 bone에 대한 정보가 있어야된다.

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName SocketName;
	//Actor를 소환하여 붙이는데 특정 Bone에 붙이기 위한 정보
	//(따로 정의되어 있지 않으면 Default는 Root)

};