// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavPoint.h"
#include "SurfaceNavigationSystem.h"
#include "SurfaceNavGridPoint.h"
#include "Kismet/KismetMathLibrary.h"

FSurfaceNavPoint::FSurfaceNavPoint()
{
	Init();
}

FSurfaceNavPoint::~FSurfaceNavPoint()
{
}

inline void FSurfaceNavPoint::Init()
{
	Super::Init();

	bIsDestroying = 0;
	bIsDestroyed = 0;
	CalcAngleFromGroundCached = 0;
	CalcNormalCached = 0;

	ExpenseModifier = 0.f;
	CachedAngleFromGround = 0.f;

	OwningGridPoint = nullptr;
	NeighborCount.Set(0);

	for (int Index = 0; Index < Neighbors.Num(); Index++)
	{
		auto& Item = Neighbors[Index];
		for (int PriorityIndex = 0; PriorityIndex < Item.Num(); PriorityIndex++)
		{
			Item[PriorityIndex] = nullptr;
		}
	}
}

/** This is the old code ***/
inline bool FSurfaceNavPoint::ResetNeighbor(TSharedPtr<FSurfaceNavPointBase>& NavPoint, const int NeighborIndex, const int NeighborPriorityIndex)
{
	if (!NavPoint)
		return false;

	TSharedPtr<FSurfaceNavPointBase> PinnedNeighborNeighbor = nullptr;
	NavPoint->GetNeighborAt(NeighborIndex, NeighborPriorityIndex, PinnedNeighborNeighbor);

	if (PinnedNeighborNeighbor.Get() == this) // || !NeighborNeighbor->IsValid()))
	{
		auto const& NeighborOwningGridPoint = NavPoint->GetOwningGridPoint();
		if (NeighborOwningGridPoint && OwningGridPoint->OwningBlock != NeighborOwningGridPoint->OwningBlock)
		{
			// Make sure the neighboring block is still valid
			if (NeighborOwningGridPoint->IsValid() && NeighborOwningGridPoint->OwningBlock->IsActive() && NeighborOwningGridPoint->OwningBlock->IsValid())
			{
				// Different nav block so need to lock while clearing
				NavPoint->ClearNeighbor(NeighborIndex, NeighborPriorityIndex, true);
			}
			else
			{
				// This is still a different nav block, but the neighbor is already being 
				// cleaned up and locking here could create a deadlock.
				// We still need to clear the neighbor, otherwise a crash could result.
				// Don't pass the parameter which instructs it to use a lock.
				NavPoint->ClearNeighbor(NeighborIndex, NeighborPriorityIndex);
			}
		}
		else
		{
			// It's the same nav block so don't attempt to lock while clearing
			NavPoint->ClearNeighbor(NeighborIndex, NeighborPriorityIndex);
		}

		return true;
	}

	return false;
}

//inline bool FSurfaceNavPoint::ResetNeighbor(TWeakPtr<FSurfaceNavPoint>& NavPoint, const int NeighborIndex, const int NeighborPriorityIndex)
//{
//	/*for (int i = 0; i < NavPoint->Neighbors.Num(); i++)
//	{*/
//		auto& NeighborItem = NavPoint->Neighbors[NeighborIndex];
//		/*for (int p = 0; p < NeighborItem.Num(); p++)
//		{*/
//			TWeakPtr<FSurfaceNavPoint>& NeighborNeighbor = NeighborItem[NeighborPriorityIndex];
//
//			if (!NeighborNeighbor)
//				return false;
//
//			if (NeighborNeighbor->WorldLocation != this->WorldLocation ||
//				NeighborNeighbor->HitNormal != this->HitNormal)
//				return false;
//
//			/*if (NeighborNeighbor->OwningGridPoint->VectorIndex != this->OwningGridPoint->VectorIndex ||
//				NeighborNeighbor->OwningGridPoint->WorldLocation != this->OwningGridPoint->WorldLocation)
//				continue;*/
//
//			if (NeighborNeighbor->OwningGridPoint != this->OwningGridPoint)
//				return false;
//
//			if (NeighborNeighbor != this)
//				UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavPoint::ResetNeighbor: Neighbor mismatch."));
//
//			// These numbers should never be different...need a better fix than looping
//			/*ensure(NeighborIndex == i);
//			if (NeighborIndex != i)
//				UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavPoint::ResetNeighbor: Neighbor index was %d, expected %d"), i, NeighborIndex);
//
//			check(NeighborPriorityIndex == p);*/
//
//			if (OwningGridPoint->OwningBlock != NavPoint->OwningGridPoint->OwningBlock)
//			{
//				// Make sure the neighboring block is still valid
//				if (NavPoint->OwningGridPoint->IsValid() && NavPoint->OwningGridPoint->OwningBlock->IsActive() && NavPoint->OwningGridPoint->OwningBlock->IsValid())
//				{
//					// Different nav block so need to lock while clearing
//					NavPoint->ClearNeighbor(NeighborIndex, NeighborPriorityIndex, true);
//				}
//				else
//				{
//					// This is still a different nav block, but the neighbor is already being 
//					// cleaned up and locking here could create a deadlock.
//					// We still need to clear the neighbor, otherwise a crash could result.
//					// Don't pass the parameter which instructs it to use a lock.
//					NavPoint->ClearNeighbor(NeighborIndex, NeighborPriorityIndex);
//				}
//			}
//			else
//			{
//				// It's the same nav block so don't attempt to lock while clearing
//				NavPoint->ClearNeighbor(NeighborIndex, NeighborPriorityIndex);
//			}
//
//			// We found it so can end now
//			return true;
//		//}
//	//}
//
//	//return false;
//}

