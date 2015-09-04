// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookOnTheFlyServer.cpp: handles polite cook requests via network ;)
=============================================================================*/

#include "UnrealEd.h"


#include "PackageHelperFunctions.h"
#include "DerivedDataCacheInterface.h"
#include "ISourceControlModule.h"
#include "GlobalShader.h"
#include "MaterialShader.h"
#include "TargetPlatform.h"
#include "IConsoleManager.h"
#include "Developer/PackageDependencyInfo/Public/PackageDependencyInfo.h"
#include "IPlatformFileSandboxWrapper.h"
#include "Messaging.h"
#include "NetworkFileSystem.h"

#include "AssetRegistryModule.h"
#include "AssetData.h"

#include "UnrealEdMessages.h"
#include "GameDelegates.h"
#include "PhysicsPublic.h"
#include "AutoSaveUtils.h"

// cooking stats
#include "CookingStatsModule.h"

// cook by the book requirements
#include "Commandlets/ChunkManifestGenerator.h"
#include "Engine/WorldComposition.h"

// error message log
#include "TokenizedMessage.h"
#include "MessageLog.h"

// shader compiler processAsyncResults
#include "ShaderCompiler.h"
#include "Engine/LevelStreaming.h"
#include "Engine/TextureLODSettings.h"

#define LOCTEXT_NAMESPACE "Cooker"

DEFINE_LOG_CATEGORY_STATIC(LogCook, Log, All);

// these are the stats which are output to the Saved\Stats\<date>.csv file
#define ENABLE_COOKER_STATS 1

#define DEBUG_COOKONTHEFLY 0
#define OUTPUT_TIMING 0

#define USEASSETREGISTRYFORDEPENDENTPACKAGES 1
#define VERIFY_GETDEPENDENTPACKAGES 0 // verify has false hits because old serialization method for generating dependencies had errors (included transient objects which shouldn't be in asset registry), but you can still use verify to build a list then cross check against transient objects.  

#if OUTPUT_TIMING

#define HEIRARCHICAL_TIMER 1
#define PERPACKAGE_TIMER 0

struct FTimerInfo
{
public:

	FTimerInfo( FTimerInfo &&InTimerInfo )
	{
		Swap( Name, InTimerInfo.Name );
		Length = InTimerInfo.Length;
	}

	FTimerInfo( const FTimerInfo &InTimerInfo )
	{
		Name = InTimerInfo.Name;
		Length = InTimerInfo.Length;
	}

	FTimerInfo( FString &&InName, double InLength ) : Name(MoveTemp(InName)), Length(InLength) { }

	FString Name;
	double Length;
};

#if HEIRARCHICAL_TIMER
struct FHierarchicalTimerInfo : public FTimerInfo
{
public:
	FHierarchicalTimerInfo *Parent;
	TMap<FString, FHierarchicalTimerInfo*> Children;

	FHierarchicalTimerInfo(const FHierarchicalTimerInfo& InTimerInfo) : FTimerInfo(InTimerInfo)
	{
		Children = InTimerInfo.Children;
		for (auto& Child : Children)
		{
			Child.Value->Parent = this;
		}
	}

	FHierarchicalTimerInfo(FHierarchicalTimerInfo&& InTimerInfo) : FTimerInfo(InTimerInfo)
	{
		Swap(Children, InTimerInfo.Children);

		for (auto& Child : Children)
		{
			Child.Value->Parent = this;
		}
	}

	FHierarchicalTimerInfo(FString &&Name) : FTimerInfo(MoveTemp(Name), 0.0)
	{
	}

	~FHierarchicalTimerInfo()
	{
		Parent = NULL;
		ClearChildren();
	}

	void ClearChildren()
	{
		for (auto& Child : Children)
		{
			delete Child.Value;
		}
		Children.Empty();
	}

	FHierarchicalTimerInfo* FindChild(const FString& Name)
	{
		FHierarchicalTimerInfo* Child = (FHierarchicalTimerInfo*)(Children.FindRef(Name));
		if ( !Child )
		{
			FString Temp = Name;
			Child = Children.Add(Name, new FHierarchicalTimerInfo(MoveTemp(Temp)));
			Child->Parent = this;
		}

		check(Child);
		return Child;
	}
};

static TMap<FName, int32> IntStats;
void IncIntStat(const FName& Name, const int32 Amount)
{
	int32* Value = IntStats.Find(Name);

	if (Value == NULL)
	{
		IntStats.Add(Name, Amount);
	}
	else
	{
		*Value += Amount;
	}
}


static FHierarchicalTimerInfo RootTimerInfo(MoveTemp(FString(TEXT("Root"))));
static FHierarchicalTimerInfo* CurrentTimerInfo = &RootTimerInfo;
#endif


static TArray<FTimerInfo> GTimerInfo;



struct FScopeTimer
{
private:
	bool Started;
	bool DecrementScope;
	static int GScopeDepth;
#if HEIRARCHICAL_TIMER
	FHierarchicalTimerInfo* HeirarchyTimerInfo;
#endif
public:

	FScopeTimer( const FScopeTimer &outer )
	{
		Index = outer.Index;
		DecrementScope = false;
		Started = false;
	}

	FScopeTimer( const FString &InName, bool IncrementScope = false )
	{
		DecrementScope = IncrementScope;
		
		FString Name = InName;
		for ( int i =0; i < GScopeDepth; ++i )
		{
			Name = FString(TEXT("  ")) + Name;
		}
		if( DecrementScope)
		{
			++GScopeDepth;
		}

#if HEIRARCHICAL_TIMER
		HeirarchyTimerInfo = CurrentTimerInfo->FindChild(Name);
		CurrentTimerInfo = HeirarchyTimerInfo;
#endif
#if PERPACKAGE_TIMER
		Index = GTimerInfo.Emplace(MoveTemp(Name), 0.0);
#endif
		Started = false;
	}

	void Start()
	{
		if ( !Started )
		{
#if PERPACKAGE_TIMER
			GTimerInfo[Index].Length -= FPlatformTime::Seconds();
#endif
			Started = true;
#if HEIRARCHICAL_TIMER
			HeirarchyTimerInfo->Length -= FPlatformTime::Seconds();
#endif
		}
	}

	void Stop()
	{
		if ( Started )
		{
#if HEIRARCHICAL_TIMER
			HeirarchyTimerInfo->Length += FPlatformTime::Seconds();
#endif
#if PERPACKAGE_TIMER
			GTimerInfo[Index].Length += FPlatformTime::Seconds();
#endif
			Started = false;
		}
	}

	~FScopeTimer()
	{
		Stop();
#if HEIRARCHICAL_TIMER
		check(CurrentTimerInfo == HeirarchyTimerInfo);
		CurrentTimerInfo = HeirarchyTimerInfo->Parent;
#endif
		if ( DecrementScope )
		{
			--GScopeDepth;
		}
	}

	int Index;
};

int FScopeTimer::GScopeDepth = 0;



void OutputTimers()
{
#if PERPACKAGE_TIMER
	if ( GTimerInfo.Num() <= 0 )
		return;
	
	static FArchive *OutputDevice = NULL;

	static TMap<FString, int> TimerIndexMap;


	if ( OutputDevice == NULL )
	{
		OutputDevice = IFileManager::Get().CreateFileWriter(TEXT("CookOnTheFlyServerTiming.csv") );
	}

	TArray<FString> OutputValues;
	OutputValues.AddZeroed(TimerIndexMap.Num());

	bool OutputTimerIndexMap = false;
	for ( auto TimerInfo : GTimerInfo )
	{
		int *IndexPtr = TimerIndexMap.Find(TimerInfo.Name);
		int Index = 0;
		if (IndexPtr == NULL)
		{
			Index = TimerIndexMap.Num();
			TimerIndexMap.Add( TimerInfo.Name, Index );
			OutputValues.AddZeroed();
			OutputTimerIndexMap = true;
		}
		else
			Index = *IndexPtr;

		OutputValues[Index] = FString::Printf(TEXT("%f"),TimerInfo.Length);
	}
	static FString NewLine = FString(TEXT("\r\n"));

	if (OutputTimerIndexMap)
	{
		TArray<FString> OutputHeader;
		OutputHeader.AddZeroed( TimerIndexMap.Num() );
		for ( auto TimerIndex : TimerIndexMap )
		{
			int LocalIndex = TimerIndex.Value;
			OutputHeader[LocalIndex] = TimerIndex.Key;
		}

		for ( auto OutputString : OutputHeader)
		{
			OutputString.Append(TEXT(", "));
			OutputDevice->Serialize( (void*)(*OutputString), OutputString.Len() * sizeof(TCHAR));
		}

		
		OutputDevice->Serialize( (void*)*NewLine, NewLine.Len() * sizeof(TCHAR) );
	}

	for ( auto OutputString : OutputValues)
	{
		OutputString.Append(TEXT(", "));
		OutputDevice->Serialize( (void*)(*OutputString), OutputString.Len() * sizeof(TCHAR));
	}
	OutputDevice->Serialize( (void*)*NewLine, NewLine.Len() * sizeof(TCHAR) );

	OutputDevice->Flush();

	UE_LOG( LogCook, Display, TEXT("Timing information for cook") );
	UE_LOG( LogCook, Display, TEXT("Name\tlength(ms)") );
	for ( auto TimerInfo : GTimerInfo )
	{
		UE_LOG( LogCook, Display, TEXT("%s\t%.2f"), *TimerInfo.Name, TimerInfo.Length * 1000.0f );
	}

	// first item is the total
	if ( GTimerInfo.Num() > 0 && ( ( GTimerInfo[0].Length * 1000.0f ) > 40.0f ) )
	{
		UE_LOG( LogCook, Display, TEXT("Cook tick exceeded 40ms by  %f"), GTimerInfo[0].Length * 1000.0f  );
	}

	GTimerInfo.Empty();
#endif
}
#if HEIRARCHICAL_TIMER
void OutputHierarchyTimers(const FHierarchicalTimerInfo* TimerInfo, FString& Output, int32& Depth)
{
	// put our line into the output buffer
	/*for (int32 I = 0; I < Depth; ++I)
	{
		Output += TEXT("\t");
	}*/
	
	Output += FString::Printf(TEXT("%s: %fms\r\n"), *TimerInfo->Name, TimerInfo->Length * 1000);

	++Depth;
	for (const auto& ChildInfo : TimerInfo->Children)
	{
		OutputHierarchyTimers(ChildInfo.Value, Output, Depth);
	}
	--Depth;
}

void OutputHierarchyTimers()
{
	FHierarchicalTimerInfo* TimerInfo = &RootTimerInfo;

	int32 Depth = 0;
	FString Output = TEXT("Hierarchy timer information\nName:  Length(ms)");
	OutputHierarchyTimers(TimerInfo, Output, Depth);

	Output += FString::Printf(TEXT("IntStats\n"));
	for (const auto& IntStat : IntStats)
	{
		Output += FString::Printf(TEXT("%s=%d\r\n"), *IntStat.Key.ToString(), IntStat.Value);
	}

	UE_LOG(LogCook, Display, TEXT("%s"), *Output);

	RootTimerInfo.ClearChildren();
}
#endif

#define CREATE_TIMER(name, incrementScope) FScopeTimer ScopeTimer##name(#name, incrementScope); 

#define SCOPE_TIMER(name) CREATE_TIMER(name, true); ScopeTimer##name.Start();
#define STOP_TIMER( name ) ScopeTimer##name.Stop();


#define ACCUMULATE_TIMER(name) CREATE_TIMER(name, false);
#define ACCUMULATE_TIMER_SCOPE(name) FScopeTimer ScopeTimerInner##name(ScopeTimer##name); ScopeTimerInner##name.Start();
#define ACCUMULATE_TIMER_START(name) ScopeTimer##name.Start();
#define ACCUMULATE_TIMER_STOP(name) ScopeTimer##name.Stop();
#if HEIRARCHICAL_TIMER
#define INC_INT_STAT( name, amount ) { static FName StaticName##name(#name); IncIntStat(StaticName##name, amount); }
#else
#define INC_INT_STAT( name, amount ) 
#endif

#define OUTPUT_TIMERS() OutputTimers();
#define OUTPUT_HIERARCHYTIMERS() OutputHierarchyTimers();

#else
#define CREATE_TIMER(name)

#define SCOPE_TIMER(name)
#define STOP_TIMER(name)

#define ACCUMULATE_TIMER(name) 
#define ACCUMULATE_TIMER_SCOPE(name) 
#define ACCUMULATE_TIMER_START(name) 
#define ACCUMULATE_TIMER_STOP(name) 
#define INC_INT_STAT( name, amount ) 

#define OUTPUT_TIMERS()
#define OUTPUT_HIERARCHYTIMERS()
#endif



#if ENABLE_COOKER_STATS
#include "TypeHash.h"
#include "CookingStatsModule.h"

// gather stats about a cooked package
namespace CookOnTheFlyStats
{
	ICookingStats* GetCookingStats()
	{
		static ICookingStats* CookingStats = nullptr;
		static bool bInitialized = false;
		if (bInitialized == false)
		{
			FCookingStatsModule* CookingStatsModule = FModuleManager::LoadModulePtr<FCookingStatsModule>(TEXT("CookingStats"));
			if (CookingStatsModule)
			{
				CookingStats = &CookingStatsModule->Get();
			}
			bInitialized = true;
		}
		return CookingStats;
	}



	class FCookerStat
	{
	private:
		bool bStarted;
		double StartTime;
		const FName TagName;
	public:
		FCookerStat(const FName& InTagName) : TagName(InTagName)
		{
			StartTime = FPlatformTime::Seconds();
			bStarted = true;
		}

		~FCookerStat()
		{
			Stop();
		}

		void Stop()
		{
			ICookingStats* CookingStats = GetCookingStats();
			if (CookingStats)
			{
				static const FName NAME_CookerStat(TEXT("CookerStat"));
				double Duration = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
				CookingStats->AddTagValue(NAME_CookerStat, TagName, FString::Printf(TEXT("%fms"), (float)(Duration)));
			}
		}

	};

	class FCookerPackageStats
	{
	private:
		const FName Filename; 
	public:
		FCookerPackageStats(const FName& InFilename) : Filename(InFilename)
		{

		};
		void AddTag(const FName& TagName, const FString& Value)
		{
			ICookingStats* CookingStats = GetCookingStats();
			if (CookingStats)
			{
				CookingStats->AddTagValue(Filename, TagName, Value);
			}
		}

		bool GetTag(const FName& TagName, FString& Value) const
		{
			ICookingStats* CookingStats = GetCookingStats();
			if (CookingStats)
			{
				return CookingStats->GetTagValue(Filename, TagName, Value);
			}
			return false;
		}

		const FName& GetFilename() const 
		{
			return Filename;
		}
	};

	struct FPackageStat
	{
		FPackageStat(const FName& InPackageName, const FName& InStatName)
		{
			PackageName = InPackageName;
			StatName = InStatName;
		}

		bool operator==(const FPackageStat& InPackageStat) const
		{
			return (InPackageStat.PackageName == PackageName) &&
				(InPackageStat.StatName == StatName);
		}

		FName PackageName;
		FName StatName;

	};

	struct FStatInfo
	{
		int HitCount;
		bool bStarted;
		double StartTime;
		double Duration;
	};

	TMap<FPackageStat, FStatInfo> StatInfoMap;

	struct FNameCache
	{
	public:
		FNameCache(const FName& BaseName) 
		{
			Duration = FName(*FString::Printf(TEXT("%s_Duration"), *BaseName.ToString()));
			HitCount = FName(*FString::Printf(TEXT("%s_HitCount"), *BaseName.ToString()));
		}
		FName Duration;
		FName HitCount;
	};
	TMap<FName, FNameCache> NameCache;

	const FNameCache& GetNameCache(const FName& BaseName)
	{
		const FNameCache* Result = NameCache.Find(BaseName);
		if (Result == nullptr)
		{
			Result = &NameCache.Add(BaseName, FNameCache(BaseName));
		}
		return *Result;
	}

	FStatInfo* GetStatInfo(const FName& StatName, const FName& PackageName)
	{
		FPackageStat PackageStat(PackageName, StatName);

		FStatInfo *StatInfo = StatInfoMap.Find(PackageStat);
		if (StatInfo == nullptr)
		{
			StatInfo = &StatInfoMap.Add(PackageStat);
		}
		return StatInfo;
	}

	void StartPackageStat(const FName& Name, const FCookerPackageStats& CookerPackageStats)
	{
		FStatInfo* StatInfo = GetStatInfo(Name, CookerPackageStats.GetFilename());

		if (StatInfo->bStarted == false)
		{
			StatInfo->StartTime = FPlatformTime::Seconds();
			StatInfo->bStarted = true;
		}
	}

	void EndPackageStat(const FName& Name, FCookerPackageStats& CookerPackageStats)
	{
		FStatInfo* StatInfo = GetStatInfo(Name, CookerPackageStats.GetFilename());

		if (StatInfo->bStarted)
		{
			++StatInfo->HitCount;
			double Duration = FPlatformTime::Seconds() - StatInfo->StartTime;
			StatInfo->Duration += Duration;
			const FNameCache& CachedFNames = GetNameCache(Name);

			CookerPackageStats.AddTag(CachedFNames.Duration, FString::Printf(TEXT("%fms"), StatInfo->Duration*1000.0f));
			CookerPackageStats.AddTag(CachedFNames.HitCount, FString::Printf(TEXT("%d"), StatInfo->HitCount));

			StatInfo->bStarted = false;
		}
	}



	class FCookerPackageScopeStat
	{
	private:
		const FName& StatName;
		FCookerPackageStats& PackageStats;
	public:
		FCookerPackageScopeStat(const FName& InStatName, FCookerPackageStats& InPackageStats) : StatName(InStatName), PackageStats(InPackageStats)
		{
			Start();
		}

		~FCookerPackageScopeStat()
		{
			End();
		}

		void Start()
		{
			StartPackageStat(StatName, PackageStats);
		}
		void End()
		{
			EndPackageStat(StatName, PackageStats);
		}
	};

	uint32 GetTypeHash(const FPackageStat& PackageStat)
	{
		return HashCombine(GetTypeHash(PackageStat.PackageName), GetTypeHash(PackageStat.StatName));
	}
}

// global cooking stat which isn't associated with a specific package
#define SCOPE_COOKING_STAT(name) \
	static const FName NAME_##name(#name); \
	CookOnTheFlyStats::FCookerStat Stat_##name(NAME_##name);

#define START_PACKAGE(filename) CookOnTheFlyStats::FCookerPackageStats CookerPackageStats(filename);

#define SCOPE_PACKAGE_STAT(name) \
	static const FName NAME_##name(#name); \
	CookOnTheFlyStats::FCookerPackageScopeStat CookerPackageScopeStat##name(NAME_##name, CookerPackageStats);

// static const FName NAME_StartTime(TEXT("StartTime"));

#define START_PACKAGE_STAT(name) \
	static const FName NAME_##name(TEXT(#name)); \
	CookOnTheFlyStats::StartPackageStat(NAME_##name, CookerPackageStats);

// CookerPackageStats.AddTag(NAME_##nameStartTime, FString::Printf(TEXT("%f"),FPlatformTime::Seconds()));

#define END_PACKAGE_STAT(name) \
	static const FName NAME2_##name(TEXT(#name)); \
	CookOnTheFlyStats::EndPackageStat(NAME2_##name, CookerPackageStats);


#else

#define SCOPE_COOKING_STAT(name)
#define START_PACKAGE(filename)
#define SCOPE_PACKAGE_STAT(name)
#define START_PACKAGE_STAT(name)
#define END_PACKAGE_STAT(name)

#endif


////////////////////////////////////////////////////////////////
/// Cook on the fly server
///////////////////////////////////////////////////////////////



/* helper structs functions
 *****************************************************************************/

/** Helper to pass a recompile request to game thread */
struct FRecompileRequest
{
	struct FShaderRecompileData RecompileData;
	bool bComplete;
};



const FString& GetAssetRegistryPath()
{
	static const FString AssetRegistryPath = FPaths::GameDir();
	return AssetRegistryPath;
}

const FString GetStatsFilename(const FString& ResponseFilename)
{
	if (ResponseFilename.Len())
	{
		return ResponseFilename + TEXT("Stats.csv");
	}
	else
	{
		static FString StatsPath = FPaths::GameSavedDir() / FString::Printf(TEXT("Stats/%sStats.csv"), *FDateTime::Now().ToString());
		return StatsPath;
	}
}

FString GetChildCookerResultFilename(const FString& ResponseFilename)
{
	FString Result = ResponseFilename + TEXT("Result.txt");
	return Result;
}

/**
 * Return the release asset registry filename for the release version supplied
 */
FString GetReleaseVersionAssetRegistryPath(const FString& ReleaseVersion, const FName& PlatformName )
{
	// cache the part of the path which is static because getting the GameDir is really slow and also string manipulation
	const static FString GameDirectory = FPaths::GameDir() / FString(TEXT("Releases"));
	return  GameDirectory / ReleaseVersion / PlatformName.ToString();
}

const FString& GetAssetRegistryFilename()
{
	static const FString AssetRegistryFilename = FString(TEXT("AssetRegistry.bin"));
	return AssetRegistryFilename;
}

/**
 * Uses the FMessageLog to log a message
 * 
 * @param Message to log
 * @param Severity of the message
 */
void LogCookerMessage( const FString& MessageText, EMessageSeverity::Type Severity)
{
	FMessageLog MessageLog("CookResults");

	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(Severity);

	Message->AddToken( FTextToken::Create( FText::FromString(MessageText) ) );
	// Message->AddToken(FTextToken::Create(MessageLogTextDetail)); 
	// Message->AddToken(FDocumentationToken::Create(TEXT("https://docs.unrealengine.com/latest/INT/Platforms/iOS/QuickStart/6/index.html"))); 
	MessageLog.AddMessage(Message);

	MessageLog.Notify(FText(), EMessageSeverity::Warning, false);
}



/* FIlename caching functions
 *****************************************************************************/

FString UCookOnTheFlyServer::GetCachedPackageFilename( const FName& PackageName ) const 
{
	return Cache( PackageName ).PackageFilename;
}

FString UCookOnTheFlyServer::GetCachedStandardPackageFilename( const FName& PackageName ) const 
{
	return Cache( PackageName ).StandardFilename;
}

FName UCookOnTheFlyServer::GetCachedStandardPackageFileFName( const FName& PackageName ) const 
{
	return Cache( PackageName ).StandardFileFName;
}


FString UCookOnTheFlyServer::GetCachedPackageFilename( const UPackage* Package ) const 
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache( Package->GetFName() ).PackageFilename;
}

FString UCookOnTheFlyServer::GetCachedStandardPackageFilename( const UPackage* Package ) const 
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache( Package->GetFName() ).StandardFilename;
}

