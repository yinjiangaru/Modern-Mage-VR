// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "SurfaceNavCommon.h"
#include "NavAreas/NavArea.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "Misc/ScopeRWLock.h"
#include "Containers/StaticArray.h"

struct FSurfaceNavGridPoint;

/** Actor which traces for world objects with collisions */
struct SURFACENAVPLUGIN_API FSurfaceNavPointBase
{

public:
	// Sets default values for this actor's properties

	FSurfaceNavPointBase();

	virtual ~FSurfaceNavPointBase();

	virtual const FVector CalcNormalFromNeighbors(bool bForceNoRecalc = false) { return FVector::ZeroVector; }

protected:

	virtual const float CalcAngleFromGround() { return 0.0f; }

	virtual bool ResetNeighbor(TSharedPtr<FSurfaceNavPointBase>& NavPoint, const int NeighborIndex, const int NeighborPriorityIndex) { return false; }

private:

	/** Calculates the normal for the current point based on vectors from this nav point
	* to the target neighbor, or alternate, then from that neighbor to an adjacent neighbor.
	*/
	virtual FVector CalcNormalFromNeighbor(TSharedPtr<FSurfaceNavPointBase>& Neighbor, const FVector& AvgCenter, TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighbor,
		TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighborAlt, TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighborAlt2, TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighborAlt3) const { return FVector::ZeroVector; }

	virtual FVector CalcNormalFromNeighborAndAdjacentNeighbor(TSharedPtr<FSurfaceNavPointBase>& Neighbor, const FVector& AvgCenter,
		TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighbor) const { return FVector::ZeroVector; }

public:

	// The expense modifier is used to help prevent AI crowding.
	// When an AI select this point as a path, this point's expense is increased slightly.
	// When an AI is evaluating a path and checks this point, this point's expense is decreased slightly.
	float ExpenseModifier = 0.f;

	uint8 bIsDestroying : 1,
		bIsDestroyed : 1,
		CalcAngleFromGroundCached : 1,
		CalcNormalCached : 1;

	FVector WorldLocation = FVector::ZeroVector;

	// TODO: Make this an FVector3f after dropping support for UE 4.27
	FVector HitNormal = FVector::ZeroVector;

	UNavArea* NavArea = nullptr;

	inline virtual const FVector GetNormal(bool bForceNoRecalc = false) { return FVector::ZeroVector; }
	inline const float GetAngleFromGround() { return CalcAngleFromGround(); }

	inline const float GetExpenseModifier() { return ExpenseModifier; }

	inline void IncreaseExpense() { ExpenseModifier = FMath::Min(ExpenseModifier + SurfaceNavProxySettings::ExpenseModifierUpStepSq, SurfaceNavProxySettings::MaxExpenseModifierSq); }
	inline void DecreaseExpense() { ExpenseModifier = FMath::Max(ExpenseModifier - SurfaceNavProxySettings::ExpenseModifierDownStepSq, 0.f); }

	//void BeginDestroy() { bIsDestroying = true; }
	virtual void Destroy();
	virtual const bool IsDestroying() const { return bIsDestroying; }
	virtual const bool IsDestroyed() const { return bIsDestroyed; }
	virtual const bool IsValid() const { return !bIsDestroying && !bIsDestroyed; }

	virtual void ClearNeighbor(int Index, int PriorityIndex, bool UseLock = false) {}
	virtual void SetNeighbor(TSharedPtr<FSurfaceNavPointBase>& Neighbor, int Index, int PriorityIndex, bool UseLock = false) {}
	//virtual FSurfaceNavPointBase* GetNeighborAt(int Index) { return SurfaceNavProxySettings::EmptyNavPointBase; }
	//virtual void GetNeighborAt(int Index, TSharedPtr<FSurfaceNavPointBase>& NavPointBase) { }
	virtual void GetNeighborAt(int Index, TSharedPtr<FSurfaceNavPointBase>& NavPoint) { }
	virtual void GetNeighborAt(int Index, int PriorityIndex, TSharedPtr<FSurfaceNavPointBase>& NavPoint) { }
	virtual FVector GetNeighborVectorAtIndex(int index) { return FVector::ZeroVector; }
	virtual bool NeighborsContainNavPoint(TSharedPtr<FSurfaceNavPointBase>& PossibleNeighbor) { return false; }
	inline virtual bool HasLocalNeighbors() const { return false; };
	inline bool HasLinkNeighbors() const { return CustomLinkNeighbors.Num() > 0; };
	inline bool HasNeighbors() const { return HasLocalNeighbors() || HasLinkNeighbors(); };
	virtual void FindOpposingNavPoint(TSharedPtr<FSurfaceNavPointBase>& OpposingNavPoint) {}

	virtual int GetHighestPriorityForIndex(int Index) { return -1; }

	// This holds all neighbors related to connections between a navlink point
	// and regular nav points.
	TArray<TWeakPtr<FSurfaceNavPointBase>> CustomLinkNeighbors;


	virtual void SetVectorIndex(FIntVector InVectorIndex) { }
	virtual FIntVector GetVectorIndex() const { return FIntVector::ZeroValue; }

	virtual void SetGroundNormal(FVector InGroundNormal) { }
	virtual FVector GetGroundNormal() const { return FVector::ZeroVector; }

	virtual void SetOwningGridPoint(FSurfaceNavGridPoint* InOwningGridpoint) { }
	virtual FSurfaceNavGridPoint* GetOwningGridPoint() const { return nullptr; }

	virtual void SetOtherLinkPoint(TSharedPtr<FSurfaceNavPointBase> InOtherLinkPoint) { }
	virtual TSharedPtr<FSurfaceNavPointBase> GetOtherLinkPoint() const { return nullptr; }

	virtual void SetLinkId(int64 InLinkId) { }
	virtual int64 GetLinkId() const { return 0; }

	inline virtual int FindLowestPriority(int Index) { return 0; }

	virtual void ResetLinkNeighbor();
	virtual void Reset();
	virtual void Init();	
};