inline void FSurfaceNavPoint::Reset()
{
	ResetLinkNeighbor();

	for (int Index = 0; Index < Neighbors.Num(); Index++)
	{
		auto& Item = Neighbors[Index];
		for (int PriorityIndex = 0; PriorityIndex < Item.Num(); PriorityIndex++)
		{
			auto& Neighbor = Item[PriorityIndex];
			TSharedPtr<FSurfaceNavPointBase> PinnedNeighbor = Neighbor != nullptr ? Neighbor.Pin() : nullptr;

			// if nullptr then skip
			if (!PinnedNeighbor)
				continue;

			// If invalid then clear and move to next point
			if (PinnedNeighbor->IsDestroyed())
			{
				ClearNeighbor(Index, PriorityIndex);
				continue;
			}

			//// TODO: Find the opposing index in the neighbor that this goes with
			//int NeighborIndex = Index ^ 1 > Index ? Index + 1 : Index - 1;
			//auto& NeighborToThis = Neighbor->Neighbors[NeighborIndex][PriorityIndex];
			//if (Neighbor->Neighbors[NeighborIndex][PriorityIndex] == this)
			//	Neighbor->Neighbors[NeighborIndex][PriorityIndex] = nullptr;

			// If the index is <=3 then it's for an axis.
			// If the index is >3 then it's diagnal from the axis
			bool IsDiag = Index > 3;
			int NeighborIndex = IsDiag ? 4 : 0;

			int NeighborPriorityIndex = PriorityIndex;

			if (!ResetNeighbor(PinnedNeighbor, NeighborIndex, NeighborPriorityIndex))
			{
				NeighborIndex++;
				if (!ResetNeighbor(PinnedNeighbor, NeighborIndex, NeighborPriorityIndex))
				{
					NeighborIndex++;
					if (!ResetNeighbor(PinnedNeighbor, NeighborIndex, NeighborPriorityIndex))
					{
						NeighborIndex++;
						if (!ResetNeighbor(PinnedNeighbor, NeighborIndex, NeighborPriorityIndex))
						{
							//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavPoint::Reset: Neighbor didn't have ref back to this."));
						}
					}
				}
			}

			ClearNeighbor(Index, PriorityIndex);
		}
	}

	CalcAngleFromGroundCached = false;
	CalcNormalCached = false;

	OwningGridPoint = nullptr;

	NeighborCount.Set(0);

	ExpenseModifier = 0.f;
	CachedAngleFromGround = 0.f;
}

void FSurfaceNavPoint::ClearNeighbor(int Index, int PriorityIndex, bool UseLock)
{
	if (Index < 0) return;

	if (UseLock)
	{
		FWriteScopeLock wNavPointLock(OwningGridPoint->OwningBlock->NavPointLock);
		ClearNeighbor(Index, PriorityIndex);
		return;
	}

	CalcAngleFromGroundCached = false;
	CalcNormalCached = false;

	int CurNeighborCount = NeighborCountAtIndex(Index);

	// If only 1 neighbor at index is left and the neighbor being cleared is not null
	// then subtract 1 from the neighor count.
	if (CurNeighborCount == 1 && Neighbors[Index][PriorityIndex] != nullptr)
		NeighborCount.Subtract(1);

	check(NeighborCount.GetValue() >= 0);

	Neighbors[Index][PriorityIndex] = nullptr;
}