FName UCookOnTheFlyServer::GetCachedStandardPackageFileFName( const UPackage* Package ) const 
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache( Package->GetFName() ).StandardFileFName;
}


bool UCookOnTheFlyServer::ClearPackageFilenameCacheForPackage( const FName& PackageName ) const
{
	return PackageFilenameCache.Remove( PackageName ) >= 1;
}

bool UCookOnTheFlyServer::ClearPackageFilenameCacheForPackage( const UPackage* Package ) const
{
	return PackageFilenameCache.Remove( Package->GetFName() ) >= 1;
}

const FString& UCookOnTheFlyServer::GetCachedSandboxFilename( const UPackage* Package, TAutoPtr<class FSandboxPlatformFile>& InSandboxFile ) const 
{
	FName PackageFName = Package->GetFName();
	static TMap<FName, FString> CachedSandboxFilenames;
	FString* CachedSandboxFilename = CachedSandboxFilenames.Find(PackageFName);
	if ( CachedSandboxFilename )
		return *CachedSandboxFilename;

	const FString& PackageFilename = GetCachedPackageFilename(Package);
	FString SandboxFilename = ConvertToFullSandboxPath(*PackageFilename, true );

	return CachedSandboxFilenames.Add( PackageFName, MoveTemp(SandboxFilename) );
}

const UCookOnTheFlyServer::FCachedPackageFilename& UCookOnTheFlyServer::Cache(const FName& PackageName) const 
{
	FCachedPackageFilename *Cached = PackageFilenameCache.Find( PackageName );
	if ( Cached != NULL )
	{
		return *Cached;
	}
	// cache all the things, like it's your birthday!

	FString Filename;
	FString PackageFilename;
	FString StandardFilename;
	FName StandardFileFName = NAME_None;
	if (FPackageName::DoesPackageExist(PackageName.ToString(), NULL, &Filename))
	{
		StandardFilename = PackageFilename = FPaths::ConvertRelativePathToFull(Filename);


		FPaths::MakeStandardFilename(StandardFilename);
		StandardFileFName = FName(*StandardFilename);
	}
	PackageFilenameToPackageFNameCache.Add(StandardFileFName, PackageName);
	return PackageFilenameCache.Emplace( PackageName, MoveTemp(FCachedPackageFilename(MoveTemp(PackageFilename),MoveTemp(StandardFilename), StandardFileFName)) );
}


const FName* UCookOnTheFlyServer::GetCachedPackageFilenameToPackageFName(const FName& StandardPackageFilename) const
{
	return PackageFilenameToPackageFNameCache.Find(StandardPackageFilename);
}

void UCookOnTheFlyServer::ClearPackageFilenameCache() const 
{
	PackageFilenameCache.Empty();
	PackageFilenameToPackageFNameCache.Empty();
	// need to clear the IniVersionStringsMap too
	CachedIniVersionStringsMap.Empty();
}


/* UCookOnTheFlyServer functions
 *****************************************************************************/

UCookOnTheFlyServer::UCookOnTheFlyServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	CurrentCookMode(ECookMode::CookOnTheFly),
	CookByTheBookOptions(NULL),
	CookFlags(ECookInitializationFlags::None),
	bIsInitializingSandbox(false),
	bIsSavingPackage(false)
{
}

UCookOnTheFlyServer::~UCookOnTheFlyServer()
{
	check(TickChildCookers() == true);
	if ( CookByTheBookOptions )
	{		
		delete CookByTheBookOptions;
		CookByTheBookOptions = NULL;
	}
}

void UCookOnTheFlyServer::Tick(float DeltaTime)
{
	uint32 CookedPackagesCount = 0;
	const static float CookOnTheSideTimeSlice = 0.1f; // seconds
	TickCookOnTheSide( CookOnTheSideTimeSlice, CookedPackagesCount);
	TickRecompileShaderRequests();
}

bool UCookOnTheFlyServer::IsTickable() const 
{ 
	return IsCookFlagSet(ECookInitializationFlags::AutoTick); 
}

TStatId UCookOnTheFlyServer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCookServer, STATGROUP_Tickables);
}

bool UCookOnTheFlyServer::StartNetworkFileServer( const bool BindAnyPort )
{
	SCOPE_COOKING_STAT(StartNetworkFileServer);
	
	check( IsCookOnTheFlyMode() );
	//GetDerivedDataCacheRef().WaitForQuiescence(false);

	InitializeSandbox();

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	const TArray<ITargetPlatform*>& Platforms = TPM.GetCookingTargetPlatforms();

	{
		// When cooking on the fly the full registry is saved at the beginning
		// in cook by the book asset registry is saved after the cook is finished
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// write it out to a memory archive
		FArrayWriter SerializedAssetRegistry;
		SerializedAssetRegistry.SetFilterEditorOnly(true);
		AssetRegistry.Serialize(SerializedAssetRegistry);
		UE_LOG(LogCook, Display, TEXT("Generated asset registry size is %5.2fkb"), (float)SerializedAssetRegistry.Num() / 1024.f);

		// now save it in each cooked directory
		FString RegistryFilename = FPaths::GameDir() / TEXT("AssetRegistry.bin");
		// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
		FString SandboxFilename = ConvertToFullSandboxPath(*RegistryFilename, true);

		for (int32 Index = 0; Index < Platforms.Num(); Index++)
		{
			FString PlatFilename = SandboxFilename.Replace(TEXT("[Platform]"), *Platforms[Index]->PlatformName());
			FFileHelper::SaveArrayToFile(SerializedAssetRegistry, *PlatFilename);
		}
	}

	// start the listening thread
	FFileRequestDelegate FileRequestDelegate(FFileRequestDelegate::CreateUObject(this, &UCookOnTheFlyServer::HandleNetworkFileServerFileRequest));
	FRecompileShadersDelegate RecompileShadersDelegate(FRecompileShadersDelegate::CreateUObject(this, &UCookOnTheFlyServer::HandleNetworkFileServerRecompileShaders));

	INetworkFileServer *TcpFileServer = FModuleManager::LoadModuleChecked<INetworkFileSystemModule>("NetworkFileSystem")
		.CreateNetworkFileServer(true, BindAnyPort ? 0 : -1, &FileRequestDelegate, &RecompileShadersDelegate, ENetworkFileServerProtocol::NFSP_Tcp);
	if ( TcpFileServer )
	{
		NetworkFileServers.Add(TcpFileServer);
	}

	INetworkFileServer *HttpFileServer = FModuleManager::LoadModuleChecked<INetworkFileSystemModule>("NetworkFileSystem")
		.CreateNetworkFileServer(true, BindAnyPort ? 0 : -1, &FileRequestDelegate, &RecompileShadersDelegate, ENetworkFileServerProtocol::NFSP_Http);
	if ( HttpFileServer )
	{
		NetworkFileServers.Add( HttpFileServer );
	}

	// loop while waiting for requests
	GIsRequestingExit = false;
	return true;
}


bool UCookOnTheFlyServer::BroadcastFileserverPresence( const FGuid &InstanceId )
{
	
	TArray<FString> AddressStringList;

	for ( int i = 0; i < NetworkFileServers.Num(); ++i )
	{
		TArray<TSharedPtr<FInternetAddr> > AddressList;
		INetworkFileServer *NetworkFileServer = NetworkFileServers[i];
		if ((NetworkFileServer == NULL || !NetworkFileServer->IsItReadyToAcceptConnections() || !NetworkFileServer->GetAddressList(AddressList)))
		{
			LogCookerMessage( FString(TEXT("Failed to create network file server")), EMessageSeverity::Error );
			UE_LOG(LogCook, Error, TEXT("Failed to create network file server"));
			continue;
		}

		// broadcast our presence
		if (InstanceId.IsValid())
		{
			for (int32 AddressIndex = 0; AddressIndex < AddressList.Num(); ++AddressIndex)
			{
				AddressStringList.Add(FString::Printf( TEXT("%s://%s"), *NetworkFileServer->GetSupportedProtocol(),  *AddressList[AddressIndex]->ToString(true)));
			}

		}
	}



	FMessageEndpointPtr MessageEndpoint = FMessageEndpoint::Builder("UCookOnTheFlyServer").Build();

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Publish(new FFileServerReady(AddressStringList, InstanceId), EMessageScope::Network);
	}		
	
	return true;
}

/*----------------------------------------------------------------------------
	FArchiveFindReferences.
----------------------------------------------------------------------------*/
/**
 * Archive for gathering all the object references to other objects
 */
class FArchiveFindReferences : public FArchiveUObject
{
private:
	/**
	 * I/O function.  Called when an object reference is encountered.
	 *
	 * @param	Obj		a pointer to the object that was encountered
	 */
	FArchive& operator<<( UObject*& Obj ) override
	{
		if( Obj )
		{
			FoundObject( Obj );
		}
		return *this;
	}

	virtual FArchive& operator<< (class FAssetPtr& Value) override
	{
		if ( Value.Get() )
		{
			Value.Get()->Serialize( *this );
		}
		return *this;
	}
	virtual FArchive& operator<< (struct FStringAssetReference& Value) override
	{
		if ( Value.ResolveObject() )
		{
			Value.ResolveObject()->Serialize( *this );
		}
		return *this;
	}


	void FoundObject( UObject* Object )
	{
		if ( RootSet.Find(Object) == NULL )
		{
			if ( Exclude.Find(Object) == INDEX_NONE )
			{
				// remove this check later because don't want this happening in development builds
				//check(RootSetArray.Find(Object)==INDEX_NONE);

				RootSetArray.Add( Object );
				RootSet.Add(Object);
				Found.Add(Object);
			}
		}
	}


	/**
	 * list of Outers to ignore;  any objects encountered that have one of
	 * these objects as an Outer will also be ignored
	 */
	TArray<UObject*> &Exclude;

	/** list of objects that have been found */
	TSet<UObject*> &Found;
	
	/** the objects to display references to */
	TArray<UObject*> RootSetArray;
	/** Reflection of the rootsetarray */
	TSet<UObject*> RootSet;

public:

	/**
	 * Constructor
	 * 
	 * @param	inOutputAr		archive to use for logging results
	 * @param	inOuter			only consider objects that do not have this object as its Outer
	 * @param	inSource		object to show references for
	 * @param	inExclude		list of objects that should be ignored if encountered while serializing SourceObject
	 */
	FArchiveFindReferences( TSet<UObject*> InRootSet, TSet<UObject*> &inFound, TArray<UObject*> &inExclude )
		: Exclude(inExclude)
		, Found(inFound)
		, RootSet(InRootSet)
	{
		ArIsObjectReferenceCollector = true;
		ArIsSaving = true;

		for ( const auto& Object : RootSet )
		{
			RootSetArray.Add( Object );
		}
		
		// loop through all the objects in the root set and serialize them
		for ( int RootIndex = 0; RootIndex < RootSetArray.Num(); ++RootIndex )
		{
			UObject* SourceObject = RootSetArray[RootIndex];

			// quick sanity check
			check(SourceObject);
			check(SourceObject->IsValidLowLevel());

			SourceObject->Serialize( *this );
		}

	}

	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const override { return TEXT("FArchiveFindReferences"); }
};

void UCookOnTheFlyServer::GetDependentPackages(const TSet<UPackage*>& RootPackages, TSet<FName>& FoundPackages)
{
	check(IsChildCooker() == false);

	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TSet<FName> RootPackageFNames;
	for (const UPackage* RootPackage : RootPackages)
	{
		RootPackageFNames.Add(RootPackage->GetFName());
	}


	GetDependentPackages(RootPackageFNames, FoundPackages);

}


void UCookOnTheFlyServer::GetDependentPackages( const TSet<FName>& RootPackages, TSet<FName>& FoundPackages )
{
	check(IsChildCooker() == false);

	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FName> FoundPackagesArray;
	for (const auto& RootPackage : RootPackages)
	{
		FoundPackagesArray.Add(RootPackage);
		FoundPackages.Add(RootPackage);
	}

	int FoundPackagesCounter = 0;
	while ( FoundPackagesCounter < FoundPackagesArray.Num() )
	{
		TArray<FName> PackageDependencies;
		if (AssetRegistry.GetDependencies(FoundPackagesArray[FoundPackagesCounter], PackageDependencies) == false)
		{
			// this could happen if we are in the editor and the dependency list is not up to date

			if (IsCookingInEditor() == false)
			{
				UE_LOG(LogCook, Fatal, TEXT("Unable to find package %s in asset registry.  Can't generate cooked asset registry"), *FoundPackagesArray[FoundPackagesCounter].ToString());
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to find package %s in asset registry, cooked asset registry information may be invalid "), *FoundPackagesArray[FoundPackagesCounter].ToString());
			}
		}
		++FoundPackagesCounter;
		for ( const auto& OriginalPackageDependency : PackageDependencies )
		{
			// check(PackageDependency.ToString().StartsWith(TEXT("/")));
			FName PackageDependency = OriginalPackageDependency;
			FString PackageDepdencyString = PackageDependency.ToString();

			FText OutReason;
			const bool bIncludeReadOnlyRoots = true; // Dependency packages are often script packages (read-only)
			if (!FPackageName::IsValidLongPackageName(PackageDepdencyString, bIncludeReadOnlyRoots, &OutReason))
			{
				const FText FailMessage = FText::Format(LOCTEXT("UnableToGeneratePackageName", "Unable to generate long package name for {0}. {1}"),
					FText::FromString(PackageDepdencyString), OutReason);

				LogCookerMessage(FailMessage.ToString(), EMessageSeverity::Warning);
				UE_LOG(LogCook, Warning, TEXT("%s"), *( FailMessage.ToString() ));
				continue;
			}
			else if (FPackageName::IsScriptPackage(PackageDepdencyString))
			{
				continue;
			}

			if ( FoundPackages.Contains(PackageDependency) == false )
			{
				FoundPackages.Add(PackageDependency);
				FoundPackagesArray.Add( PackageDependency );
			}
		}
	}	


#if VERIFY_GETDEPENDENTPACKAGES

	TSet<UObject*> RootSet;
	TSet<UObject*> FoundObjects;
	for (UPackage* Package : RootPackages)
	{
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter(Package, ObjectsInPackage, true);
		for (UObject* Obj : ObjectsInPackage)
		{
			RootSet.Add(Obj);
			FoundObjects.Add(Obj);
		}
	}

	TArray<UObject*> Exclude;
	FArchiveFindReferences ArFindReferences( RootSet, FoundObjects, Exclude );

	TSet<UPackage*> NewFoundPackages;
	for ( const auto& Obj : FoundObjects )
	{
		NewFoundPackages.Add( Obj->GetOutermost() );
	}


	// verify that the old code (directly above) functions the same as the new code

	for (const UPackage* NewPackage : NewFoundPackages)
	{
		UE_LOG(LogCook, Display, TEXT("Old Found package %s"), *NewPackage->GetFName().ToString());
	}
	for (const auto& CurrentPackage : FoundPackages )
	{
		UE_LOG(LogCook, Display, TEXT("New Found package %s"), *CurrentPackage.ToString());
	}

	for ( const UPackage* NewPackage : NewFoundPackages )
	{
		if (FoundPackages.Contains(NewPackage->GetFName()) == false)
		{
			UE_LOG(LogCook, Display, TEXT("Package was missing from new generated packages list %s"), *NewPackage->GetFName().ToString());
		}
		//check( FoundPackages.Contains( NewPackage->GetFName() ) );
	}
	check(NewFoundPackages.Num() == FoundPackages.Num());

#endif

}

void UCookOnTheFlyServer::GetDependencies( const TSet<UPackage*>& Packages, TSet<UObject*>& Found)
{
	TSet<UObject*> RootSet;

	for (UPackage* Package : Packages)
	{
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter(Package, ObjectsInPackage, true);
		for (UObject* Obj : ObjectsInPackage)
		{
			RootSet.Add(Obj);
			Found.Add(Obj);
		}
	}

	TArray<UObject*> Exclude;
	FArchiveFindReferences ArFindReferences( RootSet, Found, Exclude );
}

bool UCookOnTheFlyServer::ContainsMap(const FName& PackageName) const
{
	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();


	TArray<FAssetData> Assets;
	ensure( AssetRegistry.GetAssetsByPackageName(PackageName, Assets) );

	for (const auto& Asset : Assets)
	{
		if (Asset.GetClass()->IsChildOf(UWorld::StaticClass()) || Asset.GetClass()->IsChildOf(ULevel::StaticClass()))
		{
			return true;
		}
	}
	return false;
}

void UCookOnTheFlyServer::GenerateManifestInfo(UPackage* Package, const TArray<FName>& TargetPlatformNames)
{
	if (!CookByTheBookOptions)
		return;

	check(IsChildCooker() == false);

	// generate dependency information for this package

	TSet<UPackage*> RootPackages;
	RootPackages.Add(Package);

	FString LastLoadedMapName;

	// load sublevels
	UWorld* World = UWorld::FindWorldInPackage(Package);

#define VERIFY_LEVELS_IN_DEPENDENCIES_LIST 1
#if VERIFY_LEVELS_IN_DEPENDENCIES_LIST
	TArray<FString> LevelPackageNames;
#endif

	if (World)
	{
		for (const auto& StreamingLevel : World->StreamingLevels)
		{
			if (StreamingLevel->GetLoadedLevel())
			{
				RootPackages.Add(StreamingLevel->GetLoadedLevel()->GetOutermost());
#if VERIFY_LEVELS_IN_DEPENDENCIES_LIST
				LevelPackageNames.AddUnique(StreamingLevel->GetLoadedLevel()->GetOutermost()->GetName());
#endif
			}
		}

		TArray<FString> NewPackagesToCook;

		// Collect world composition tile packages to cook
		if (World->WorldComposition)
		{
			World->WorldComposition->CollectTilesToCook(NewPackagesToCook);
		}

#if VERIFY_LEVELS_IN_DEPENDENCIES_LIST
		for (const auto& PackageToCook : NewPackagesToCook)
		{
			LevelPackageNames.AddUnique(PackageToCook);
		}
#endif

		for (const auto& PackageName : NewPackagesToCook)
		{
			UPackage* PackageToCook = LoadPackage(NULL, *PackageName, LOAD_None);

			RootPackages.Add(PackageToCook);
			//GetDependencies( PackageToCook, Dependencies );

			// Dependencies.Add(PackageToCook);
		}

		LastLoadedMapName = Package->GetName();
	}
#if USEASSETREGISTRYFORDEPENDENTPACKAGES

	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TSet<FName> Packages;

	GetDependentPackages(RootPackages, Packages);

	auto GetPackageFName = [](const FName& Name) { return Name; };


#if VERIFY_LEVELS_IN_DEPENDENCIES_LIST
	if (World)
	{
		TSet<UPackage*> WorldPackages;
		WorldPackages.Add(Package);
		TSet<FName> DependentWorldPackages;
		GetDependentPackages(WorldPackages, DependentWorldPackages);

		for (const auto& LevelPackageName : LevelPackageNames)
		{
			check(DependentWorldPackages.Contains(FName(*LevelPackageName)));
		}
	}
#endif

#else
	TSet<UObject*> Dependencies;
	GetDependencies(RootPackages, Dependencies);

	TSet<UPackage*> Packages;
	for (const auto& Object : Dependencies)
	{
		Packages.Add(Object->GetOutermost());
	}

	auto GetPackageFName = [](const UPackage* Package) { return Package->GetFName(); };
#endif


}
bool UCookOnTheFlyServer::IsCookingInEditor() const
{
	return CurrentCookMode == ECookMode::CookByTheBookFromTheEditor || CurrentCookMode == ECookMode::CookOnTheFlyFromTheEditor;;
}

bool UCookOnTheFlyServer::IsRealtimeMode() const 
{
	return CurrentCookMode == ECookMode::CookByTheBookFromTheEditor || CurrentCookMode == ECookMode::CookOnTheFlyFromTheEditor;
}

bool UCookOnTheFlyServer::IsCookByTheBookMode() const
{
	return CurrentCookMode == ECookMode::CookByTheBookFromTheEditor || CurrentCookMode == ECookMode::CookByTheBook;
}

bool UCookOnTheFlyServer::IsCookOnTheFlyMode() const
{
	return CurrentCookMode == ECookMode::CookOnTheFly || CurrentCookMode == ECookMode::CookOnTheFlyFromTheEditor; 
}

FString UCookOnTheFlyServer::GetDLCContentPath()
{
	return FPaths::GamePluginsDir() / CookByTheBookOptions->DlcName / FString(TEXT("Content"));
}

COREUOBJECT_API extern bool GOutputCookingWarnings;


bool UCookOnTheFlyServer::RequestPackage(const FName& StandardPackageFName, const TArray<FName>& TargetPlatforms, const bool bForceFrontOfQueue)
{
	FFilePlatformRequest FileRequest(StandardPackageFName, TargetPlatforms);
	CookRequests.EnqueueUnique(FileRequest, bForceFrontOfQueue);
	return true;
}


bool UCookOnTheFlyServer::RequestPackage(const FName& StandardPackageFName, const bool bForceFrontOfQueue)
{
	check(IsCookByTheBookMode());
	// need target platforms if we are not in cook by the book mode
	FFilePlatformRequest FileRequest(StandardPackageFName, CookByTheBookOptions->TargetPlatformNames);
	CookRequests.EnqueueUnique(FileRequest, bForceFrontOfQueue);
	return true;
}

// should only call this after tickchildcookers returns false
void UCookOnTheFlyServer::CleanUpChildCookers()
{
	if (IsCookByTheBookMode())
	{
		const FString MasterCookerStatsFilename = GetStatsFilename(FString());

		FString AllStats;
		ensure(FFileHelper::LoadFileToString(AllStats, *MasterCookerStatsFilename));
		
		for (auto& ChildCooker : CookByTheBookOptions->ChildCookers)
		{
			check(ChildCooker.bFinished == true);
			const FString ChildCookerStatsFilename = GetStatsFilename(ChildCooker.ResponseFileName);

			FString ChildStats;
			ensure(FFileHelper::LoadFileToString(ChildStats, *ChildCookerStatsFilename));

			AllStats += ChildStats;

			if (ChildCooker.Thread != nullptr)
			{
				ChildCooker.Thread->WaitForCompletion();
				delete ChildCooker.Thread;
				ChildCooker.Thread = nullptr;
			}
		}

		ensure(FFileHelper::SaveStringToFile(AllStats, *MasterCookerStatsFilename));
	}
}


