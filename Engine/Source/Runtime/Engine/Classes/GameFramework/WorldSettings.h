// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameFramework/Info.h"
#include "Sound/AudioVolume.h"
#include "WorldSettings.generated.h"

UENUM()
enum EVisibilityAggressiveness
{
	VIS_LeastAggressive,
	VIS_ModeratelyAggressive,
	VIS_MostAggressive,
	VIS_Max,
};

/** Helper structure, used to associate GameModes for a map via its filename prefix. */
USTRUCT()
struct FGameModePrefix
{
	GENERATED_USTRUCT_BODY()

	/** map prefix, e.g. "DM" */
	UPROPERTY()
	FString Prefix;

	/** GameMode used if none specified on the URL */
	UPROPERTY()
	FString GameMode;
};

USTRUCT()
struct FLightmassWorldInfoSettings
{
	GENERATED_USTRUCT_BODY()

	/** 
	 * Warning: Setting this to less than 1 will greatly increase build times!
	 * Scale of the level relative to real world scale (1 Unreal Unit = 1 cm). 
	 * All scale-dependent Lightmass setting defaults have been tweaked to work well with real world scale, 
	 * Any levels with a different scale should use this scale to compensate. 
	 * For large levels it can drastically reduce build times to set this to 2 or 4.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassGeneral, AdvancedDisplay, meta=(UIMin = "1.0", UIMax = "4.0"))
	float StaticLightingLevelScale;

	/** 
	 * Number of times light is allowed to bounce off of surfaces, starting from the light source. 
	 * 0 is direct lighting only, 1 is one bounce, etc. 
	 * Bounce 1 takes the most time to calculate and contributes the most to visual quality, followed by bounce 2.
	 * Successive bounces don't really affect build times, but have a much lower visual impact.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassGeneral, meta=(UIMin = "1.0", UIMax = "4.0"))
	int32 NumIndirectLightingBounces;

	/** 
	 * Warning: Setting this higher than 1 will greatly increase build times!
	 * Can be used to increase the GI solver sample counts in order to get higher quality for levels that need it.
	 * It can be useful to reduce IndirectLightingSmoothness somewhat (~.75) when increasing quality to get defined indirect shadows.
	 * Note that this can't affect compression artifacts or other texture based artifacts.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassGeneral, AdvancedDisplay, meta=(UIMin = "1.0", UIMax = "4.0"))
	float IndirectLightingQuality;

	/** 
	 * Smoothness factor to apply to indirect lighting.  This is useful in some lighting conditions when Lightmass cannot resolve accurate indirect lighting.
	 * 1 is default smoothness tweaked for a variety of lighting situations.
	 * Higher values like 3 smooth out the indirect lighting more, but at the cost of indirect shadows losing detail.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassGeneral, AdvancedDisplay, meta=(UIMin = "0.5", UIMax = "6.0"))
	float IndirectLightingSmoothness;

	/** 
	 * Represents a constant color light surrounding the upper hemisphere of the level, like a sky.
	 * This light source currently does not get bounced as indirect lighting.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassGeneral)
	FColor EnvironmentColor;

	/** Scales EnvironmentColor to allow independent color and brightness controls. */
	UPROPERTY(EditAnywhere, Category=LightmassGeneral, meta=(UIMin = "0", UIMax = "10"))
	float EnvironmentIntensity;

	/** Scales the emissive contribution of all materials in the scene.  Currently disabled and should be removed with mesh area lights. */
	UPROPERTY()
	float EmissiveBoost;

	/** Scales the diffuse contribution of all materials in the scene. */
	UPROPERTY(EditAnywhere, Category=LightmassGeneral, meta=(UIMin = "0.1", UIMax = "6.0"))
	float DiffuseBoost;

	/** If true, AmbientOcclusion will be enabled. */
	UPROPERTY(EditAnywhere, Category=LightmassOcclusion)
	uint32 bUseAmbientOcclusion:1;

