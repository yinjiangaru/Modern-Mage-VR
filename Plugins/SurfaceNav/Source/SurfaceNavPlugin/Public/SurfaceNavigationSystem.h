// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "NavLinkComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavModifierVolume.h"
#include "NavModifierComponent.h"
#include "NavigationSystemTypes.h"
#include "Containers/Map.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "Runtime/Engine/Classes/Components/BoxComponent.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "Misc/ScopeRWLock.h"

#include "SurfaceNavProxy.h"
#include "SurfaceNavBlockQueueWorker.h"
#include "SurfaceNavInvokerComponent.h"
#include "SurfaceNavCommon.h"
#include "SurfaceNavPath.h"
#include "SurfaceNavBlockPool.h"
#include "SurfaceNavGridPointPool.h"
#include "SurfaceNavPointPool.h"
#include "SurfaceNavPointBase.h"
#include "SurfaceNavPoint.h"
#include "SurfaceNavLinkPoint.h"
#include "SurfaceNavBlock.h"
#include "SurfaceNavGridPoint.h"

#include "SurfaceNavigationSystem.generated.h"


class USurfaceNavInvokerComponent;
class USurfaceNavPlanetComponent;
struct FSurfaceNavBlock;
struct FSurfaceNavPoint;
struct FSurfaceNavPathResult;


DECLARE_EVENT_OneParam(USurfaceNavigationSystem, FComponentUpdated, const UActorComponent&);

struct FUpdatedComponentQueueItem
{
	FUpdatedComponentQueueItem() {};

	FUpdatedComponentQueueItem(FVector InVelocity, FBox InBounds, bool InIsBeingDestroyed) :
		Velocity(InVelocity), Bounds(InBounds), IsBeingDestroyed(InIsBeingDestroyed)
	{}

	bool operator==(const FUpdatedComponentQueueItem& Other) const
	{
		return Velocity == Other.Velocity && Bounds == Other.Bounds && IsBeingDestroyed == Other.IsBeingDestroyed;
	}

	FVector Velocity;
	FBox Bounds;
	bool IsBeingDestroyed;
};

inline uint32 GetTypeHash(const FUpdatedComponentQueueItem CompQueItem)
{
	return HashCombine(GetTypeHash(CompQueItem.Velocity),
		HashCombine(GetTypeHash(CompQueItem.Bounds.GetCenter()),
			HashCombine(GetTypeHash(CompQueItem.Bounds.GetExtent()),
				GetTypeHash(CompQueItem.IsBeingDestroyed))));
}

struct FComponentDataCacheItem
{
	FComponentDataCacheItem() {};

	FComponentDataCacheItem(FBox InBounds) :
		Bounds(InBounds)
	{

	}

	FBox Bounds;
};

USTRUCT()
struct FInvokerQueDataItem
{
	GENERATED_BODY()

	FInvokerQueDataItem()
	{

	}

	FInvokerQueDataItem(
		FVector& initInvokerLoc,
		float initNearBlockRadius,
		float initFarBlockRadius,
		float initMaxDistanceFromPlayer) :
		InvokerLoc(initInvokerLoc),
		NearBlockRadius(initNearBlockRadius),
		FarBlockRadius(initFarBlockRadius),
		MaxDistanceFromPlayer(initMaxDistanceFromPlayer)
	{

	}

	FVector InvokerLoc;
	float NearBlockRadius;
	float FarBlockRadius;
	float MaxDistanceFromPlayer;
};


UCLASS(BlueprintType)
class SURFACENAVPLUGIN_API USurfaceNavPath : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path")
	TArray<FVector> Points;
};

struct SURFACENAVPLUGIN_API FSurfaceNavPathResult //: public UObject
{

	FSurfaceNavPathSharedPtr Path;

	USurfaceNavPath* SurfaceNavPath = NewObject<USurfaceNavPath>(); //; // = new USurfaceNavPath();
	ENavigationQueryResult::Type Result;

	FSurfaceNavPathResult(ENavigationQueryResult::Type InResult = ENavigationQueryResult::Invalid) : Result(InResult)
	{ }



	FORCEINLINE bool IsSuccessful() const
	{
		return Result == ENavigationQueryResult::Success;
	}
	FORCEINLINE bool IsPartial() const
	{
		return Path && Path->IsPartial();
	}
};

struct PathNode
{
	PathNode(TSharedPtr<FSurfaceNavPointBase> InitNavPoint);
	~PathNode()
	{
		NavPoint.Reset();
	}

	bool bVisited = false;			// Have we searched this node before?
	double fGlobalGoal = INFINITY;	// Distance to goal so far
	double fLocalGoal = INFINITY;	// Distance to goal if we took the alternative route
	TSharedPtr<FSurfaceNavPointBase> NavPoint;	// Ref to associated nav point
	PathNode* parent = nullptr;		// Node connecting to this node that offers shortest parent
};

/** This is simply a standard UE Nav Link Component with a wrapper so it can register with
this plugin’s navigation system. There was no simple way, found at this time, to get the
standard UE Nav Link Component to register with the DSN plugin’s navigation in the way
which was needed so a custom link component was required. */
UCLASS(ClassGroup = (Navigation), meta = (BlueprintSpawnableComponent))
class SURFACENAVPLUGIN_API USurfaceNavLinkComponent : public UNavLinkComponent
{
	GENERATED_UCLASS_BODY()


	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;

	/** Unregister the component, remove it from its outer Actor's Components array and mark for pending kill. */
	virtual void DestroyComponent(bool bPromoteChildren = false) override;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	virtual void OnRegister()  override;
	virtual void OnUnregister()  override;
};


struct SURFACENAVPLUGIN_API FSurfaceNavBounds : FNavigationBounds
{
	typedef FNavigationBounds Super;

	FSurfaceNavBounds(FNavigationBounds NavBounds) : Super(NavBounds) {};

	//TArray<TWeakPtr<FSurfaceNavBlock>> SurfaceNavBlocks;
	TSet<FIntVector> SurfaceNavBlockIndices;
};


