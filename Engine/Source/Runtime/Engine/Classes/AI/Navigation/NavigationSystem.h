// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/Navigation/NavigationData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GenericOctreePublic.h"
#include "NavigationSystem.generated.h"

#define NAVSYS_DEBUG (0 && UE_BUILD_DEBUG)

#define NAV_USE_MAIN_NAVIGATION_DATA NULL

class UNavigationPath;
class ANavigationData;
class UNavigationQueryFilter;
class UWorld;
class UCrowdManager;
struct FNavDataConfig;
struct FPathFindingResult;
class UActorComponent;
class AActor;
class UEnum;
class AController;
class UCrowdManager;
class UNavArea;
class INavLinkCustomInterface;
class INavRelevantInterface;
class FNavigationOctree;
class ANavMeshBoundsVolume;
class FNavDataGenerator;
class AWorldSettings;
#if WITH_EDITOR
class FEdMode;
#endif // WITH_EDITOR

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogNavigation, Warning, All);

/** delegate to let interested parties know that new nav area class has been registered */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNavAreaChanged, const UClass* /*AreaClass*/);

/** Delegate to let interested parties know that Nav Data has been registered */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavDataGenerigEvent, ANavigationData*, NavData);

namespace NavigationDebugDrawing
{
	extern const ENGINE_API float PathLineThickness;
	extern const ENGINE_API FVector PathOffset;
	extern const ENGINE_API FVector PathNodeBoxExtent;
}

namespace FNavigationSystem
{
	enum EMode
	{
		InvalidMode = -1,
		GameMode,
		EditorMode,
		SimulationMode,
		PIEMode,
	};
	
	/** 
	 * Used to construct an ANavigationData instance for specified navigation data agent 
	 */
	typedef ANavigationData* (*FNavigationDataInstanceCreator)(UWorld*, const FNavDataConfig&);
}

struct FNavigationSystemExec: public FSelfRegisteringExec
{
	// Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End FExec Interface
};

namespace ENavigationLockReason
{
	enum Type
	{
		Unknown					= 1 << 0,
		AllowUnregister			= 1 << 1,

		MaterialUpdate			= 1 << 2,
		LightingUpdate			= 1 << 3,
		ContinuousEditorMove	= 1 << 4,
		SpawnOnDragEnter		= 1 << 5,
	};
}

class ENGINE_API FNavigationLockContext
{
public:
	FNavigationLockContext(ENavigationLockReason::Type Reason = ENavigationLockReason::Unknown, bool bApplyLock = true) 
		: MyWorld(NULL), LockReason(Reason), bSingleWorld(false), bIsLocked(false)
	{
		if (bApplyLock)
		{
			LockUpdates();
		}
	}

	FNavigationLockContext(UWorld* InWorld, ENavigationLockReason::Type Reason = ENavigationLockReason::Unknown, bool bApplyLock = true)
		: MyWorld(InWorld), LockReason(Reason), bSingleWorld(true), bIsLocked(false)
	{
		if (bApplyLock)
		{
			LockUpdates();
		}
	}

	~FNavigationLockContext()
	{
		UnlockUpdates(); 
	}

private:
	UWorld* MyWorld;
	uint8 LockReason;
	uint8 bSingleWorld : 1;
	uint8 bIsLocked : 1;

	void LockUpdates();
	void UnlockUpdates();
};