void FSurfaceNavPoint::SetNeighbor(TSharedPtr<FSurfaceNavPointBase>& Neighbor, int Index, int PriorityIndex, bool UseLock)
{
	if (UseLock)
	{
		FWriteScopeLock wNavPointLock(OwningGridPoint->OwningBlock->NavPointLock);
		SetNeighbor(Neighbor, Index, PriorityIndex);
		return;
	}

	CalcAngleFromGroundCached = false;
	CalcNormalCached = false;

	// Make sure everything is valid and not getting cleaned up before continuing
	if (!Neighbor || !Neighbor->IsValid() || !Neighbor->GetOwningGridPoint()->IsValid() ||
		!Neighbor->GetOwningGridPoint()->OwningBlock->IsValid() || !Neighbor->GetOwningGridPoint()->OwningBlock->IsActive())
	{
		ClearNeighbor(Index, PriorityIndex);
		return;
	}

	int CurNeighborCount = NeighborCountAtIndex(Index);

	if (CurNeighborCount == 0)
		NeighborCount.Add(1);

	check(NeighborCount.GetValue() <= MaxNeighborCount);

	Neighbors[Index][PriorityIndex] = Neighbor;
}

int FSurfaceNavPoint::GetHighestPriorityForIndex(int Index)
{
	// We should only ever be using this for the first 4 neighbors (0-3)
	check(Index >= 0 && Index < 4);

	TSharedPtr<FSurfaceNavPointBase> PinnedMyNeighbor = nullptr;
	GetNeighborAt(Index, 0, PinnedMyNeighbor);
	if (PinnedMyNeighbor && PinnedMyNeighbor->IsValid() && PinnedMyNeighbor->GetOwningGridPoint() && PinnedMyNeighbor->GetOwningGridPoint()->IsValid() && PinnedMyNeighbor->HasLocalNeighbors())
		return 0;

	PinnedMyNeighbor = nullptr;
	GetNeighborAt(Index, 1, PinnedMyNeighbor);
	if (PinnedMyNeighbor && PinnedMyNeighbor->IsValid() && PinnedMyNeighbor->GetOwningGridPoint() && PinnedMyNeighbor->GetOwningGridPoint()->IsValid() && PinnedMyNeighbor->HasLocalNeighbors())
		return 1;

	PinnedMyNeighbor = nullptr;
	GetNeighborAt(Index, 2, PinnedMyNeighbor);
	if (PinnedMyNeighbor && PinnedMyNeighbor->IsValid() && PinnedMyNeighbor->GetOwningGridPoint() && PinnedMyNeighbor->GetOwningGridPoint()->IsValid() && PinnedMyNeighbor->HasLocalNeighbors())
		return 2;

	return -1;
};

void FSurfaceNavPoint::GetNeighborAt(int Index, int PriorityIndex, TSharedPtr<FSurfaceNavPointBase>& NavPoint)
{
	if (Index >= Neighbors.Num())
		return;

	auto& NeighborItem = Neighbors[Index];
	if (PriorityIndex >= NeighborItem.Num())
		return;

	TWeakPtr<FSurfaceNavPointBase>& MyNeighbor = NeighborItem[PriorityIndex];
	if (MyNeighbor == nullptr)
		return;

	if (TSharedPtr<FSurfaceNavPointBase> MyPinnedNeighbor = MyNeighbor.Pin())
		NavPoint = MyPinnedNeighbor;
}

