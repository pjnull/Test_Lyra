// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraCustomVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FNiagaraCustomVersion::GUID(0xFCF57AFA, 0x50764283, 0xB9A9E658, 0xFFA02D32);

// Register the custom version with core
FCustomVersionRegistration GRegisterNiagaraCustomVersion(FNiagaraCustomVersion::GUID, FNiagaraCustomVersion::LatestVersion, TEXT("NiagaraVer"));

>>>> ORIGINAL //UE5/Release-Engine-Test/Engine/Plugins/FX/Niagara/Source/NiagaraCore/Private/NiagaraCustomVersion.cpp#7
const FGuid FNiagaraCustomVersion::LatestScriptCompileVersion(0xA83D40AB, 0xE1F64D9F, 0x9F5451FB, 0x8C954501);
==== THEIRS //UE5/Release-Engine-Test/Engine/Plugins/FX/Niagara/Source/NiagaraCore/Private/NiagaraCustomVersion.cpp#8
const FGuid FNiagaraCustomVersion::LatestScriptCompileVersion(0xA83D40AB, 0xE1F64D9F, 0x9F5451FB, 0x8C95FFFF);
==== YOURS //Andy.Davidson_UE5_Release_Engine_Staging/Engine/Plugins/FX/Niagara/Source/NiagaraCore/Private/NiagaraCustomVersion.cpp
const FGuid FNiagaraCustomVersion::LatestScriptCompileVersion(0x8063B243, 0x0A384C01, 0x8A508683, 0x12072D63);