bool UCookOnTheFlyServer::TickChildCookers()
{
	if (IsCookByTheBookMode())
	{
		for (auto& ChildCooker : CookByTheBookOptions->ChildCookers)
		{
			if (ChildCooker.bFinished == false)
			{
				return false;
			}
		}
	}
	return true;

}


uint32 UCookOnTheFlyServer::TickCookOnTheSide( const float TimeSlice, uint32 &CookedPackageCount )
{
	struct FCookerTimer
	{
		const bool bIsRealtimeMode;
		const double StartTime;
		const float &TimeSlice;
		const int MaxNumPackagesToSave; // maximum packages to save before exiting tick (this should never really hit unless we are not using realtime mode)
		int NumPackagesSaved;
		
		FCookerTimer(const float &InTimeSlice, bool bInIsRealtimeMode, int InMaxNumPackagesToSave = 30 ) : 
			bIsRealtimeMode( bInIsRealtimeMode), StartTime(FPlatformTime::Seconds()), TimeSlice(InTimeSlice), 
			MaxNumPackagesToSave(InMaxNumPackagesToSave), NumPackagesSaved(0)
		{
		}
		double GetTimeTillNow()
		{
			return FPlatformTime::Seconds() - StartTime;
		}
		bool IsTimeUp()
		{
			if ( bIsRealtimeMode) 
			{
				if ((FPlatformTime::Seconds() - StartTime) > TimeSlice)
				{
					return true;
				}
			}
			if ( NumPackagesSaved >= MaxNumPackagesToSave )
			{
				return true;
			}
			return false;
		}

		void SavedPackage()
		{
			++NumPackagesSaved;
		}
	};
	FCookerTimer Timer(TimeSlice, IsRealtimeMode());

	uint32 Result = 0;

	if (IsChildCooker() == false)
	{
		static const FName AssetRegistryName("AssetRegistry");
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		if (AssetRegistry.IsLoadingAssets())
		{
			// early out
			return Result;
		}
	}

	// This is all the target platforms which we needed to process requests for this iteration
	// we use this in the unsolicited packages processing below
	TArray<FName> AllTargetPlatformNames;
	
	if (CurrentCookMode == ECookMode::CookByTheBook)
	{
		if (CookRequests.HasItems() == false)
		{
			SCOPE_TIMER(WaitingForChildCookers);
			// we have nothing left to cook so we are now waiting for child cookers to finish
			if (TickChildCookers() == false)
			{
				Result |= COSR_WaitingOnChildCookers;
			}
			
		}
		else
		{
			if (TickChildCookers() == false)
			{
				Result |= COSR_WaitingOnChildCookers;
			}
		}
	}

	{
		static double SavedStatsStart = FPlatformTime::Seconds();
		double Now = FPlatformTime::Seconds();
		const double Timeout = 60 * 10; // 10 minutes
		if ((Now - SavedStatsStart) > Timeout)
		{
			SaveCookerStats();
			SavedStatsStart = Now;
		}
	}

	while (!GIsRequestingExit || CurrentCookMode == ECookMode::CookByTheBook)
	{
		// if we just cooked a map then don't process anything the rest of this tick
		if ( Result & COSR_RequiresGC )
		{
			break;
		}

		if ( IsCookByTheBookMode() )
		{
			check( CookByTheBookOptions );
			if ( CookByTheBookOptions->bCancel )
			{
				CancelCookByTheBook();
			}
		}

		FFilePlatformRequest ToBuild;

		//bool bIsUnsolicitedRequest = false;
		if ( CookRequests.HasItems() )
		{
			CookRequests.Dequeue( &ToBuild );
		}
		else
		{
			// no more to do this tick break out and do some other stuff
			break;
		}

		// prevent autosave from happening until we are finished cooking
		// causes really bad hitches
		if ( GUnrealEd )
		{
			const static float SecondsWarningTillAutosave = 10.0f;
			GUnrealEd->GetPackageAutoSaver().ForceMinimumTimeTillAutoSave(SecondsWarningTillAutosave);
		}

		if (CookedPackages.Exists(ToBuild))
		{
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Display, TEXT("Package for platform already cooked %s, discarding request"), *ToBuild.GetFilename().ToString());
#endif
			continue;
		}

#if DEBUG_COOKONTHEFLY
		UE_LOG(LogCook, Display, TEXT("Processing package %s"), *ToBuild.GetFilename().ToString());
#endif
		SCOPE_TIMER(TickCookOnTheSide);

		check( ToBuild.IsValid() );
		const TArray<FName> &TargetPlatformNames = ToBuild.GetPlatformnames();

		TArray<UPackage*> PackagesToSave;

#if OUTPUT_TIMING
		//FScopeTimer PackageManualTimer( ToBuild.GetFilename().ToString(), false );
#endif

		for ( const auto &PlatformName : TargetPlatformNames )
		{
			AllTargetPlatformNames.AddUnique(PlatformName);
		}

		for ( const auto &PlatformName : AllTargetPlatformNames )
		{
			if ( ToBuild.HasPlatform(PlatformName) == false )
			{
				ToBuild.AddPlatform(PlatformName);
			}
		}

		bool bWasUpToDate = false;

		bool bLastLoadWasMap = false;
		FString LastLoadedMapName;

		START_PACKAGE(ToBuild.GetFilename());

		const FString BuildFilename = ToBuild.GetFilename().ToString();

		if ( ToBuild.GetFilename() != CurrentReentryData.FileName )
		{
			CurrentReentryData.Reset( ToBuild.GetFilename() );
		}

		// if we have no target platforms then we want to cook because this will cook for all target platforms in that case
		bool bShouldCook = TargetPlatformNames.Num() > 0 ? false : ShouldCook( BuildFilename, NAME_None );
		{
			SCOPE_TIMER(ShouldCook);
			for ( int Index = 0; Index < TargetPlatformNames.Num(); ++Index )
			{
				bShouldCook |= ShouldCook( ToBuild.GetFilename().ToString(), TargetPlatformNames[Index] );
			}
		}
		
		if( CookByTheBookOptions && CookByTheBookOptions->bErrorOnEngineContentUse )
		{
			check(IsCookingDLC());
			FString DLCPath = GetDLCContentPath();
			if ( ToBuild.GetFilename().ToString().StartsWith(DLCPath) == false ) // if we don't start with the dlc path then we shouldn't be cooking this data 
			{
				UE_LOG(LogCook, Error, TEXT("Engine content %s is being referenced by DLC!"), *ToBuild.GetFilename().ToString() );
				bShouldCook = false;
			}
		}

		check(IsInGameThread());
		if (NeverCookPackageList.Contains(ToBuild.GetFilename()))
		{
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Display, TEXT("Package %s requested but is in the never cook package list, discarding request"), *ToBuild.GetFilename().ToString());
#endif
			bShouldCook = false;
		}


		if ( bShouldCook ) // if we should cook the package then cook it otherwise add it to the list of already cooked packages below
		{
			SCOPE_PACKAGE_STAT(AllOfLoadPackage);


			SCOPE_TIMER(AllOfLoadPackage);
			UPackage *Package = NULL;
			{
				FString PackageName;
				if ( FPackageName::TryConvertFilenameToLongPackageName(BuildFilename, PackageName) )
				{
					Package = FindObject<UPackage>( ANY_PACKAGE, *PackageName );
				}
			}

#if DEBUG_COOKONTHEFLY
			UE_LOG( LogCook, Display, TEXT("Processing request %s"), *BuildFilename);
#endif
			static TSet<FString> CookWarningsList;
			if ( CookWarningsList.Contains(BuildFilename) == false )
			{
				CookWarningsList.Add( BuildFilename );
				GOutputCookingWarnings = true;
			}
			
			//  if the package is already loaded then try to avoid reloading it :)
			if ( ( Package == NULL ) || ( Package->IsFullyLoaded() == false ) )
			{
				// moved to GenerateDependencies 
				/*if (CookByTheBookOptions && CookByTheBookOptions->bGenerateStreamingInstallManifests)
				{
					CookByTheBookOptions->ManifestGenerator->PrepareToLoadNewPackage(BuildFilename);
				}*/

				SCOPE_TIMER(LoadPackage);
				Package = LoadPackage( NULL, *BuildFilename, LOAD_None );
				INC_INT_STAT(LoadPackage, 1);
			}
#if DEBUG_COOKONTHEFLY
			else
			{
				UE_LOG(LogCook, Display, TEXT("Package already loaded %s avoiding reload"), *BuildFilename );
			}
#endif

			if( Package == NULL )
			{
				LogCookerMessage( FString::Printf(TEXT("Error loading %s!"), *BuildFilename), EMessageSeverity::Error );
				UE_LOG(LogCook, Error, TEXT("Error loading %s!"), *BuildFilename );
				Result |= COSR_ErrorLoadingPackage;
			}
			else
			{
				check(Package);


				if (Package->ContainsMap())
				{
					// child cookers can't process maps
					// check(IsChildCooker() == false);
					if ( IsCookByTheBookMode() )
					{
						// load sublevels
						UWorld* World = UWorld::FindWorldInPackage(Package);

						// TArray<FString> PreviouslyCookedPackages;
						if (World->StreamingLevels.Num())
						{
							//World->LoadSecondaryLevels(true, &PreviouslyCookedPackages);
							World->LoadSecondaryLevels(true, NULL);
						}

						TArray<FString> NewPackagesToCook;

						// Collect world composition tile packages to cook
						if (World->WorldComposition)
						{
							World->WorldComposition->CollectTilesToCook(NewPackagesToCook);
						}

						for ( auto PackageName : NewPackagesToCook )
						{
							FString Filename;
							if ( FPackageName::DoesPackageExist(PackageName, NULL, &Filename) )
							{


								FString StandardFilename = FPaths::ConvertRelativePathToFull(Filename);
								FPaths::MakeStandardFilename(StandardFilename);
								FName StandardPackageFName = FName(*StandardFilename);
								if (IsChildCooker())
								{
									check(IsCookByTheBookMode());
									// notify the main cooker that it should make sure this package gets cooked
									CookByTheBookOptions->ChildUnsolicitedPackages.Add(StandardPackageFName);
								}
								else
								{
									CookRequests.EnqueueUnique(FFilePlatformRequest(MoveTemp(StandardPackageFName), MoveTemp(TargetPlatformNames)));
								}
							}
						}

						// maps don't compile level script actors correctly unless we do FULL GC's, they may also hold weak pointer refs that need to be reset
						// NumProcessedSinceLastGC = GCInterval; 
						// ForceGarbageCollect();

						LastLoadedMapName = Package->GetName();
						bLastLoadWasMap = true;
					}

				}

				FString Name = Package->GetPathName();
				FString PackageFilename( GetCachedStandardPackageFilename( Package ) );
				if ( PackageFilename != BuildFilename )
				{
					// we have saved something which we didn't mean to load 
					//  sounds unpossible.... but it is due to searching for files and such
					//  mark the original request as processed (if this isn't actually the file they were requesting then it will fail)
					//	and then also save our new request as processed so we don't do it again
					PackagesToSave.AddUnique( Package );
#if DEBUG_COOKONTHEFLY
					UE_LOG( LogCook, Display, TEXT("Request for %s received going to save %s"), *BuildFilename, *PackageFilename );
#endif
					// CookedPackages.Add( ToBuild );
					
					// ToBuild.SetFilename( PackageFilename );
				}
				else
				{
					PackagesToSave.AddUnique( Package );
				}
			}
			GOutputCookingWarnings = false;
		}

		
		if ( PackagesToSave.Num() == 0 )
		{
			// if we are iterative cooking the package might already be cooked
			// so just add the package to the cooked packages list
			// this could also happen if the source file doesn't exist which is often as we request files with different extensions when we are searching for files
			// just return that we processed the cook request
			// the network file manager will then handle the missing file and search somewhere else
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Display, TEXT("Not cooking package %s"), *ToBuild.GetFilename().ToString());
#endif
			CookedPackages.Add( FFilePlatformRequest( ToBuild.GetFilename(), TargetPlatformNames ) );
			continue;
		}


		
		// if we are doing cook by the book we need to resolve string asset references
		// this will load more packages :)
		if ( IsCookByTheBookMode() )
		{
			SCOPE_PACKAGE_STAT(ResolveRedirectors);
			SCOPE_TIMER(ResolveRedirectors);
			GRedirectCollector.ResolveStringAssetReference();
		}


		bool bIsAllDataCached = true;

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		TArray<const ITargetPlatform*> TargetPlatforms;
		for ( const auto& TargetPlatformName : AllTargetPlatformNames )
		{
			TargetPlatforms.Add( TPM.FindTargetPlatform( TargetPlatformName.ToString() ) );
		}


		auto BeginPackageCacheForCookedPlatformData = [&]( const TArray<UObject*>& ObjectsInPackage )
		{
			if (CurrentReentryData.bBeginCacheFinished)
				return true;
			for (; CurrentReentryData.BeginCacheCount < ObjectsInPackage.Num(); ++CurrentReentryData.BeginCacheCount)
			{
				const auto& Obj = ObjectsInPackage[CurrentReentryData.BeginCacheCount];
				for ( const auto& TargetPlatform : TargetPlatforms )
				{
					Obj->BeginCacheForCookedPlatformData( TargetPlatform );
				}

				if ( Timer.IsTimeUp() )
				{
#if DEBUG_COOKONTHEFLY
					UE_LOG(LogCook, Display, TEXT("Object %s took too long to cache"), *Obj->GetFullName());
#endif
					return false;
				}
			}
			CurrentReentryData.bBeginCacheFinished = true;
			return true;
		};

		auto FinishPackageCacheForCookedPlatformData = [&]( const TArray<UObject*>& ObjectsInPackage )
		{
			for ( const auto& Obj : ObjectsInPackage )
			{
				for ( const auto& TargetPlatform : TargetPlatforms )
				{
					// These begin cache calls should be quick 
					// because they will just be checking that the data is already cached and kicking off new multithreaded requests if not
					// all sync requests should have been caught in the first begincache call above
					Obj->BeginCacheForCookedPlatformData( TargetPlatform );
					if ( Obj->IsCachedCookedPlatformDataLoaded(TargetPlatform) == false )
					{
#if DEBUG_COOKONTHEFLY
						UE_LOG(LogCook, Display, TEXT("Object %s isn't cached yet"), *Obj->GetFullName());
#endif
						return false;
					}
				}

				// if this objects data is cached then we can call FinishedCookedPLatformDataCache
				// we can only safely call this when we are finished caching this object completely.
				// this doesn't ever happen for cook in editor or cook on the fly mode
				if ( bIsAllDataCached && (CurrentCookMode == ECookMode::CookByTheBook) )
				{
					// this might be run multiple times for a single object
					Obj->WillNeverCacheCookedPlatformDataAgain();
				}
			}
			return true;
		};

		if ( PackagesToSave.Num() )
		{
			START_PACKAGE_STAT(CacheForCookedPlatformData);

			SCOPE_TIMER(CallBeginCacheForCookedPlatformData);
			// cache the resources for this package for each platform
			
			TArray<UObject*> ObjectsInPackage;
			GetObjectsWithOuter( PackagesToSave[0], ObjectsInPackage );

			bIsAllDataCached &= BeginPackageCacheForCookedPlatformData(ObjectsInPackage);
			if( bIsAllDataCached )
			{
				bIsAllDataCached &= FinishPackageCacheForCookedPlatformData(ObjectsInPackage);
			}
		}

		// if ( IsRealtimeMode() )
		{
			if ( bIsAllDataCached == false )
			{
				// reque the current task and process it next tick
				CookRequests.EnqueueUnique(ToBuild, true);
				Result |= COSR_WaitingOnCache;
				break; // break out of the package tick loop
			}
		}

		END_PACKAGE_STAT(CacheForCookedPlatformData);

		int32 FirstUnsolicitedPackage = PackagesToSave.Num();

		// generate a list of other packages which were loaded with this one
		{
			SCOPE_PACKAGE_STAT(UnsolicitedMarkup);
			SCOPE_TIMER(UnsolicitedMarkup);

			TArray<UObject *> ObjectsInOuter;
			{
				SCOPE_TIMER(GetObjectsWithOuter);
				GetObjectsWithOuter(NULL, ObjectsInOuter, false);
			}

			TArray<FName> PackageNames;
			PackageNames.Empty(ObjectsInOuter.Num());
			{
				SCOPE_TIMER(GeneratePackageNames);
				ACCUMULATE_TIMER(UnsolicitedPackageAlreadyCooked);
				ACCUMULATE_TIMER(PackageCast);
				ACCUMULATE_TIMER(AddUnassignedPackageToManifest);
				ACCUMULATE_TIMER(GetCachedName);
				for( int32 Index = 0; Index < ObjectsInOuter.Num(); Index++ )
				{
					ACCUMULATE_TIMER_START(PackageCast);
					UPackage* Package = Cast<UPackage>(ObjectsInOuter[Index]);
					ACCUMULATE_TIMER_STOP(PackageCast);

					UObject* Object = ObjectsInOuter[Index];
					if ( FullGCAssetClasses.Contains(Object->GetClass()) )
					{
						Result |= COSR_RequiresGC;
					}
					
					if (Package)
					{
						ACCUMULATE_TIMER_START(GetCachedName);
						FName StandardPackageFName = GetCachedStandardPackageFileFName(Package);
						ACCUMULATE_TIMER_STOP(GetCachedName);
						if ( StandardPackageFName == NAME_None )
							continue;

						ACCUMULATE_TIMER_START(UnsolicitedPackageAlreadyCooked);
						// package is already cooked don't care about processing it again here
						if ( CookRequests.Exists( StandardPackageFName, AllTargetPlatformNames) )
							continue;
						if ( CookedPackages.Exists(StandardPackageFName,AllTargetPlatformNames) )
							continue;
						ACCUMULATE_TIMER_STOP(UnsolicitedPackageAlreadyCooked);

						if ( StandardPackageFName != NAME_None ) // if we have name none that means we are in core packages or something...
						{
							if (IsChildCooker())
							{
								// notify the main cooker that it should make sure this package gets cooked
								CookByTheBookOptions->ChildUnsolicitedPackages.Add(StandardPackageFName);
							}
							else
							{
								// check if the package has already been saved
								PackagesToSave.AddUnique(Package);
							}
						}
					}
				}
			}
		}


		bool bFinishedSave = true;

		START_PACKAGE_STAT(SavePackageCacheForCookedPlatformData);

		if ( PackagesToSave.Num() )
		{
			int32 OriginalPackagesToSaveCount = PackagesToSave.Num();
			SCOPE_TIMER(SavingPackages);
			for ( int32 I = 0; I < PackagesToSave.Num(); ++I )
			{
				UPackage* Package = PackagesToSave[I];
				if (Package->IsLoadedByEditorPropertiesOnly() && UncookedEditorOnlyPackages.Contains(Package->GetFName()))
				{
					// We already attempted to cook this package and it's still not referenced by any non editor-only properties.
					continue;
				}

				const FName StandardPackageFilename = GetCachedStandardPackageFileFName(Package);
				check(IsInGameThread());
				if (NeverCookPackageList.Contains(StandardPackageFilename))
				{
					// refuse to save this package, it's clearly one of the undesirables
					continue;
				}

				// if we are processing unsolicited packages we can optionally not save these right now
				// the unsolicited packages which we missed now will be picked up on next run
				// we want to do this in cook on the fly also, if there is a new network package request instead of saving unsolicited packages we can process the requested package
				if ( (IsRealtimeMode() || IsCookOnTheFlyMode()) && (I >= FirstUnsolicitedPackage) )
				{
					SCOPE_TIMER(WaitingForCachedCookedPlatformData);

					bool bShouldFinishTick = false;

					if ( CookRequests.HasItems() )
					{
						bShouldFinishTick = true;
					}

					if ( Timer.IsTimeUp() )
					{
						bShouldFinishTick = true;
						// our timeslice is up
					}

					bool bFinishedCachingCookedPlatformData = false;
					// if we are in realtime mode then don't wait forever for the package to be ready
					TArray<UObject*> ObjectsInPackage;
					GetObjectsWithOuter( Package, ObjectsInPackage );
					while ( (!Timer.IsTimeUp()) && IsRealtimeMode() && (bShouldFinishTick == false) )
					{
						if ( FinishPackageCacheForCookedPlatformData(ObjectsInPackage) == true )
						{
							bFinishedCachingCookedPlatformData = true;
							break;
						}

						// sleep for a bit
						FPlatformProcess::Sleep(0.0f);
					}

					bShouldFinishTick |= !bFinishedCachingCookedPlatformData;

					if ( bShouldFinishTick )
					{
						SCOPE_TIMER(EnqueueUnsavedPackages);
						// don't save these packages because we have a real request
						// enqueue all the packages which we were about to save
						if (CurrentCookMode==ECookMode::CookByTheBookFromTheEditor)
						{
							for ( int32 RemainingIndex = I; RemainingIndex < PackagesToSave.Num(); ++RemainingIndex )
							{
								FName StandardFilename = GetCachedStandardPackageFileFName(PackagesToSave[RemainingIndex]);
								CookRequests.EnqueueUnique(FFilePlatformRequest(StandardFilename, AllTargetPlatformNames));
							}
						}

						// break out of the loop
						bFinishedSave = false;
						break;
					}
				}

				FName PackageFName = GetCachedStandardPackageFileFName(Package);
				TArray<FName> SaveTargetPlatformNames = AllTargetPlatformNames;
				TArray<FName> CookedTargetPlatforms;
				if ( CookedPackages.GetCookedPlatforms(PackageFName, CookedTargetPlatforms) )
				{
					for ( auto const &CookedPlatform : CookedTargetPlatforms )
					{
						SaveTargetPlatformNames.Remove( CookedPlatform );
					}
				}

				if ( SaveTargetPlatformNames.Num() == 0 )
				{
					// already saved this package
					continue;
				}

				// precache platform data for next package 
				UPackage *NextPackage = PackagesToSave[FMath::Min( PackagesToSave.Num()-1, I + 1 )];
				UPackage *NextNextPackage = PackagesToSave[FMath::Min( PackagesToSave.Num()-1, I + 2 )];
				if ( NextPackage != Package )
				{
					SCOPE_TIMER(PrecachePlatformDataForNextPackage);
					TArray<UObject*> ObjectsInPackage;
					GetObjectsWithOuter( NextPackage, ObjectsInPackage );
					BeginPackageCacheForCookedPlatformData(ObjectsInPackage);
				}
				if ( NextNextPackage != NextPackage )
				{
					SCOPE_TIMER(PrecachePlatformDataForNextNextPackage);
					TArray<UObject*> ObjectsInPackage;
					GetObjectsWithOuter( NextNextPackage, ObjectsInPackage );
					BeginPackageCacheForCookedPlatformData(ObjectsInPackage);
				}


				// if we are running the cook commandlet
				// if we already went through the entire package list then don't keep requeuing requests
				if (!IsCookOnTheFlyMode() && !IsRealtimeMode() && (I < OriginalPackagesToSaveCount))
				{
					// we don't want to break out of this function without saving all the packages
					// we can skip this package and do it later though

					TArray<UObject*> ObjectsInPackage;
					GetObjectsWithOuter(Package, ObjectsInPackage);
					if (FinishPackageCacheForCookedPlatformData(ObjectsInPackage) == false)
					{
						// add to back of queue
						PackagesToSave.Add(Package);
					}
				}


				bool bShouldSaveAsync = true;
				FString Temp;
				if ( FParse::Value( FCommandLine::Get(), TEXT("-diffagainstcookdirectory="), Temp ) || FParse::Value(FCommandLine::Get(), TEXT("-breakonfile="), Temp))
				{
					// async save doesn't work with this flags
					bShouldSaveAsync = false;
				}

				ESavePackageResult SavePackageResult = ESavePackageResult::Error;
				END_PACKAGE_STAT(SavePackageCacheForCookedPlatformData);
				{
					SCOPE_PACKAGE_STAT(SaveCookedPackage);

					SCOPE_TIMER(SaveCookedPackage);
					SavePackageResult = SaveCookedPackage(Package, SAVE_KeepGUID | (bShouldSaveAsync ? SAVE_Async : SAVE_None) | (IsCookFlagSet(ECookInitializationFlags::Unversioned) ? SAVE_Unversioned : 0), bWasUpToDate, AllTargetPlatformNames);
					if (SavePackageResult == ESavePackageResult::Success)
					{
						// Update flags used to determine garbage collection.
						if (Package->ContainsMap())
						{
							Result |= COSR_CookedMap;
						}
						else
						{
							++CookedPackageCount;
							Result |= COSR_CookedPackage;
						}
					}
					Timer.SavedPackage();
				}
				START_PACKAGE_STAT(SavePackageCacheForCookedPlatformData);


				if ( IsCookingInEditor() == false )
				{
					SCOPE_TIMER(ClearAllCachedCookedPlatformData);
					TArray<UObject*> ObjectsInPackage;
					GetObjectsWithOuter(Package, ObjectsInPackage);
					for ( const auto& Object : ObjectsInPackage )
					{
						Object->ClearAllCachedCookedPlatformData();
					}
				}

				//@todo ResetLoaders outside of this (ie when Package is NULL) causes problems w/ default materials
				if (Package->HasAnyFlags(RF_RootSet) == false && ((CurrentCookMode==ECookMode::CookOnTheFly)) )
				{
					SCOPE_TIMER(ResetLoaders);
					ResetLoaders(Package);
				}

				FName StandardFilename = GetCachedStandardPackageFileFName(Package);

				// We always want to mark package as processed unless it wasn't saved because it was referenced by editor-only data
				// in which case we may still need to save it later when new content loads it through non editor-only references
				if (StandardFilename != NAME_None)
				{
					// mark the package as cooked
					FFilePlatformRequest FileRequest(StandardFilename, AllTargetPlatformNames);
					if (SavePackageResult != ESavePackageResult::ReferencedOnlyByEditorOnlyData)
					{
						CookedPackages.Add(FileRequest);

						if ((CurrentCookMode == ECookMode::CookOnTheFly) && (I >= FirstUnsolicitedPackage))
						{
							// this is an unsolicited package
							if ((FPaths::FileExists(FileRequest.GetFilename().ToString()) == true) &&
								(bWasUpToDate == false))
							{
								UnsolicitedCookedPackages.AddCookedPackage( FileRequest );
#if DEBUG_COOKONTHEFLY
								UE_LOG(LogCook, Display, TEXT("UnsolicitedCookedPackages: %s"), *FileRequest.GetFilename().ToString());
#endif
							}
						}
					}
					else
					{
						UncookedEditorOnlyPackages.AddUnique(Package->GetFName());
					}
				}
			}
		}
		END_PACKAGE_STAT(SavePackageCacheForCookedPlatformData);

		// if after all this our requested file didn't get saved then we need to mark it as saved (this can happen if the file loaded didn't match the one we saved)
		// for example say we request A.uasset which doesn't exist however A.umap exists, our load call will succeed as it will find A.umap, then the save will save A.umap
		// our original package request will fail however
		if ( !CookedPackages.Exists(ToBuild) )
		{
			CookedPackages.Add( ToBuild );
		}

		// TODO: Daniel: this is reference code needs to be reimplemented on the callee side.
		//  cooker can't depend on callee being able to garbage collect
		// collect garbage
		if ( CookByTheBookOptions && CookByTheBookOptions->bLeakTest && bFinishedSave )
		{
			check( CurrentCookMode == ECookMode::CookByTheBook);
			UE_LOG(LogCook, Display, TEXT("Full GC..."));

			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
			for (FObjectIterator It; It; ++It)
			{
				if (!CookByTheBookOptions->LastGCItems.Contains(FWeakObjectPtr(*It)))
				{
					UE_LOG(LogCook, Warning, TEXT("\tLeaked %s"), *(It->GetFullName()));
					CookByTheBookOptions->LastGCItems.Add(FWeakObjectPtr(*It));
				}
			}
		}

		if ( Timer.IsTimeUp() )
		{
			break;
		}
	}
	

	OUTPUT_TIMERS();

	if ( CookByTheBookOptions )
	{
		CookByTheBookOptions->CookTime += Timer.GetTimeTillNow();
	}

	if ( IsCookByTheBookRunning() && 
		( CookRequests.HasItems() == false ) &&
		(!(Result & COSR_WaitingOnChildCookers)) )
	{
		check(IsCookByTheBookMode());
		// if we are out of stuff and we are in cook by the book from the editor mode then we finish up
		CookByTheBookFinished();
	}

	return Result;
}