void FSurfaceNavPoint::GetNeighborAt(int Index, TSharedPtr<FSurfaceNavPointBase>& NavPoint)
{
	if (Index < 4)
	{
		TWeakPtr<FSurfaceNavPointBase> MyNeighbor = Neighbors[Index][0];
		if (MyNeighbor != nullptr)
		{
			if (TSharedPtr<FSurfaceNavPointBase> MyPinnedNeighbor = MyNeighbor.Pin())
			{
				auto const& NeighborOwningGridpoint = MyPinnedNeighbor->GetOwningGridPoint();
				if (NeighborOwningGridpoint && NeighborOwningGridpoint->IsValid() && MyPinnedNeighbor->HasLocalNeighbors())
				{
					NavPoint = MyPinnedNeighbor;
					return;
				}
			}
		}

		MyNeighbor = Neighbors[Index][1];
		if (MyNeighbor != nullptr)
		{
			if (TSharedPtr<FSurfaceNavPointBase> MyPinnedNeighbor = MyNeighbor.Pin())
			{
				auto const& NeighborOwningGridpoint = MyPinnedNeighbor->GetOwningGridPoint();
				if (NeighborOwningGridpoint && NeighborOwningGridpoint->IsValid() && MyPinnedNeighbor->HasLocalNeighbors())
				{
					NavPoint = MyPinnedNeighbor;
					return;
				}
			}
		}

		MyNeighbor = Neighbors[Index][2];
		if (MyNeighbor != nullptr)
		{
			if (TSharedPtr<FSurfaceNavPointBase> MyPinnedNeighbor = MyNeighbor.Pin())
			{
				auto const& NeighborOwningGridpoint = MyPinnedNeighbor->GetOwningGridPoint();
				if (NeighborOwningGridpoint && NeighborOwningGridpoint->IsValid() && MyPinnedNeighbor->HasLocalNeighbors())
				{
					NavPoint = MyPinnedNeighbor;
					return;
				}
			}
		}

		return;
	}

	// Any index >= 4 is diagonal so we must do an extra check to help prevent getting a neighbor that's actually
	// in the ground.
	int LowestAllowedPriority = FindLowestPriority(Index);
	TWeakPtr<FSurfaceNavPointBase> MyNeighbor = Neighbors[Index][0];
	if (MyNeighbor != nullptr)
	{
		if (TSharedPtr<FSurfaceNavPointBase> MyPinnedNeighbor = MyNeighbor.Pin())
		{
			auto const& NeighborOwningGridpoint = MyPinnedNeighbor->GetOwningGridPoint();
			if (NeighborOwningGridpoint && NeighborOwningGridpoint->IsValid() && MyPinnedNeighbor->HasLocalNeighbors())
			{
				NavPoint = MyPinnedNeighbor;
				return;
			}
		}
	}

	MyNeighbor = Neighbors[Index][1];
	if (LowestAllowedPriority > 0 && MyNeighbor != nullptr)
	{
		if (TSharedPtr<FSurfaceNavPointBase> MyPinnedNeighbor = MyNeighbor.Pin())
		{
			auto const& NeighborOwningGridpoint = MyPinnedNeighbor->GetOwningGridPoint();
			if (NeighborOwningGridpoint && NeighborOwningGridpoint->IsValid() && MyPinnedNeighbor->HasLocalNeighbors())
			{
				NavPoint = MyPinnedNeighbor;
				return;
			}
		}
	}

	MyNeighbor = Neighbors[Index][2];
	if (LowestAllowedPriority > 1 && MyNeighbor != nullptr)
	{
		if (TSharedPtr<FSurfaceNavPointBase> MyPinnedNeighbor = MyNeighbor.Pin())
		{
			auto const& NeighborOwningGridpoint = MyPinnedNeighbor->GetOwningGridPoint();
			if (NeighborOwningGridpoint && NeighborOwningGridpoint->IsValid() && MyPinnedNeighbor->HasLocalNeighbors())
			{
				NavPoint = MyPinnedNeighbor;
				return;
			}
		}
	}
}

