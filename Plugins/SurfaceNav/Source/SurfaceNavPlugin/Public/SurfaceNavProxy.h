// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

//#include "Debug/DebugDrawComponent.h"

#include "GameFramework/Actor.h"
#include "SurfaceNavCommon.h"
#include "SurfaceNavProxy.generated.h"


#pragma region Draw Degug String

// *************************************************************************
// Leave here as this can give useful information when enabled. The problem is
// it's doesn't compile in all modes...may just need the right conditional compilation
// *************************************************************************
//struct FDebugSceneProxyData
//{
//	struct FDebugText
//	{
//		FVector Location;
//		FString Text;
//
//		FDebugText() {}
//		FDebugText(const FVector& InLocation, const FString& InText) : Location(InLocation), Text(InText) {}
//	};
//
//	TArray<FDebugText> DebugLabels;
//};
//
//class FDebugSceneProxy : public FDebugRenderSceneProxy
//{
//public:
//	//FDebugSceneProxy(const UPrimitiveComponent* InComponent, FDebugSceneProxyData* ProxyData);
//	FDebugSceneProxy(const UPrimitiveComponent* InComponent);
//
//	FDebugSceneProxyData ProxyData;
//};
//
//class FDebugTextDelegateHelper : public FDebugDrawDelegateHelper
//{
//public:
//	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController*) override;
//
//	//void SetupFromProxy(const FDebugSceneProxy* InSceneProxy);
//	void SetupFromProxy(const FDebugSceneProxyData& InProxyData);
//	
//	TArray<FDebugSceneProxyData::FDebugText> DebugLabels;
//};
//
//
//
//
//
//UCLASS(ClassGroup = Custom, meta = (BlueprintSpawnableComponent))
//class UDebugStrings : public UDebugDrawComponent
//{
//	GENERATED_BODY()
//
//
//public:
//	UDebugStrings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
//
//	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
//
//#if UE_ENABLE_DEBUG_DRAWING
//	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return DebugDrawDelegateManager; }
//	void RefreshDebugSceneProxy(TArray<FDebugSceneProxyData::FDebugText>& DebugTextData);
//	FDebugTextDelegateHelper DebugDrawDelegateManager;
//#endif
//
//	//virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
//};


#pragma endregion



/** This actor is required to be in your level for the DSN plugin to work as it exposes
parameters to control the navigation system. If you're using level streaming, then add
this to your persistent level only. Having multiple of these actors loaded at once is
not supported and could lead to undefined behavior. */
UCLASS(AutoCollapseCategories = "SurfaceNav|General,SurfaceNav|Block Config,SurfaceNav|AI,SurfaceNav|Advanced,SurfaceNav|Debug,SurfaceNav|Path Finding",
	hideCategories = "Replication, Rendering, Collision, HLOD, Input, Actor, Cooking")
	class SURFACENAVPLUGIN_API ASurfaceNavProxy : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	virtual void PostLoad() override;

#if WITH_EDITORONLY_DATA

	// Don't remove this, useful when needed, but won't compile under all circumstances
	/*UPROPERTY()
		class UDebugStrings* DebugStringsComp;*/
#endif