/** This inherits from UE’s native navigation system and should be
used instead of UE’s native navigation for any world you plan to
use the DSN plugin with. The native UE navigation and this plugin’s
navigation will function side-by-side. This will allow you to have
some AI using the native navigation and some AI using the DSN plugin’s
navigation if needed. */
UCLASS(Within = World, config = Engine, defaultconfig)
//UCLASS()
class SURFACENAVPLUGIN_API USurfaceNavigationSystem : public UNavigationSystemV1
{
	GENERATED_BODY()


public:

	void OnSurfaceNavPIEEnd();

public:

	virtual void Tick(float DeltaSeconds) override;

	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;


	FComponentUpdated& OnComponentUpdated() { return ComponentUpdated; }

	USurfaceNavigationSystem(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SurfaceNavRebuildAll();

	static void OnActorRegistered(AActor* Actor);
	static void OnActorUnregistered(AActor* Actor);
	static void OnComponentRegistered(UActorComponent* Comp);
	static void OnComponentUnregistered(UActorComponent* Comp);
	static void UpdateComponentData(UActorComponent& Comp);
	static void UpdateActorData(AActor& Actor);


	static void RegisterSurfaceNavInvoker(AActor*& Invoker);
	static void UnregisterNavigationInvoker(AActor*& Invoker);

	static TWeakObjectPtr<USurfaceNavigationSystem> CurrentSurfaceNavSystem;

	virtual void RegisterSurfaceNavLinkComponent(USurfaceNavLinkComponent* NavLinkComp);
	virtual void UnregisterSurfaceNavLinkComponent(USurfaceNavLinkComponent* NavLinkComp);
	FORCEINLINE virtual TSet<USurfaceNavLinkComponent*> GetSurfaceNavLinkComponents() { return SurfaceNavLinkComponents; }

	virtual void RegisterSurfaceNavPlanetComponent(USurfaceNavPlanetComponent* NavLinkComp);
	virtual void UnregisterSurfaceNavPlanetComponent(USurfaceNavPlanetComponent* NavLinkComp);
	FORCEINLINE virtual TSet<TWeakObjectPtr<USurfaceNavPlanetComponent>> GetSurfaceNavPlanetComponents() { return SurfaceNavPlanetComponents; }

	virtual void _RegisterSurfaceNavInvoker(AActor*& Invoker);
	virtual void _UnregisterSurfaceNavInvoker(AActor*& Invoker);


	// @todo document
	bool ProjectPointToNavigation(const FVector& Point, const bool ForceNormalToDirFromSurface, TSharedPtr<FSurfaceNavPointBase>& OutLocation,
		const float& MinAngleFromGround = 0.f, const float& MaxAngleFromGround = 180.f, const float& AcceptanceRadius = 100.f,
		UNavigationQueryFilter* FilterClass = nullptr) const
	{
		return ProjectPointToNavigation(Point, Point, ForceNormalToDirFromSurface, OutLocation, MinAngleFromGround, MaxAngleFromGround, AcceptanceRadius, FilterClass);
	}

	bool ProjectPointToNavigation(const FVector& Point, const FVector& NavSourcePoint, const bool ForceNormalToDirFromSurface,
		TSharedPtr<FSurfaceNavPointBase>& OutLocation, const float& MinAngleFromGround = 0.f, const float& MaxAngleFromGround = 180.f,
		const float& AcceptanceRadius = 100.f, UNavigationQueryFilter* FilterClass = nullptr) const;


	bool bIsReadyToPathfind = false;

	int GridPointPoolSize = 1;
	int NavPointPoolSize = 1;

	inline const bool IsInitDone() { return bInitDone; }


	int CurPathingCount = 0;

	bool CanAcceptPathingRequest() const { return bIsReadyToPathfind && bSurfaceNavGenerationEnabled && CurPathingCount < SurfaceNavProxySettings::MaxPathRequestsPerTick; }

	bool bIsCleaningUp = false;

	FRWLock PathingLock;


protected:

	bool IsBound = false;

	virtual void DoInitialSetup() override;
	void SurfaceNavDoInitialSetup();


	bool bSurfaceNavGenerationEnabled = true;

	float DelayBeforeInit = 1.f;

	FSurfaceNavBlockPoolComponent* BlockPool;
	FSurfaceNavGridPointPool* GridPointPool;
	FSurfaceNavPointPool* NavPointPool;

	void SetInitSurfaceNavTimer();
	void DelayTryInitSurfaceNav();
	void InitSurfaceNav();

	/** Called upon UWorld destruction to release what needs to be released */
	virtual void CleanUp(const FNavigationSystem::ECleanupMode Mode = FNavigationSystem::ECleanupMode::CleanupUnsafe) override;
	void SurfaceNavCleanUp(bool FinalizeCleanup = true);

	bool bInitDone = false;
	bool bNavHasGenerated = false;

	//FTimerHandle TimerGenGridHandle;
	//FTimerHandle TimerProcessQueueHandle;
	//FTimerHandle TimerCleanBlockHandle;
	UPROPERTY()
	FTimerHandle TimerProcessPendingDirtyHandle;
	UPROPERTY()
	FTimerHandle TimerUpdateInvokerLocationsHandle;
	UPROPERTY()
	FTimerHandle TimerProcessUpdatedComponentsHandle;
	UPROPERTY()
	FTimerHandle TimerDelayPathfindingHandle;
	UPROPERTY()
	FTimerHandle TimerDelayedCleanupObjectsHandle;

	FReleaseBlockDelegate ReleaseBlockDel;
	FBlockOverlapsFoundDelegate BlockOverlapsFoundDel;
	FBlockNavUpdatedDelegate BlockNavUpdatedDel;
	FHandleBlockMarkedDirtyDelegate HandleBlockMarkedDirtyDel;
	FActorsInBlockChangedDelegate ActorsInBlockChangedDel;
	FQueueBlocksNearInvokersDelegate QueueBlocksNearInvokersDel;
	FGridPointFinishedDelegate GridPointFinishedDel;

	int LastBlockQueueCount = 0;
	int PathingRequestCount = 0;

	FThreadSafeCounter BlockQueueCount = 0;
	FThreadSafeCounter BlockGridPointQueueCount = 0;
	FThreadSafeCounter FinishProcessingBlockGridPointQueueCount = 0;
	FThreadSafeCounter BlocksProcessingCount = 0;
	FThreadSafeCounter ThreadsProcessingCount = 0;
	FThreadSafeCounter RefreshingVolumeCounter = 0;

	bool bIsProcessing;

	TQueue<FIntVector> BlockQueue;
	TQueue<FSurfaceNavBlock*> BlockGridPointQueue;
	TQueue<TTuple<FBox, FBox>> VolumeBoundsChangedQueue;
	TQueue<FIntVector, EQueueMode::Mpsc> BlocksPendingDirty;
	TQueue<FIntVector, EQueueMode::Mpsc> BlocksMarkedDirty;

	TSet<FIntVector> BlocksInRange;
	FRWLock BlocksInRangeLock;


	// This queue is letting the system know these nav points are need
	// to be deleted. They will be moved to the ready to delete queue after
	// a short delay.
	TQueue<TSharedPtr<FSurfaceNavPointBase>> NavPointsPendingDelete;
	TQueue<FSurfaceNavGridPoint*> NavGridPointsPendingDelete;
	TQueue<FSurfaceNavBlock*, EQueueMode::Mpsc> NavBlocksPendingDelete;
	TQueue<TMap<TSharedPtr<FSurfaceNavPointBase>, PathNode*>> PathNodeMapsPendingDelete;
	TQueue<FSurfaceNavLinkData*> NavLinksPendingDelete;

	// This queue is lets the system know to go ahead and delete these
	// nav points.
	TQueue<TSharedPtr<FSurfaceNavPointBase>> NavPointsReadyToDelete;
	TQueue<FSurfaceNavGridPoint*> NavGridPointsReadyToDelete;
	TQueue<FSurfaceNavBlock*> NavBlocksReadyToDelete;
	TQueue<PathNode*> PathNodesReadyToDelete;
	TQueue<FSurfaceNavLinkData*> NavLinksReadyToDelete;

	bool bProcessingDeleteObjectQueues = false;
	bool bProcessingCleanupObjectQueues = false;

	mutable FRWLock NavBlocksLock;
	FRWLock BlocksWithUpdatedNavLock;
	TSet<FIntVector> BlocksWithUpdatedNav;

	UPROPERTY()
	TArray<FInvokerQueDataItem> InvokerData;

	TArray<FVector> PlayerLocations;

	TMap<FIntVector, FSurfaceNavBlock*> NavBlocks;
	TSet<FIntVector> BlocksPendingAdd;
	TSet<FIntVector> BlocksPendingRefresh;
	TQueue<FIntVector> BlocksRecentlyProcessed;

	TQueue<FIntVector> InvalidateBlocksQueue;
	TQueue<FSurfaceNavBlock*> CleanupBlocksLvl1Queue;
	TQueue<FSurfaceNavBlock*> CleanupBlocksLvl2Queue;
	FThreadSafeCounter CleanupLvl2QueueCount = 0;

	TQueue<FSurfaceNavGridPoint*> CleanupGridPointsLvl1Queue;
	TQueue<FSurfaceNavGridPoint*> CleanupGridPointsLvl2Queue;

	/** We have 2 queue levels here because there appears to be a problem if the changes are processed
	too quickly. Need to start in Lvl1, then flow to Lvl2. Lvl1 needs to be Mpsc, but Lvl2 will only
	ever have a single producer and consumer. */
	TQueue<FUpdatedComponentQueueItem, EQueueMode::Mpsc> UpdatedComponentsQueue;
	//FRWLock UpdatedCompQueLock;
	FThreadSafeCounter UpdatedComponentsQueueCount = 0;

	TMap<UActorComponent*, FComponentDataCacheItem> ComponentDataCache;

	bool bIsProcessingComponentUpdates = false;

	TSet<FUpdatedComponentQueueItem> ReprocessComponents;

	TQueue<FIntVector> UpdatedComponentBlocks;

	SurfaceNavBlockQueueWorker* BlockQueueWorker1 = nullptr;
	SurfaceNavBlockQueueWorker* BlockQueueWorker2 = nullptr;
	SurfaceNavBlockQueueWorker* BlockQueueWorker3 = nullptr;
	SurfaceNavBlockQueueWorker* BlockQueueWorker4 = nullptr;

	inline bool AreDeleteQueuesEmpty()
	{
		return NavPointsReadyToDelete.IsEmpty() && NavPointsPendingDelete.IsEmpty() &&
			NavGridPointsReadyToDelete.IsEmpty() && NavGridPointsPendingDelete.IsEmpty() &&
			NavBlocksReadyToDelete.IsEmpty() && NavBlocksPendingDelete.IsEmpty() &&
			PathNodeMapsPendingDelete.IsEmpty() && PathNodesReadyToDelete.IsEmpty() &&
			NavLinksPendingDelete.IsEmpty() && NavLinksReadyToDelete.IsEmpty();
	}

	inline bool AreCleanupQueuesEmpty()
	{
		return CleanupBlocksLvl1Queue.IsEmpty() && CleanupGridPointsLvl1Queue.IsEmpty();
	}

	void CleanupBlocks(int NumToProcess, bool NeedsLock = false);

	void SetUpdateInvokerLocationsTimer();
	void SetProcessUpdatedComponentsTimer();


	void DecrementGridPointIndex(int& GridPointIndex, FIntVector& OffsetIndex, FSurfaceNavBlock*& OtherBlock, const int& VoxelSize, const FIntVector& SubtractFromIndex);
	void IncrementGridPointIndex(int& GridPointIndex, FIntVector& OffsetIndex, FSurfaceNavBlock*& OtherBlock, const int& VoxelSize, const FIntVector& AddToIndex);
	bool DetermineNewNeighbor(TSharedPtr<FSurfaceNavPointBase>& NewNeighbor, int& MyIndexForNewNeighbor, const FSurfaceNavGridPoint*& GridPoint, const FIntVector& NavPointIndex, const int& NewNeighborIndex);


	void NegXAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase);
	void PosXAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase);
	void NegYAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase);
	void PosYAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase);

	void NegZAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase);
	void PosZAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase);

	void PosAxisFindNeg(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir1, const int& TestAxisIndex1,
		const FIntVector& TestDir2, const int& TestAxisIndex2, const FIntVector& TestDir3, const int& TestAxisIndex3, const int& PrimaryGridPointAxisIndex, const int& SecondaryGridPointAxisIndex);

	void PosAxisFindPos(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir1, const int& TestAxisIndex1,
		const FIntVector& TestDir2, const int& TestAxisIndex2, const FIntVector& TestDir3, const int& TestAxisIndex3, const int& PrimaryGridPointAxisIndex, const int& SecondaryGridPointAxisIndex);

	void NegAxisFindNeg(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir1, const int& TestAxisIndex1,
		const FIntVector& TestDir2, const int& TestAxisIndex2, const FIntVector& TestDir3, const int& TestAxisIndex3, const int& PrimaryGridPointAxisIndex, const int& SecondaryGridPointAxisIndex);

	void NegAxisFindPos(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir1, const int& TestAxisIndex1,
		const FIntVector& TestDir2, const int& TestAxisIndex2, const FIntVector& TestDir3, const int& TestAxisIndex3, const int& PrimaryGridPointAxisIndex, const int& SecondaryGridPointAxisIndex);


	void NegAxisFindDiagNegNegAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
		const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex);

	void NegAxisFindDiagPosNegAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
		const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex);

	void NegAxisFindDiagNegPosAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
		const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex);

	void NegAxisFindDiagPosPosAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
		const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex);


	void PosAxisFindDiagNegNegAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
		const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex);

	void PosAxisFindDiagPosNegAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
		const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex);

	void PosAxisFindDiagNegPosAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
		const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex);

	void PosAxisFindDiagPosPosAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
		FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
		const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex);




	const int XAxisIndex = 0;
	const int YAxisIndex = 1;
	const int ZAxisIndex = 2;


	const FIntVector NegXDir = FIntVector(-1, 0, 0);
	const FIntVector PosXDir = FIntVector(1, 0, 0);
	const FIntVector NegYDir = FIntVector(0, -1, 0);
	const FIntVector PosYDir = FIntVector(0, 1, 0);
	const FIntVector NegZDir = FIntVector(0, 0, -1);
	const FIntVector PosZDir = FIntVector(0, 0, 1);

	const int XAxisNegYIndex = 0;
	const int XAxisPosYIndex = 1;
	const int XAxisNegZIndex = 2;
	const int XAxisPosZIndex = 3;
	const int XAxisNegYNegZIndex = 4;
	const int XAxisPosYNegZIndex = 5;
	const int XAxisNegYPosZIndex = 6;
	const int XAxisPosYPosZIndex = 7;

	const int YAxisNegXIndex = 0;
	const int YAxisPosXIndex = 1;
	const int YAxisNegZIndex = 2;
	const int YAxisPosZIndex = 3;
	const int YAxisNegXNegZIndex = 4;
	const int YAxisPosXNegZIndex = 5;
	const int YAxisNegXPosZIndex = 6;
	const int YAxisPosXPosZIndex = 7;

	const int ZAxisNegXIndex = 0;
	const int ZAxisPosXIndex = 1;
	const int ZAxisNegYIndex = 2;
	const int ZAxisPosYIndex = 3;
	const int ZAxisNegXNegYIndex = 4;
	const int ZAxisPosXNegYIndex = 5;
	const int ZAxisNegXPosYIndex = 6;
	const int ZAxisPosXPosYIndex = 7;





	//void UpdateNavNeighbors(FSurfaceNavPoint* CurNavPoint, int WorkingNeighborIndex, FSurfaceNavPoint* NewNeighbor, int MyIndexForNewNeighbor);
	void UpdateNavNeighbors(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const int& WorkingNeighborIndex, TSharedPtr<FSurfaceNavPointBase>& NewNeighbor,
		const int& MyIndexForNewNeighbor, const int& PriorityIndex); //, const bool IsDiag = false);


	void FlagIsReadyToPathfind() { bIsReadyToPathfind = true; };

	void DeterminePath(TSharedPtr<FSurfaceNavPointBase>& StartNavPoint, TSharedPtr<FSurfaceNavPointBase>& EndNavPoint, FSurfaceNavPathFindingQueryData& Query, PathNode*& CurrentNode);

	void PathingIterateNavPointNeighborLockProxy(TSharedPtr<FSurfaceNavPointBase>& navPoint, TMap<TSharedPtr<FSurfaceNavPointBase>, PathNode*>& NodePoints, TSortedMap<double, PathNode*>& PointsNotTestedMap,
		UNavigationQueryFilter*& FilterClass, const FVector& curLoc, PathNode*& CurrentNode, PathNode*& ShortestPathNode, const FVector& EndLocation,
		const float& MinAngleFromGround, const float& MaxAngleFromGround, uint32& Counter, FSurfaceNavPathFindingQueryData& Query);

	void PathingIterateNavPointNeighbor(TSharedPtr<FSurfaceNavPointBase>& navPoint, TMap<TSharedPtr<FSurfaceNavPointBase>, PathNode*>& NodePoints, TSortedMap<double, PathNode*>& PointsNotTestedMap,
		UNavigationQueryFilter* FilterClass, const FVector& curLoc, PathNode*& CurrentNode, PathNode*& ShortestPathNode, const FVector& EndLocation,
		const float& MinAngleFromGround, const float& MaxAngleFromGround, uint32& Counter, FSurfaceNavPathFindingQueryData& Query);

	void ProcessNavPointForPathing(TSharedPtr<FSurfaceNavPointBase>& navPoint, TMap<TSharedPtr<FSurfaceNavPointBase>, PathNode*>& NodePoints, TSortedMap<double, PathNode*>& PointsNotTestedMap,
		UNavigationQueryFilter*& FilterClass, const FVector& curLoc, PathNode*& CurrentNode, PathNode*& ShortestPathNode, const FVector& EndLocation,
		const float& MinAngleFromGround, const float& MaxAngleFromGround, uint32& Counter, FSurfaceNavPathFindingQueryData& Query);

	float GetWorldTimeStamp() const
	{
		const UWorld* World = GetWorld();
		return World ? World->GetTimeSeconds() : 0.f;
	}

	void ProcessBlockWithUpdatedNav();
	void ProcessChangedVolumeBounds();

	/**
	 *	Contains paths that requested observing its goal's location. These paths will be
	 *	processed on a regular basis (@see ObservedPathsTickInterval) */
	TArray<FSurfaceNavPathWeakPtr> ObservedPaths;

	/** paths that requested re-calculation */
	TArray<FSurfaceNavPathRecalculationRequest> RepathRequests;
	//TMap<FSurfaceNavPathWeakPtr, FSurfaceNavPathRecalculationRequest> RepathRequests;


	/** contains how much time left to the next ObservedPaths processing */
	float NextObservedPathsTickInSeconds;

	void ProcessObservedPaths(float DeltaTime);
	void ProcessRepathRequests();

	/** removes from ActivePaths all paths that no longer have shared references (and are invalid in fact) */
	void PurgeUnusedPaths();

	/**
	 *	Container for all path objects generated with this Navigation Data instance.
	 *	Is meant to be added to only on GameThread, and in fact should user should never
	 *	add items to it manually, @see CreatePathInstance
	 */
	TArray<FSurfaceNavPathWeakPtr> ActivePaths;

	/** Synchronization object for paths registration from main thread and async pathfinding thread */
	mutable FCriticalSection ActivePathsLock;

	void RegisterActivePath(FSurfaceNavPathSharedPtr SharedPath)
	{
		// Paths can be registered from main thread and async pathfinding thread
		FScopeLock PathLock(&ActivePathsLock);
		ActivePaths.Add(SharedPath);
	}

