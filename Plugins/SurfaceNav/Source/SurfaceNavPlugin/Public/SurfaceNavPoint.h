// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "SurfaceNavPointBase.h"
#include "SurfaceNavCommon.h"
#include "NavAreas/NavArea.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "Misc/ScopeRWLock.h"
#include "Containers/StaticArray.h"

struct FSurfaceNavGridPoint;

/** Actor which traces for world objects with collisions */
struct SURFACENAVPLUGIN_API FSurfaceNavPoint : FSurfaceNavPointBase
{
	typedef FSurfaceNavPointBase Super;

public:
	// Sets default values for this actor's properties

	FSurfaceNavPoint();

	virtual ~FSurfaceNavPoint();

	virtual const FVector CalcNormalFromNeighbors(bool bForceNoRecalc = false) override;

protected:
	const static int MaxNeighborCount = 8;

	/** Each direction could have multiple points it "could" be neighbors with
	so we need to prioritize those neighbors. The highest priority valid neighbor
	is considered the current neighbor. Right now, we allow up to 3 priority levels,
	but it could be higher in the future. */
	TStaticArray<TStaticArray<TWeakPtr<FSurfaceNavPointBase>, 3>, 8> Neighbors;

	virtual const float CalcAngleFromGround() override;

	virtual bool ResetNeighbor(TSharedPtr<FSurfaceNavPointBase>& NavPoint, const int NeighborIndex, const int NeighborPriorityIndex) override;

private:

	inline int NeighborCountAtIndex(int Index)
	{
		return (Neighbors[Index][0] != nullptr)
			+ (Neighbors[Index][1] != nullptr)
			+ (Neighbors[Index][2] != nullptr);
	}

	FThreadSafeCounter NeighborCount;

	float CachedAngleFromGround = 0.f;

	FVector CachedNormal = FVector::ZeroVector;

	/** Calculates the normal for the current point based on vectors from this nav point
	* to the target neighbor, or alternate, then from that neighbor to an adjacent neighbor.
	*/
	virtual FVector CalcNormalFromNeighbor(TSharedPtr<FSurfaceNavPointBase>& Neighbor, const FVector& AvgCenter, TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighbor,
		TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighborAlt, TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighborAlt2, TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighborAlt3) const override;

	virtual FVector CalcNormalFromNeighborAndAdjacentNeighbor(TSharedPtr<FSurfaceNavPointBase>& Neighbor, const FVector& AvgCenter,
		TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighbor) const override;

public:

	FSurfaceNavGridPoint* OwningGridPoint = nullptr;

	FIntVector VectorIndex;

	FVector GroundNormal = FVector::ZeroVector;

	inline virtual const FVector GetNormal(bool bForceNoRecalc = false) override { return CalcNormalFromNeighbors(bForceNoRecalc); }

	virtual const bool IsValid() const override { return !bIsDestroying && !bIsDestroyed && NeighborCount.GetValue() >= 0 && NeighborCount.GetValue() <= MaxNeighborCount; }

	virtual void ClearNeighbor(int Index, int PriorityIndex, bool UseLock = false) override;
	virtual void SetNeighbor(TSharedPtr<FSurfaceNavPointBase>& Neighbor, int Index, int PriorityIndex, bool UseLock = false) override;
	//virtual FSurfaceNavPointBase* GetNeighborAt(int Index) override;
	//virtual void GetNeighborAt(int Index, TSharedPtr<FSurfaceNavPointBase>& NavPointBase) override;
	virtual void GetNeighborAt(int Index, TSharedPtr<FSurfaceNavPointBase>& NavPoint) override;
	virtual void GetNeighborAt(int Index, int PriorityIndex, TSharedPtr<FSurfaceNavPointBase>& NavPoint) override;
	virtual FVector GetNeighborVectorAtIndex(int index) override;
	virtual bool NeighborsContainNavPoint(TSharedPtr<FSurfaceNavPointBase>& PossibleNeighbor) override;
	inline virtual bool HasLocalNeighbors() const override { return NeighborCount.GetValue() > 0 && NeighborCount.GetValue() <= MaxNeighborCount; };
	virtual void FindOpposingNavPoint(TSharedPtr<FSurfaceNavPointBase>& OpposingNavPoint) override;

	virtual int GetHighestPriorityForIndex(int Index) override;

	inline virtual int FindLowestPriority(int Index) override
	{
		// Any neighbor index along axis (0 - 3) allow for all priority indexes
		if (Index < 4)
			return 2;

		// NeighborIndex of 4 then test neighbors 0 and 2
		if (Index == 4)
			return FMath::Min(GetHighestPriorityForIndex(0), GetHighestPriorityForIndex(2));

		// NeighborIndex of 5 then test neighbors 1 and 2
		if (Index == 5)
			return FMath::Min(GetHighestPriorityForIndex(1), GetHighestPriorityForIndex(2));

		// NeighborIndex of 6 then test neighbors 0 and 3
		if (Index == 6)
			return FMath::Min(GetHighestPriorityForIndex(0), GetHighestPriorityForIndex(3));

		// NeighborIndex of 7 then test neighbors 1 and 3
		return FMath::Min(GetHighestPriorityForIndex(1), GetHighestPriorityForIndex(3));
	}

	virtual void SetVectorIndex(FIntVector InVectorIndex) override { VectorIndex = InVectorIndex; }
	virtual FIntVector GetVectorIndex() const override { return VectorIndex; }

	virtual void SetGroundNormal(FVector InGroundNormal) override { GroundNormal = InGroundNormal; }
	virtual FVector GetGroundNormal() const override { return GroundNormal; }

	virtual FSurfaceNavGridPoint* GetOwningGridPoint() const override { return OwningGridPoint; }
	virtual void SetOwningGridPoint(FSurfaceNavGridPoint* InOwningGridpoint) override { OwningGridPoint = InOwningGridpoint; }

	virtual void Reset() override;
	virtual void Init() override;
};