void UCookOnTheFlyServer::OnObjectModified( UObject *ObjectMoving )
{
	OnObjectUpdated( ObjectMoving );
}

void UCookOnTheFlyServer::OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	OnObjectUpdated( ObjectBeingModified );
}

void UCookOnTheFlyServer::OnObjectSaved( UObject* ObjectSaved )
{
	OnObjectUpdated( ObjectSaved );
}

void UCookOnTheFlyServer::OnObjectUpdated( UObject *Object )
{
	// get the outer of the object
	UPackage *Package = Object->GetOutermost();

	MarkPackageDirtyForCooker( Package );
}

void UCookOnTheFlyServer::MarkPackageDirtyForCooker( UPackage *Package )
{
	if ( !bIsSavingPackage )
	{
		// could have just cooked a file which we might need to write
		UPackage::WaitForAsyncFileWrites();

		// force that package to be recooked
		const FString Name = Package->GetPathName();
		/*FString PackageFilename(GetPackageFilename(Package));
		FPaths::MakeStandardFilename(PackageFilename);*/

		FName PackageFFileName = GetCachedStandardPackageFileFName(Package);

		if ( PackageFFileName == NAME_None )
		{
			ClearPackageFilenameCacheForPackage( Package );
		}

#if DEBUG_COOKONTHEFLY
		UE_LOG(LogCook, Display, TEXT("Modification detected to package %s"), *PackageFilename);
#endif

		if ( CurrentCookMode == ECookMode::CookByTheBookFromTheEditor )
		{
			TArray<FName> CookedPlatforms;
			// if we have already cooked this package and we have made changes then recook ;)
			if ( CookedPackages.GetCookedPlatforms(PackageFFileName, CookedPlatforms) )
			{
				if (IsCookByTheBookRunning())
				{
					// if this package was previously cooked and we are doing a cook by the book 
					// we need to recook this package before finishing cook by the book
					CookRequests.EnqueueUnique(FFilePlatformRequest(PackageFFileName, CookedPlatforms));
				}
				else
				{
					CookByTheBookOptions->PreviousCookRequests.Add(FFilePlatformRequest(PackageFFileName, CookedPlatforms));
				}
			}
		}
		CookedPackages.RemoveFile( PackageFFileName );
	}
}


void UCookOnTheFlyServer::EndNetworkFileServer()
{
	for ( int i = 0; i < NetworkFileServers.Num(); ++i )
	{
		INetworkFileServer *NetworkFileServer = NetworkFileServers[i];
		// shutdown the server
		NetworkFileServer->Shutdown();
		delete NetworkFileServer;
		NetworkFileServer = NULL;
	}
	NetworkFileServers.Empty();
}

void UCookOnTheFlyServer::SetFullGCAssetClasses( const TArray<UClass*>& InFullGCAssetClasses)
{
	FullGCAssetClasses = InFullGCAssetClasses;
}

uint32 UCookOnTheFlyServer::GetPackagesPerGC() const
{
	return PackagesPerGC;
}

double UCookOnTheFlyServer::GetIdleTimeToGC() const
{
	return IdleTimeToGC;
}

uint64 UCookOnTheFlyServer::GetMaxMemoryAllowance() const
{
	return MaxMemoryAllowance;
}

void UCookOnTheFlyServer::BeginDestroy()
{
	EndNetworkFileServer();

	Super::BeginDestroy();

}

void UCookOnTheFlyServer::TickRecompileShaderRequests()
{
	// try to pull off a request
	FRecompileRequest* Request = NULL;

	RecompileRequests.Dequeue(&Request);

	// process it
	if (Request)
	{
		HandleNetworkFileServerRecompileShaders(Request->RecompileData);


		// all done! other thread can unblock now
		Request->bComplete = true;
	}
}



bool UCookOnTheFlyServer::GetPackageTimestamp( const FString& InFilename, FDateTime& OutDateTime )
{
	FPackageDependencyInfoModule& PDInfoModule = FModuleManager::LoadModuleChecked<FPackageDependencyInfoModule>("PackageDependencyInfo");
	FDateTime DependentTime;

	if (PDInfoModule.DeterminePackageDependentTimeStamp(*InFilename, DependentTime) == true)
	{
		OutDateTime = DependentTime;

		return true;
	}

	return false;
}


ESavePackageResult UCookOnTheFlyServer::SaveCookedPackage(UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate)
{
	TArray<FName> TargetPlatformNames; 
	return SaveCookedPackage( Package, SaveFlags, bOutWasUpToDate, TargetPlatformNames );
}

bool UCookOnTheFlyServer::ShouldCook(const FString& InFileName, const FName &InPlatformName)
{
	// this only does anything if we are using iterative cooking
	return true;
	/*
	bool bDoCook = false;

	FString PkgFile;
	FString PkgFilename;
	FDateTime DependentTimeStamp = FDateTime::MinValue();

	if (IsCookFlagSet(ECookInitializationFlags::Iterative) && FPackageName::DoesPackageExist(InFileName, NULL, &PkgFile))
	{
		PkgFilename = PkgFile;

		if (GetPackageTimestamp(FPaths::GetBaseFilename(PkgFilename, false), DependentTimeStamp) == false)
		{
			UE_LOG(LogCook, Display, TEXT("Failed to find dependency timestamp for: %s"), *PkgFilename);
		}
	}

	// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	PkgFilename = ConvertToFullSandboxPath(*PkgFilename, true);

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

	static const TArray<ITargetPlatform*> &ActiveTargetPlatforms = TPM.GetCookingTargetPlatforms();

	TArray<ITargetPlatform*> Platforms;

	if ( InPlatformName != NAME_None )
	{
		Platforms.Add( TPM.FindTargetPlatform( InPlatformName.ToString() ) );
	}
	else 
	{
		Platforms = ActiveTargetPlatforms;
	}

	for (int32 Index = 0; Index < Platforms.Num() && !bDoCook; Index++)
	{
		ITargetPlatform* Target = Platforms[Index];
		FString PlatFilename = PkgFilename.Replace(TEXT("[Platform]"), *Target->PlatformName());

		// If we are not iterative cooking, then cook the package
		bool bCookPackage = (IsCookFlagSet(ECookInitializationFlags::Iterative) == false);

		if (bCookPackage == false)
		{
			// If the cooked package doesn't exist, or if the cooked is older than the dependent, re-cook it
			FDateTime CookedTimeStamp = IFileManager::Get().GetTimeStamp(*PlatFilename);
			int32 CookedTimespanSeconds = (CookedTimeStamp - DependentTimeStamp).GetTotalSeconds();
			bCookPackage = (CookedTimeStamp == FDateTime::MinValue()) || (CookedTimespanSeconds < 0);
		}
		bDoCook |= bCookPackage;
	}

	return bDoCook;*/
}

bool UCookOnTheFlyServer::ShouldConsiderCompressedPackageFileLengthRequirements() const
{
	bool bConsiderCompressedPackageFileLengthRequirements = true;
	GConfig->GetBool(TEXT("CookSettings"), TEXT("bConsiderCompressedPackageFileLengthRequirements"), bConsiderCompressedPackageFileLengthRequirements, GEditorIni);
	return bConsiderCompressedPackageFileLengthRequirements;
}

ESavePackageResult UCookOnTheFlyServer::SaveCookedPackage(UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate, TArray<FName> &TargetPlatformNames)
{
	ESavePackageResult Result = ESavePackageResult::Success;
	check( bIsSavingPackage == false );
	bIsSavingPackage = true;
	FString Filename(GetCachedPackageFilename(Package));

	if (Filename.Len())
	{
		if (Package->PackageFlags & PKG_ReloadingForCooker)
		{
			UE_LOG(LogCook, Warning, TEXT("Package %s marked as reloading for cook by was requested to save"), *Package->GetPathName());
			UE_LOG(LogCook, Fatal, TEXT("Package %s marked as reloading for cook by was requested to save"), *Package->GetPathName());
		}

		FString Name = Package->GetPathName();

		// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
		Filename = ConvertToFullSandboxPath(*Filename, true);

		uint32 OriginalPackageFlags = Package->PackageFlags;
		UWorld* World = NULL;
		EObjectFlags Flags = RF_NoFlags;
		bool bPackageFullyLoaded = false;

		bool bShouldCompressPackage = IsCookFlagSet(ECookInitializationFlags::Compressed);

		if (CookByTheBookOptions)
		{
			bShouldCompressPackage &= (CookByTheBookOptions->bForceDisableCompressedPackages == false);
			bShouldCompressPackage |= CookByTheBookOptions->bForceEnableCompressedPackages;
		}

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

		static TArray<ITargetPlatform*> ActiveStartupPlatforms = TPM.GetCookingTargetPlatforms();

		TArray<ITargetPlatform*> Platforms;

		if ( TargetPlatformNames.Num() )
		{
			const TArray<ITargetPlatform*>& TargetPlatforms = TPM.GetTargetPlatforms();

			for (const FName TargetPlatformFName : TargetPlatformNames)
			{
				const FString TargetPlatformName = TargetPlatformFName.ToString();

				for (ITargetPlatform *TargetPlatform  : TargetPlatforms)
				{
					if ( TargetPlatform->PlatformName() == TargetPlatformName )
					{
						Platforms.Add( TargetPlatform );
					}
				}
			}
		}
		else
		{
			Platforms = ActiveStartupPlatforms;

			for (ITargetPlatform *Platform : Platforms)
			{
				TargetPlatformNames.Add(FName(*Platform->PlatformName()));
			}
		}
		
		for (const auto& TargetPlatform : Platforms)
		{
			bShouldCompressPackage &= TargetPlatform->SupportsFeature(ETargetPlatformFeatures::ShouldUseCompressedCookedPackages);
		}

		if (bShouldCompressPackage)
		{
			Package->PackageFlags |= PKG_StoreCompressed;
		}

		for (ITargetPlatform* Target : Platforms)
		{
			FString PlatFilename = Filename.Replace(TEXT("[Platform]"), *Target->PlatformName());

			bool bCookPackage = true;


			// this code has been moved to the clean sandbox so that it's only processed in one location on startup
			// will remove this code in future release
			// If we are not iterative cooking, then cook the package
			/*bool bCookPackage = (IsCookFlagSet(ECookInitializationFlags::Iterative) == false);

			if (bCookPackage == false)
			{
				// If the cooked package doesn't exist, or if the cooked is older than the dependent, re-cook it
				FDateTime CookedTimeStamp = IFileManager::Get().GetTimeStamp(*PlatFilename);
				int32 CookedTimespanSeconds = (CookedTimeStamp - DependentTimeStamp).GetTotalSeconds();
				bCookPackage = (CookedTimeStamp == FDateTime::MinValue()) || (CookedTimespanSeconds < 0);
			}*/

			// don't save Editor resources from the Engine if the target doesn't have editoronly data
			if (IsCookFlagSet(ECookInitializationFlags::SkipEditorContent) && Name.StartsWith(TEXT("/Engine/Editor")) && !Target->HasEditorOnlyData())
			{
				bCookPackage = false;
			}

			if (bCookPackage == true)
			{
				if (bPackageFullyLoaded == false)
				{
					SCOPE_TIMER(LoadPackage);

					Package->FullyLoad();
					if (!Package->IsFullyLoaded())
					{
						LogCookerMessage( FString::Printf(TEXT("Package %s supposed to be fully loaded but isn't. RF_WasLoaded is %s"), 
							*Package->GetName(), Package->HasAnyFlags(RF_WasLoaded) ? TEXT("set") : TEXT("not set")), EMessageSeverity::Warning);

						UE_LOG(LogCook, Warning, TEXT("Package %s supposed to be fully loaded but isn't. RF_WasLoaded is %s"), 
							*Package->GetName(), Package->HasAnyFlags(RF_WasLoaded) ? TEXT("set") : TEXT("not set"));
					}
					bPackageFullyLoaded = true;

					// If fully loading has caused a blueprint to be regenerated, make sure we eliminate all meta data outside the package
					UMetaData* MetaData = Package->GetMetaData();
					MetaData->RemoveMetaDataOutsidePackage();

					// look for a world object in the package (if there is one, there's a map)
					World = UWorld::FindWorldInPackage(Package);
					Flags = World ? RF_NoFlags : RF_Standalone;
				}

				UE_LOG(LogCook, Display, TEXT("Cooking %s -> %s"), *Package->GetName(), *PlatFilename);

				bool bSwap = (!Target->IsLittleEndian()) ^ (!PLATFORM_LITTLE_ENDIAN);


				if (!Target->HasEditorOnlyData())
				{
					Package->PackageFlags |= PKG_FilterEditorOnly;
				}
				else
				{
					Package->PackageFlags &= ~PKG_FilterEditorOnly;
				}

				// need to subtract 32 because the SavePackage code creates temporary files with longer file names then the one we provide
				// projects may ignore this restriction if desired
				static bool bConsiderCompressedPackageFileLengthRequirements = ShouldConsiderCompressedPackageFileLengthRequirements();
				const int32 CompressedPackageFileLengthRequirement = bConsiderCompressedPackageFileLengthRequirements ? 32 : 0;
				const FString FullFilename = FPaths::ConvertRelativePathToFull( PlatFilename );
				if( FullFilename.Len() >= (PLATFORM_MAX_FILEPATH_LENGTH-CompressedPackageFileLengthRequirement) )
				{
					LogCookerMessage( FString::Printf(TEXT("Couldn't save package, filename is too long: %s"), *PlatFilename), EMessageSeverity::Error );
					UE_LOG( LogCook, Error, TEXT( "Couldn't save package, filename is too long :%s" ), *PlatFilename );
					Result = ESavePackageResult::Error;
				}
				else
				{
					SCOPE_TIMER(GEditorSavePackage);
					Result = GEditor->Save(Package, World, Flags, *PlatFilename, GError, NULL, bSwap, false, SaveFlags, Target, FDateTime::MinValue(), false);
					INC_INT_STAT(SavedPackage, 1);
				}

				bOutWasUpToDate = false;
			}
			else
			{
#if DEBUG_COOKONTHEFLY
				UE_LOG(LogCook, Display, TEXT("Up to date: %s"), *PlatFilename);
#endif

				bOutWasUpToDate = true;
			}
		}

		Package->PackageFlags = OriginalPackageFlags;
	}

	check( bIsSavingPackage == true );
	bIsSavingPackage = false;

	// return success
	return Result;
}

// this is a duplciate of the 
static bool IsMobileHDR()
{
	static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	return MobileHDRCvar->GetValueOnAnyThread() == 1;
}