//FSurfaceNavPointBase* FSurfaceNavPoint::GetNeighborAt(int Index)
//{
//	if (Index < 4)
//	{
//		TWeakPtr<FSurfaceNavPoint> MyNeighbor = Neighbors[Index][0];
//		if (MyNeighbor != nullptr && MyNeighbor->OwningGridPoint && MyNeighbor->OwningGridPoint->IsValid() && MyNeighbor->HasLocalNeighbors())
//			return (FSurfaceNavPointBase* const)Neighbors[Index][0];
//
//		MyNeighbor = Neighbors[Index][1];
//		if (MyNeighbor != nullptr && MyNeighbor->OwningGridPoint && MyNeighbor->OwningGridPoint->IsValid() && MyNeighbor->HasLocalNeighbors())
//			return (FSurfaceNavPointBase*)Neighbors[Index][1];
//
//		MyNeighbor = Neighbors[Index][2];
//		if (MyNeighbor != nullptr && MyNeighbor->OwningGridPoint && MyNeighbor->OwningGridPoint->IsValid() && MyNeighbor->HasLocalNeighbors())
//			return (FSurfaceNavPointBase*)Neighbors[Index][2];
//
//		return SurfaceNavProxySettings::EmptyNavPointBase;
//	}
//
//	// Any index >= 4 is diagonal so we must do an extra check to help prevent getting a neighbor that's actually
//	// in the ground.
//	int LowestAllowedPriority = FindLowestPriority(Index);
//	TWeakPtr<FSurfaceNavPoint> MyNeighbor = Neighbors[Index][0];
//	if (MyNeighbor != nullptr && MyNeighbor->OwningGridPoint && MyNeighbor->OwningGridPoint->IsValid() && MyNeighbor->HasLocalNeighbors())
//		return (FSurfaceNavPointBase*)Neighbors[Index][0];
//
//	MyNeighbor = Neighbors[Index][1];
//	if (LowestAllowedPriority > 0 && MyNeighbor != nullptr && MyNeighbor->OwningGridPoint && MyNeighbor->OwningGridPoint->IsValid() && MyNeighbor->HasLocalNeighbors())
//		return (FSurfaceNavPointBase*)Neighbors[Index][1];
//
//	MyNeighbor = Neighbors[Index][2];
//	if (LowestAllowedPriority > 1 && MyNeighbor != nullptr && MyNeighbor->OwningGridPoint && MyNeighbor->OwningGridPoint->IsValid() && MyNeighbor->HasLocalNeighbors())
//		return (FSurfaceNavPointBase*)Neighbors[Index][2];
//
//	return SurfaceNavProxySettings::EmptyNavPointBase;
//}

inline const FVector FSurfaceNavPoint::CalcNormalFromNeighbors(bool bForceNoRecalc)
{
	if (IsDestroying())
		return HitNormal;

	if (CalcNormalCached)
		return CachedNormal;

	int MyNeighborCount = NeighborCount.GetValue();

	if (bForceNoRecalc || MyNeighborCount == 0)
		return HitNormal;

	FVector SumNormal(FVector::ZeroVector);

	// Get our neighbors
	TSharedPtr<FSurfaceNavPointBase> Neighbor0 = nullptr;
	TSharedPtr<FSurfaceNavPointBase> Neighbor1 = nullptr;
	TSharedPtr<FSurfaceNavPointBase> Neighbor2 = nullptr;
	TSharedPtr<FSurfaceNavPointBase> Neighbor3 = nullptr;
	TSharedPtr<FSurfaceNavPointBase> Neighbor4 = nullptr;
	TSharedPtr<FSurfaceNavPointBase> Neighbor5 = nullptr;
	TSharedPtr<FSurfaceNavPointBase> Neighbor6 = nullptr;
	TSharedPtr<FSurfaceNavPointBase> Neighbor7 = nullptr;
	GetNeighborAt(0, Neighbor0);
	GetNeighborAt(1, Neighbor1);
	GetNeighborAt(2, Neighbor2);
	GetNeighborAt(3, Neighbor3);
	GetNeighborAt(4, Neighbor4);
	GetNeighborAt(5, Neighbor5);
	GetNeighborAt(6, Neighbor6);
	GetNeighborAt(7, Neighbor7);

	FVector SumNeighborPoints = FVector::ZeroVector;

	// Sum up all of the neighors world locations to use to calculate and avg center.
	// TODO: This needs some adjustment to handle edge cases near end of nav volumes
	SumNeighborPoints += Neighbor0 && Neighbor0->IsValid() ? Neighbor0->WorldLocation : FVector::ZeroVector;
	SumNeighborPoints += Neighbor1 && Neighbor1->IsValid() ? Neighbor1->WorldLocation : FVector::ZeroVector;
	SumNeighborPoints += Neighbor2 && Neighbor2->IsValid() ? Neighbor2->WorldLocation : FVector::ZeroVector;
	SumNeighborPoints += Neighbor3 && Neighbor3->IsValid() ? Neighbor3->WorldLocation : FVector::ZeroVector;
	SumNeighborPoints += Neighbor4 && Neighbor4->IsValid() ? Neighbor4->WorldLocation : FVector::ZeroVector;
	SumNeighborPoints += Neighbor5 && Neighbor5->IsValid() ? Neighbor5->WorldLocation : FVector::ZeroVector;
	SumNeighborPoints += Neighbor6 && Neighbor6->IsValid() ? Neighbor6->WorldLocation : FVector::ZeroVector;
	SumNeighborPoints += Neighbor7 && Neighbor7->IsValid() ? Neighbor7->WorldLocation : FVector::ZeroVector;

	// Get the nav point's world location as a default
	// TODO: Determine if getting the avg center is even needed...will the world location work?
	FVector AvgCenter = WorldLocation;

	// Determine which calculation to use
	if (MyNeighborCount > 2)
	{
		AvgCenter = SumNeighborPoints / MyNeighborCount;
	}
	else if (MyNeighborCount == 2)
	{
		SumNeighborPoints += WorldLocation;
		AvgCenter = SumNeighborPoints / 3;
	}

	SumNormal += CalcNormalFromNeighbor(Neighbor0, AvgCenter, Neighbor4, Neighbor6, Neighbor2, Neighbor3);
	SumNormal += CalcNormalFromNeighbor(Neighbor1, AvgCenter, Neighbor7, Neighbor5, Neighbor3, Neighbor2);
	SumNormal += CalcNormalFromNeighbor(Neighbor2, AvgCenter, Neighbor5, Neighbor4, Neighbor1, Neighbor0);
	SumNormal += CalcNormalFromNeighbor(Neighbor3, AvgCenter, Neighbor6, Neighbor7, Neighbor0, Neighbor1);

	if (SumNormal == FVector::ZeroVector)
	{
		return HitNormal;
	}
	else
	{
		CachedNormal = FVector(SumNormal.GetSafeNormal());

		// HACK: On small geometry, some of the normals could be flipped after the
		// calculations so just check them and fix if needed. Need to see if there's
		// a better solution that can be done during the calculation.
		if (FVector::DotProduct(CachedNormal, HitNormal) < 0.f)
			CachedNormal *= -1.f;

		CalcNormalCached = true;
		return CachedNormal;
	}
}

