// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "WorldCollision.h"
#include "SurfaceNavCommon.h"
#include "Math/Quat.h"
#include "Engine/World.h"
#include "Math/IntVector.h"
#include "Math/Vector.h"
#include "CollisionQueryParams.h"

//#include "Runtime/Engine/Classes/Engine/EngineTypes.h"

struct FSurfaceNavPoint;
struct FSurfaceNavPointPool;
struct FSurfaceNavBlock;
struct FSurfaceNavGridPointTraceResult;

struct SURFACENAVPLUGIN_API FSurfaceNavGridPoint
{
private:
	uint8 TraceHitCount : 3,
		bForceIsOrphan : 1,
		bOrphanedWithSiblings : 1,
		bIsNarrow : 1,
		bIsDestroying : 1,
		bIsDestroyed : 1;

public:
	// Sets default values for this actor's properties

	FSurfaceNavGridPoint(
		UWorld& initWorld,
		const FCollisionShape& initGridPointTestSphere,
		const FCollisionQueryParams& initGridPointOverlapSphereQueryParams,
		const FCollisionQueryParams& initGridPointTraceQueryParams,
		const FGridPointFinishedDelegate& initGridPointFinishedDel,
		FSurfaceNavPointPool& initNavPointPool
	);

	FSurfaceNavBlock* OwningBlock;

	bool HasHitInAxisDir(FIntVector const& AxisDir) const
	{
		// *******************************
		// TODO: Figure out why the map doesn't work the same as below
		// if statements
		//const int8* OffsetPtr = AxisIndexMap.Find(AxisDir);
		//return LocalAxisHits & (0x1 << (OffsetPtr == nullptr ? 0 : *OffsetPtr));
		// *******************************

		if (AxisDir.X == -1)
			return LocalAxisHits & 0x1;
		if (AxisDir.X == 1)
			return LocalAxisHits & (0x1 << 1);

		if (AxisDir.Y == -1)
			return LocalAxisHits & (0x1 << 2);
		if (AxisDir.Y == 1)
			return LocalAxisHits & (0x1 << 3);

		if (AxisDir.Z == -1)
			return LocalAxisHits & (0x1 << 4);
		if (AxisDir.Z == 1)
			return LocalAxisHits & (0x1 << 5);

		return false;
	}

	FVector GetAvgLocation() { return CalcAvgLocation(); };
	FVector GetAvgNormal() { return CalcAvgNormal(); };

protected:
		
	FVector CachedAvgNormal = FVector::ZeroVector;
	FVector CalcAvgNormal();

	FVector CachedAvgLocation = FVector::ZeroVector;
	FVector CalcAvgLocation();

	// Hold values indicating which axis+dir have a hit
	int8 LocalAxisHits = 0;

	// We need this to compare to sibling grid points so
	// it needs the inverse as LocalAxisHits
	int8 SiblingAxisHits = 0;

	//TStaticBitArray<6> OrphanedSiblings; // = TStaticBitArray<>(false, 6);
	//TStaticBitArray<6> SiblingGridPoints; // = TStaticBitArray<>(false, 6);

	//int CountSetBits_6(TStaticBitArray<6>& MyBitArray);

	UWorld& World;
	const FCollisionShape& GridPointTestSphere;
	const FCollisionQueryParams& GridPointOverlapSphereQueryParams;
	const FCollisionQueryParams& GridPointTraceQueryParams;

	const FGridPointFinishedDelegate& GridPointFinishedDel;

	FSurfaceNavPointPool& NavPointPool;

	void SetLocalAxisHit(int8 AxisHit) { LocalAxisHits |= AxisHit; };
	void UnsetLocalAxisHit(int8 AxisHit) { LocalAxisHits &= ~AxisHit; };

	// Check is the trace hits are opposing directions. If they are then we consider
	// the grid point a narrow corridor.
	void DetermineIfNarrowCorridor(FSurfaceNavGridPointTraceResult& TraceResult, int HitResultIndex1, int HitResultIndex2);

	// This function was used in the past to attempt to determine if this grid point should be an orphan based
	// on if it has sibling which are orphaned, but it had issues so not used right now.
	//void TestForOrphanedSibling(FIntVector SiblingVectorIndexOffset, int LocalIndex, int SiblingIndex);
	// 
	//void UpdateOrphanedWithSiblings();

	void TraceFromGridPoint(const FVector& startLoc, const FVector& endLoc);
	const bool IsBoxedIn();
	const bool HasNoSurface();

	void Init();
	void InitNavPoints();

	//const void DrawDebugNavPoint(const FIntVector& NavPointIndex);

public:
	
	FIntVector VectorIndex;
	FVector WorldLocation;

	void FinalizeNavPointsAlt(int Phase);

	void SetSiblingAxisHit(int8 AxisHit) { SiblingAxisHits |= AxisHit; }
	void UnsetSiblingAxisHit(int8 AxisHit) { SiblingAxisHits &= ~AxisHit; }
	void SetForceIsOrphaned(bool IsOrphaned) { bForceIsOrphan = IsOrphaned; }

	UWorld& GetWorld() { return World; }

	TMap<FIntVector, TSharedPtr<FSurfaceNavPointBase>> NavPoints;

	void FindOpposingNavPoint(FSurfaceNavPoint* const& CurrentNavPoint, TSharedPtr<FSurfaceNavPointBase>& OpposingNavPoint);
	void Reset();
	void ResetAllBitFlags();
	void ResetSiblingBitFlag(int AxisIndex);
	void SetLocation(const FVector location);
	void SetVectorIndex(const FIntVector vectorIndex);

	void GridPointOverlapCheckResponse(const FTraceHandle& TraceHandle, FOverlapDatum& OverlapData);
	void GridPointTraceCheckResponse(const FTraceHandle& TraceHandle, FTraceDatum& TraceData);

	void AsyncCheckGridPointForOverlaps();
	void CheckGridPointForOverlaps();

	void ConfigNavPoints(FSurfaceNavGridPointTraceResult& TraceResult);

	FSurfaceNavGridPoint* GetSiblingGridPoint(FIntVector SiblingDirection);

	void BeginDestroy() { bIsDestroying = true; }
	void Destroy() { bIsDestroyed = true; }
	const bool IsValid() { return !bIsDestroying && !bIsDestroyed; }
	const bool IsDestroyed() { return bIsDestroyed; }
	const bool IsDestroying() { return bIsDestroying; }

	bool IsOrphaned(bool IncludeSiblings = true) const;

	bool IsNarrow() { return bIsNarrow; }
};