#if WITH_EDITOR
	bool IsEditorInstanceWhileRunningPIE()
	{
		// There's certain processes we want to skip if the current instance is
		// the editor while we're running PIE. There may be a better way to achieve this,
		// but the PIE active flag was not working for me at the time.
		UWorld* MyWorld = GetWorld();
		UWorld* CurNavWorld = USurfaceNavigationSystem::CurrentSurfaceNavSystem.Get() && USurfaceNavigationSystem::CurrentSurfaceNavSystem.IsValid(false, true) ? USurfaceNavigationSystem::CurrentSurfaceNavSystem->GetWorld() : nullptr;
		if (CurNavWorld && MyWorld->WorldType == EWorldType::Editor && MyWorld->WorldType != CurNavWorld->WorldType)
			return true;

		return false;
	}
#endif 

private:
	FComponentUpdated ComponentUpdated;

	TSet<ANavModifierVolume*> NavModVolumes;
	TSet<UNavModifierComponent*> NavModComponents;

	TSet<USurfaceNavLinkComponent*> SurfaceNavLinkComponents;
	TSet<TWeakObjectPtr<USurfaceNavPlanetComponent>> SurfaceNavPlanetComponents;
	TSet<TWeakObjectPtr<AActor>> SurfaceNavInvokers;

	bool InvokersRefreshing = false;