void UCookOnTheFlyServer::Initialize( ECookMode::Type DesiredCookMode, ECookInitializationFlags InCookFlags, const FString &InOutputDirectoryOverride )
{
	SCOPE_COOKING_STAT(Initialize);

	OutputDirectoryOverride = InOutputDirectoryOverride;
	CurrentCookMode = DesiredCookMode;
	CookFlags = InCookFlags;

	if (IsCookByTheBookMode() && !IsCookingInEditor())
	{
		FCoreUObjectDelegates::PackageCreatedForLoad.AddUObject(this, &UCookOnTheFlyServer::MaybeMarkPackageAsAlreadyLoaded);
	}

	if (IsCookingInEditor())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UCookOnTheFlyServer::OnObjectPropertyChanged);
		FCoreUObjectDelegates::OnObjectModified.AddUObject(this, &UCookOnTheFlyServer::OnObjectModified);
		FCoreUObjectDelegates::OnObjectSaved.AddUObject(this, &UCookOnTheFlyServer::OnObjectSaved);
	}

	bool bUseFullGCAssetClassNames = true;
	GConfig->GetBool(TEXT("CookSettings"), TEXT("bUseFullGCAssetClassNames"), bUseFullGCAssetClassNames, GEditorIni);
	
	if (bUseFullGCAssetClassNames)
	{
		TArray<FString> FullGCAssetClassNames;
		GConfig->GetArray( TEXT("CookSettings"), TEXT("FullGCAssetClassNames"), FullGCAssetClassNames, GEditorIni );
		for ( const auto& FullGCAssetClassName : FullGCAssetClassNames )
		{
			UClass* FullGCAssetClass = FindObject<UClass>(ANY_PACKAGE, *FullGCAssetClassName, true);
			if( FullGCAssetClass == NULL )
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to find full gc asset class name %s may result in bad cook"), *FullGCAssetClassName);
			}
			else
			{
				FullGCAssetClasses.Add( FullGCAssetClass );
			}
		}
		if ( FullGCAssetClasses.Num() == 0 )
		{
			// default to UWorld
			FullGCAssetClasses.Add( UWorld::StaticClass() );
		}
	}

	PackagesPerGC = 50;
	int32 ConfigPackagesPerGC = 0;
	if (GConfig->GetInt( TEXT("CookSettings"), TEXT("PackagesPerGC"), ConfigPackagesPerGC, GEditorIni ))
	{
		// Going unsigned. Make negative values 0
		PackagesPerGC = ConfigPackagesPerGC > 0 ? ConfigPackagesPerGC : 0;
	}

	IdleTimeToGC = 20.0;
	GConfig->GetDouble( TEXT("CookSettings"), TEXT("IdleTimeToGC"), IdleTimeToGC, GEditorIni );

	int32 MaxMemoryAllowanceInMB = 8 * 1024;
	GConfig->GetInt( TEXT("CookSettings"), TEXT("MaxMemoryAllowance"), MaxMemoryAllowanceInMB, GEditorIni );
	MaxMemoryAllowance = MaxMemoryAllowanceInMB * 1024LL * 1024LL;
	
	UE_LOG(LogCook, Display, TEXT("Max memory allowance for cook %dmb, MaxMemoryAllowanceInMB"));

	if ( IsCookByTheBookMode() )
	{
		CookByTheBookOptions = new FCookByTheBookOptions();
	}

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	const TArray<ITargetPlatform*>& Platforms = TPM.GetCookingTargetPlatforms();
	UE_LOG(LogCook, Display, TEXT("Mobile HDR setting %d"), IsMobileHDR());

	// always generate the asset registry before starting to cook, for either method
	GenerateAssetRegistry(Platforms);
}

uint32 UCookOnTheFlyServer::NumConnections() const
{
	int Result= 0;
	for ( int i = 0; i < NetworkFileServers.Num(); ++i )
	{
		INetworkFileServer *NetworkFileServer = NetworkFileServers[i];
		if ( NetworkFileServer )
		{
			Result += NetworkFileServer->NumConnections();
		}
	}
	return Result;
}

FString UCookOnTheFlyServer::GetOutputDirectoryOverride() const
{
	FString OutputDirectory = OutputDirectoryOverride;
	// Output directory override.	
	if (OutputDirectory.Len() <= 0)
	{
		if ( IsCookingDLC() )
		{
			check( IsCookByTheBookMode() );
			OutputDirectory = FPaths::Combine(*FPaths::GamePluginsDir(), *CookByTheBookOptions->DlcName, TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]"));
		}
		else
		{
			// Full path so that the sandbox wrapper doesn't try to re-base it under Sandboxes
			OutputDirectory = FPaths::Combine(*FPaths::GameDir(), TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]"));
		}
		
		OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
	}
	else if (!OutputDirectory.Contains(TEXT("[Platform]"), ESearchCase::IgnoreCase, ESearchDir::FromEnd) )
	{
		// Output directory needs to contain [Platform] token to be able to cook for multiple targets.
		OutputDirectory = FPaths::Combine(*OutputDirectory, TEXT("[Platform]"));
	}
	FPaths::NormalizeDirectoryName(OutputDirectory);

	return OutputDirectory;
}

template<class T>
void GetVersionFormatNumbersForIniVersionStrings( TArray<FString>& IniVersionStrings, const FString& FormatName, const TArray<T> &FormatArray )
{
	for ( const auto& Format : FormatArray )
	{
		TArray<FName> SupportedFormats;
		Format->GetSupportedFormats(SupportedFormats);
		for ( const auto& SupportedFormat : SupportedFormats )
		{
			int32 VersionNumber = Format->GetVersion(SupportedFormat);
			FString IniVersionString = FString::Printf( TEXT("%s:%s:VersionNumber%d"), *FormatName, *SupportedFormat.ToString(), VersionNumber);
			IniVersionStrings.Emplace( IniVersionString );
		}
	}
}

#define INVALIDATE_ON_ANY_INI_CHANGE 1

bool UCookOnTheFlyServer::GetCurrentIniVersionStrings( const ITargetPlatform* TargetPlatform, TArray<FString>& IniVersionStrings ) const
{
	SCOPE_COOKING_STAT(GetCurrentIniVersionStrings);
	// there is a list of important ini settings in the Editor config 
#if INVALIDATE_ON_ANY_INI_CHANGE // use a full list of ini settings to invalidate the cooked content or use the entire engine / editor ini
	TArray<FString> IniFiles;
	IniFiles.Add(FString(TEXT("Engine")));
	IniFiles.Add(FString(TEXT("Editor")));
	IniFiles.Add(FString(TEXT("EditorKeyBindings")));
	IniFiles.Add(FString(TEXT("EditorLayout")));
	IniFiles.Add(FString(TEXT("EditorSettings")));
	IniFiles.Add(FString(TEXT("EditorPerProject")));
	IniFiles.Add(FString(TEXT("Compat")));
	IniFiles.Add(FString(TEXT("Lightmass")));
	IniFiles.Add(FString(TEXT("Scalability")));
	IniFiles.Add(FString(TEXT("Input")));
	IniFiles.Add(FString(TEXT("GameUserSettings")));
	IniFiles.Add(FString(TEXT("Game")));


	for (const auto& IniFilename : IniFiles)
	{
		FConfigFile PlatformIniFile;
		FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, *IniFilename, true, *TargetPlatform->IniPlatformName());

		for ( const auto& IniSection : PlatformIniFile)
		{
			const FString SectionName = FString::Printf(TEXT("%s:%s"), *IniFilename, *IniSection.Key);

			for (const auto& IniValue : IniSection.Value)
			{
				IniVersionStrings.Add(FString::Printf(TEXT("%s:%s:%s"), *SectionName, *IniValue.Key.ToString(), *IniValue.Value));
			}
		}
	}
#else // invalidate only on some ini changes which we know cause problems (less full recooks but could be unstable)
	TArray<FString> IniVersionedParams;
	GConfig->GetArray(TEXT("CookSettings"), TEXT("VersionedIniParams"), IniVersionedParams, GEditorIni);


	// used to store temporary platform specific ini files
	TMap<FString, FConfigFile*> PlatformIniFiles;

	// if the old one doesn't contain all the settings in the new one then we fail this check
	for (const auto& IniVersioned : IniVersionedParams)
	{

		TArray<FString> IniVersionedArray;
		IniVersioned.ParseIntoArray(IniVersionedArray, TEXT(":"), false);

		if (IniVersionedArray.Num() != 3)
		{
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Warning, TEXT("Invalid entry in CookSettings, VersionedIniParams %s"), *IniVersioned);
#endif
			return false;
		}

		const FString& Filename = IniVersionedArray[0];
		const FString& Section = IniVersionedArray[1];
		const FString& Key = IniVersionedArray[2];

		// const FString& IniFilename = FPaths::GeneratedConfigDir() / TargetPlatform->IniPlatformName() / Filename + TEXT(".ini");
		FConfigFile *PlatformIniFile = PlatformIniFiles.FindRef(Filename);
		if (PlatformIniFile == NULL)
		{
			PlatformIniFile = new FConfigFile();
			FConfigCacheIni::LoadLocalIniFile(*PlatformIniFile, *Filename, true, *TargetPlatform->IniPlatformName());
			PlatformIniFiles.Add(Filename, PlatformIniFile);
		}

		// get the value of the entry
		FString Value;
		if (!PlatformIniFile->GetString(*Section, *Key, Value))
		{
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Warning, TEXT("Unable to find entry in CookSettings, VersionedIniParams %s, assume default is being used"), *IniVersioned);
#endif
			continue;
		}

		FString CurrentVersionString = FString::Printf(TEXT("%s:%s:%s:%s"), *Filename, *Section, *Key, *Value);

		IniVersionStrings.Emplace(MoveTemp(CurrentVersionString));
	}

	// clean up our temporary platform ini files
	for (const auto& PlatformIniFile : PlatformIniFiles)
	{
		delete PlatformIniFile.Value;
	}
	PlatformIniFiles.Empty();
#endif
	TArray<FString> VersionedRValues;
	GConfig->GetArray(TEXT("CookSettings"), TEXT("VersionedIntRValues"), VersionedRValues, GEditorIni);

	for (const auto& RValue : VersionedRValues)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(*RValue);
		if (CVar)
		{
			FString VersionedRValueString = FString::Printf(TEXT("%s:%d"), *RValue, CVar->GetValueOnGameThread());
			IniVersionStrings.Emplace(MoveTemp(VersionedRValueString));
		}
	}


	const UTextureLODSettings& LodSettings = TargetPlatform->GetTextureLODSettings();

	UEnum* TextureGroupEnum = FindObject<UEnum>( NULL, TEXT("Engine.TextureGroup") );
	UEnum* TextureMipGenSettingsEnum = FindObject<UEnum>( NULL, TEXT("Engine.TextureMipGenSettings") );

	for ( int I = 0; I < TextureGroup::TEXTUREGROUP_MAX; ++I )
	{
		const TextureMipGenSettings& MipGenSettings = LodSettings.GetTextureMipGenSettings((TextureGroup)(I));
		FString MipGenVersionString = FString::Printf( TEXT("TextureLODGroupMipGenSettings:%s:%s"), *TextureGroupEnum->GetEnumName( I ), *TextureMipGenSettingsEnum->GetEnumName((int32)(MipGenSettings)) );
		IniVersionStrings.Emplace( MoveTemp( MipGenVersionString ) );

		const int32 MinMipCount = LodSettings.GetMinLODMipCount((TextureGroup)(I));
		FString MinMipVersionString = FString::Printf( TEXT("TextureLODGroupMinMipCount:%s:%d"), *TextureGroupEnum->GetEnumName( I ), MinMipCount);
		IniVersionStrings.Emplace( MoveTemp( MinMipVersionString ) );

		const int32 MaxMipCount = LodSettings.GetMaxLODMipCount((TextureGroup)(I));
		FString MaxMipVersionString = FString::Printf( TEXT("TextureLODGroupMaxMipCount:%s:%d"), *TextureGroupEnum->GetEnumName( I ), MaxMipCount);
		IniVersionStrings.Emplace( MoveTemp( MaxMipVersionString ) );
	}

	// save off the ddc version numbers also
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	check(TPM);

	GetVersionFormatNumbersForIniVersionStrings( IniVersionStrings, TEXT("AudioFormat"), TPM->GetAudioFormats() );
	GetVersionFormatNumbersForIniVersionStrings( IniVersionStrings, TEXT("TextureFormat"), TPM->GetTextureFormats() );
	GetVersionFormatNumbersForIniVersionStrings( IniVersionStrings, TEXT("ShaderFormat"), TPM->GetShaderFormats() );


	static const FCustomVersionContainer& CustomVersionContainer = FCustomVersionContainer::GetRegistered();
	for (const auto& CustomVersion : CustomVersionContainer.GetAllVersions())
	{
		FString CustomVersionString = FString::Printf(TEXT("%s:%s:%d"), *CustomVersion.GetFriendlyName().ToString(), *CustomVersion.Key.ToString(), CustomVersion.Version);
		IniVersionStrings.Emplace(MoveTemp(CustomVersionString));
	}

	FString UE4Ver = FString::Printf(TEXT("PackageFileVersions:%d:%d"), GPackageFileUE4Version, GPackageFileLicenseeUE4Version);
	IniVersionStrings.Emplace(MoveTemp(UE4Ver));
	FString UE4NetVer = FString::Printf(TEXT("NetFileVersions:%d:%d"), GEngineNetVersion, GEngineNegotiationVersion);
	IniVersionStrings.Emplace(MoveTemp(UE4NetVer));

	FString MaterialShaderMapDDCVersion = FString::Printf(TEXT("MaterialShaderMapDDCVersion:%s"), *GetMaterialShaderMapDDCKey());
	IniVersionStrings.Emplace( MoveTemp(MaterialShaderMapDDCVersion) );
	FString GlobalDDCVersion = FString::Printf(TEXT("GlobalDDCVersion:%s"), *GetGlobalShaderMapDDCKey());
	IniVersionStrings.Emplace( MoveTemp(GlobalDDCVersion) );

	return true;
}

bool UCookOnTheFlyServer::GetCookedIniVersionStrings( const ITargetPlatform* TargetPlatform, TArray<FString>& IniVersionStrings ) const
{
	SCOPE_COOKING_STAT(GetCookedIniVersionStrings);
	const FString EditorIni = FPaths::GameDir() / TEXT("CookedIniVersion.txt");
	const FString SandboxEditorIni = ConvertToFullSandboxPath(*EditorIni, true);


	const FString PlatformSandboxEditorIni = SandboxEditorIni.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());

	TArray<FString> SavedIniVersionedParams;
	return GConfig->GetArray( TEXT("CookSettings"), TEXT("VersionedIniParams"), IniVersionStrings, PlatformSandboxEditorIni ) > 0;
}

bool UCookOnTheFlyServer::CacheIniVersionStringsMap( const ITargetPlatform* TargetPlatform ) const
{
	SCOPE_COOKING_STAT(CacheIniVersionStringsMap);

	// check if the cached ones are filled out
	const FName TargetPlatformName = FName(*TargetPlatform->PlatformName());
	TArray<FString>* FoundCookedIniVersionStrings = CachedIniVersionStringsMap.Find( TargetPlatformName );

	if ( FoundCookedIniVersionStrings == NULL )
	{
		TArray<FString> CookedIniVersionStrings;
		if ( GetCookedIniVersionStrings( TargetPlatform, CookedIniVersionStrings ) )
		{
			FoundCookedIniVersionStrings = &CachedIniVersionStringsMap.Emplace( TargetPlatformName, MoveTemp(CookedIniVersionStrings) );
		}
	}
	return true;
}

bool UCookOnTheFlyServer::IniSettingsOutOfDate( const ITargetPlatform* TargetPlatform ) const
{
	SCOPE_COOKING_STAT(IniSettingsOutOfDate);

	TArray<FString> CurrentIniVersionStrings;
	if ( GetCurrentIniVersionStrings(TargetPlatform, CurrentIniVersionStrings) == false )
	{
		// previous cook seems half baked... bomb out and recook all the things
		return true;
	}

	// check if the cached ones are filled out
	const FName TargetPlatformName = FName(*TargetPlatform->PlatformName());
	TArray<FString>* FoundCookedIniVersionStrings = CachedIniVersionStringsMap.Find( TargetPlatformName );

	if ( FoundCookedIniVersionStrings == NULL )
	{
		TArray<FString> CookedIniVersionStrings;
		GetCookedIniVersionStrings( TargetPlatform, CookedIniVersionStrings );
		FoundCookedIniVersionStrings = &CachedIniVersionStringsMap.Emplace( TargetPlatformName, MoveTemp(CookedIniVersionStrings) );
	}

	check( FoundCookedIniVersionStrings );

	TSet<FString> CookedIniVersionStrings;
	for (const auto& CookedString : *FoundCookedIniVersionStrings)
	{
		CookedIniVersionStrings.Add(CookedString);
	}

	bool bCurrentIniSettingsChanged = false;
	for ( const auto& CurrentVersionString : CurrentIniVersionStrings ) 
	{
		if ( CookedIniVersionStrings.Contains(CurrentVersionString) == false )
		{
			bCurrentIniSettingsChanged = true;
			break;
		}
	}

	if ( bCurrentIniSettingsChanged )
	{
		*FoundCookedIniVersionStrings = CurrentIniVersionStrings;
	}

	return bCurrentIniSettingsChanged;
}

bool UCookOnTheFlyServer::SaveCurrentIniSettings( const ITargetPlatform* TargetPlatform ) const
{
	check(IsChildCooker()==false);
	const FString CookedIni = FPaths::GameDir() / TEXT("CookedIniVersion.txt");
	const FString SandboxCookedIni = ConvertToFullSandboxPath(CookedIni, true);


	const FName TargetPlatformName = FName(*TargetPlatform->PlatformName());
	TArray<FString>* FoundCookedIniVersionStrings = CachedIniVersionStringsMap.Find( TargetPlatformName );
		
	// need to call IniSettingsOutOfDate before calling this function
	check( FoundCookedIniVersionStrings );

	// save the iniversion strings
	const FString PlatformSandboxCookedIni = SandboxCookedIni.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());
	GConfig->SetArray(TEXT("CookSettings"), TEXT("VersionedIniParams"), *FoundCookedIniVersionStrings, PlatformSandboxCookedIni);
	GConfig->Flush(false, PlatformSandboxCookedIni);

	return true;
}

void UCookOnTheFlyServer::PopulateCookedPackagesFromDisk( const TArray<ITargetPlatform*>& Platforms )
{
	check(IsChildCooker() == false);

	// check each package to see if it's out of date
	FPackageDependencyInfoModule& PDInfoModule = FModuleManager::LoadModuleChecked<FPackageDependencyInfoModule>("PackageDependencyInfo");

	// list of directories to skip
	TArray<FString> DirectoriesToSkip;
	TArray<FString> DirectoriesToNotRecurse;

	// See what files are out of date in the sandbox folder
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		ITargetPlatform* Target = Platforms[Index];
		FString SandboxPath = GetSandboxDirectory(Target->PlatformName());
		FName PlatformFName( *Target->PlatformName() );

		FString EngineSandboxPath= SandboxFile->ConvertToSandboxPath(*FPaths::EngineDir()) + TEXT("/");
		EngineSandboxPath.ReplaceInline(TEXT("[Platform]"), *Target->PlatformName());

		FString GameSandboxPath = SandboxFile->ConvertToSandboxPath(*(FPaths::GameDir() + TEXT("a.txt")));
		GameSandboxPath.ReplaceInline(TEXT("a.txt"), TEXT(""));
		GameSandboxPath.ReplaceInline(TEXT("[Platform]"), *Target->PlatformName());

		FString LocalGamePath = FPaths::GameDir();
		if (FPaths::IsProjectFilePathSet())
		{
			LocalGamePath = FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/");
		}

		FString LocalEnginePath = FPaths::EngineDir();

		// use the timestamp grabbing visitor
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FLocalTimestampDirectoryVisitor Visitor(PlatformFile, DirectoriesToSkip, DirectoriesToNotRecurse, false);

		PlatformFile.IterateDirectory(*SandboxPath, Visitor);

		/*FString SandboxEngine = Sandbox->ConvertToSandboxPath(*LocalEngineDir) + TEXT("/");
		// we need to add an extra bit to the game path to make the sandbox convert it correctly (investigate?)
		// @todo: double check this
		FString SandboxGame = Sandbox->ConvertToSandboxPath(*(LocalGameDir + TEXT("a.txt"))).Replace(TEXT("a.txt"), TEXT(""));*/
		

		for (TMap<FString, FDateTime>::TIterator TimestampIt(Visitor.FileTimes); TimestampIt; ++TimestampIt)
		{
			FString CookedFilename = TimestampIt.Key();
			if ((CookedFilename.EndsWith(TEXT(".uasset")) == false) &&
				(CookedFilename.EndsWith(TEXT(".umap")) == false))
			{
				// don't care if it's not a map or an asset
				continue;
			}
			FDateTime CookedTimestamp = TimestampIt.Value();
			CookedFilename = FPaths::ConvertRelativePathToFull(CookedFilename);
			FString StandardCookedFilename = CookedFilename;

			StandardCookedFilename.ReplaceInline(*SandboxPath, *(FPaths::GetRelativePathToRoot()));

			StandardCookedFilename = FPaths::ConvertRelativePathToFull(StandardCookedFilename);
			FPaths::MakeStandardFilename(StandardCookedFilename);
			FDateTime DependentTimestamp;
			if (PDInfoModule.DeterminePackageDependentTimeStamp(*(FPaths::GetBaseFilename(StandardCookedFilename, false)), DependentTimestamp) == true)
			{
				double Diff = (CookedTimestamp - DependentTimestamp).GetTotalSeconds();

				if (Diff >= 0.0)
				{
					CookedPackages.Add(FFilePlatformRequest( FName(*StandardCookedFilename), PlatformFName) );
					UE_LOG(LogCook, Display, TEXT("Package %s for %s up to date, avoiding cook"), *StandardCookedFilename, *PlatformFName.ToString());
				}
			}
		}
	}
}

