// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "NiagaraCore.generated.h"

typedef uint64 FNiagaraSystemInstanceID;


/** A utility class allowing for references to FNiagaraVariableBase outside of the Niagara module. */
USTRUCT()
struct NIAGARACORE_API FNiagaraVariableCommonReference
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	TObjectPtr<UObject> UnderlyingType;

	bool Serialize(FArchive& Ar);
	friend bool operator<<(FArchive& Ar, FNiagaraVariableCommonReference& VariableReference);

	bool operator==(const FNiagaraVariableCommonReference& Other)const
	{
		return Name == Other.Name && UnderlyingType == Other.UnderlyingType;
	}	
};

template<> struct TStructOpsTypeTraits<FNiagaraVariableCommonReference> : public TStructOpsTypeTraitsBase2<FNiagaraVariableCommonReference>
{
	enum
	{
		WithSerializer = true,
	};
};

inline bool operator<<(FArchive& Ar, FNiagaraVariableCommonReference& VariableReference)
{
	return VariableReference.Serialize(Ar);
}