#if ENGINE_MAJOR_VERSION < 5

	// **********************************************
	// Create a couple of useful functions to use with UE 4.27
	// **********************************************


	/**
	 * Calculates squared distance between two boxes.
	 */
	FORCEINLINE float ComputeSquaredDistanceToBox(const FBox& OwnerBox, const FBox& OtherBox) const
	{
		//FVector::ComponentMax
		FVector AxisDistances = (OwnerBox.GetCenter() - OtherBox.GetCenter()).GetAbs() - (OwnerBox.GetExtent() + OtherBox.GetExtent());
		AxisDistances = AxisDistances.ComponentMax(FVector(0.0f, 0.0f, 0.0f));
		return FVector::DotProduct(AxisDistances, AxisDistances);
	};

	bool BoxEquals(const FBox& OwnerBox, const FBox& OtherBox, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return (!OwnerBox.IsValid && !OtherBox.IsValid) || ((OwnerBox.IsValid && OtherBox.IsValid) && OwnerBox.Min.Equals(OtherBox.Min, Tolerance) && OwnerBox.Max.Equals(OtherBox.Max, Tolerance));
	}

#if WITH_EDITOR
	static bool GetLevelViewportCameraInfo(FVector& CameraLocation);
#endif

#endif

public:

	TQueue<FIntVector> FinishProcessingBlockGridPointQueue;
	void FinishProcessingBlockGridPoints(int NumToProcess);

	//----------------------------------------------------------------------//
	//~ Begin Public querying Interface                                                                
	//----------------------------------------------------------------------//
	/**
	 *	Synchronously looks for a path from @fLocation to @EndLocation for agent with properties @AgentProperties. NavData actor appropriate for specified
	 *	FNavAgentProperties will be found automatically
	 *	@param ResultPath results are put here
	 *	@param NavData optional navigation data that will be used instead of the one that would be deducted from AgentProperties
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 */
	FSurfaceNavPathResult FindSurfacePathSync(const FNavAgentProperties& AgentProperties, FSurfaceNavPathFindingQueryData& Query); // , EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/**
	 *	Does a simple path finding from @StartLocation to @EndLocation on specified NavData. If none passed MainNavData will be used
	 *	Result gets placed in ResultPath
	 *	@param NavData optional navigation data that will be used instead main navigation data
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 */
	FSurfaceNavPathResult FindSurfacePathSync(FSurfaceNavPathFindingQueryData& Query); // , EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	//FSurfaceNavPathResult FindRandomWanderPathSync(FVector SourcLoc);

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
	uint32 FindSurfacePathAsync(const FNavAgentProperties& AgentProperties, FSurfaceNavPathFindingQueryData& Query, const FNavPathQueryDelegate& ResultDelegate); // , EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/** Removes query indicated by given ID from queue of path finding requests to process. */
	void AbortAsyncFindSurfacePathRequest(uint32 AsynPathQueryID);


	FRWLock CleanupLock;
	FRWLock CleanupPendingLock;
	FRWLock DeleteQueueLock;
	FRWLock NavModVolumesLock;
	FRWLock NavModComponentsLock;
	FRWLock NavPlanetComponentsLock;
	FRWLock NavInvokerLock;
	FRWLock InvokerDataLock;

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
	TSet<const UClass*> GetNavAreaClasses();
