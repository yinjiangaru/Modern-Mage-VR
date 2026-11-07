// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "WorldCollision.h"
#include "Containers/UnrealString.h"
#include "Math/UnrealMathUtility.h"
#include "NavigationSystem.h"
#include "NavModifierVolume.h"
#include "Engine/Polys.h"
#include "Math/Plane.h"
#include "Math/Vector.h"
#include "CoreMinimal.h"
#include "NavModifierComponent.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/PlatformMath.h"
#include "Templates/Decay.h"
#include "Templates/IsFloatingPoint.h"
#include "Templates/IsIntegral.h"
#include "CoreMinimal.h"
#include "Misc/MemStack.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "Model.h"
#include "Engine/Brush.h"
#include "GeomTools.h"
#include "Engine/Polys.h"
#include "TimerManager.h"
#include <memory>
#include "Math/Quat.h"
#include "Components/BrushComponent.h"
#include "Runtime/Engine/Classes/Components/BoxComponent.h"
#include "Runtime/Engine/Classes/Components/SphereComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_LowHeight.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavAreas/NavArea_Default.h"
#include "Misc/ScopeRWLock.h"

#include "SurfaceNavCommon.h"
#include "SurfaceNavPoint.h"


class USurfaceNavigationSystem;
struct FSurfaceNavGridPoint;
struct FSurfaceNavGridPointPool;
struct FSurfaceNavLinkData;


struct SURFACENAVPLUGIN_API FSurfaceNavGridPointSingleTraceResult
{
	FSurfaceNavGridPointSingleTraceResult() :
		Hit(0), IsInNavModVolume(0)
	{}

	uint8 Hit : 1,
		IsInNavModVolume : 1;

	FIntVector TraceIndex;
	FVector ImpactLocation;
	FVector ImpactNormal;
	FVector GroundNormal;
	UNavArea* NavArea = nullptr;

	TArray<FSurfaceNavLinkData> PointALinks;
	TArray<FSurfaceNavLinkData> PointBLinks;
};


struct SURFACENAVPLUGIN_API FSurfaceNavGridPointTraceResult
{

	FSurfaceNavGridPointTraceResult(FIntVector initGridPointIndex, FVector initWorldLocation, bool initOverlapFound);

	FIntVector GridPointIndex;
	FVector WorldLocation;
	uint8 OverlapFound : 1, // = false;
		IsOrphaned : 1, // = false;
		TraceHitCount : 3; // = 0;

	TStaticArray<FSurfaceNavGridPointSingleTraceResult, 6> SingleTraceResults;

	bool IsBoxedIn() { return TraceHitCount >= 6; };
};

struct SURFACENAVPLUGIN_API FSurfaceNavBlock
{

public:
	// Sets default values for this actor's properties
	FSurfaceNavBlock(UWorld* InWorld);
	virtual ~FSurfaceNavBlock();

	TArray<TArray<TArray<FSurfaceNavGridPoint*>>> SurfaceNavGridPoints;

	FSurfaceNavGridPointPool* GridPointPool;

	virtual void SetLocation(const FVector& InLocation)
	{
		ActorLoc = FVector(InLocation);
		Bounds = FBox::BuildAABB(ActorLoc, FVector(SurfaceNavProxySettings::BlockExtent));
	}

	virtual FVector& GetLocation() { return ActorLoc; }
	virtual FBox& GetBounds() { return Bounds; }
	virtual UWorld* GetWorld() { return World.IsValid() ? World.Get() : nullptr; }
	virtual void SetWorld(UWorld* InWorld) { World = InWorld; }

	void Destroy() { IsDestroyed = true; }
	const bool IsValid() { return !IsDestroyed; }

	// This is used to make sure all grid points have been released before deleting this block
	FThreadSafeCounter LivingGridPointCount = 0;
	uint8 CleanupRetryCount = 0;

private:
	FBox Bounds;
	TWeakObjectPtr<UWorld> World;

	TArray<FSurfaceNavGridPointTraceResult> TraceResults;

protected:

	uint32 IsDestroyed : 1, // = false;
		IsFlatWorld : 1, // = true;
		bIsActive : 1, // = false;
		bIsProcessingGrid : 1, //= false;
		bIsDirty : 1, //= true;
		bIsPendingCleanup : 1,
		SingleAxisVoxelCount : 7,
		FoundSurfaceCount : 19; // = 0; // 19 bits - Max is 64 cubed = 262,144
	

	TSet<TWeakObjectPtr<UPrimitiveComponent>> OverlappingComponents;
	