inline FVector FSurfaceNavPoint::CalcNormalFromNeighbor(TSharedPtr<FSurfaceNavPointBase>& Neighbor, const FVector& AvgCenter, TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighbor,
	TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighborAlt1, TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighborAlt2, TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighborAlt3) const
{
	FVector CrossVector(FVector::ZeroVector);

	if (Neighbor && Neighbor->IsValid()) // Make sure the target neighbor is valid
	{
		if (AdjacentNeighbor && AdjacentNeighbor->IsValid()) // Is the nearby diagonal neighbor valid
		{
			CrossVector = CalcNormalFromNeighborAndAdjacentNeighbor(Neighbor, AvgCenter, AdjacentNeighbor);
		}
		else if (AdjacentNeighborAlt1 && AdjacentNeighborAlt1->IsValid()) // Is the on axis adjacent neighbor valid
		{
			CrossVector = CalcNormalFromNeighborAndAdjacentNeighbor(Neighbor, AvgCenter, AdjacentNeighborAlt1);
		}
		else if (AdjacentNeighborAlt2 && AdjacentNeighborAlt2->IsValid()) // Is the on axis adjacent neighbor valid
		{
			CrossVector = CalcNormalFromNeighborAndAdjacentNeighbor(Neighbor, AvgCenter, AdjacentNeighborAlt2);
		}
		else if (AdjacentNeighborAlt3 && AdjacentNeighborAlt3->IsValid()) // Is the on axis adjacent neighbor valid
		{
			CrossVector = CalcNormalFromNeighborAndAdjacentNeighbor(Neighbor, AvgCenter, AdjacentNeighborAlt3);
		}
		else // Last resort, calculate the normal with just this point and the single neighbor
		{
			// Get the direction from this point to the neighbor
			FVector DirToNeighbor = Neighbor->WorldLocation - AvgCenter;

			// Get the avg normal using the hit normal for this point and the neighbor
			FVector AvgNormal = FVector(Neighbor->HitNormal + HitNormal).GetSafeNormal();

			// First, get the cross product of the direction to neighbor and the avg normal, but this
			// won't be our final result. This will be like our right vector.
			CrossVector = FVector::CrossProduct(DirToNeighbor, AvgNormal);

			// Next, use the direction to neighbor again, but this time with the result of the cross product
			// from above. This should fine tune our normal value a bit.
			CrossVector = FVector::CrossProduct(DirToNeighbor, CrossVector);

			// If the dot product of the above normal and the avg of the hit normals is < than 0 then let's assume
			// we need to flip the normal, otherwise it'll be pointing in the wrong direction.
			if (FVector::DotProduct(CrossVector, AvgNormal) < 0.f)
				CrossVector *= -1.f;
		}
	}

	return CrossVector;
}