#else
	TSet<TObjectPtr<const UClass>> GetNavAreaClasses();
#endif

	TSet<ANavModifierVolume*>& GetNavModVolumes() { return NavModVolumes; };
	TSet<UNavModifierComponent*>& GetNavModComponents() { return NavModComponents; };
	TSet<TWeakObjectPtr<AActor>> GetNavInvokers() { return SurfaceNavInvokers; };


	void RegisterNavModVolume(ANavModifierVolume* NavModVolume);
	void UnregisterNavModVolume(ANavModifierVolume* NavModVolume);
	void RegisterNavModComponent(UNavModifierComponent* NavModComp);
	void UnregisterNavModComponent(UNavModifierComponent* NavModComp);


	float FindClosestPlanet(const AActor* CurrentActor, AActor*& PlanetActor);
	float FindClosestPlanet(const FVector& InLocation, AActor*& PlanetActor);


	void RegisterObservedPath(FSurfaceNavPathSharedPtr SharedPath)
	{
		check(IsInGameThread());
		if (ObservedPaths.Num() == 0)
		{
			NextObservedPathsTickInSeconds = SurfaceNavProxySettings::ObservedPathsTickInterval;
		}
		ObservedPaths.Add(SharedPath);
	}

	void RequestRePath(FSurfaceNavPathSharedPtr Path, ENavPathUpdateType::Type Reason);

	FSurfaceNavBlock* FindBlockByIndex(FIntVector BlockIndex) {
		FReadScopeLock rNavBlocksLock(NavBlocksLock);

		auto NavBlockPtr = NavBlocks.Find(BlockIndex);

		return !NavBlockPtr ? nullptr : *NavBlockPtr;
	}

	void PerformBackgroundWork(int ThreadId);

	/** This will queue up blocks near invokers to be processed. This can take time to run so it's best
		to call this from a separate thread. */
	void QueueBlocksNearInvokers();

	TSharedPtr<FSurfaceNavPointBase> FindClosestNavPointToLocation(const FVector& TargetLoc, const FVector& SourceLoc,
		const FVector& NonOffsetTargetLoc, const bool ForceNormalToDirFromSurface,
		const float& MinAngleFromGround, const float& MaxAngleFromGround, const float& AcceptanceRadius,
		const bool AllowWideSearch = false, const bool RequireWithinAcceptanceRadius = true) const;

	bool ProjectPoint(const FVector& TargetLoc, const FVector& SourceLoc, const FVector& NonOffsetTargetLoc,
		const bool ForceNormalToDirFromSurface, TSharedPtr<FSurfaceNavPointBase>& OutLocation,
		const float& MinAngleFromGround, const float& MaxAngleFromGround, const float& AcceptanceRadius,
		const bool AllowWideSearch = false, const bool RequireWithinAcceptanceRadius = true) const;


	/** Generates a random location in navigable space within given radius.
	 *	@return Return Value represents if the call was successful */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "SurfaceNavFindRandomNavPointNearby", ScriptName = "SurfaceNavFindRandomNavPointNearby"))
	static bool K2_SurfaceNavFindRandomNavPointNearby(UObject* WorldContextObject, FVector& RandomLocation, const float SearchRadius = 4000.f, const FVector TetherPoint = FVector::ZeroVector);

	bool GetRandomReachableSurfaceNavPointNearby(FSurfaceNavPathFindingQueryData Query, FVector& RandomLocation, const float SearchRadius = 4000.f);

	bool FindRandomNavPointNearby(FSurfaceNavPathFindingQueryData& Query, FVector& RandomLocation, const float SearchRadius = 4000.f, const FVector TetherPoint = FVector::ZeroVector);

	/** This is the original implementation.
	It's not the slowest, but it's not super fast either. */
	bool FindRandomNavPointNearby_Original(FSurfaceNavPathFindingQueryData& Query, FVector& RandomLocation, const float SearchRadius = 4000.f, const FVector TetherPoint = FVector::ZeroVector);

	/** This makes a path out to a random point.
	This is very slow. */
	bool FindRandomNavPointNearby_RandPath(FSurfaceNavPathFindingQueryData& Query, FVector& RandomLocation, const float SearchRadius = 4000.f, const FVector TetherPoint = FVector::ZeroVector);

	/** This simply picks a random point up to search radius away and that's it.
	This is the fastest. */
	bool FindRandomNavPointNearby_SimpleRandPoint(FSurfaceNavPathFindingQueryData& Query, FVector& RandomLocation, const float SearchRadius = 4000.f, const FVector TetherPoint = FVector(0));

	bool TestPathSync(FSurfaceNavPathFindingQueryData Query)
	{
		int32 dummy = 0; return TestPathSync(Query, &dummy);
	}

	bool TestPathSync(FSurfaceNavPathFindingQueryData Query, int32* NumVisitedNodes);

	FSurfaceNavPathResult FindPath(FSurfaceNavPathFindingQueryData& Query);

	/** Check all neighbors for a nav point to find the cheapest cost nav point. */
	TSharedPtr<FSurfaceNavPointBase> DetermineCheapestNavPoint(TSharedPtr<FSurfaceNavPointBase> NavPoint,
		const float& MinAngleFromGround, const float& MaxAngleFromGround, const UNavigationQueryFilter*& FilterClass) const; // , FSurfaceNavPoint*& CheapestNavPoint);

	/** Check if the passed in neighbor for the provided nav point is a cheaper cost and if so
		then use the cheaper nav point. */
	void DetermineCheaperNavPoint(TSharedPtr<FSurfaceNavPointBase> NavPointNeighbor, TSharedPtr<FSurfaceNavPointBase>& CheapestNavPoint,
		const float& MinAngleFromGround, const float& MaxAngleFromGround, const UNavigationQueryFilter*& FilterClass) const;

	void BuildNavPointsForPath(TArray<FVector>& PathPoints, TArray<float>& PointExpenses, TArray<FVector>& PointNormals, TArray<int64>& CustomLinkIDs, TSharedPtr<FSurfaceNavPointBase>& StartNavPoint,
		TSharedPtr<FSurfaceNavPointBase>& EndNavPoint, FSurfaceNavPathFindingQueryData& Query);

	void DrawPath(TArray<FVector>& PathPoints);


	void ProcessBlockQueue(int NumToProcess);


	void ProcessBlockGridPointQueue(int NumToProcess);

	void ProcessPendingDirtyQueue();

	void ProcessDeletesQueues();
	void ProcessPendingDeletesQueue();
	void ProcessCleanupQueues();
	void ProcessPendingCleanupQueue();
	void ProcessDelayedCleanupTasksAsync();

	void ReleaseBlockCallback(const FIntVector& BlockKey);
	void ReleaseGridPointCallback(FSurfaceNavGridPoint* GridPoint);

	void BlockOverlapsFoundCallback(const FIntVector& BlockKey);

	void BlockNavUpdatedCallback(const FIntVector& BlockKey);

	void BlockMarkedDirtyCallback(const FIntVector& BlockKey);

	void NavigableComponentUpdated(const UActorComponent& Comp);

	void NavigableComponentUpdated(const FVector& Velocity, const FBox& BoundingBox, const bool IsBeingDestroyed);

	void QueueBlockPendingDirty(const FIntVector& IntLoc);

	void QueueFinishProcessingBlockGridPoint(FIntVector BlockKey);

	void QueueBlocksPendingDirtyWithinBounds(const FBox& InBoundsBox, const FBox& InExclusionBoundsBox);

	void UpdateInvokerLocations();

	void ProcessUpdatedComponents();

	void ProcessUpdatedComponent(FUpdatedComponentQueueItem& CompQueItem, TArray<FVector> MyPlayerLocations,
		int& CompUpdateCount, TSet<FIntVector>& UpdatedComponentsSet, TSet<FUpdatedComponentQueueItem>& NewReprocessComponents,
		bool IgnoreVelocity = false, bool IgnoreBeingDestroyed = false);


	/** Draws debug data representing different parts of the navigation data. This may
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
	void DrawDebugNavData(const float DrawRadius = 2000.f,
		const bool ShowBlocks = false, const bool ShowGridPoints = false, const bool ShowNavPoints = false,
		const bool ShowNavPointNormals = false, const bool ShowNeighborDirLines = false, const bool ShowWhereNavigationEnds = false,
		const bool ShowNavLinkPoints = false, const float Duration = 1.f);

	void NeighborDebugLine(const FVector& StartLocation, const FVector& EndLocation, const float Duration = 1.f) const;

	void GridPointFinished(FSurfaceNavGridPoint* CurGridPoint, int Phase);

	/** Get the Surface Nav System for the current world. */
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject"))
	static USurfaceNavigationSystem* GetSurfaceNavigationSystem(UObject* WorldContextObject);


	/** Disable Surface Nav generation. */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void DisableSurfaceNavGeneration();

	/** Enable Surface Nav generation. */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void EnableSurfaceNavGeneration();


	static void QueueNavPointToDelete(TSharedPtr<FSurfaceNavPointBase> NavPoint) {

		USurfaceNavigationSystem* const& CurNavSys = USurfaceNavigationSystem::CurrentSurfaceNavSystem.Get();

		if (CurNavSys)
		{
			CurNavSys->NavPointsPendingDelete.Enqueue(NavPoint);
		}
		else
		{
			// Do nothing, this should only happen if the process is ending.
			//delete NavPoint;
		}
	}

	static void QueueNavGridPointToDelete(FSurfaceNavGridPoint* NavGridPoint) {
		USurfaceNavigationSystem* const& CurNavSys = USurfaceNavigationSystem::CurrentSurfaceNavSystem.Get();

		if (CurNavSys)
		{
			//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::QueueNavGridPointToDelete(): Queue"));

			check(!NavGridPoint->IsValid());
			CurNavSys->NavGridPointsPendingDelete.Enqueue(NavGridPoint);
		}
		else
		{
			// Do nothing, this should only happen if the process is ending.
			//delete NavGridPoint;
		}
	}

	static void QueueBlockToDelete(FSurfaceNavBlock* NavBlock) {
		USurfaceNavigationSystem* const& CurNavSys = USurfaceNavigationSystem::CurrentSurfaceNavSystem.Get();

		if (CurNavSys)
		{
			CurNavSys->NavBlocksPendingDelete.Enqueue(NavBlock);
		}
		else
		{
			// Do nothing, this should only happen if the process is ending.
			//delete NavBlock;
			UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::QueueBlockToDelete(): Can't delete nav block"));
		}
	}

	static void QueueNavLinkToDelete(FSurfaceNavLinkData* NavLink, USurfaceNavigationSystem* const& CurNavSys) {
		if (CurNavSys)
		{
			CurNavSys->NavLinksPendingDelete.Enqueue(NavLink);
		}
		else
		{
			// Don't do this, could cause crash
			//delete NavLink;
		}
	}

	static void QueuePathNodeMapToDelete(TMap<TSharedPtr<FSurfaceNavPointBase>, PathNode*>& NodePoints, USurfaceNavigationSystem* const& CurNavSys) {
		if (CurNavSys)
		{
			CurNavSys->PathNodeMapsPendingDelete.Enqueue(NodePoints);
		}
		else
		{
			for (TPair<TSharedPtr<FSurfaceNavPointBase>, PathNode*>& NodePointPair : NodePoints)
			{
				delete& NodePointPair.Value;
			}
			NodePoints.Empty();
		}
	}

	bool bIsProcessingNavLinks = false;

	FRWLock PlayerLocationLock;
	FRWLock VolumeBoundLock;
	FRWLock SurfaceNavCustomLinkLock;
	FRWLock WorkerLock1;
	FRWLock WorkerLock2;
	FRWLock WorkerLock3;
	FRWLock WorkerLock4;
	FRWLock DestroyLock;

	TSet<FSurfaceNavBounds> NavVolumeBounds;
	void RefreshVolumeBounds();
	void RefreshCustomLinks();
	void RefreshPlayerLocations();

	void QueueBlocksAffectedByLink(FSurfaceNavLinkData* NavLink);