UCLASS(Within=World, config=Engine, defaultconfig)
class ENGINE_API UNavigationSystem : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	virtual ~UNavigationSystem();

	UPROPERTY()
	ANavigationData* MainNavData;

	/** special navigation data for managing direct paths, not part of NavDataSet! */
	UPROPERTY()
	ANavigationData* AbstractNavData;

	/** Should navigation system spawn default Navigation Data when there's none and there are navigation bounds present? */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bAutoCreateNavigationData:1;

	/** Should navigation system rebuild navigation data at game/pie runtime? 
	 *	It also includes spawning navigation data instances if there are none present. */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bBuildNavigationAtRuntime:1;

	/** if set to true will result navigation system not rebuild navigation until 
	 *	a call to ReleaseInitialBuildingLock() is called. Does not influence 
	 *	editor-time generation (i.e. does influence PIE and Game).
	 *	Defaults to false.*/
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bInitialBuildingLocked:1;

	/** If set to true (default) navigation will be generated only within special navigation 
	 *	bounds volumes (like ANavMeshBoundsVolume). Set to false means navigation should be generated
	 *	everywhere.
	 */
	// @todo removing it from edition since it's currently broken and I'm not sure we want that at all
	// since I'm not sure we can make it efficient in a generic case
	//UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bWholeWorldNavigable:1;

	/** If set to true (default) generation seeds will include locations of all player controlled pawns */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bAddPlayersToGenerationSeeds:1;

	/** false by default, if set to true will result in not caring about nav agent height 
	 *	when trying to match navigation data to passed in nav agent */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bSkipAgentHeightCheckWhenPickingNavData:1;

	UPROPERTY(config, EditAnywhere, Category=Agents)
	TArray<FNavDataConfig> SupportedAgents;
	
	/** update frequency for dirty areas on navmesh */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	float DirtyAreasUpdateFreq;

	UPROPERTY()
	TArray<ANavigationData*> NavDataSet;

	UPROPERTY(transient)
	TArray<ANavigationData*> NavDataRegistrationQueue;

	TSet<FNavigationDirtyElement> PendingOctreeUpdates;

	// List of pending navigation bounds update requests (add, remove, update size)
	TArray<FNavigationBoundsUpdateRequest> PendingNavBoundsUpdates;

 	UPROPERTY(/*BlueprintAssignable, */Transient)
	FOnNavDataGenerigEvent OnNavDataRegisteredEvent;

	UPROPERTY(BlueprintAssignable, Transient, meta = (displayname = OnNavigationGenerationFinished))
	FOnNavDataGenerigEvent OnNavigationGenerationFinishedDelegate;
	
private:
	TWeakObjectPtr<UCrowdManager> CrowdManager;

	// required navigation data 
	UPROPERTY(config)
	TArray<FStringClassReference> RequiredNavigationDataClassNames;

	/** set to true when navigation processing was blocked due to missing nav bounds */
	uint32 bNavDataRemovedDueToMissingNavBounds:1;

	/** All areas where we build/have navigation */
	TSet<FNavigationBounds> RegisteredNavBounds;