inline FVector FSurfaceNavPoint::CalcNormalFromNeighborAndAdjacentNeighbor(TSharedPtr<FSurfaceNavPointBase>& Neighbor, const FVector& AvgCenter,
	TSharedPtr<FSurfaceNavPointBase>& AdjacentNeighbor) const
{
	// Find the cross product using the vector from this point's avg center out to the 2 neighbor locations.
	// This will be our normal.
	FVector CrossVector = FVector::CrossProduct(Neighbor->WorldLocation - AvgCenter, AdjacentNeighbor->WorldLocation - AvgCenter);

	// If the dot product of the above normal and the avg of the hit normals is < than 0 then let's assume
	// we need to flip the normal, otherwise it'll be pointing in the wrong direction.
	if (FVector::DotProduct(CrossVector.GetSafeNormal(), FVector(Neighbor->HitNormal + AdjacentNeighbor->HitNormal).GetSafeNormal()) < 0.f)
		CrossVector *= -1.f;

	return CrossVector;
}

const float FSurfaceNavPoint::CalcAngleFromGround()
{
	if (CalcAngleFromGroundCached)
		return CachedAngleFromGround;

	float DotFromGround = FVector::DotProduct(GroundNormal, GetNormal());
	float AngleFromGround = UKismetMathLibrary::DegAcos(DotFromGround);
	CachedAngleFromGround = AngleFromGround;
	CalcAngleFromGroundCached = true;

	return AngleFromGround;
}

void FSurfaceNavPoint::FindOpposingNavPoint(TSharedPtr<FSurfaceNavPointBase>& OpposingNavPoint) {
	OwningGridPoint->FindOpposingNavPoint(this, OpposingNavPoint);
}

FVector FSurfaceNavPoint::GetNeighborVectorAtIndex(int index)
{
	if (VectorIndex.X != 0)
	{
		switch (index)
		{
		case 0:
			return FVector(0.f, -1.f, 0.f);
		case 1:
			return FVector(0.f, 1.f, 0.f);
		case 2:
			return FVector(0.f, 0.f, -1.f);
		case 3:
			return FVector(0.f, 0.f, 1.f);
		default:
			return FVector(-VectorIndex.X, -VectorIndex.Y, -VectorIndex.Z);
		}
	}
	else if (VectorIndex.Y != 0)
	{
		switch (index)
		{
		case 0:
			return FVector(-1.f, 0.f, 0.f);
		case 1:
			return FVector(1.f, 0.f, 0.f);
		case 2:
			return FVector(0.f, 0.f, -1.f);
		case 3:
			return FVector(0.f, 0.f, 1.f);
		default:
			return FVector(-VectorIndex.X, -VectorIndex.Y, -VectorIndex.Z);
		}
	}
	else if (VectorIndex.Z != 0)
	{
		switch (index)
		{
		case 0:
			return FVector(-1.f, 0.f, 0.f);
		case 1:
			return FVector(1.f, 0.f, 0.f);
		case 2:
			return FVector(0.f, -1.f, 0.f);
		case 3:
			return FVector(0.f, 1.f, 0.f);
		default:
			return FVector(-VectorIndex.X, -VectorIndex.Y, -VectorIndex.Z);
		}
	}

	return FVector(-VectorIndex.X, -VectorIndex.Y, -VectorIndex.Z);
}

bool FSurfaceNavPoint::NeighborsContainNavPoint(TSharedPtr<FSurfaceNavPointBase>& PossibleNeighbor)
{
	if (PossibleNeighbor)
	{
		for (int Index = 0; Index < Neighbors.Num(); Index++)
		{
			auto& Item = Neighbors[Index];
			for (int PriorityIndex = 0; PriorityIndex < Item.Num(); PriorityIndex++)
			{
				TWeakPtr<FSurfaceNavPointBase>& Neighbor = Item[PriorityIndex];
				if (Neighbor == nullptr)
					continue;

				if (TSharedPtr<FSurfaceNavPointBase> const& PinnedNeighbor = Neighbor.Pin())
				{
					if (PinnedNeighbor == PossibleNeighbor)
						return true;
				}
			}
		}
	}

	return false;
}