void UCookOnTheFlyServer::CleanSandbox( const bool bIterative )
{
	//child cookers shouldn't clean sandbox we would be deleting the master cookers / other cookers data
	check(IsChildCooker() == false);

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	const TArray<ITargetPlatform*>& Platforms = TPM.GetCookingTargetPlatforms();	

	// before we can delete any cooked files we need to make sure that we have finished writing them
	UPackage::WaitForAsyncFileWrites();

#if OUTPUT_TIMING
	double SandboxCleanTime = 0.0;
#endif
	{
#if OUTPUT_TIMING
		SCOPE_SECONDS_COUNTER(SandboxCleanTime);
#endif
		
		for ( const auto& Platform : Platforms )
		{
			CacheIniVersionStringsMap(Platform);
		}

		if (bIterative == false)
		{
			// for now we are going to wipe the cooked directory
			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				ITargetPlatform* Target = Platforms[Index];
				
				FString SandboxDirectory = GetSandboxDirectory( Target->PlatformName() ); // GetOutputDirectory(Target->PlatformName());
				IFileManager::Get().DeleteDirectory(*SandboxDirectory, false, true);

				ClearPlatformCookedData( FName( *Target->PlatformName() ) );

				IniSettingsOutOfDate( Target );
				SaveCurrentIniSettings( Target );
			}

		}
		else
		{
			for ( const auto& Target : Platforms )
			{
				if ( IniSettingsOutOfDate(Target) )
				{
					ClearPlatformCookedData( FName( *Target->PlatformName() ) );

					FString SandboxDirectory = GetSandboxDirectory(Target->PlatformName());
					IFileManager::Get().DeleteDirectory(*SandboxDirectory, false, true);


					SaveCurrentIniSettings(Target);
				}
			}

			// check each package to see if it's out of date
			FPackageDependencyInfoModule& PDInfoModule = FModuleManager::LoadModuleChecked<FPackageDependencyInfoModule>("PackageDependencyInfo");
			
			// list of directories to skip
			TArray<FString> DirectoriesToSkip;
			TArray<FString> DirectoriesToNotRecurse;


			PackagesKeptFromPreviousCook.Empty();

			// See what files are out of date in the sandbox folder
			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				ITargetPlatform* Target = Platforms[Index];
				
				FString EngineSandboxPath = SandboxFile->ConvertToSandboxPath(*FPaths::EngineDir()) + TEXT("/");
				EngineSandboxPath.ReplaceInline(TEXT("[Platform]"), *Target->PlatformName());

				FString GameSandboxPath = SandboxFile->ConvertToSandboxPath(*(FPaths::GameDir() + TEXT("a.txt")));
				GameSandboxPath.ReplaceInline(TEXT("a.txt"), TEXT(""));
				GameSandboxPath.ReplaceInline(TEXT("[Platform]"), *Target->PlatformName());

				FString LocalGamePath = FPaths::GameDir();
				if (FPaths::IsProjectFilePathSet())
				{
					LocalGamePath = FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/");
				}

				FString LocalEnginePath = FPaths::EngineDir();


				FName PlatformFName( *Target->PlatformName() );
				// use the timestamp grabbing visitor
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				FLocalTimestampDirectoryVisitor Visitor(PlatformFile, DirectoriesToSkip, DirectoriesToNotRecurse, false);
			
				FString SandboxPath = GetSandboxDirectory(Target->PlatformName());

				PlatformFile.IterateDirectory(*SandboxPath, Visitor);

				for (TMap<FString, FDateTime>::TIterator TimestampIt(Visitor.FileTimes); TimestampIt; ++TimestampIt)
				{
					FString CookedFilename = TimestampIt.Key();
					if ((CookedFilename.EndsWith(TEXT(".uasset")) == false) &&
						(CookedFilename.EndsWith(TEXT(".umap")) == false))
					{
						// don't care if it's not a map or an asset
						//IFileManager::Get().Delete(*CookedFilename);
						continue;
					}

					FDateTime CookedTimestamp = TimestampIt.Value();
					CookedFilename = FPaths::ConvertRelativePathToFull(CookedFilename);
					FString StandardCookedFilename = CookedFilename;

					StandardCookedFilename.ReplaceInline(*SandboxPath, *(FPaths::GetRelativePathToRoot()));

					StandardCookedFilename = FPaths::ConvertRelativePathToFull(StandardCookedFilename);
					FPaths::MakeStandardFilename(StandardCookedFilename);
					
					FDateTime DependentTimestamp;
					FName StandardCookedFileFName = FName(*StandardCookedFilename);

					bool bShouldKeep = true;

					if (PDInfoModule.DeterminePackageDependentTimeStamp(*(FPaths::GetBaseFilename(StandardCookedFilename, false)), DependentTimestamp) == true)
					{
						double Diff = (CookedTimestamp - DependentTimestamp).GetTotalSeconds();

						if (Diff < 0.0)
						{
#if DEBUG_COOKONTHEFLY
							UE_LOG(LogCook, Display, TEXT("Deleting out of date cooked file: %s"), *CookedFilename);
#endif
							IFileManager::Get().Delete(*CookedFilename);

							CookedPackages.RemoveFileForPlatform(StandardCookedFileFName, PlatformFName);

							bShouldKeep = false;
						}
					}

					if (bShouldKeep)
					{
						FString LongPackageName;
						FString FailureReason;
						if (FPackageName::TryConvertFilenameToLongPackageName(StandardCookedFilename, LongPackageName, &FailureReason))
						{
							const FName LongPackageFName(*LongPackageName);
							PackagesKeptFromPreviousCook.Add(LongPackageFName);
						}
						else
						{
							LogCookerMessage(FString::Printf(TEXT("Unable to generate long package name for %s because %s"), *StandardCookedFilename, *FailureReason), EMessageSeverity::Warning);
							UE_LOG(LogCook, Warning, TEXT("Unable to generate long package name for %s because %s"), *StandardCookedFilename, *FailureReason);
						}
					}
				}
			}

			// Collect garbage to ensure we don't have any packages hanging around from dependent time stamp determination
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}
#if OUTPUT_TIMING
	UE_LOG(LogCook, Display, TEXT("Sandbox cleanup took %5.3f seconds"), SandboxCleanTime);
#endif
}

void UCookOnTheFlyServer::GenerateAssetRegistry(const TArray<ITargetPlatform*>& Platforms)
{
	SCOPE_COOKING_STAT(GenerateAssetRegistry);
	if (IsChildCooker())
	{
		// don't generate the asset registry
		return;
	}

	if (IsCookingInEditor() == false)
	{
		// we want to register the temporary save directory if we are cooking outside the editor.  
		// If we are cooking inside the editor we never use this directory so don't worry about registring it
		FPackageName::RegisterMountPoint(TEXT("/TempAutosave/"), FPaths::GameSavedDir() / GEngine->PlayOnConsoleSaveDir);
	}

	// load the interface
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	double GenerateAssetRegistryTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(GenerateAssetRegistryTime);
#if DEBUG_COOKONTHEFLY
		UE_LOG(LogCook, Display, TEXT("Creating asset registry [is editor: %d]"), GIsEditor);
#endif
		// Perform a synchronous search of any .ini based asset paths (note that the per-game delegate may
		// have already scanned paths on its own)
		// We want the registry to be fully initialized when generating streaming manifests too.
		bool bEditor = IsRealtimeMode();

		// editor will scan asset registry automagically 
		if ( !bEditor )
		{
			TArray<FString> ScanPaths;
			if (GConfig->GetArray(TEXT("AssetRegistry"), TEXT("PathsToScanForCook"), ScanPaths, GEngineIni) > 0)
			{
				AssetRegistry.ScanPathsSynchronous(ScanPaths);
			}
			else
			{
				AssetRegistry.SearchAllAssets(true);
			}
		}
	}
	UE_LOG(LogCook, Display, TEXT("Done creating registry. It took %5.2fs."), GenerateAssetRegistryTime);
}

void UCookOnTheFlyServer::GenerateLongPackageNames(TArray<FName>& FilesInPath)
{
#if 0
	TArray<FString> FilesInPathReverse;
	FilesInPathReverse.Reserve(FilesInPath.Num());
	for( int32 FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FString& FileInPath = FilesInPath[FilesInPath.Num() - FileIndex - 1];
		if (FPackageName::IsValidLongPackageName(FileInPath))
		{
			FilesInPathReverse.AddUnique(FileInPath);
		}
		else
		{
			FString LongPackageName;
			if (FPackageName::TryConvertFilenameToLongPackageName(FileInPath, LongPackageName))
			{
				FilesInPathReverse.AddUnique(LongPackageName);
			}
			else
			{
				LogCookerMessage( FString::Printf(TEXT("Unable to generate long package name for %s"), *FileInPath), EMessageSeverity::Warning);
				UE_LOG(LogCook, Warning, TEXT("Unable to generate long package name for %s"), *FileInPath);
			}
		}
	}
	Exchange(FilesInPathReverse, FilesInPath);
#else
	TArray<FName> FilesInPathReverse;
	FilesInPathReverse.Reserve(FilesInPath.Num());

	for( int32 FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FString& FileInPath = FilesInPath[FilesInPath.Num() - FileIndex - 1].ToString();
		if (FPackageName::IsValidLongPackageName(FileInPath))
		{
			const FName FileInPathFName(*FileInPath);
			FilesInPathReverse.AddUnique(FileInPathFName);
		}
		else
		{
			FString LongPackageName;
			FString FailureReason;
			if (FPackageName::TryConvertFilenameToLongPackageName(FileInPath, LongPackageName, &FailureReason))
			{
				const FName LongPackageFName(*LongPackageName);
				FilesInPathReverse.AddUnique(LongPackageFName);
			}
			else
			{
				LogCookerMessage(FString::Printf(TEXT("Unable to generate long package name for %s because %s"), *FileInPath, *FailureReason), EMessageSeverity::Warning);
				UE_LOG(LogCook, Warning, TEXT("Unable to generate long package name for %s because %s"), *FileInPath, *FailureReason);
			}
		}
	}
	// Exchange(FilesInPathReverse, FilesInPath);
	FilesInPath.Empty(FilesInPathReverse.Num());
	for ( const auto& Files : FilesInPathReverse )
	{
		FilesInPath.Add(Files);
	}
#endif
}

void UCookOnTheFlyServer::AddFileToCook( TArray<FName>& InOutFilesToCook, const FString &InFilename ) const
{ 
	if (!FPackageName::IsScriptPackage(InFilename))
	{
#if 0 // randomize cook file order, don't check in enabled...
		if ( !InOutFilesToCook.Contains(InFilename) )
		{
			int Index = FMath::RandRange(0,InOutFilesToCook.Num()-1);
			InOutFilesToCook.Insert(InFilename, Index );
		}
#else
		InOutFilesToCook.AddUnique(FName(*InFilename));
#endif
	}
}

void UCookOnTheFlyServer::CollectFilesToCook(TArray<FName>& FilesInPath, const TArray<FString>& CookMaps, const TArray<FString>& InCookDirectories, const TArray<FString> &CookCultures, const TArray<FString> &IniMapSections, bool bCookAll, bool bMapsOnly, bool bNoDev )
{
	if (CookByTheBookOptions->bIsChildCooker)
	{
		const FString ChildCookFilename = CookByTheBookOptions->ChildCookFilename;
		check(ChildCookFilename.Len());
		FString ChildCookString;
		ensure(FFileHelper::LoadFileToString(ChildCookString, *ChildCookFilename));
	
		TArray<FString> ChildCookArray;
		ChildCookString.ParseIntoArrayLines(ChildCookArray);

		for (const FString& ChildFile : ChildCookArray)
		{
			AddFileToCook(FilesInPath, ChildFile);
		}
		// if we are a child cooker then just add the files for the child cooker and early out 
		return;
	}

	TArray<FString> CookDirectories = InCookDirectories;
	
	if (!IsCookingDLC())
	{
		TArray<FString> MapList;
		// Add the default map section
		GEditor->LoadMapListFromIni(TEXT("AlwaysCookMaps"), MapList);

		for (const auto &IniMapSection : IniMapSections)
		{
			GEditor->LoadMapListFromIni(*IniMapSection, MapList);
		}

		for (int32 MapIdx = 0; MapIdx < MapList.Num(); MapIdx++)
		{
			AddFileToCook(FilesInPath, MapList[MapIdx]);
		}


		// Also append any cookdirs from the project ini files; these dirs are relative to the game content directory
		{
			const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir());
			const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
			for (const auto& DirToCook : PackagingSettings->DirectoriesToAlwaysCook)
			{
				CookDirectories.Add(AbsoluteGameContentDir / DirToCook.Path);
			}
		}
	}

	

	for ( const auto &CurrEntry : CookMaps )
	{
		if (FPackageName::IsShortPackageName(CurrEntry))
		{
			FString OutFilename;
			if (FPackageName::SearchForPackageOnDisk(CurrEntry, NULL, &OutFilename) == false)
			{
				LogCookerMessage( FString::Printf(TEXT("Unable to find package for map %s."), *CurrEntry), EMessageSeverity::Warning);
				UE_LOG(LogCook, Warning, TEXT("Unable to find package for map %s."), *CurrEntry);
			}
			else
			{
				AddFileToCook( FilesInPath, OutFilename);
			}
		}
		else
		{
			AddFileToCook( FilesInPath,CurrEntry);
		}
	}

	const FString ExternalMountPointName(TEXT("/Game/"));
	for ( const auto &CurrEntry : CookDirectories )
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *CurrEntry, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
		for (int32 Index = 0; Index < Files.Num(); Index++)
		{
			FString StdFile = Files[Index];
			FPaths::MakeStandardFilename(StdFile);
			AddFileToCook( FilesInPath,StdFile);

			// this asset may not be in our currently mounted content directories, so try to mount a new one now
			FString LongPackageName;
			if(!FPackageName::IsValidLongPackageName(StdFile) && !FPackageName::TryConvertFilenameToLongPackageName(StdFile, LongPackageName))
			{
				FPackageName::RegisterMountPoint(ExternalMountPointName, CurrEntry);
			}
		}
	}

	if ( IsCookingDLC() )
	{
		// get the dlc and make sure we cook that directory 
		FString DLCPath = FPaths::GamePluginsDir() / CookByTheBookOptions->DlcName / FString(TEXT("Content"));

		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *DLCPath, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
		IFileManager::Get().FindFilesRecursive(Files, *DLCPath, *(FString(TEXT("*")) + FPackageName::GetMapPackageExtension()), true, false);
		for (int32 Index = 0; Index < Files.Num(); Index++)
		{
			FString StdFile = Files[Index];
			FPaths::MakeStandardFilename(StdFile);
			AddFileToCook( FilesInPath,StdFile);

			// this asset may not be in our currently mounted content directories, so try to mount a new one now
			FString LongPackageName;
			if(!FPackageName::IsValidLongPackageName(StdFile) && !FPackageName::TryConvertFilenameToLongPackageName(StdFile, LongPackageName))
			{
				FPackageName::RegisterMountPoint(ExternalMountPointName, DLCPath);
			}
		}
	}

	if ((FilesInPath.Num() == 0) || bCookAll)
	{
		TArray<FString> Tokens;
		Tokens.Empty(2);
		Tokens.Add(FString("*") + FPackageName::GetAssetPackageExtension());
		Tokens.Add(FString("*") + FPackageName::GetMapPackageExtension());

		uint8 PackageFilter = NORMALIZE_DefaultFlags | NORMALIZE_ExcludeEnginePackages;
		if ( bMapsOnly )
		{
			PackageFilter |= NORMALIZE_ExcludeContentPackages;
		}

		if ( bNoDev )
		{
			PackageFilter |= NORMALIZE_ExcludeDeveloperPackages;
		}

		// assume the first token is the map wildcard/pathname
		TArray<FString> Unused;
		for ( int32 TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
		{
			TArray<FString> TokenFiles;
			if ( !NormalizePackageNames( Unused, TokenFiles, Tokens[TokenIndex], PackageFilter) )
			{
				UE_LOG(LogCook, Display, TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens[TokenIndex]);
				continue;
			}

			for (int32 TokenFileIndex = 0; TokenFileIndex < TokenFiles.Num(); ++TokenFileIndex)
			{
				AddFileToCook( FilesInPath, TokenFiles[TokenFileIndex]);
			}
		}
	}

	// make sure we cook the default maps
	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	static const TArray<ITargetPlatform*>& Platforms =  TPM.GetTargetPlatforms();
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		// load the platform specific ini to get its DefaultMap
		FConfigFile PlatformEngineIni;
		FConfigCacheIni::LoadLocalIniFile(PlatformEngineIni, TEXT("Engine"), true, *Platforms[Index]->IniPlatformName());

		// get the server and game default maps and cook them
		FString Obj;
		if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), Obj))
		{
			if (Obj != FName(NAME_None).ToString())
			{
				AddFileToCook( FilesInPath, Obj);
			}
		}
		if ( IsCookFlagSet(ECookInitializationFlags::IncludeServerMaps) )
		{
			if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("ServerDefaultMap"), Obj))
			{
				if (Obj != FName(NAME_None).ToString())
				{
					AddFileToCook( FilesInPath, Obj);
				}
			}
		}
		if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultGameMode"), Obj))
		{
			if (Obj != FName(NAME_None).ToString())
			{
				AddFileToCook( FilesInPath, Obj);
			}
		}
		if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultServerGameMode"), Obj))
		{
			if (Obj != FName(NAME_None).ToString())
			{
				AddFileToCook( FilesInPath, Obj);
			}
		}
		if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameInstanceClass"), Obj))
		{
			if (Obj != FName(NAME_None).ToString())
			{
				AddFileToCook( FilesInPath, Obj);
			}
		}
	}

	// make sure we cook any extra assets for the default touch interface
	// @todo need a better approach to cooking assets which are dynamically loaded by engine code based on settings
	FConfigFile InputIni;
	FString InterfaceFile;
	FConfigCacheIni::LoadLocalIniFile(InputIni, TEXT("Input"), true);
	if (InputIni.GetString(TEXT("/Script/Engine.InputSettings"), TEXT("DefaultTouchInterface"), InterfaceFile))
	{
		if (InterfaceFile != TEXT("None") && InterfaceFile != TEXT(""))
		{
			AddFileToCook( FilesInPath, InterfaceFile);
		}
	}

	//@todo SLATE: This is a hack to ensure all slate referenced assets get cooked.
	// Slate needs to be refactored to properly identify required assets at cook time.
	// Simply jamming everything in a given directory into the cook list is error-prone
	// on many levels - assets not required getting cooked/shipped; assets not put under 
	// the correct folder; etc.
	{
		TArray<FString> UIContentPaths;
		TSet <FName> ContentDirectoryAssets; 
		if (GConfig->GetArray(TEXT("UI"), TEXT("ContentDirectories"), UIContentPaths, GEditorIni) > 0)
		{
			for (int32 DirIdx = 0; DirIdx < UIContentPaths.Num(); DirIdx++)
			{
				FString ContentPath = FPackageName::LongPackageNameToFilename(UIContentPaths[DirIdx]);

				TArray<FString> Files;
				IFileManager::Get().FindFilesRecursive(Files, *ContentPath, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
				for (int32 Index = 0; Index < Files.Num(); Index++)
				{
					FString StdFile = Files[Index];
					FName PackageName = FName(*FPackageName::FilenameToLongPackageName(StdFile));
					ContentDirectoryAssets.Add(PackageName);
					FPaths::MakeStandardFilename(StdFile);
					AddFileToCook( FilesInPath, StdFile);
				}
			}
		}

		if (CookByTheBookOptions && CookByTheBookOptions->bGenerateDependenciesForMaps) 
		{
			for (auto& MapDependencyGraph : CookByTheBookOptions->MapDependencyGraphs)
			{
				MapDependencyGraph.Value.Add(FName(TEXT("ContentDirectoryAssets")), ContentDirectoryAssets);
			}
		}
	}
}

bool UCookOnTheFlyServer::IsCookByTheBookRunning() const
{
	return CookByTheBookOptions && CookByTheBookOptions->bRunning;
}


void UCookOnTheFlyServer::SaveGlobalShaderMapFiles(const TArray<ITargetPlatform*>& Platforms)
{
	// we don't support this behavior
	check( !IsCookingDLC() );
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		// make sure global shaders are up to date!
		TArray<FString> Files;
		FShaderRecompileData RecompileData;
		RecompileData.PlatformName = Platforms[Index]->PlatformName();
		// Compile for all platforms
		RecompileData.ShaderPlatform = -1;
		RecompileData.ModifiedFiles = &Files;
		RecompileData.MeshMaterialMaps = NULL;

		check( IsInGameThread() );

		FString OutputDir = GetSandboxDirectory(RecompileData.PlatformName);

		RecompileShadersForRemote
			(RecompileData.PlatformName, 
			RecompileData.ShaderPlatform == -1 ? SP_NumPlatforms : (EShaderPlatform)RecompileData.ShaderPlatform,
			OutputDir, 
			RecompileData.MaterialsToLoad, 
			RecompileData.SerializedShaderResources, 
			RecompileData.MeshMaterialMaps, 
			RecompileData.ModifiedFiles);
	}
}

FString UCookOnTheFlyServer::GetSandboxDirectory( const FString& PlatformName ) const
{
	FString Result;
	Result = SandboxFile->GetSandboxDirectory();

	Result.ReplaceInline(TEXT("[Platform]"), *PlatformName);

	/*if ( IsCookingDLC() )
	{
		check( IsCookByTheBookRunning() );
		Result.ReplaceInline(TEXT("/Cooked/"), *FString::Printf(TEXT("/CookedDLC_%s/"), *CookByTheBookOptions->DlcName) );
	}*/
	return Result;
}

FString UCookOnTheFlyServer::ConvertToFullSandboxPath( const FString &FileName, bool bForWrite ) const
{
	check( SandboxFile );

	FString Result;
	if ( bForWrite)
	{
		Result = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*FileName);
	}
	else
	{
		Result = SandboxFile->ConvertToAbsolutePathForExternalAppForRead(*FileName);
	}

	/*if ( IsCookingDLC() )
	{
		check( IsCookByTheBookRunning() );
		Result.ReplaceInline(TEXT("/Cooked/"), *FString::Printf(TEXT("/CookedDLC_%s/"), *CookByTheBookOptions->DlcName) );
	}*/
	return Result;
}

FString UCookOnTheFlyServer::ConvertToFullSandboxPath( const FString &FileName, bool bForWrite, const FString& PlatformName ) const
{
	FString Result = ConvertToFullSandboxPath( FileName, bForWrite );
	Result.ReplaceInline(TEXT("[Platform]"), *PlatformName);
	return Result;
}

const FString UCookOnTheFlyServer::GetSandboxAssetRegistryFilename()
{
	static const FString RegistryFilename = FPaths::GameDir() / TEXT("AssetRegistry.bin");
	const FString SandboxRegistryFilename = ConvertToFullSandboxPath(*RegistryFilename, true);
	return SandboxRegistryFilename;
}

const FString UCookOnTheFlyServer::GetCookedAssetRegistryFilename(const FString& PlatformName )
{
	const FString CookedAssetRegistryFilename = GetSandboxAssetRegistryFilename().Replace(TEXT("[Platform]"), *PlatformName);
	return CookedAssetRegistryFilename;
}

void UCookOnTheFlyServer::SaveCookerStats() const
{
	static const FName CookingStatsName("CookingStats");
	FCookingStatsModule& CookingStatsModule = FModuleManager::LoadModuleChecked<FCookingStatsModule>(CookingStatsName);
	ICookingStats& CookingStats = CookingStatsModule.Get();

	const FString StatsFilename = GetStatsFilename(CookByTheBookOptions->ChildCookFilename);
	CookingStats.SaveStatsAsCSV(StatsFilename);
}