public:
	//----------------------------------------------------------------------//
	// Blueprint functions
	//----------------------------------------------------------------------//

	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContext"))
	static UNavigationSystem* GetNavigationSystem(UObject* WorldContext);

	/** Project a point onto the NavigationData */
	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContext" ) )
	static FVector ProjectPointToNavigation(UObject* WorldContext, const FVector& Point, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);
	
	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContext" ) )
	static FVector GetRandomPoint(UObject* WorldContext, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContext" ) )
	static FVector GetRandomPointInRadius(UObject* WorldContext, const FVector& Origin, float Radius, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Potentially expensive. Use with caution. Consider using UPathFollowingComponent::GetRemainingPathCost instead */
	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContext" ) )
	static ENavigationQueryResult::Type GetPathCost(UObject* WorldContext, const FVector& PathStart, const FVector& PathEnd, float& PathCost, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Potentially expensive. Use with caution */
	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContext" ) )
	static ENavigationQueryResult::Type GetPathLength(UObject* WorldContext, const FVector& PathStart, const FVector& PathEnd, float& PathLength, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContext" ) )
	static bool IsNavigationBeingBuilt(UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	static void SimpleMoveToActor(AController* Controller, const AActor* Goal);

	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	static void SimpleMoveToLocation(AController* Controller, const FVector& Goal);

	/** Finds path instantly, in a FindPath Synchronously. 
	 *	@param PathfindingContext could be one of following: NavigationData (like Navmesh actor), Pawn or Controller. This parameter determines parameters of specific pathfinding query */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta = (HidePin = "WorldContext", DefaultToSelf = "WorldContext"))
	static UNavigationPath* FindPathToLocationSynchronously(UObject* WorldContext, const FVector& PathStart, const FVector& PathEnd, AActor* PathfindingContext = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Finds path instantly, in a FindPath Synchronously. Main advantage over FindPathToLocationSynchronously is that 
	 *	the resulting path with automatically get updated if goal actor moves more then TetherDistance away from last path node
	 *	@param PathfindingContext could be one of following: NavigationData (like Navmesh actor), Pawn or Controller. This parameter determines parameters of specific pathfinding query */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta = (HidePin = "WorldContext", DefaultToSelf = "WorldContext"))
	static UNavigationPath* FindPathToActorSynchronously(UObject* WorldContext, const FVector& PathStart, AActor* GoalActor, float TetherDistance = 50.f, AActor* PathfindingContext = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Performs navigation raycast on NavigationData appropriate for given Querier.
	 *	@param Querier if not passed default navigation data will be used
	 *	@param HitLocation if line was obstructed this will be set to hit location. Otherwise it contains SegmentEnd
	 *	@return true if line from RayStart to RayEnd was obstructed. Also, true when no navigation data present */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation", meta=(WorldContext="WorldContext" ))
	static bool NavigationRaycast(UObject* WorldContext, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL, AController* Querier = NULL);

	/** delegate type for events that dirty the navigation data ( Params: const FBox& DirtyBounds ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNavigationDirty, const FBox&);
	/** called after navigation influencing event takes place*/
	static FOnNavigationDirty NavigationDirtyEvent;

	enum ERegistrationResult
	{
		RegistrationError,
		RegistrationFailed_DataPendingKill,			// means navigation data being registered is marked as pending kill
		RegistrationFailed_AgentAlreadySupported,	// this means that navigation agent supported by given nav data is already handled by some other, previously registered instance
		RegistrationFailed_AgentNotValid,			// given instance contains navmesh that doesn't support any of expected agent types, or instance doesn't specify any agent
		RegistrationSuccessful,
	};

	enum EOctreeUpdateMode
	{
		OctreeUpdate_Default = 0,						// regular update, mark dirty areas depending on exported content
		OctreeUpdate_Geometry = 1,						// full update, mark dirty areas for geometry rebuild
		OctreeUpdate_Modifiers = 2,						// quick update, mark dirty areas for modifier rebuild
		OctreeUpdate_Refresh = 4,						// update is used for refresh, don't invalidate pending queue
		OctreeUpdate_ParentChain = 8,					// update child nodes, don't remove anything
	};

	enum ECleanupMode
	{
		CleanupWithWorld,
		CleanupUnsafe,
	};

	// Begin UObject Interface
	virtual void PostInitProperties() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject Interface

	virtual void Tick(float DeltaSeconds);	

	UWorld* GetWorld() const { return GetOuterUWorld(); }

	UCrowdManager* GetCrowdManager() const { return CrowdManager.Get(); }

	/** spawn new crowd manager */
	virtual void CreateCrowdManager();

	//----------------------------------------------------------------------//
	// Public querying interface                                                                
	//----------------------------------------------------------------------//
	/** 
	 *	Synchronously looks for a path from @fLocation to @EndLocation for agent with properties @AgentProperties. NavData actor appropriate for specified 
	 *	FNavAgentProperties will be found automatically
	 *	@param ResultPath results are put here
	 *	@param NavData optional navigation data that will be used instead of the one that would be deducted from AgentProperties
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 */
	FPathFindingResult FindPathSync(const FNavAgentProperties& AgentProperties, FPathFindingQuery Query, EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/** 
	 *	Does a simple path finding from @StartLocation to @EndLocation on specified NavData. If none passed MainNavData will be used
	 *	Result gets placed in ResultPath
	 *	@param NavData optional navigation data that will be used instead main navigation data
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 */
	FPathFindingResult FindPathSync(FPathFindingQuery Query, EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/** 
	 *	Asynchronously looks for a path from @StartLocation to @EndLocation for agent with properties @AgentProperties. NavData actor appropriate for specified 
	 *	FNavAgentProperties will be found automatically
	 *	@param ResultDelegate delegate that will be called once query has been processed and finished. Will be called even if query fails - in such case see comments for delegate's params
	 *	@param NavData optional navigation data that will be used instead of the one that would be deducted from AgentProperties
	 *	@param PathToFill if points to an actual navigation path instance than this instance will be filled with resulting path. Otherwise a new instance will be created and 
	 *		used in call to ResultDelegate
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 *	@return request ID
	 */
	uint32 FindPathAsync(const FNavAgentProperties& AgentProperties, FPathFindingQuery Query, const FNavPathQueryDelegate& ResultDelegate, EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/** Removes query indicated by given ID from queue of path finding requests to process. */
	void AbortAsyncFindPathRequest(uint32 AsynPathQueryID);
	
	/** 
	 *	Synchronously check if path between two points exists
	 *  Does not return path object, but will run faster (especially in hierarchical mode)
	 *  @param Mode switch between normal and hierarchical path finding algorithms. @note Hierarchical mode ignores QueryFilter
	 *	@return true if path exists
	 */
	bool TestPathSync(FPathFindingQuery Query, EPathFindingMode::Type Mode = EPathFindingMode::Regular, int32* NumVisitedNodes = NULL) const;

	/** Finds random point in navigable space
	 *	@param ResultLocation Found point is put here
	 *	@param NavData If NavData == NULL then MainNavData is used.
	 *	@return true if any location found, false otherwise */
	bool GetRandomPoint(FNavLocation& ResultLocation, ANavigationData* NavData = NULL, TSharedPtr<const FNavigationQueryFilter> QueryFilter = NULL);

	/** Finds random point in navigable space restricted to Radius around Origin
	 *	@param ResultLocation Found point is put here
	 *	@param NavData If NavData == NULL then MainNavData is used.
	 *	@return true if any location found, false otherwise */
	bool GetRandomPointInRadius(const FVector& Origin, float Radius, FNavLocation& ResultLocation, ANavigationData* NavData = NULL, TSharedPtr<const FNavigationQueryFilter> QueryFilter = NULL) const;

	/** Calculates a path from PathStart to PathEnd and retrieves its cost. 
	 *	@NOTE potentially expensive, so use it with caution */
	ENavigationQueryResult::Type GetPathCost(const FVector& PathStart, const FVector& PathEnd, float& PathCost, const ANavigationData* NavData = NULL, TSharedPtr<const FNavigationQueryFilter> QueryFilter = NULL) const;

	/** Calculates a path from PathStart to PathEnd and retrieves its overestimated length.
	 *	@NOTE potentially expensive, so use it with caution */
	ENavigationQueryResult::Type GetPathLength(const FVector& PathStart, const FVector& PathEnd, float& PathLength, const ANavigationData* NavData = NULL, TSharedPtr<const FNavigationQueryFilter> QueryFilter = NULL) const;

	/** Calculates a path from PathStart to PathEnd and retrieves its overestimated length and cost.
	 *	@NOTE potentially expensive, so use it with caution */
	ENavigationQueryResult::Type GetPathLengthAndCost(const FVector& PathStart, const FVector& PathEnd, float& PathLength, float& PathCost, const ANavigationData* NavData = NULL, TSharedPtr<const FNavigationQueryFilter> QueryFilter = NULL) const;

	// @todo document
	bool ProjectPointToNavigation(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent = INVALID_NAVEXTENT, const FNavAgentProperties* AgentProperties = NULL, TSharedPtr<const FNavigationQueryFilter> QueryFilter = NULL)
	{
		return ProjectPointToNavigation(Point, OutLocation, Extent, AgentProperties != NULL ? GetNavDataForProps(*AgentProperties) : GetMainNavData(FNavigationSystem::DontCreate), QueryFilter);
	}

	// @todo document
	bool ProjectPointToNavigation(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent = INVALID_NAVEXTENT, const ANavigationData* NavData = NULL, TSharedPtr<const FNavigationQueryFilter> QueryFilter = NULL) const;

	/** 
	 * Looks for NavData generated for specified movement properties and returns it. NULL if not found;
	 */
	ANavigationData* GetNavDataForProps(const FNavAgentProperties& AgentProperties);

	/** 
	 * Looks for NavData generated for specified movement properties and returns it. NULL if not found; Const version.
	 */
	const ANavigationData* GetNavDataForProps(const FNavAgentProperties& AgentProperties) const;

	/** Returns the world nav mesh object.  Creates one if it doesn't exist. */
	ANavigationData* GetMainNavData(FNavigationSystem::ECreateIfEmpty CreateNewIfNoneFound);
	/** Returns the world nav mesh object.  Creates one if it doesn't exist. */
	const ANavigationData* GetMainNavData() const { return MainNavData; }

	ANavigationData* GetAbstractNavData() const { return AbstractNavData; }

	TSharedPtr<FNavigationQueryFilter> CreateDefaultQueryFilterCopy() const;

	/** Super-hacky safety feature for threaded navmesh building. Will be gone once figure out why keeping TSharedPointer to Navigation Generator doesn't 
	 *	guarantee its existence */
	bool ShouldGeneratorRun(const FNavDataGenerator* Generator) const;

	bool IsNavigationBuilt(const AWorldSettings* Settings) const;

	bool IsThereAnywhereToBuildNavigation() const;

	bool ShouldGenerateNavigationEverywhere() const { return bWholeWorldNavigable; }

	FBox GetWorldBounds() const;
	
	FBox GetLevelBounds(ULevel* InLevel) const;

	bool IsNavigationRelevant(const AActor* TestActor) const;

	const TSet<FNavigationBounds>& GetNavigationBounds() const;

	/** @return default walkable area class */
	FORCEINLINE static TSubclassOf<UNavArea> GetDefaultWalkableArea() { return DefaultWalkableArea; }

	/** @return default obstacle area class */
	FORCEINLINE static TSubclassOf<UNavArea> GetDefaultObstacleArea() { return DefaultObstacleArea; }

	FORCEINLINE const FNavDataConfig& GetDefaultSupportedAgentConfig() const { check(SupportedAgents.Num() > 0);  return SupportedAgents[0]; }

	void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift);

	/** checks if navigation/navmesh is dirty and needs to be rebuilt */
	bool IsNavigationDirty() const;

	/** checks if dirty navigation data can rebuild itself */
	bool CanRebuildDirtyNavigation() const;

	static bool DoesPathIntersectBox(const FNavigationPath* Path, const FBox& Box, uint32 StartingIndex = 0);
	static bool DoesPathIntersectBox(const FNavigationPath* Path, const FBox& Box, const FVector& AgentLocation, uint32 StartingIndex = 0);


	//----------------------------------------------------------------------//
	// Bookkeeping 
	//----------------------------------------------------------------------//
	
	// @todo document
	void UnregisterNavData(ANavigationData* NavData);

	/** adds NavData to registration candidates queue - NavDataRegistrationQueue */
	void RequestRegistration(ANavigationData* NavData, bool bTriggerRegistrationProcessing = true);

protected:
	/** Processes registration of candidates queues via RequestRegistration and stored in NavDataRegistrationQueue */
	void ProcessRegistrationCandidates();

	/** registers NavArea classes awaiting registration in PendingNavAreaRegistration */
	void ProcessNavAreaPendingRegistration();

	/** used to apply updates of nav volumes in navigation system's tick */
	void PerformNavigationBoundsUpdate(const TArray<FNavigationBoundsUpdateRequest>& UpdateRequests);
	
	/** Searches for all valid navigation bounds in the world and stores them */
	void GatherNavigationBounds();

	/** @return pointer to ANavigationData instance of given ID, or NULL if it was not found. Note it looks only through registered navigation data */
	ANavigationData* GetNavDataWithID(const uint16 NavDataID) const;

public:
	void ReleaseInitialBuildingLock();

	//----------------------------------------------------------------------//
	// navigation octree related functions
	//----------------------------------------------------------------------//
	static void OnComponentRegistered(UActorComponent* Comp);
	static void OnComponentUnregistered(UActorComponent* Comp);
	static void OnActorRegistered(AActor* Actor);
	static void OnActorUnregistered(AActor* Actor);

	/** update navoctree entry for specified actor/component */
	static void UpdateNavOctree(AActor* Actor);
	static void UpdateNavOctree(UActorComponent* Comp);
	/** update all navoctree entries for actor and its components */
	static void UpdateNavOctreeAll(AActor* Actor);
	/** removes all navoctree entries for actor and its components */
	static void ClearNavOctreeAll(AActor* Actor);
	/** updates bounds of all components implementing INavRelevantInterface */
	static void UpdateNavOctreeBounds(AActor* Actor);

	void AddDirtyArea(const FBox& NewArea, int32 Flags);
	void AddDirtyAreas(const TArray<FBox>& NewAreas, int32 Flags);

	const FNavigationOctree* GetNavOctree() const { return NavOctree; }

	/** called to gather navigation relevant actors that have been created while
	 *	Navigation System was not present */
	void PopulateNavOctree();

	FORCEINLINE void SetObjectsNavOctreeId(UObject* Object, FOctreeElementId Id) { ObjectToOctreeId.Add(Object, Id); }
	FORCEINLINE const FOctreeElementId* GetObjectsNavOctreeId(const UObject* Object) const { return ObjectToOctreeId.Find(Object); }
	FORCEINLINE	void RemoveObjectsNavOctreeId(UObject* Object) { ObjectToOctreeId.Remove(Object); }
	void RemoveNavOctreeElementId(const FOctreeElementId& ElementId, int32 UpdateFlags);

	/** find all elements in navigation octree within given box (intersection) */
	void FindElementsInNavOctree(const FBox& QueryBox, const struct FNavigationOctreeFilter& Filter, TArray<struct FNavigationOctreeElement>& Elements);

	/** update single element in navoctree */
	void UpdateNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags);

	/** force updating parent node and all its children */
	void UpdateNavOctreeParentChain(UObject* ElementOwner);

	//----------------------------------------------------------------------//
	// Custom navigation links
	//----------------------------------------------------------------------//
	void RegisterCustomLink(INavLinkCustomInterface* CustomLink);
	void UnregisterCustomLink(INavLinkCustomInterface* CustomLink);
	
	/** find custom link by unique ID */
	INavLinkCustomInterface* GetCustomLink(uint32 UniqueLinkId) const;

	/** updates custom link for all active navigation data instances */
	void UpdateCustomLink(const INavLinkCustomInterface* CustomLink);

	//----------------------------------------------------------------------//
	// Areas
	//----------------------------------------------------------------------//
	static void RequestAreaRegistering(UClass* NavAreaClass);
	static void RequestAreaUnregistering(UClass* NavAreaClass);

	/** find index in SupportedAgents array for given navigation data */
	int32 GetSupportedAgentIndex(const ANavigationData* NavData) const;

	/** find index in SupportedAgents array for agent type */
	int32 GetSupportedAgentIndex(const FNavAgentProperties& NavAgent) const;

	//----------------------------------------------------------------------//
	// Filters
	//----------------------------------------------------------------------//
	
	/** prepare descriptions of navigation flags in UNavigationQueryFilter class: using enum */
	void DescribeFilterFlags(UEnum* FlagsEnum) const;

	/** prepare descriptions of navigation flags in UNavigationQueryFilter class: using array */
	void DescribeFilterFlags(const TArray<FString>& FlagsDesc) const;

	/** removes cached filters from currently registered navigation data */
	void ResetCachedFilter(TSubclassOf<UNavigationQueryFilter> FilterClass);

	//----------------------------------------------------------------------//
	// building
	//----------------------------------------------------------------------//
	
	/** Triggers navigation building on all eligible navigation data. */
	virtual void Build();

	// @todo document
	void OnPIEStart();
	// @todo document
	void OnPIEEnd();
	
	// @todo document
	FORCEINLINE bool IsNavigationBuildingLocked() const { return bNavigationBuildingLocked || bInitialBuildingLockActive; }

	// @todo document
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void OnNavigationBoundsUpdated(ANavMeshBoundsVolume* NavVolume);
	void OnNavigationBoundsAdded(ANavMeshBoundsVolume* NavVolume);
	void OnNavigationBoundsRemoved(ANavMeshBoundsVolume* NavVolume);

	/** Used to display "navigation building in progress" notify */
	bool IsNavigationBuildInProgress(bool bCheckDirtyToo = true);

	void OnNavigationGenerationFinished(ANavigationData& NavData);

	/** Used to display "navigation building in progress" counter */
	int32 GetNumRemainingBuildTasks() const;

	/** Number of currently running tasks */
	int32 GetNumRunningBuildTasks() const;

	/** Sets up SuportedAgents and NavigationDataCreators. Override it to add additional setup, but make sure to call Super implementation */
	virtual void DoInitialSetup();

	/** Called upon UWorld destruction to release what needs to be released */
	void CleanUp(ECleanupMode Mode = ECleanupMode::CleanupUnsafe);

	/** 
	 *	Called when owner-UWorld initializes actors
	 */
	virtual void OnInitializeActors();

	/** */
	virtual void OnWorldInitDone(FNavigationSystem::EMode Mode);

	/** adds BSP collisions of currently streamed in levels to octree */
	void InitializeLevelCollisions();

#if WITH_EDITOR
	/** allow editor to toggle whether seamless navigation building is enabled */
	static void SetNavigationAutoUpdateEnabled(bool bNewEnable, UNavigationSystem* InNavigationsystem);

	/** check whether seamless navigation building is enabled*/
	FORCEINLINE static bool GetIsNavigationAutoUpdateEnabled() { return bNavigationAutoUpdateEnabled; }

	FORCEINLINE bool IsNavigationRegisterLocked() const { return NavUpdateLockFlags != 0; }
	FORCEINLINE bool IsNavigationUnregisterLocked() const { return NavUpdateLockFlags && !(NavUpdateLockFlags & ENavigationLockReason::AllowUnregister); }
	FORCEINLINE bool IsNavigationUpdateLocked() const { return IsNavigationRegisterLocked(); }
	FORCEINLINE void AddNavigationUpdateLock(uint8 Flags) { NavUpdateLockFlags |= Flags; }
	FORCEINLINE void RemoveNavigationUpdateLock(uint8 Flags) { NavUpdateLockFlags &= ~Flags; }

	DEPRECATED(4.5, "AreFakeComponentChangesBeingApplied is deprecated. Use IsNavigationUpdateLocked instead.")
	bool AreFakeComponentChangesBeingApplied() { return IsNavigationUpdateLocked(); }
	
	DEPRECATED(4.5, "BeginFakeComponentChanges is deprecated. Use AddNavigationUpdateLock instead.")
	void BeginFakeComponentChanges() { AddNavigationUpdateLock(ENavigationLockReason::Unknown); }
	
	DEPRECATED(4.5, "EndFakeComponentChanges is deprecated. Use RemoveNavigationUpdateLock instead.")
	void EndFakeComponentChanges() { RemoveNavigationUpdateLock(ENavigationLockReason::Unknown); }

	void UpdateLevelCollision(ULevel* InLevel);

	virtual void OnEditorModeChanged(FEdMode* Mode, bool IsEntering);
#endif // WITH_EDITOR

	/** register actor important for generation (navigation data will be build around them first) */
	void RegisterGenerationSeed(AActor* SeedActor);
	void UnregisterGenerationSeed(AActor* SeedActor);
	void GetGenerationSeeds(TArray<FVector>& SeedLocations) const;

	static UNavigationSystem* CreateNavigationSystem(UWorld* WorldOwner);

	static UNavigationSystem* GetCurrent(UWorld* World);
	static UNavigationSystem* GetCurrent(UObject* WorldContextObject);
	
	/** try to create and setup navigation system */
	static void InitializeForWorld(UWorld* World, FNavigationSystem::EMode Mode);

	// Fetch the array of all nav-agent properties.
	void GetNavAgentPropertiesArray(TArray<FNavAgentProperties>& OutNavAgentProperties) const;

	static FORCEINLINE bool ShouldUpdateNavOctreeOnComponentChange()
	{
		return bUpdateNavOctreeOnComponentChange;
	}

	/** 
	 * Exec command handlers
	 */
	bool HandleCycleNavDrawnCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleCountNavMemCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	
	//----------------------------------------------------------------------//
	// debug
	//----------------------------------------------------------------------//
	void CycleNavigationDataDrawn();

protected:
#if WITH_EDITOR
	uint8 NavUpdateLockFlags;
#endif

	FNavigationSystem::EMode OperationMode;

	FNavigationOctree* NavOctree;

	TArray<FAsyncPathFindingQuery> AsyncPathFindingQueries;

	FCriticalSection NavDataRegistration;

	TMap<FNavAgentProperties, ANavigationData*> AgentToNavDataMap;
	
	TMap<const UObject*, FOctreeElementId> ObjectToOctreeId;

	/** Map of all objects that are tied to indexed navigation parent */
	TMultiMap<UObject*, FWeakObjectPtr> OctreeChildNodesMap;

	/** Map of all custom navigation links, that are relevant for path following */
	TMap<uint32, INavLinkCustomInterface*> CustomLinksMap;

	/** List of actors relevant to generation of navigation data */
	TArray<TWeakObjectPtr<AActor> > GenerationSeeds;

	/** stores areas marked as dirty throughout the frame, processes them 
	 *	once a frame in Tick function */
	TArray<FNavigationDirtyArea> DirtyAreas;

	// async queries
	FCriticalSection NavDataRegistrationSection;
	
	uint32 bNavigationBuildingLocked:1;
	uint32 bInitialBuildingLockActive:1;
	uint32 bInitialSetupHasBeenPerformed:1;
	uint32 bInitialLevelsAdded:1;
	uint32 bSkipDirtyAreasOnce:1;
	uint32 bAsyncBuildPaused:1;

	/** cached navigable world bounding box*/
	mutable FBox NavigableWorldBounds;

	/** indicates which of multiple navigation data instances to draw*/
	int32 CurrentlyDrawnNavDataIndex;

	/** temporary cumulative time to calculate when we need to update dirty areas */
	float DirtyAreasUpdateTime;

#if !UE_BUILD_SHIPPING
	/** self-registering exec command to handle nav sys console commands */
	static FNavigationSystemExec ExecHandler;
#endif // !UE_BUILD_SHIPPING

	/** collection of delegates to create all available navigation data types */
	static TArray<TSubclassOf<ANavigationData> > NavDataClasses;
	
	/** whether seamless navigation building is enabled */
	static bool bNavigationAutoUpdateEnabled;

	static bool bUpdateNavOctreeOnComponentChange;

	static TArray<UClass*> PendingNavAreaRegistration;
	static TArray<const UClass*> NavAreaClasses;
	static TSubclassOf<UNavArea> DefaultWalkableArea;
	static TSubclassOf<UNavArea> DefaultObstacleArea;

	/** delegate handler for PostLoadMap event */
	void OnPostLoadMap();
#if WITH_EDITOR
	/** delegate handler for ActorMoved events */
	void OnActorMoved(AActor* Actor);
#endif
	/** delegate handler called when navigation is dirtied*/
	void OnNavigationDirtied(const FBox& Bounds);
	
	/** Registers given navigation data with this Navigation System.
	 *	@return RegistrationSuccessful if registration was successful, other results mean it failed
	 *	@see ERegistrationResult
	 */
	ERegistrationResult RegisterNavData(ANavigationData* NavData);

	/** tries to register navigation area */
	void RegisterNavAreaClass(UClass* NavAreaClass);

	void OnNavigationAreaEvent(UClass* AreaClass, ENavAreaEvent::Type Event);
	
 	FSetElementId RegisterNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags);
	void UnregisterNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags);
	
	/** read element data from navigation octree */
	bool GetNavOctreeElementData(UObject* NodeOwner, int32& DirtyFlags, FBox& DirtyBounds);

	/** Adds given element to NavOctree. No check for owner's validity are performed, 
	 *	nor its presence in NavOctree - function assumes callee responsibility 
	 *	in this regard **/
	void AddElementToNavOctree(const FNavigationDirtyElement& DirtyElement);

	void SetCrowdManager(UCrowdManager* NewCrowdManager); 

	/** Add BSP collision data to navigation octree */
	void AddLevelCollisionToOctree(ULevel* Level);
	
	/** Remove BSP collision data from navigation octree */
	void RemoveLevelCollisionFromOctree(ULevel* Level);

	/** registers or unregisters all generators from rebuilding callbacks */
	void EnableAllGenerators(bool bEnable, bool bForce = false);

private:
	// @todo document
	void NavigationBuildingLock();
	// @todo document
	void NavigationBuildingUnlock(bool bForce = false);
	// adds navigation bounds update request to a pending list
	void AddNavigationBoundsUpdateRequest(const FNavigationBoundsUpdateRequest& UpdateRequest);

	void SpawnMissingNavigationData();

	/** constructs a navigation data instance of specified NavDataClass, in passed World
	 *	for supplied NavConfig */
	virtual ANavigationData* CreateNavigationDataInstance(const FNavDataConfig& NavConfig);

	/** Triggers navigation building on all eligible navigation data. */
	void RebuildAll();
		 
	/** Handler for FWorldDelegates::LevelAddedToWorld event */
	 void OnLevelAddedToWorld(ULevel* InLevel, UWorld* InWorld);
	 
	/** Handler for FWorldDelegates::LevelRemovedFromWorld event */
	void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld);

	/** Adds given request to requests queue. Note it's to be called only on game thread only */
	void AddAsyncQuery(const FAsyncPathFindingQuery& Query);
		 
	/** spawns a non-game-thread task to process requests given in PathFindingQueries.
	 *	In the process PathFindingQueries gets copied. */
	void TriggerAsyncQueries(TArray<FAsyncPathFindingQuery>& PathFindingQueries);

	/** Processes pathfinding requests given in PathFindingQueries.*/
	void PerformAsyncQueries(TArray<FAsyncPathFindingQuery> PathFindingQueries);
};