#if WITH_EDITOR
	
	// Don't remove this, useful when needed, but won't compile under all circumstances
	//void RefreshDebugSceneProxy(TArray<FDebugSceneProxyData::FDebugText>& DebugTextData);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Forces the plugin to rebuild navigation. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Surface Nav")
	void SurfaceNavRebuildAll();


	/** This should normally be true unless your dynamically streaming/loading in NavMeshBoundsVolume actors which are
	positioned in a grid or other way that leaves no gaps between volumes. If there are gaps between the volumes
	then the SurfaceNav plugin won't be notified of changes in those areas where there is no NavMeshBoundsVolume which
	could lead to incorrect navigation generation. */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceNav|General")
	bool LimitSurfaceNavGenerationToNavMeshBoundsVolumes = true;

	/** How far blocks will be generated from the invokers when near to a player. This could
	also be considered the MAX generation radius. This must be >= FarBlockGenerationRadius and will be adjusted
	automatically if this value is < FarBlockGenerationRadius. The lower you're able to get this value, the
	better your performance will be. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|General", meta = (ClampMin = "1.0", UIMin = "1.0", ClampMax = "100000.0", UIMax = "100000.0"))
	float NearBlockGenerationRadius = 6000.f;

	/** How far should blocks be generated from the invoker when far from a player. This could
	also be considered the MIN generation radius. This will default to 1.5x the block size if left ZERO or the provided
	value is less than 1.5x block size. The lower you're able to get this value, the better your performance will be. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|General", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "100000.0", UIMax = "100000.0"))
	float FarBlockGenerationRadius = 0.f;

	/** The max distance from a player that blocks will be generated for navigation. The lower
	you're able to get this value, the better your performance will be. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|General", meta = (ClampMin = "1000.0", UIMin = "1000.0", ClampMax = "100000.0", UIMax = "100000.0"))
	float MaxDistanceFromPlayerForGeneration = 10000.f;

	/** Enable this option if you need to have navigation generated even if there is no player nearby. If this option
	is enabled, then the NearBlockGenerationRadius will be used for generation radius. This can be useful if you would
	like to simulate without a player in the game, but could hurt performance as you're always generating navigation
	at the max radius. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|General")
	bool AllowNavigationGenerationWithoutPlayer = false;

	/** The default volume detection tests if a nav point is wihtin a volume using a single direction and is
	typically over 99% accurate as it is. However, if you need more accuracy, you can enable this setting
	which will cause 3 tests instead of 1 to be	performed. You may see an increase in CPU usage since there
	will be 3 tests instead of 1. */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceNav|General")
	bool EnableMoreAccurateVolumeDetection = false;

	/** Whether the level represents a flat world or if it's 1 or more planets. Unchecking this option will
	require that you add a Surface Nav Planet Component to your planet actors. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|General")
	bool IsFlatWorld = true;

	/** How often to refresh location data for invokers and other navigation related objects. Decreasing this value
	will cause certain navigation processes to happen more often, but at higher resource cost. Increasing this value
	will save on resources, but may cause delays with certain navigation related events. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|General", meta = (ClampMin = "0.02", UIMin = "0.02", ClampMax = "5.0", UIMax = "5.0"))
	float RefreshLocationDataInterval = 0.5f;

	/** As actor components move around in the world, we need to rebuild navigation around them. This value controls
	how often the changes are processed. Decreasing this value will cause certain navigation to refresh more quickly after
	a component moves, but at higher resource cost. Increasing this value will save on resources, but may cause delays
	from the time a component moves to when the navigation is updated. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|General", meta = (ClampMin = "0.02", UIMin = "0.02", ClampMax = "1.0", UIMax = "1.0"))
	float ProcessComponentUpdatesInterval = 0.5f;

	/** As navigation becomes dirty and needing to be updated, this interval determines how long to wait to update. If this value
	is too low then an area receiving multiple back to back changes will get rapid fire updates and will eat up resources. If
	the value is too high then your AI will have a longer delay before the new navigation is ready. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|General", meta = (ClampMin = "0.02", UIMin = "0.02", ClampMax = "1.0", UIMax = "1.0"))
	float ProcessPendingDirtyNavigationInterval = 0.5f;

	inline float CalcSingleAxisBlockSize() {
		return GridPointsPerBlockAxis * GridPointDistance;
	}

	/** The interval, in seconds, used to cleanup old blocks. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Block Config", meta = (ClampMin = "5.0", UIMin = "5.0", ClampMax = "30.0", UIMax = "30.0"))
	float CleanBlockInterval = 5.f;

	/** The number of grid points per block along a single axis. This parameter and GridPointDistance
	are used to calculate the block size. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Block Config", meta = (ClampMin = "5", UIMin = "5", ClampMax = "50", UIMax = "50"))
	int GridPointsPerBlockAxis = 10;

	/** The distance between each grid point used for finding navigation points. The smaller the
	number the higher your navigation resolution will be, but at a greater processing cost both in
	navigation generation as well as path finding. You will need to find a distance that works
	best for your needs. This parameter and GridPointsPerBlockAxis are used to calculate the block size. 
	Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Block Config", meta = (ClampMin = "40", UIMin = "40", ClampMax = "500", UIMax = "500"))
	float GridPointDistance = 80.f;

	/** Offsets the navigation by a percentage (0-0.99) of GridPointDistance with 0.5 being 50% for example. 
	You may provide a different value for each axis if needed. This could be useful in certain situations 
	such as a blocky voxel style world where you would likely want the nav points aligned to the center of 
	the voxels. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Block Config", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "0.99", UIMax = "0.99"))
	FVector GridOffsetPercent = FVector::ZeroVector;

	/** The method used for doing traces to find surfaces for navigation. You can trace by channel or by profile. 
	Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere, Category = "Surface Nav|Block Config")
	TEnumAsByte<enum ESurfaceNavTraceMethod> TraceMethod = ESurfaceNavTraceMethod::Profile;

	/** The trace profile to use for tracing to detect surfaces for navigation generation. Triggers nav rebuild 
	when changed. */
	UPROPERTY(EditAnywhere, Category = "Surface Nav|Block Config", meta = (EditCondition = "TraceMethod == 0"))
	FString TraceProfile = "IgnoreOnlyPawn";

	/** The trace channel to use for tracing to detect surfaces for navigation generation. Triggers nav rebuild 
	when changed. */
	UPROPERTY(EditAnywhere, Category = "Surface Nav|Block Config", meta = (EditCondition = "TraceMethod == 1"))
	TEnumAsByte<enum ECollisionChannel> TraceChannel = ECollisionChannel::ECC_Camera;

	/** This setting limits the max number of iterations that path finding can perform before
	stopping. The higher this setting the more CPU usage you will see. If the setting is too
	low it could result in an incorrect partial path to the target. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|AI", meta = (ClampMin = "100", UIMin = "100", ClampMax = "50000", UIMax = "50000"))
	int MaxPathingIterations = 10000;

	/** How many path requests will be processed each tick. The higher this number the higher the impact on performance.
	This affects both initial path requests and re-path requests. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|AI", meta = (ClampMin = "1", UIMin = "1", ClampMax = "100", UIMax = "100"))
	int MaxPathRequestsPerTick = 5;

	/** Re-path requests are queued up and processed at this interval. The lower this number the higher the impact on performance.
	MaxPathRequestsPerTick also affects how many re-path requests may be processed per tick of this interval. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|AI", meta = (ClampMin = "0.01", UIMin = "0.01", ClampMax = "10.0", UIMax = "10.0"))
	float ObservedPathsTickInterval = 0.2f;

	/** The max distance from any DSN navigation that the pawn can be in order to be considered for pathfinding. Any further than
	the provided value and move requests will fail. Keep this value moderate because on certain terrain it can cause AI to appear 
	to float towards the navigation if they're far away. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|AI", meta = (ClampMin = "60", UIMin = "60", ClampMax = "1000", UIMax = "1000"))
	float MaxDistanceFromNavigation = 300.f;

	/** This is the very small sphere radius used for sweep tracing surfaces for navigation. Line traces could create issues
	by missing some surfaces at very specific locations in space and the sphere trace greatly decreases the chance of that
	happening. You should only consider changing this if you have surfaces missing navigation. You will most likely never
	need to adjust this value, but it's here just in case. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere, Category = "Surface Nav|Advanced",
		meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "0.001", UIMax = "0.001"))
	float TraceSphereRadius = 0.0001f;

	/** Allows offsetting the start of the surface traces and must be <= 0. You should only consider changing this if you have
	surfaces missing navigation. You will most likely never need to adjust this value, but it's here just in case. Triggers
	nav rebuild when changed. */
	UPROPERTY(EditAnywhere, Category = "Surface Nav|Advanced",
		meta = (ClampMin = "-0.0001", UIMin = "-0.0001", ClampMax = "0.0", UIMax = "0.0"))
	float TraceSphereStartOffset = 0.0f;

	/** Allows offsetting the end of the surface traces and must be >= 0. You should only consider changing this if you have
	surfaces missing navigation. You will most likely never need to adjust this value, but it's here just in case. Triggers
	nav rebuild when changed. */
	UPROPERTY(EditAnywhere, Category = "Surface Nav|Advanced",
		meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "0.0001", UIMax = "0.0001"))
	float TraceSphereEndOffset = 0.00004f;

	/** This controls the max component bounding sphere radius that should be able to affect navigation.
	If this is too small, you may miss some large components such as those used by some systems for
	large collision chunks. If you have massive components in your scene that are changing and affect navigation,
	then you may want to find a way to break them up into smaller components as having this value
	too large can affect performance. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere, Category = "Surface Nav|Advanced",
		meta = (ClampMin = "10000", UIMin = "10000", ClampMax = "200000", UIMax = "200000"))
	float MaxComponentSphereRadiusToProcess = 100000.f;

	/** This controls the shape used when detecting overlaps for blocks. Hopefully you never need to adjust this,
	but I have witnessed strange overlap misses with both shapes in different scenarios. Triggers nav rebuild 
	when changed.*/
	UPROPERTY(EditAnywhere, Category = "Surface Nav|Advanced")
	TEnumAsByte<ESurfaceNavOverlapShape::Type> BlockOverlapTestShape = ESurfaceNavOverlapShape::Sphere;

	/** This controls the shape used when detecting overlaps for grid points. Hopefully you never need to adjust this,
	but I have witnessed strange overlap misses with both shapes in different scenarios. Triggers nav rebuild 
	when changed.*/
	UPROPERTY(EditAnywhere, Category = "Surface Nav|Advanced")
	TEnumAsByte<ESurfaceNavOverlapShape::Type> GridPointOverlapTestShape = ESurfaceNavOverlapShape::Sphere;

	/** You won't need this unless you're using nav modifiers in your level. And only when those nav modifiers are 
	using brushes other than a basic cube such as a sphere, cylinder, etc....
	NOTE: This is not able to fix issues with nav modifiers that contain more complex brushes with internal faces 
	or with hollow areas at this time. */
	UPROPERTY(EditAnywhere, Category = "Surface Nav|Advanced")
	bool EnhancedNavModBrushTest = false;

	/** Max amount the expense modifier can affect path finding. If this is too high,
	then it could cause AI to take much more expensive paths. If it's too low, then
	the AI will continue to line up when path finding to the same target. Find a good
	balance. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Path Finding", meta = (ClampMin = "0", UIMin = "0", ClampMax = "10000", UIMax = "10000"))
	float MaxExpenseModifier = 450.f;

	/** How quickly the expense modifier is increased as AI choose the same path up to MaxExpenseModifier. 
	Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Path Finding", meta = (ClampMin = "0", UIMin = "0", ClampMax = "10000", UIMax = "10000"))
	float ExpenseModifierUpStep = 200.f;

	/** How quickly the expense modifier is decreased as AI evaluate the point but choose not to use the path. 
	Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Path Finding", meta = (ClampMin = "0", UIMin = "0", ClampMax = "10000", UIMax = "10000"))
	float ExpenseModifierDownStep = 70.f;

	float MaxExpenseModifierSq = 0.f;
	float ExpenseModifierUpStepSq = 0.f;
	float ExpenseModifierDownStepSq = 0.f;

	/** This setting gives the navigation some time to build after initialization before allowing
	any path finding. Pathing requests will fail until the delay has elapsed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Path Finding", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "60.0", UIMax = "60.0"))
	float DelayBeforePathFindingAllowed = 1.f;

	/** Whether to automatically mark nav points as narrow. You may want to turn this off if you have
	small pawns that need to go through smaller areas. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Path Finding")
	bool FlagNarrowCorridors = true;

	/** This will give a bit more control over when the system flags a narrow nav point. This value acts as a percentage of
	GridPointDistance which should be flagged as narrow. For instance, if your GridPointDistance is 80 and you set this value
	to 0.5, then opposing nav points <= 40 units apart will be flagged as narrow. Triggers nav rebuild when changed. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Path Finding", meta = (ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0", EditCondition = "FlagNarrowCorridors == true"))
	float NarrowCorridorPercentToFlag = 1.0f;

	/** Enabling this option will show some data about queue sizes and some other navigation data. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization")
	bool ShowDebugStats = false;

	/** This value controls how far the navigation should be drawn from the camera when the various show options below are used.
	Avoid making this too large as it can have a substantial impact on performance. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization|Persistent Limited Range", meta = (ClampMin = "1000.0", UIMin = "1000.0", ClampMax = "20000.0", UIMax = "20000.0"))
	float DisplayRadiusFromCamera = 2000.f;

	/** Show navigation blocks if their bounds fall inside the specified radius. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization|Persistent Limited Range")
	bool ShowNavBlocks = false;

	/** Show navigation grid points which fall inside the specified radius. Only grid points with navigation data
	will be shown. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization|Persistent Limited Range")
	bool ShowNavGridPoints = false;

	/** Show nav points which fall inside the specified radius. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization|Persistent Limited Range")
	bool ShowNavPoints = false;

	/** Show the normals for nav points which fall inside the specified radius. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization|Persistent Limited Range")
	bool ShowNavPointNormals = false;

	/** Show neighbor directions for nav points which fall inside the specified radius. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization|Persistent Limited Range")
	bool ShowNavNeighborDirections = false;

	/** Show red boxes to visually represent areas where navigation ends. Some surface orientations 
	may result in the system unable to build nav point relationships. Areas like these may need minor adjustments
	or extra attention to work around. This option will help you visualize those problem areas so you're 
	able to find ways to work around them. End of navigation can also happen due to reaching the end of 
	the nav mesh volume which is expected behavior. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization|Persistent Limited Range")
	bool ShowWhereNavigationEnds = false;

	/** Show nav links which fall inside the specified radius. If ShowNavPoints is enabled then you'll also see lines
	indicating what nav points are connected neighbors for a nav link point. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization|Persistent Limited Range")
	bool ShowNavLinks = false;

	/** Shows all nav points for a short time as they're generated. If you have a large navigation area then this
	may significantly drop your FPS anytime the navigation is being built or re-built. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization|Unlimited Range")
	bool ShowNavPointsDuringBuild = false;

	/** Show points representing the path chosen by your AI. The starting point is shown as green, end point as red and
	points in between as purple. The pawn's current target point will be yellow. */
	UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debug Visualization|Unlimited Range")
	bool Show_AI_Paths = false;

	/** When enabled this option will perform additional testing. This will add some processing time, even in shipping builds
	so make sure to disable before publishing unless you want the added tests for troubleshooting an issue. */
	/*UPROPERTY(EditAnywhere,
		Category = "Surface Nav|Debugging")*/
	bool PerformExtraDebuggingTests = false;

	/** This is simply a convenience pass-through function and calls a function with the same name and purpose
	on Surface Navigation System.
	Draws debug data representing different parts of the navigation data. This may
	significantly drop your FPS while it's being displayed.
	@param DrawRadius - How far out to draw navigation data.
	@param ShowBlocks - Show nav blocks within range.
	@param ShowGridPoints - Show the grid points which are the owners of the surrounding nav points.
	@param ShowNavPoints - Show nav points colored using their nav area.
	@param ShowNavPointNormals - Show nav point normals colored using their normal vector.
	@param ShowNeighborDirLines - Shows the direction of each nav point neighbor. The link originates from the owning nav point.
	@param ShowWhereNavigationEnds - Shows the direction of each nav point neighbor. The link originates from the owning nav point.
	@param ShowNavLinkPoints - Show points for nav links and smart nav links.
	@param Duration - How long the drawn debug data should be shown.
	*/
	UFUNCTION(BlueprintCallable, Category = "Surface Nav|Debug")
	void DrawDebugNavData(const float InDrawRadius = 2000.f, const bool InShowBlocks = false, const bool InShowGridPoints = false, const bool InShowNavPoints = false, const bool InShowNavPointNormals = false, const bool InShowNeighborDirLines = false, const bool InShowWhereNavigationEnds = false, const bool InShowNavLinkPoints = false, const float InDuration = -1.f) const;


protected:



	FTimerHandle TimerRebuildAllHandle;
	FRWLock RebuildAllLock;
	FThreadSafeCounter BlockGridPointQueueCount = 0;


public:
	void RefreshSettings();

};