	bool CheckGridPointForOverlaps(const FVector& GridPoint);

	void DeleteTraceResultsData(); 

	bool TraceFromGridPoint(const FVector& startLoc, const FVector& endLoc, const FCollisionQueryParams& GridPointTraceQueryParams,
		FSurfaceNavGridPointTraceResult& TraceResult, const int SingleTraceIndex, TArray<TTuple<INavRelevantInterface*, FBox>> IntersectingNavMods,
		TArray<FSurfaceNavLinkData> IntersectingNavLinks);

	void FinalizeNavPoints();

	FCollisionQueryParams CollisionQueryParams;

	float GridPointDist;
	float GridPointStartOffset;
	float AxisStart;

	FVector ActorLoc;
	FVector* PlanetLoc = nullptr;

	void PreInitGridPoints();
	void ClearGridPoints();

	void GridPointCanBeReleased(const FIntVector& GridPointIndex);
	void ReleaseAllGridPoints();
	void ReleaseGridPoint(FSurfaceNavGridPoint*& GridPoint);


#pragma region Functions for testing if point is inside complex shapes

#if ENGINE_MAJOR_VERSION < 5
	int RayMeshIntersectionCount(FVector rayOrigin, FVector rayVector, UModel* Brush);
#else
	int RayMeshIntersectionCount(FVector rayOrigin, FVector rayVector, TObjectPtr<UModel> Brush);
#endif

	bool DoesLineIntersectPoly(TArray<FVector> Verts, FVector Normal, FVector Start, FVector End, FVector& Intersect, bool& ForceInside, bool& ForceOutside);

#pragma endregion


	void DeterminePlanet();

#if WITH_EDITOR
	const void DrawDebugNavPoints();
#endif

public:

	int GetSingleAxisVoxelCount() { return SingleAxisVoxelCount;  }

	bool HasFoundSurface();

	void CheckIfShouldRemove();

	bool IsReadyToRelease();

	void BlockOverlapCheckResponse(const FTraceHandle& TraceHandle, FOverlapDatum& TraceData);

	void GridPointOverlapCheckResponse(const FTraceHandle& TraceHandle, FOverlapDatum& TraceData);
	void GridPointTraceCheckResponse(const FTraceHandle& TraceHandle, FTraceDatum& TraceData);

	void AsyncCheckForOverlappingActors();

	/** Check for overlapping actors if conditions are met. Returns true if an overlap test was done. */
	bool CheckForOverlappingActors(UWorld* MyWorld);
	bool BlockOverlapTest(TArray<struct FOverlapResult>& OutOverlaps, UWorld* MyWorld);
	void CheckForSurface();
	void FinishProcessingGridPoints();

	bool TraceForNav(FHitResult& Hit, const FVector& StartLoc, const FVector& EndLoc,
		const FCollisionShape& TraceSphere, const FCollisionQueryParams& GridPointTraceQueryParams);

	bool CheckPointForSurface(FIntVector& VectorIndex, FVector& GridPointLoc, FSurfaceNavGridPoint*& GridPoint, FCollisionQueryParams& collisionQueryParams);

	bool IsAvailableForPathing() const { return bIsActive && !bIsPendingCleanup && !bIsProcessingGrid && !bIsDirty; };
	bool IsDirty() const { return bIsDirty; };
	bool IsActive(bool bTestPendingCleanup = true) const { return bIsActive && (!bTestPendingCleanup || !bIsPendingCleanup); };

	void InvalidateBlockProcessing(bool TriggerRemoval = false);

	const FIntVector* BlockKey;

	const FReleaseBlockDelegate* ReleaseBlockDel;
	const FBlockOverlapsFoundDelegate* BlockOverlapsFoundDel;
	const FHandleBlockMarkedDirtyDelegate* HandleBlockMarkedDirtyDel;
	const FBlockNavUpdatedDelegate* BlockNavUpdatedDel;
	const FActorsInBlockChangedDelegate* ActorsInBlockChangedDel;
	FOverlapDelegate BlockOverlapDel;
	const FGridPointFinishedDelegate* GridPointFinishedDel;

	void MarkDirty();
	void ClearDirty();

	// Cleans up values before being destroyed.
	void ResetAll();

	/** Call this function when you need to activate this block and do the initial check for overlaps. */
	void Activate();

	/** Call this function when you need to refresh an active block which includes checking for overlaps again. */
	bool Refresh(UWorld* MyWorld);

	void Init();

	FRWLock NavBlockLock;
	FRWLock NavPointLock;
};