#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
	TMap<uint32, FNavigationSystem::FCustomLinkOwnerInfo> GetCustomLinks() { return CustomLinksMap; }
#else
	TMap<FNavLinkId, FNavigationSystem::FCustomLinkOwnerInfo> GetCustomLinks() { return CustomNavLinksMap; }
#endif

	/** Copy of custom links so we can safely use it on background threads with locks */
	TMap<int64, FSurfaceNavLinkData> SurfaceNavCustomLinksMap;

	FRWLock& GetNavLinkLock() { return SurfaceNavCustomLinkLock; }

	TMap<int64, FSurfaceNavLinkData> GetSurfaceNavCustomLinks() { return SurfaceNavCustomLinksMap; }

	void SetInitTimer();
	void Init();

	virtual TArray<FSurfaceNavGridPoint*> GetGridPointsInBox(FVector CenterLoc, float Extent)
	{
		FBox TestBox(CenterLoc - Extent, CenterLoc + Extent);
		return GetGridPointsInBox(TestBox);
	}
	virtual TArray<FSurfaceNavGridPoint*> GetGridPointsInBox(FBox TestBox);
	virtual TArray<FSurfaceNavGridPoint*> GetGridPointsInRadius(FVector CenterLoc, float Radius);
	virtual TArray<FSurfaceNavPoint*> GetNavPointsInRadius(FVector CenterLoc, float Radius);

	inline bool IsReady() { return bInitDone && bNavHasGenerated; }

	void ReleaseAllBlocks();

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/** Calculates a path from PathStart to PathEnd and retrieves its approximate length.
	 *  @param PathStart - The start location.
	 *  @param PathEnd - The end location.
	 *  @param FilterClass - The filter class to use for pathfinding.
	 *  @param MinAngleLimit - The min angle limit to use for pathfinding.
	 *  @param MaxAngleLimit - The max angle limit to use for pathfinding.
	 *  @param AcceptanceRadius - Acceptance radius to use to determine if the end is reached during pathfinding.
	 *  @param MaxPathingIterations - Max pathing iterations to use during pathfinding.
	 *  @param PathLength - Outputs the resulting path length.
	 *  @return The navigation query result.
	 *  @NOTE potentially expensive, so use it with caution
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject"))
	static ENavigationQueryResult::Type GetSurfaceNavPathLength(UObject* WorldContextObject, const FVector& PathStart,
		const FVector& PathEnd, float& PathLength, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL,
		const float MinAngleLimit = 0.f, const float MaxAngleLimit = 180.f, const float AcceptanceRadius = 100.f,
		const int MaxPathingIterations = 10000);

	/** Determines if the target location can be reached with the provided conditions.
	 *  @param PathStart - The start location.
	 *  @param PathEnd - The end location.
	 *  @param FilterClass - The filter class to use for pathfinding.
	 *  @param MinAngleLimit - The min angle limit to use for pathfinding.
	 *  @param MaxAngleLimit - The max angle limit to use for pathfinding.
	 *  @param AcceptanceRadius - Acceptance radius to use to determine if the end is reached during pathfinding.
	 *  @param MaxPathingIterations - Max pathing iterations to use during pathfinding.
	 *  @param Success - True if target can be reached.
	 *  @NOTE potentially expensive, so use it with caution
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Navigation|Surface Nav", meta = (WorldContext = "WorldContextObject"))
	static void CanReachTarget(UObject* WorldContextObject, const FVector& PathStart,
		const FVector& PathEnd, bool& OutSuccess, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL,
		const float MinAngleLimit = 0.f, const float MaxAngleLimit = 180.f, const float AcceptanceRadius = 100.f,
		const int MaxPathingIterations = 10000);

	/** Attempts to find a complete path to target with the provided conditions and returns the resulting path data.
	 *  @param PathStart - The start location.
	 *  @param PathEnd - The end location.
	 *  @param FilterClass - The filter class to use for pathfinding.
	 *  @param MinAngleLimit - The min angle limit to use for pathfinding.
	 *  @param MaxAngleLimit - The max angle limit to use for pathfinding.
	 *  @param AcceptanceRadius - Acceptance radius to use to determine if the end is reached during pathfinding.
	 *  @param MaxPathingIterations - Max pathing iterations to use during pathfinding.
	 *  @param Success - True if a complete path was found.
	 *  @param PathData - Resulting pathing data.
	 *  @NOTE potentially expensive, so use it with caution
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Navigation|Surface Nav", meta = (WorldContext = "WorldContextObject"))
	static void GetPathToTarget(UObject* WorldContextObject, const FVector& PathStart,
		const FVector& PathEnd, bool& OutSuccess, TArray<FSurfaceNavPathPointData>& OutPathData,
		TSubclassOf<UNavigationQueryFilter> FilterClass = NULL, const float MinAngleLimit = 0.f,
		const float MaxAngleLimit = 180.f, const float AcceptanceRadius = 100.f, const int MaxPathingIterations = 10000);

	bool CanReachTarget(FSurfaceNavPathFindingQueryData Query);
	void GetPathToTarget(FSurfaceNavPathFindingQueryData InQuery, bool& OutSuccess, TArray<FSurfaceNavPathPointData>& OutPathData);


	/** Calculates a path from PathStart to PathEnd and retrieves its approximate length.
	@NOTE potentially expensive, so use it with caution */
	ENavigationQueryResult::Type GetSurfaceNavPathLength(float& PathLength, FSurfaceNavPathFindingQueryData& QueryData);

};