void UCookOnTheFlyServer::CookByTheBookFinished()
{
	check( IsInGameThread() );
	check( IsCookByTheBookMode() );
	check( CookByTheBookOptions->bRunning == true );

	UPackage::WaitForAsyncFileWrites();

	GetDerivedDataCacheRef().WaitForQuiescence(true);


	SaveCookerStats();

	if (IsChildCooker())
	{
		// if we are the child cooker create a list of all the packages which we think wes hould have cooked but didn't
		FString UncookedPackageList;
		for (const auto& UncookedPackage : CookByTheBookOptions->ChildUnsolicitedPackages)
		{
			UncookedPackageList.Append(UncookedPackage.ToString() + TEXT("\n\r"));
		}
		FFileHelper::SaveStringToFile(UncookedPackageList, *GetChildCookerResultFilename(CookByTheBookOptions->ChildCookFilename));

		
		
	}
	else
	{
		CleanUpChildCookers();

		check(CookByTheBookOptions->ChildUnsolicitedPackages.Num() == 0);
		SCOPE_COOKING_STAT(SavingAssetRegistry);
		SCOPE_TIMER(SavingAssetRegistry);
		// Save modified asset registry with all streaming chunk info generated during cook
		const FString& SandboxRegistryFilename = GetSandboxAssetRegistryFilename();
		// the registry filename will be modified when we call save asset registry
		
		const FString CookedAssetRegistry = FPaths::GameDir() / TEXT("CookedAssetRegistry.json");
		const FString SandboxCookedAssetRegistryFilename = ConvertToFullSandboxPath(*CookedAssetRegistry, true);

		for ( auto& Manifest : CookByTheBookOptions->ManifestGenerators )
		{
			TArray<FName> CookedPackagesForPlatform;
			CookedPackages.GetCookedFilesForPlatform(Manifest.Key, CookedPackagesForPlatform);

			TArray<FName> LongCookedPackageNames;
			for (const auto& CookedPackageFileName : CookedPackagesForPlatform)
			{
				FString LongPackageName;
				verify(FPackageName::TryConvertFilenameToLongPackageName(CookedPackageFileName.ToString(), LongPackageName));
				LongCookedPackageNames.Add(FName(*LongPackageName));
			}


			Manifest.Value->BuildChunkManifest(LongCookedPackageNames, SandboxFile, CookByTheBookOptions->bGenerateStreamingInstallManifests);


			// Always try to save the manifests, this is required to make the asset registry work, but doesn't necessarily write a file
			Manifest.Value->SaveManifests(SandboxFile.GetOwnedPointer());

			const FName& PlatformName = Manifest.Key;
			const TArray<FName>& PackageFilenames = CookByTheBookOptions->BasedOnReleaseCookedPackages.FindRef(PlatformName);
			TArray<FName> LongPackageNames;
			LongPackageNames.Empty(PackageFilenames.Num());
			for (const auto& PackageFilename : PackageFilenames)
			{
				FString LongPackageName;
				verify(FPackageName::TryConvertFilenameToLongPackageName(PackageFilename.ToString(), LongPackageName));
				LongPackageNames.Add(FName(*LongPackageName));
			}

			Manifest.Value->SaveAssetRegistry(SandboxRegistryFilename, &LongPackageNames);

			Manifest.Value->SaveCookedPackageAssetRegistry(SandboxCookedAssetRegistryFilename, true);

			if ( IsCreatingReleaseVersion() )
			{
				const FString VersionedRegistryPath = GetReleaseVersionAssetRegistryPath( CookByTheBookOptions->CreateReleaseVersion, Manifest.Key ); 

				IFileManager::Get().MakeDirectory( *VersionedRegistryPath ,true );

				const FString VersionedRegistryFilename = VersionedRegistryPath / GetAssetRegistryFilename();

				const FString CookedAssetRegistryFilename = SandboxRegistryFilename.Replace(TEXT("[Platform]"), *Manifest.Key.ToString());

				IFileManager::Get().Copy(*VersionedRegistryFilename, *CookedAssetRegistryFilename, true, true);
				// Manifest.Value->SaveManifests( VersionedRegistryFilename );
			}
		}
	}

	if (CookByTheBookOptions->bGenerateDependenciesForMaps && (IsChildCooker() == false))
	{
		for (auto& MapDependencyGraphIt : CookByTheBookOptions->MapDependencyGraphs)
		{
			BuildMapDependencyGraph(MapDependencyGraphIt.Key);
			WriteMapDependencyGraph(MapDependencyGraphIt.Key);
		}
	}

	CookByTheBookOptions->LastGCItems.Empty();
	const float TotalCookTime = (float)(FPlatformTime::Seconds() - CookByTheBookOptions->CookStartTime);
	UE_LOG(LogCook, Display, TEXT("Cook by the book total time in tick %fs total time %f"), CookByTheBookOptions->CookTime, TotalCookTime);

	CookByTheBookOptions->BasedOnReleaseCookedPackages.Empty();

	CookByTheBookOptions->bRunning = false;

	OUTPUT_HIERARCHYTIMERS();
}

void UCookOnTheFlyServer::BuildMapDependencyGraph(const FName& PlatformName)
{
	auto& MapDependencyGraph = CookByTheBookOptions->MapDependencyGraphs.FindChecked(PlatformName);



	TArray<FName> PlatformCookedPackages;
	CookedPackages.GetCookedFilesForPlatform(PlatformName, PlatformCookedPackages);

	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// assign chunks for all the map packages
	for (const auto& CookedPackage : PlatformCookedPackages)
	{
		TArray<FAssetData> PackageAssets;
		FName Name = FName(*FPackageName::FilenameToLongPackageName(CookedPackage.ToString()));

		if (!ContainsMap(Name))
			continue;

		TSet<FName> DependentPackages;
		TSet<FName> Roots; 

		Roots.Add(Name);

		GetDependentPackages(Roots, DependentPackages);

		MapDependencyGraph.Add(Name, DependentPackages);
	}
}

void UCookOnTheFlyServer::WriteMapDependencyGraph(const FName& PlatformName)
{
	auto& MapDependencyGraph = CookByTheBookOptions->MapDependencyGraphs.FindChecked(PlatformName);

	FString MapDependencyGraphFile = FPaths::GameDir() / TEXT("MapDependencyGraph.json");
	// dump dependency graph. 
	FString DependencyString;
	DependencyString += "{";
	for (auto& Ele : MapDependencyGraph)
	{
		TSet <FName>& Deps = Ele.Value;
		FName MapName = Ele.Key;
		DependencyString += TEXT("\t\"") + MapName.ToString() + TEXT("\" : \n\t[\n ");
		for (auto& Val : Deps)
		{
			DependencyString += TEXT("\t\t\"") + Val.ToString() + TEXT("\",\n");
		}
		DependencyString.RemoveFromEnd(TEXT(",\n"));
		DependencyString += TEXT("\n\t],\n");
	}
	DependencyString.RemoveFromEnd(TEXT(",\n"));
	DependencyString += "\n}";

	FString CookedMapDependencyGraphFilePlatform = ConvertToFullSandboxPath(MapDependencyGraphFile, true).Replace(TEXT("[Platform]"), *PlatformName.ToString());
	FFileHelper::SaveStringToFile(DependencyString, *CookedMapDependencyGraphFilePlatform, FFileHelper::EEncodingOptions::ForceUnicode);
}

void UCookOnTheFlyServer::QueueCancelCookByTheBook()
{
	if ( IsCookByTheBookMode() )
	{
		check( CookByTheBookOptions != NULL );
		CookByTheBookOptions->bCancel = true;
	}
}

void UCookOnTheFlyServer::CancelCookByTheBook()
{
	if ( IsCookByTheBookMode() && CookByTheBookOptions->bRunning )
	{
		check(CookByTheBookOptions);
		check( IsInGameThread() );

		// save the cook requests 
		CookRequests.DequeueAllRequests(CookByTheBookOptions->PreviousCookRequests);
		CookByTheBookOptions->bRunning = false;

		SandboxFile = NULL;
	}	
}

void UCookOnTheFlyServer::StopAndClearCookedData()
{
	if ( IsCookByTheBookMode() )
	{
		check( CookByTheBookOptions != NULL );
		check( CookByTheBookOptions->bRunning == false );
		CancelCookByTheBook();
		CookByTheBookOptions->PreviousCookRequests.Empty();
	}

	RecompileRequests.Empty();
	CookRequests.Empty();
	UnsolicitedCookedPackages.Empty();
	CookedPackages.Empty(); // set of files which have been cooked when needing to recook a file the entry will need to be removed from here
}

void UCookOnTheFlyServer::ClearAllCookedData()
{
	// if we are going to clear the cooked packages it is conceivable that we will recook the packages which we just cooked 
	// that means it's also conceivable that we will recook the same package which currently has an outstanding async write request
	UPackage::WaitForAsyncFileWrites();

	UnsolicitedCookedPackages.Empty();
	CookedPackages.Empty(); // set of files which have been cooked when needing to recook a file the entry will need to be removed from here
}

void UCookOnTheFlyServer::ClearPlatformCookedData( const FName& PlatformName )
{
	// if we are going to clear the cooked packages it is conceivable that we will recook the packages which we just cooked 
	// that means it's also conceivable that we will recook the same package which currently has an outstanding async write request
	UPackage::WaitForAsyncFileWrites();

	CookedPackages.RemoveAllFilesForPlatform( PlatformName );
	TArray<FName> PackageNames;
	UnsolicitedCookedPackages.GetPackagesForPlatformAndRemove(PlatformName, PackageNames);
}

void UCookOnTheFlyServer::CreateSandboxFile()
{

	// initialize the sandbox file after determining if we are cooking dlc
	// Local sandbox file wrapper. This will be used to handle path conversions,
	// but will not be used to actually write/read files so we can safely
	// use [Platform] token in the sandbox directory name and then replace it
	// with the actual platform name.
	check( SandboxFile == NULL );
	SandboxFile = new FSandboxPlatformFile(false);

	// Output directory override.	
	FString OutputDirectory = GetOutputDirectoryOverride();

	// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	SandboxFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), *FString::Printf(TEXT("-sandbox=\"%s\""), *OutputDirectory));
}

void UCookOnTheFlyServer::InitializeSandbox()
{
	SCOPE_COOKING_STAT(InitializeSandbox);
	if ( SandboxFile == NULL )
	{
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		const TArray<ITargetPlatform*>& TargetPlatforms = TPM.GetCookingTargetPlatforms();

		CreateSandboxFile();

		if (!IsChildCooker())
		{
			bIsInitializingSandbox = true;
			if (IsCookFlagSet(ECookInitializationFlags::Iterative))
			{
				PopulateCookedPackagesFromDisk(TargetPlatforms);
			}

			CleanSandbox(IsCookFlagSet(ECookInitializationFlags::Iterative));
			bIsInitializingSandbox = false;

		}
	}
}

void UCookOnTheFlyServer::TermSandbox()
{
	ClearAllCookedData();
	ClearPackageFilenameCache();
	SandboxFile = NULL;
}

void UCookOnTheFlyServer::ValidateCookByTheBookSettings() const
{
	if (IsChildCooker())
	{
		// should never be generating dependency maps / streaming install manifests for child cookers
		check(CookByTheBookOptions->bGenerateDependenciesForMaps == false);
		check(CookByTheBookOptions->bGenerateStreamingInstallManifests == false);
	}
}

void UCookOnTheFlyServer::StartCookByTheBook( const FCookByTheBookStartupOptions& CookByTheBookStartupOptions )
{

	const TArray<ITargetPlatform*>& TargetPlatforms = CookByTheBookStartupOptions.TargetPlatforms;
	const TArray<FString>& CookMaps = CookByTheBookStartupOptions.CookMaps;
	const TArray<FString>& CookDirectories = CookByTheBookStartupOptions.CookDirectories;
	const TArray<FString>& CookCultures = CookByTheBookStartupOptions.CookCultures;
	const TArray<FString>& IniMapSections = CookByTheBookStartupOptions.IniMapSections;
	const ECookByTheBookOptions& CookOptions = CookByTheBookStartupOptions.CookOptions;
	const FString& DLCName = CookByTheBookStartupOptions.DLCName;

	const FString& CreateReleaseVersion = CookByTheBookStartupOptions.CreateReleaseVersion;
	const FString& BasedOnReleaseVersion = CookByTheBookStartupOptions.BasedOnReleaseVersion;

	check( IsInGameThread() );
	check( IsCookByTheBookMode() );

	CookByTheBookOptions->bRunning = true;
	CookByTheBookOptions->bCancel = false;
	CookByTheBookOptions->CookTime = 0.0f;
	CookByTheBookOptions->CookStartTime = FPlatformTime::Seconds();
	CookByTheBookOptions->bGenerateStreamingInstallManifests = CookByTheBookStartupOptions.bGenerateStreamingInstallManifests;
	CookByTheBookOptions->bGenerateDependenciesForMaps = CookByTheBookStartupOptions.bGenerateDependenciesForMaps;
	CookByTheBookOptions->CreateReleaseVersion = CreateReleaseVersion;
	CookByTheBookOptions->bForceEnableCompressedPackages = !!(CookOptions & ECookByTheBookOptions::ForceEnableCompressed);
	CookByTheBookOptions->bForceDisableCompressedPackages = !!(CookOptions & ECookByTheBookOptions::ForceDisableCompressed);
	CookByTheBookOptions->bIsChildCooker = CookByTheBookStartupOptions.ChildCookFileName.Len() > 0 ? true : false;
	CookByTheBookOptions->ChildCookFilename = CookByTheBookStartupOptions.ChildCookFileName;
	
	NeverCookPackageList.Empty();
	{
		const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir());
		const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();

		TArray<FString> NeverCookDirectories = CookByTheBookStartupOptions.NeverCookDirectories;

		for (const auto& DirToNotCook : PackagingSettings->DirectoriesToNeverCook)
		{
			NeverCookDirectories.Add(AbsoluteGameContentDir / DirToNotCook.Path);
		}

		for (const auto& NeverCookDirectory : NeverCookDirectories)
		{
			// add the packages to the never cook package list
			struct FNeverCookDirectoryWalker : public IPlatformFile::FDirectoryVisitor
			{
			private:
				FThreadSafeNameSet &NeverCookPackageList;
			public:
				FNeverCookDirectoryWalker(FThreadSafeNameSet &InNeverCookPackageList) : NeverCookPackageList(InNeverCookPackageList) { }

				// IPlatformFile::FDirectoryVisitor interface
				virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
				{
					if (bIsDirectory)
					{
						return true;
					}
					FString StandardFilename = FString(FilenameOrDirectory);
					FPaths::MakeStandardFilename(StandardFilename);

					NeverCookPackageList.Add(FName(*StandardFilename));
					return true;
				}

			} NeverCookDirectoryWalker(NeverCookPackageList);

			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

			PlatformFile.IterateDirectoryRecursively(*NeverCookDirectory, NeverCookDirectoryWalker);
		}

	}
	

	CookByTheBookOptions->TargetPlatformNames.Empty();
	for (const auto &Platform : TargetPlatforms)
	{
		FName PlatformName = FName(*Platform->PlatformName());
		CookByTheBookOptions->TargetPlatformNames.Add(PlatformName); // build list of all target platform names
	}
	const TArray<FName>& TargetPlatformNames = CookByTheBookOptions->TargetPlatformNames;

	bool bCookAll = (CookOptions & ECookByTheBookOptions::CookAll) != ECookByTheBookOptions::None;
	bool bMapsOnly = (CookOptions & ECookByTheBookOptions::MapsOnly) != ECookByTheBookOptions::None;
	bool bNoDev = (CookOptions & ECookByTheBookOptions::NoDevContent) != ECookByTheBookOptions::None;

	ValidateCookByTheBookSettings();

	if ( CookByTheBookOptions->DlcName != DLCName )
	{
		// we are going to change the state of dlc we need to clean out our package filename cache (the generated filename cache is dependent on this key)
		CookByTheBookOptions->DlcName = DLCName;
		check( OutputDirectoryOverride.Len() == 0 );

		TermSandbox();
	}

	InitializeSandbox();


	// need to test this out
	/*if (RecompileChangedShaders(TargetPlatformNames))
	{
		// clean everything :(
		CleanSandbox(false);
	}*/

	CookByTheBookOptions->bLeakTest = (CookOptions & ECookByTheBookOptions::LeakTest) != ECookByTheBookOptions::None; // this won't work from the editor this needs to be standalone
	check(!CookByTheBookOptions->bLeakTest || CurrentCookMode == ECookMode::CookByTheBook);

	CookByTheBookOptions->LastGCItems.Empty();
	if (CookByTheBookOptions->bLeakTest)
	{
		for (FObjectIterator It; It; ++It)
		{
			CookByTheBookOptions->LastGCItems.Add(FWeakObjectPtr(*It));
		}
	}

	if (!IsChildCooker())
	{
		// iteratively clean any old files out of the sandbox (check if ini settings are out of date and clean out any stale files)
		CleanSandbox(true);

		for (const auto &Platform : TargetPlatforms)
		{
			FName PlatformName = FName(*Platform->PlatformName());

			// make sure we have a manifest for all the platforms 
			// we want a seperate manifest for each platform because they can all be in different states of cooked content
			FChunkManifestGenerator* ManifestGenerator = CookByTheBookOptions->ManifestGenerators.FindRef(PlatformName);
			if (ManifestGenerator == NULL)
			{
				TArray<ITargetPlatform*> Platforms;
				Platforms.Add(Platform);
				ManifestGenerator = new FChunkManifestGenerator(Platforms);
				ManifestGenerator->CleanManifestDirectories();

				TArray<FName> StartupPackages;

				for (TObjectIterator<UPackage> It; It; ++It)
				{
					StartupPackages.Add( It->GetFName());
				}

				ManifestGenerator->Initialize(StartupPackages);
				CookByTheBookOptions->ManifestGenerators.Add(PlatformName, ManifestGenerator);
			}

			if (CookByTheBookOptions->bGenerateDependenciesForMaps)
			{
				CookByTheBookOptions->MapDependencyGraphs.Add(PlatformName);
			}
		}
	}

	if ( IsCookingDLC() )
	{
		// if we are cooking dlc we must be based of a release version cook
		check( !BasedOnReleaseVersion.IsEmpty() );

		for ( const auto& PlatformName : TargetPlatformNames )
		{
			const FString OriginalSandboxRegistryFilename = GetReleaseVersionAssetRegistryPath(BasedOnReleaseVersion, PlatformName ) / GetAssetRegistryFilename();

			TArray<FName> PackageList;
			// if this check fails probably because the asset registry can't be found or read
			bool bSucceeded = GetAllPackagesFromAssetRegistry(OriginalSandboxRegistryFilename, PackageList);
			check( bSucceeded );

			if ( bSucceeded )
			{
				TArray<FName> PlatformNames;
				PlatformNames.Add(PlatformName);
				for (const auto& PackageFilename : PackageList)
				{
					CookedPackages.Add( MoveTemp( FFilePlatformRequest( PackageFilename, PlatformNames ) ) );
				}
			}
			CookByTheBookOptions->BasedOnReleaseCookedPackages.Add(PlatformName, MoveTemp(PackageList));
		}
	}
	
	TArray<FName> FilesInPath;

	if (!IsChildCooker())
	{
		// allow the game to fill out the asset registry, as well as get a list of objects to always cook
		TArray<FString> FilesInPathStrings;
		FGameDelegates::Get().GetCookModificationDelegate().ExecuteIfBound(FilesInPathStrings);

		for (const auto& FileString : FilesInPathStrings)
		{
			FilesInPath.Add(FName(*FileString));
		}
	}

	// don't resave the global shader map files in dlc
	if ( !IsCookingDLC() && !IsChildCooker())
	{
		SaveGlobalShaderMapFiles(TargetPlatforms);
	}
	
	
	CollectFilesToCook(FilesInPath, CookMaps, CookDirectories, CookCultures, IniMapSections, bCookAll, bMapsOnly, bNoDev );
	if (FilesInPath.Num() == 0)
	{
		LogCookerMessage( FString::Printf(TEXT("No files found to cook.")), EMessageSeverity::Warning );
		UE_LOG(LogCook, Warning, TEXT("No files found."));
	}

	{
#if OUTPUT_TIMING
		SCOPE_TIMER(GenerateLongPackageName);
#endif
		GenerateLongPackageNames(FilesInPath);
	}
	// add all the files for the requested platform to the cook list
	for ( const auto& FileFName : FilesInPath )
	{
		// FName FileFName = FName(*FileName);
		FString FileName = FileFName.ToString();
		FName PackageFileFName = GetCachedStandardPackageFileFName(FileFName);
		
		if (PackageFileFName != NAME_None)
		{
			CookRequests.EnqueueUnique( MoveTemp( FFilePlatformRequest( PackageFileFName, TargetPlatformNames ) ) );
		}
		else
		{
			LogCookerMessage( FString::Printf(TEXT("Unable to find package for cooking %s"), *FileName), EMessageSeverity::Warning );
			UE_LOG(LogCook, Warning, TEXT("Unable to find package for cooking %s"), *FileName)
		}	
	}


	if ( !IsCookingDLC() && !IsChildCooker() )
	{
		// if we are not cooking dlc then basedOfRelease version just needs to make sure that we cook all the packages which are in the previous release (as well as the new ones)
		if ( !BasedOnReleaseVersion.IsEmpty() )
		{
			// if we are based of a release and we are not cooking dlc then we should always be creating a new one (note that we could be creating the same one we are based of).
			// note that we might erroneously enter here if we are generating a patch instead and we accidentally passed in BasedOnReleaseVersion to the cooker instead of to unrealpak
			check( !CreateReleaseVersion.IsEmpty() );

			for ( const auto& PlatformName : TargetPlatformNames )
			{
				TArray<FName> PlatformArray;
				PlatformArray.Add( PlatformName );

				// if we are based of a cook and we are creating a new one we need to make sure that at least all the old packages are cooked as well as the new ones
				FString OriginalAssetRegistryPath = GetReleaseVersionAssetRegistryPath( BasedOnReleaseVersion, PlatformName ) / GetAssetRegistryFilename();

				TArray<FName> PackageFiles;
				verify( !GetAllPackagesFromAssetRegistry(OriginalAssetRegistryPath, PackageFiles) );

				for ( const auto& PackageFilename : PackageFiles )
				{
					CookRequests.EnqueueUnique( MoveTemp( FFilePlatformRequest( PackageFilename, PlatformArray) ) );
				}

			}

		}
	}


	// this is to support canceling cooks from the editor
	// this is required to make sure that the cooker is in a good state after cancel occurs
	// if too many packages are being recooked after resume then we may need to figure out a different way to do this
	for ( const auto& PreviousRequest : CookByTheBookOptions->PreviousCookRequests )
	{
		CookRequests.EnqueueUnique( MoveTemp( PreviousRequest ) );
	}
	CookByTheBookOptions->PreviousCookRequests.Empty();

	if (CookByTheBookStartupOptions.NumProcesses)
	{
		StartChildCookers(CookByTheBookStartupOptions.NumProcesses, TargetPlatformNames);
	}
}