	/** How much of the AO to apply to direct lighting. */
	UPROPERTY(EditAnywhere, Category=LightmassOcclusion, meta=(UIMin = "0", UIMax = "1"))
	float DirectIlluminationOcclusionFraction;

	/** How much of the AO to apply to indirect lighting. */
	UPROPERTY(EditAnywhere, Category=LightmassOcclusion, meta=(UIMin = "0", UIMax = "1"))
	float IndirectIlluminationOcclusionFraction;

	/** Higher exponents increase contrast. */
	UPROPERTY(EditAnywhere, Category=LightmassOcclusion, meta=(UIMin = ".5", UIMax = "8"))
	float OcclusionExponent;

	/** Fraction of samples taken that must be occluded in order to reach full occlusion. */
	UPROPERTY(EditAnywhere, Category=LightmassOcclusion, meta=(UIMin = "0", UIMax = "1"))
	float FullyOccludedSamplesFraction;

	/** Maximum distance for an object to cause occlusion on another object. */
	UPROPERTY(EditAnywhere, Category=LightmassOcclusion)
	float MaxOcclusionDistance;

	/** If true, override normal direct and indirect lighting with just the exported diffuse term. */
	UPROPERTY(EditAnywhere, Category=LightmassDebug, AdvancedDisplay)
	uint32 bVisualizeMaterialDiffuse:1;

	/** If true, override normal direct and indirect lighting with just the AO term. */
	UPROPERTY(EditAnywhere, Category=LightmassDebug, AdvancedDisplay)
	uint32 bVisualizeAmbientOcclusion:1;

	/** 
	 * Scales the distances at which volume lighting samples are placed.  Volume lighting samples are computed by Lightmass and are used for GI on movable components.
	 * Using larger scales results in less sample memory usage and reduces Indirect Lighting Cache update times.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassGeneral, AdvancedDisplay, meta=(UIMin = "0.1", UIMax = "100.0"))
	float VolumeLightSamplePlacementScale;

	/** 
	 * Whether to compress lightmap textures.  Disabling lightmap texture compression will reduce artifacts but increase memory and disk size by 4x.
	 * Use caution when disabling this.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassGeneral, AdvancedDisplay)
	uint32 bCompressLightmaps:1;

	FLightmassWorldInfoSettings()
		: StaticLightingLevelScale(1)
		, NumIndirectLightingBounces(3)
		, IndirectLightingQuality(1)
		, IndirectLightingSmoothness(1)
		, EnvironmentColor(ForceInit)
		, EnvironmentIntensity(1.0f)
		, EmissiveBoost(1.0f)
		, DiffuseBoost(1.0f)
		, bUseAmbientOcclusion(false)
		, DirectIlluminationOcclusionFraction(0.5f)
		, IndirectIlluminationOcclusionFraction(1.0f)
		, OcclusionExponent(1.0f)
		, FullyOccludedSamplesFraction(1.0f)
		, MaxOcclusionDistance(200.0f)
		, bVisualizeMaterialDiffuse(false)
		, bVisualizeAmbientOcclusion(false)
		, VolumeLightSamplePlacementScale(1)
		, bCompressLightmaps(true)
	{
	}
};

/** stores information on a viewer that actors need to be checked against for relevancy */
USTRUCT()
struct ENGINE_API FNetViewer
{
	GENERATED_USTRUCT_BODY()

	/** The "controlling net object" associated with this view (typically player controller) */
	UPROPERTY()
	class AActor* InViewer;

	/** The actor that is being directly viewed, usually a pawn.  Could also be the net actor of consequence */
	UPROPERTY()
	class AActor* ViewTarget;

	/** Where the viewer is looking from */
	UPROPERTY()
	FVector ViewLocation;

	/** Direction the viewer is looking */
	UPROPERTY()
	FVector ViewDir;

	FNetViewer()
		: InViewer(NULL)
		, ViewTarget(NULL)
		, ViewLocation(ForceInit)
		, ViewDir(ForceInit)
	{
	}

	FNetViewer(UNetConnection* InConnection, float DeltaSeconds);
};


USTRUCT()
struct ENGINE_API FHierarchicalSimplification
{
	GENERATED_USTRUCT_BODY()