bool UCookOnTheFlyServer::RecompileChangedShaders(const TArray<FName>& TargetPlatforms)
{
	bool bShadersRecompiled = false;
	for (const auto& TargetPlatform : TargetPlatforms)
	{
		bShadersRecompiled |= RecompileChangedShadersForPlatform(TargetPlatform.ToString());
	}
	return bShadersRecompiled;
}




class FChildCookerRunnable : public FRunnable
{
private:
	UCookOnTheFlyServer* CookServer;
	UCookOnTheFlyServer::FChildCooker* ChildCooker;

	void ProcessChildLogOutput()
	{
		// process the log output from the child cooker even if we just finished to ensure that we get the end of the log
		FString PipeContents = FPlatformProcess::ReadPipe(ChildCooker->ReadPipe);
		if (PipeContents.Len())
		{
			TArray<FString> PipeLines;
			PipeContents.ParseIntoArrayLines(PipeLines);

			for (const auto& Line : PipeLines)
			{
				UE_LOG(LogCook, Display, TEXT("Cooker output %s: %s"), *ChildCooker->BaseResponseFileName, *Line);
			}
		}
	}

public:
	FChildCookerRunnable(UCookOnTheFlyServer::FChildCooker* InChildCooker, UCookOnTheFlyServer* InCookServer) : FRunnable(), CookServer(InCookServer), ChildCooker(InChildCooker)
	{ }

	
	virtual uint32 Run() override
	{
		while (true)
		{
			check(ChildCooker != nullptr);
			check(ChildCooker->bFinished == false); 

			int32 ReturnCode;
			if (FPlatformProcess::GetProcReturnCode(ChildCooker->ProcessHandle, &ReturnCode))
			{
				if (ReturnCode != 0)
				{
					UE_LOG(LogCook, Error, TEXT("Child cooker %s returned error code %d"), *ChildCooker->BaseResponseFileName, ReturnCode);
				}
				else
				{
					UE_LOG(LogCook, Display, TEXT("Child cooker %s returned %d"), *ChildCooker->BaseResponseFileName, ReturnCode);
				}


				// if the child cooker completed successfully then it would have output a list of files which the main cooker needs to process
				const FString AdditionalPackagesFileName = GetChildCookerResultFilename(ChildCooker->ResponseFileName);

				FString AdditionalPackages;
				if (FFileHelper::LoadFileToString(AdditionalPackages, *AdditionalPackagesFileName) == false)
				{
					UE_LOG(LogCook, Fatal, TEXT("ChildCooker failed to write out additional packages file to location %s"), *AdditionalPackagesFileName);
				}
				TArray<FString> AdditionalPackageList;
				AdditionalPackages.ParseIntoArrayLines(AdditionalPackageList);

				for (const auto& AdditionalPackage : AdditionalPackageList)
				{
					UE_LOG(LogCook, Display, TEXT("Child cooker %s requested additional package %s to be cooked"), *ChildCooker->BaseResponseFileName, *AdditionalPackage);
					FName Filename = FName(*AdditionalPackage);
					CookServer->RequestPackage(Filename, false);
				}
				ProcessChildLogOutput();

				ChildCooker->ReturnCode = ReturnCode;
				ChildCooker->bFinished = true;
				ChildCooker = nullptr;
				return 1;
			}

			ProcessChildLogOutput();

			FPlatformProcess::Sleep(0.05f);
		}
		return 1;
	}

};

// sue chefs away!!
void UCookOnTheFlyServer::StartChildCookers(int32 NumCookersToSpawn, const TArray<FName>& TargetPlatformNames)
{
	SCOPE_TIMER(StartingChildCookers);
	// create a comprehensive list of all the files we need to cook
	// then get the packages with least dependencies and give them to some sue chefs to handle
	
	check(!IsChildCooker());

	// PackageNames contains a sorted list of the packages we want to distribute to child cookers
	TArray<FName> PackageNames;
	// PackageNamesSet is a set of the same information for quick lookup into packagenames array
	TSet<FName> PackageNamesSet;
	PackageNames.Empty(CookRequests.Num());

	// TArray<FFilePlatformRequest>;

	

	for (const auto& CookRequest : CookRequests.GetQueue())
	{
		FString LongPackageName = FPackageName::FilenameToLongPackageName(CookRequest.ToString());

		const FName PackageFName = FName(*LongPackageName);
		PackageNames.Add(PackageFName);
		PackageNamesSet.Add(PackageFName);
	}


	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();


	int32 PackageCounter = 0;
	while (PackageCounter < PackageNames.Num())
	{
		const auto& PackageName = PackageNames[PackageCounter];
		++PackageCounter;
		TArray<FName> UnfilteredDependencies;
		AssetRegistry.GetDependencies(PackageName, UnfilteredDependencies);

		for (const auto& Dependency : UnfilteredDependencies)
		{
			if (FPackageName::IsScriptPackage(Dependency.ToString()) == false)
			{
				if (PackageNamesSet.Contains(Dependency) == false)
				{
					PackageNamesSet.Add(Dependency);
					PackageNames.Insert(Dependency, PackageCounter);
				}
			}
		}
	}

	TArray<FName> DistributeStandardFilenames;
	for (const auto& DistributeCandidate : PackageNames)
	{
		FText OutReason;
		FString LongPackageName = DistributeCandidate.ToString();
		if (!FPackageName::IsValidLongPackageName(LongPackageName, true, &OutReason))
		{
			const FText FailMessage = FText::Format(LOCTEXT("UnableToGeneratePackageName", "Unable to generate long package name for {0}. {1}"),
				FText::FromString(LongPackageName), OutReason);

			LogCookerMessage(FailMessage.ToString(), EMessageSeverity::Warning);
			UE_LOG(LogCook, Warning, TEXT("%s"), *(FailMessage.ToString()));
			continue;
		}
		else if (FPackageName::IsScriptPackage(LongPackageName))
		{
			continue;
		}
		DistributeStandardFilenames.Add(FName(*LongPackageName));
	}

	UE_LOG(LogCook, Display, TEXT("Distributing %d packages to %d cookers for processing"), DistributeStandardFilenames.Num(), NumCookersToSpawn);

	FString TargetPlatformString;
	for (const auto& TargetPlatformName : TargetPlatformNames)
	{
		if (TargetPlatformString.Len() != 0)
		{
			TargetPlatformString += TEXT("+");
		}
		TargetPlatformString += TargetPlatformName.ToString();
	}

	// allocate the memory here, this can't change while we are running the child threads which are handling input because they have a ptr to the childcookers array
	CookByTheBookOptions->ChildCookers.Empty(NumCookersToSpawn);

	// start the child cookers and give them each some distribution candidates
	for (int32 CookerCounter = 0; CookerCounter < NumCookersToSpawn; ++CookerCounter )
	{
		// count our selves as a cooker
		int32 NumFilesForCooker = DistributeStandardFilenames.Num() / ((NumCookersToSpawn+1) - CookerCounter);

		// don't spawn a cooker unless it has a minimum amount of files to do
		if (NumFilesForCooker < 5)
		{
			continue;
		}


		int32 ChildCookerIndex = CookByTheBookOptions->ChildCookers.AddDefaulted(1);
		auto& ChildCooker = CookByTheBookOptions->ChildCookers[ChildCookerIndex];
		// starting child cookers
		//FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *CommandLine, false, true, true, NULL, 0, NULL, WritePipe);
		// ChildCooker.

		ChildCooker.ResponseFileName = FPaths::CreateTempFilename(*(FPaths::GameSavedDir() / TEXT("CookingTemp")));
		ChildCooker.BaseResponseFileName = FPaths::GetBaseFilename(ChildCooker.ResponseFileName);
		// FArchive* ResponseFile = IFileManager::CreateFileWriter(ChildCooker.UniqueTempName);
		FString ResponseFileText;



		for (int32 I = 0; I < NumFilesForCooker; ++I)
		{
			FName PackageFName = DistributeStandardFilenames[I];
			FString PackageName = PackageFName.ToString();

			ResponseFileText += FString::Printf(TEXT("%s%s"), *PackageName, LINE_TERMINATOR);

			// these are long package names
			FName StandardPackageName = GetCachedStandardPackageFileFName(PackageFName);
			if (StandardPackageName == NAME_None)
				continue;
#if 0 // validation code can be disabled
			PackageName = FPackageName::LongPackageNameToFilename(PackageName,FPaths::GetExtension(PackageName));
			FPaths::MakeStandardFilename(PackageName);
			FName PackageFName = FName(*PackageName);
			check(PackageFName== StandardPackageName);
#endif

			/*PackageName = FPackageName::LongPackageNameToFilename(PackageName, FPaths::GetExtension(PackageName));
			FPaths::MakeStandardFilename(PackageName);
			FName StandardPackageName = FName(*PackageName);*/
			
			CookedPackages.Add(FFilePlatformRequest(StandardPackageName, TargetPlatformNames));
		}
		DistributeStandardFilenames.RemoveAt(0, NumFilesForCooker);

		UE_LOG(LogCook, Display, TEXT("Child cooker %d working on %d files"), CookerCounter, NumFilesForCooker);

		FFileHelper::SaveStringToFile(ResponseFileText, *ChildCooker.ResponseFileName);

		// default commands
		// multiprocess tells unreal in general we shouldn't do things like save ddc, clean shader working directory, and other various multiprocess unsafe things
		FString CommandLine = FString::Printf(TEXT("\"%s\" -run=cook -multiprocess -targetplatform=%s -cookchild=\"%s\" -abslog=\"%sLog.txt\""), *FPaths::GetProjectFilePath(), *TargetPlatformString, *ChildCooker.ResponseFileName, *ChildCooker.ResponseFileName);

		auto KeepCommandlineValue = [&](const TCHAR* CommandlineToKeep)
		{
			FString CommandlineValue;
			if (FParse::Value(FCommandLine::Get(), CommandlineToKeep, CommandlineValue ))
			{
				CommandLine += TEXT(" -");
				CommandLine += CommandlineToKeep;
				CommandLine += TEXT("=");
				CommandLine += *CommandlineValue;
			}
		};

		auto KeepCommandlineParam = [&](const TCHAR* CommandlineToKeep)
		{
			if (FParse::Param(FCommandLine::Get(), CommandlineToKeep))
			{
				CommandLine += TEXT(" -");
				CommandLine += CommandlineToKeep;
			}
		};



		KeepCommandlineValue(TEXT("ddc="));
		KeepCommandlineParam(TEXT("SkipEditorContent"));
		KeepCommandlineParam(TEXT("compressed"));
		KeepCommandlineParam(TEXT("Unversioned"));
		KeepCommandlineParam(TEXT("buildmachine"));
		KeepCommandlineParam(TEXT("fileopenlog"));
		KeepCommandlineParam(TEXT("stdout"));
		KeepCommandlineParam(TEXT("FORCELOGFLUSH"));
		KeepCommandlineParam(TEXT("CrashForUAT"));
		KeepCommandlineParam(TEXT("AllowStdOutLogVerbosity"));
		KeepCommandlineParam(TEXT("UTF8Output"));

		FString ExecutablePath = FPlatformProcess::ExecutableName(true);

		UE_LOG(LogCook, Display, TEXT("Launching cooker using commandline %s %s"), *ExecutablePath, *CommandLine);

		void* ReadPipe = NULL;
		void* WritePipe = NULL;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

		ChildCooker.ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *CommandLine, false, true, true, NULL, 0, NULL, WritePipe);
		ChildCooker.ReadPipe = ReadPipe;

		// start threads to monitor output and finished state
		ChildCooker.Thread = FRunnableThread::Create(new FChildCookerRunnable(&ChildCooker, this), *FString::Printf(TEXT("ChildCookerInputHandleThreadFor:%s"), *ChildCooker.BaseResponseFileName));
	}

}



/* UCookOnTheFlyServer callbacks
 *****************************************************************************/

void UCookOnTheFlyServer::MaybeMarkPackageAsAlreadyLoaded(UPackage *Package)
{
	// can't use this optimization while cooking in editor
	check(IsCookingInEditor()==false);
	check(IsCookByTheBookMode());

	if (bIsInitializingSandbox)
	{
		return;
	}

	FName StandardName = GetCachedStandardPackageFileFName(Package);

	bool bShouldMarkAsAlreadyProcessed = false;

	TArray<FName> CookedPlatforms;
	if (CookedPackages.GetCookedPlatforms(StandardName, CookedPlatforms))
	{
		FString Platforms;
		for (const auto& CookedPlatform : CookedPlatforms)
		{
			Platforms += TEXT(" ");
			Platforms += CookedPlatform.ToString();
		}
		bShouldMarkAsAlreadyProcessed = true;
		UE_LOG(LogCook, Display, TEXT("Marking %s as reloading for cooker because it's been cooked for platforms%s."), *StandardName.ToString(), *Platforms);
	}

	check(IsInGameThread());
	if (NeverCookPackageList.Contains(StandardName))
	{
		bShouldMarkAsAlreadyProcessed = true;
		UE_LOG(LogCook, Display, TEXT("Marking %s as reloading for cooker because it was requested as never cook package."), *StandardName.ToString());
	}

	/*
	// if we are currently loading a package then the request would already be dequed so this check doesn't work
	// need to figure out a better way to do this 
	if (IsChildCooker())
	{
		// if we are the child cooker then we will never save anything that isn't in the cook requests
		if (CookRequests.Exists(StandardName) == false)
		{
			bShouldMarkAsAlreadyProcessed = true;
		}
	}
	*/

	if (bShouldMarkAsAlreadyProcessed)
	{
		if (Package->IsFullyLoaded() == false)
		{
			Package->PackageFlags |= PKG_ReloadingForCooker;
		}
	}

	/*FString Name = Package->GetName();


	if (PackagesToNotReload.Contains(Name))
	{
		UE_LOG(LogCookCommandlet, Verbose, TEXT("Marking %s already loaded."), *Name);
		Package->PackageFlags |= PKG_ReloadingForCooker;
	}*/
}


void UCookOnTheFlyServer::HandleNetworkFileServerFileRequest( const FString& Filename, const FString &Platformname, TArray<FString>& UnsolicitedFiles )
{
	check( IsCookOnTheFlyMode() );	
	
	bool bIsCookable = FPackageName::IsPackageExtension(*FPaths::GetExtension(Filename, true));


	FName PlatformFname = FName( *Platformname );

	if (!bIsCookable)
	{
		TArray<FName> UnsolicitedFilenames;
		UnsolicitedCookedPackages.GetPackagesForPlatformAndRemove(PlatformFname, UnsolicitedFilenames);

		for ( const auto &UnsolicitedFile : UnsolicitedFilenames )
		{	
			FString StandardFilename = UnsolicitedFile.ToString();
			FPaths::MakeStandardFilename( StandardFilename );
			UnsolicitedFiles.Add( StandardFilename );
		}
		UPackage::WaitForAsyncFileWrites();
		return;
	}

	FString StandardFileName = Filename;
	FPaths::MakeStandardFilename( StandardFileName );

	FName StandardFileFname = FName( *StandardFileName );
	TArray<FName> Platforms;
	Platforms.Add( PlatformFname );
	FFilePlatformRequest FileRequest( StandardFileFname, Platforms);
	CookRequests.EnqueueUnique(FileRequest, true);

	do
	{
		FPlatformProcess::Sleep(0.0f);
	}
	while (!CookedPackages.Exists(FileRequest));

	UE_LOG( LogCook, Display, TEXT("Cook complete %s"), *FileRequest.GetFilename().ToString())

	TArray<FName> UnsolicitedFilenames;
	UnsolicitedCookedPackages.GetPackagesForPlatformAndRemove(PlatformFname, UnsolicitedFilenames);
	UnsolicitedFilenames.Remove(FileRequest.GetFilename());

	for ( const auto &UnsolicitedFile : UnsolicitedFilenames )
	{	
		FString StandardFilename = UnsolicitedFile.ToString();
		FPaths::MakeStandardFilename( StandardFilename );
		UnsolicitedFiles.Add( StandardFilename );
	}

	UPackage::WaitForAsyncFileWrites();


#if DEBUG_COOKONTHEFLY
	UE_LOG( LogCook, Display, TEXT("Processed file request %s"), *Filename );
#endif

}


void UCookOnTheFlyServer::HandleNetworkFileServerRecompileShaders(const FShaderRecompileData& RecompileData)
{
	// shouldn't receive network requests unless we are in cook on the fly mode
	check( IsCookOnTheFlyMode() );
	check( !IsCookingDLC() );
	// if we aren't in the game thread, we need to push this over to the game thread and wait for it to finish
	if (!IsInGameThread())
	{
		UE_LOG(LogCook, Display, TEXT("Got a recompile request on non-game thread"));

		// make a new request
		FRecompileRequest* Request = new FRecompileRequest;
		Request->RecompileData = RecompileData;
		Request->bComplete = false;

		// push the request for the game thread to process
		RecompileRequests.Enqueue(Request);

		// wait for it to complete (the game thread will pull it out of the TArray, but I will delete it)
		while (!Request->bComplete)
		{
			FPlatformProcess::Sleep(0);
		}
		delete Request;
		UE_LOG(LogCook, Display, TEXT("Completed recompile..."));

		// at this point, we are done on the game thread, and ModifiedFiles will have been filled out
		return;
	}

	FString OutputDir = GetSandboxDirectory(RecompileData.PlatformName);

	RecompileShadersForRemote
		(RecompileData.PlatformName, 
		RecompileData.ShaderPlatform == -1 ? SP_NumPlatforms : (EShaderPlatform)RecompileData.ShaderPlatform,
		OutputDir, 
		RecompileData.MaterialsToLoad, 
		RecompileData.SerializedShaderResources, 
		RecompileData.MeshMaterialMaps, 
		RecompileData.ModifiedFiles,
		RecompileData.bCompileChangedShaders);
}

/*void UCookOnTheFlyServer::WarmCookedPackages(const FString& AssetRegistryPath, const TArray<FName>& TargetPlatformNames)
{
	FArrayReader SerializedAssetData;
	if (FFileHelper::LoadFileToArray(SerializedAssetData, *AssetRegistryPath))
	{
		int32 LocalNumAssets = 0;
		SerializedAssetData << LocalNumAssets;

		// allocate one single block for all asset data structs (to reduce tens of thousands of heap allocations)
		FAssetData* PreallocatedAssetDataBuffer = new FAssetData[LocalNumAssets];
		CookedPackages.Empty(LocalNumAssets);

		FFilePlatformRequest FileRequest;
		for (const auto &Platform : TargetPlatformNames)
		{
			FileRequest.AddPlatform(Platform);
		}

		for (int32 AssetIndex = 0; AssetIndex < LocalNumAssets; AssetIndex++)
		{
			// make a new asset data object
			FAssetData* NewAssetData = &PreallocatedAssetDataBuffer[AssetIndex];

			// load it
			SerializedAssetData << *NewAssetData;

			UE_LOG(LogCook, Verbose, TEXT("Read package %s from %s"), *GetCachedStandardPackageFilename(NewAssetData->ObjectPath), *AssetRegistryPath);

			FileRequest.SetFilename(GetCachedStandardPackageFilename(NewAssetData->ObjectPath));
			if (FileRequest.IsValid())
			{
				CookedPackages.Add(FileRequest);
			}
		}

		delete[] PreallocatedAssetDataBuffer;
	}
}*/


bool UCookOnTheFlyServer::GetAllPackagesFromAssetRegistry( const FString& AssetRegistryPath, TArray<FName>& OutPackageNames ) const
{

	static const FName AssetRegistryName("AssetRegistry");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();


	FArrayReader SerializedAssetData;
	if (FFileHelper::LoadFileToArray(SerializedAssetData, *AssetRegistryPath))
	{
		TMap<FName, FAssetData*> RegistryDataMap;
		AssetRegistry.LoadRegistryData(SerializedAssetData, RegistryDataMap);


		for (const auto& RegistryData : RegistryDataMap)
		{
			const auto& NewAssetData = RegistryData.Value;
			FName CachedPackageFileFName = GetCachedStandardPackageFileFName(NewAssetData->ObjectPath);
			if (CachedPackageFileFName != NAME_None)
			{
				OutPackageNames.Add(CachedPackageFileFName);
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Could not resolve package %s from %s"), *NewAssetData->ObjectPath.ToString(), *AssetRegistryPath);
			}
		}
		

		// clean up memory
		for (auto RegistryData : RegistryDataMap)
		{
			delete RegistryData.Value;
		}
		return true;
	}

	return false;
	/*


	FArrayReader SerializedAssetData;
	if (FFileHelper::LoadFileToArray(SerializedAssetData, *AssetRegistryPath))
	{


		int32 LocalNumAssets = 0;
		SerializedAssetData << LocalNumAssets;

		// allocate one single block for all asset data structs (to reduce tens of thousands of heap allocations)
		FAssetData* PreallocatedAssetDataBuffer = new FAssetData[LocalNumAssets];
		
		for (int32 AssetIndex = 0; AssetIndex < LocalNumAssets; AssetIndex++)
		{
			// make a new asset data object
			FAssetData* NewAssetData = &PreallocatedAssetDataBuffer[AssetIndex];

			// load it
			SerializedAssetData << *NewAssetData;

			UE_LOG(LogCook, Verbose, TEXT("Read package %s from %s"), *GetCachedStandardPackageFilename(NewAssetData->ObjectPath), *AssetRegistryPath);

			FName CachedPackageFileFName = GetCachedStandardPackageFileFName(NewAssetData->ObjectPath);
			if (CachedPackageFileFName != NAME_None)
			{
				OutPackageNames.Add(CachedPackageFileFName);
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Could not resolve package %s from %s"), *NewAssetData->ObjectPath.ToString(), *AssetRegistryPath);
			}
		}

		delete[] PreallocatedAssetDataBuffer;

		return true;
	}
	return false;*/

}

#undef LOCTEXT_NAMESPACE