	/** If this is true, it will simplify mesh. However it is slower. If false, it will just merge actors but not simplify */
	UPROPERTY(Category=FHierarchicalSimplification, EditAnywhere)
	bool bSimplifyMesh;

	/** What are the draw distance for this LOD */
	UPROPERTY(Category=FHierarchicalSimplification, EditAnywhere, AdvancedDisplay, meta=(UIMin=10.f, ClampMin=10.f))
	float DrawDistance;

	/** Desired Bounding Box Size for clustering */
	UPROPERTY(EditAnywhere, Category=FHierarchicalSimplification, AdvancedDisplay, meta=(UIMin=10.f, ClampMin=10.f))
	float DesiredBoundBoxSize;

	/** Desired Filling Percentage for clustering */
	UPROPERTY(EditAnywhere, Category=FHierarchicalSimplification, AdvancedDisplay, meta=(ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
	float DesiredFillingPercentage;

	/** Simplification Setting if bSimplifyMesh is true */
	UPROPERTY(Category=FHierarchicalSimplification, EditAnywhere, AdvancedDisplay, meta=(editcondition = "bSimplifyMesh"))
	FMeshProxySettings Setting;

	FHierarchicalSimplification()
		: bSimplifyMesh(true)
		, DrawDistance(5000)
		, DesiredBoundBoxSize(10000)
		, DesiredFillingPercentage(30)
	{
		Setting.ScreenSize = 300;
	}
};

/**
 * Actor containing all script accessible world properties.
 */
UCLASS(config=game, hidecategories=(Actor, Advanced, Display, Events, Object, Attachment, Info, Input, Blueprint, Layers, Tags, Replication), showcategories=("Input|MouseInput", "Input|TouchInput"), notplaceable)
class ENGINE_API AWorldSettings : public AInfo, public IInterface_AssetUserData
{
	GENERATED_UCLASS_BODY()

	/** DEFAULT BASIC PHYSICS SETTINGS **/

	/** If true, enables CheckStillInWorld checks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=World, AdvancedDisplay)
	uint32 bEnableWorldBoundsChecks:1;

	/** if set to false navigation system will not get created (and all navigation functionality won't be accessible)*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category=World, AdvancedDisplay)
	uint32 bEnableNavigationSystem:1;

#if WITH_EDITORONLY_DATA
	/** if set to true, hierarchical LODs will be built, which will create hierarchical LODActors*/
	UPROPERTY(EditAnywhere, config, Category=World, AdvancedDisplay)
	uint32 bEnableHierarchicalLODSystem:1;
#endif
	/** 
	 * Enables tools for composing a tiled world. 
	 * Level has to be saved and all sub-levels removed before enabling this option.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=World)
	uint32 bEnableWorldComposition:1;

	/** World origin will shift to a camera position when camera goes far away from current origin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=World, AdvancedDisplay, meta=(editcondition = "bEnableWorldComposition"))
	uint32 bEnableWorldOriginRebasing:1;
		
	/** if set to true, when we call GetGravityZ we assume WorldGravityZ has already been initialized and skip the lookup of DefaultGravityZ and GlobalGravityZ */
	UPROPERTY(transient)
	uint32 bWorldGravitySet:1;

	/** If set to true we will use GlobalGravityZ instead of project setting DefaultGravityZ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName = "Override World Gravity"), Category = Physics)
	uint32 bGlobalGravitySet:1;

	// any actor falling below this level gets destroyed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=World, meta=(editcondition = "bEnableWorldBoundsChecks"))
	float KillZ;

	// The type of damage inflicted when a actor falls below KillZ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=World, AdvancedDisplay, meta=(editcondition = "bEnableWorldBoundsChecks"))
	TSubclassOf<UDamageType> KillZDamageType;

	// current gravity actually being used
	UPROPERTY(transient, replicated)
	float WorldGravityZ;

	// optional level specific gravity override set by level designer
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Physics, meta=(editcondition = "bGlobalGravitySet"))
	float GlobalGravityZ;

	// optional level specific collision handler
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Physics, AdvancedDisplay)
	TSubclassOf<class UPhysicsCollisionHandler>	PhysicsCollisionHandlerClass;

	/************************************/
	
	/** GAMEMODE SETTINGS **/
	
	/** The default GameMode to use when starting this map in the game. If this value is NULL, the INI setting for default game type is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GameMode, meta=(DisplayName="GameMode Override"))
	TSubclassOf<class AGameMode> DefaultGameMode;

	/** Used for loading appropriate game type if non-specified in URL */
	UPROPERTY(config)
	TArray<struct FGameModePrefix> DefaultMapPrefixes;

	/** Class of GameNetworkManager to spawn for network games */
	UPROPERTY()
	TSubclassOf<class AGameNetworkManager> GameNetworkManagerClass;

	/************************************/
	
	/** RENDERING SETTINGS **/
	/** Maximum size of textures for packed light and shadow maps */
	UPROPERTY(EditAnywhere, Category=Lightmass, AdvancedDisplay)
	int32 PackedLightAndShadowMapTextureSize;

	/** 
	 * Causes the BSP build to generate as few sections as possible.
	 * This is useful when you need to reduce draw calls but can reduce texture streaming efficiency and effective lightmap resolution.
	 * Note - changes require a rebuild to propagate.  Also, be sure to select all surfaces and make sure they all have the same flags to minimize section count.
	 */
	UPROPERTY(EditAnywhere, Category=World, AdvancedDisplay)
	uint32 bMinimizeBSPSections:1;

	/** Default color scale for the level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=World, AdvancedDisplay)
	FVector DefaultColorScale;

	/************************************/
	
	/** PRECOMPUTED VISIBILITY SETTINGS **/
	/** 
	 * Whether to place visibility cells inside Precomputed Visibility Volumes and along camera tracks in this level. 
	 * Precomputing visibility reduces rendering thread time at the cost of some runtime memory and somewhat increased lighting build times.
	 */
	UPROPERTY(EditAnywhere, Category=PrecomputedVisibility)
	uint32 bPrecomputeVisibility:1;

	/** 
	 * Whether to place visibility cells only along camera tracks or only above shadow casting surfaces.
	 */
	UPROPERTY(EditAnywhere, Category=PrecomputedVisibility, AdvancedDisplay)
	uint32 bPlaceCellsOnlyAlongCameraTracks:1;

	/** 
	 * World space size of precomputed visibility cells in x and y.
	 * Smaller sizes produce more effective occlusion culling at the cost of increased runtime memory usage and lighting build times.
	 */
	UPROPERTY(EditAnywhere, Category=PrecomputedVisibility, AdvancedDisplay)
	int32 VisibilityCellSize;

	/** 
	 * Determines how aggressive precomputed visibility should be.
	 * More aggressive settings cull more objects but also cause more visibility errors like popping.
	 */
	UPROPERTY(EditAnywhere, Category=PrecomputedVisibility, AdvancedDisplay)
	TEnumAsByte<enum EVisibilityAggressiveness> VisibilityAggressiveness;

	/************************************/
	
	/** LIGHTMASS RELATED SETTINGS **/
	
	/** 
	 * Whether to force lightmaps and other precomputed lighting to not be created even when the engine thinks they are needed.
	 * This is useful for improving iteration in levels with fully dynamic lighting and shadowing.
	 * Note that any lighting and shadowing interactions that are usually precomputed will be lost if this is enabled.
	 */
	UPROPERTY(EditAnywhere, Category=Lightmass, AdvancedDisplay)
	uint32 bForceNoPrecomputedLighting:1;

	UPROPERTY(EditAnywhere, Category=Lightmass)
	struct FLightmassWorldInfoSettings LightmassSettings;

	/** The lighting quality the level was last built with */
	UPROPERTY(Category=Lightmass, VisibleAnywhere)
	TEnumAsByte<enum ELightingBuildQuality> LevelLightingQuality;

	/************************************/
	/** AUDIO SETTINGS **/
	/** Default reverb settings used by audio volumes.													*/
	UPROPERTY(EditAnywhere, config, Category=Audio)
	struct FReverbSettings DefaultReverbSettings;

	/** Default interior settings used by audio volumes.												*/
	UPROPERTY(EditAnywhere, config, Category=Audio)
	struct FInteriorSettings DefaultAmbientZoneSettings;

	/** Default Base SoundMix.																			*/
	UPROPERTY(EditAnywhere, Category=Audio)
	class USoundMix* DefaultBaseSoundMix;

#if WITH_EDITORONLY_DATA
	/** Hierarchical LOD Setup */
	UPROPERTY(EditAnywhere, Category=LODSystem, AdvancedDisplay, meta=(editcondition = "bEnableHierarchicalLODSystem"))
	TArray<struct FHierarchicalSimplification>	HierarchicalLODSetup;
#endif
	/************************************/
	/** DEFAULT SETTINGS **/

	/** scale of 1uu to 1m in real world measurements, for HMD and other physically tracked devices (e.g. 1uu = 1cm would be 100.0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=VR)
	float WorldToMeters;

	/************************************/
	/** EDITOR ONLY SETTINGS **/
	
	/** Level Bookmarks: 10 should be MAX_BOOKMARK_NUMBER @fixmeconst */
	UPROPERTY()
	class UBookMark* BookMarks[10];

	/************************************/

	/** 
	 * Normally 1 - scales real time passage.
	 * Warning - most use cases should use GetEffectiveTimeDilation() instead of reading from this directly
	 */
	UPROPERTY(replicated)
	float TimeDilation;

	// additional TimeDilation used by Matinee slomo track
	UPROPERTY(replicated)
	float MatineeTimeDilation;

	// Additional TimeDilation used to control demo playback speed
	UPROPERTY()
	float DemoPlayTimeDilation;

	// If paused, FName of person pausing the game.
	UPROPERTY(replicated)
	class APlayerState* Pauser;

	/** when this flag is set, more time is allocated to background loading (replicated) */
	UPROPERTY(replicated)
	uint32 bHighPriorityLoading:1;

	/** copy of bHighPriorityLoading that is not replicated, for clientside-only loading operations */
	UPROPERTY()
	uint32 bHighPriorityLoadingLocal:1;

	/** valid only during replication - information about the player(s) being replicated to
	 * (there could be more than one in the case of a splitscreen client)
	 */
	UPROPERTY()
	TArray<struct FNetViewer> ReplicationViewers;

	// ************************************

	/** Maximum number of bookmarks	*/
	static const int32 MAX_BOOKMARK_NUMBER = 10;

	protected:

	/** Array of user data stored with the asset */
	UPROPERTY()
	TArray<UAssetUserData*> AssetUserData;

public:
	// Begin UObject interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject interface.


	// Begin AActor interface.
#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif
	virtual void PreInitializeComponents() override;
	virtual void PostInitializeComponents() override;
	// End AActor interface.

	/**
	 * Returns the Z component of the current world gravity and initializes it to the default
	 * gravity if called for the first time.
	 *
	 * @return Z component of current world gravity.
	 */
	virtual float GetGravityZ() const;

	float GetEffectiveTimeDilation() const
	{
		return TimeDilation * MatineeTimeDilation * DemoPlayTimeDilation;
	}

	/** 
	 * Called by GameMode.HandleMatchIsWaitingToStart, calls BeginPlay on all actors
	 */
	virtual void NotifyBeginPlay();

	/** 
	 * Called by GameMode.HandleMatchHasStarted, used to notify native classes of match startup (such as level scripting)
	 */	
	virtual void NotifyMatchStarted();

	// Begin IInterface_AssetUserData Interface
	virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	// End IInterface_AssetUserData Interface


private:

	// Hidden functions that don't make sense to use on this class.
	HIDE_ACTOR_TRANSFORM_FUNCTIONS();

	virtual void Serialize( FArchive& Ar ) override;
};

