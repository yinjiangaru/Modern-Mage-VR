// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavGridPoint.h"
#include "SurfaceNavBlock.h"
#include "SurfaceNavigationSystem.h"
#include "SurfaceNavPoint.h"
#include "SurfaceNavPointPool.h"
#include "Async/Async.h"
#include "DrawDebugHelpers.h"

namespace SurfaceNavGridPointConsts
{
	const FQuat DefaultFQuat = FQuat();
	const TArray<FIntVector> AxisVectors = {
		FIntVector(-1,0,0),
		FIntVector(1,0,0),
		FIntVector(0,-1,0),
		FIntVector(0,1,0),
		FIntVector(0,0,-1),
		FIntVector(0,0,1)
	};
}

FSurfaceNavGridPoint::FSurfaceNavGridPoint(
	UWorld& initWorld,
	const FCollisionShape& initGridPointTestSphere,
	const FCollisionQueryParams& initGridPointOverlapSphereQueryParams,
	const FCollisionQueryParams& initGridPointTraceQueryParams,
	const FGridPointFinishedDelegate& initGridPointFinishedDel,
	FSurfaceNavPointPool& initNavPointPool) :
	World(initWorld),
	GridPointTestSphere(initGridPointTestSphere),
	GridPointOverlapSphereQueryParams(initGridPointOverlapSphereQueryParams),
	GridPointTraceQueryParams(initGridPointTraceQueryParams),
	GridPointFinishedDel(initGridPointFinishedDel),
	NavPointPool(initNavPointPool)
{
	Init();
}

inline void FSurfaceNavGridPoint::Init()
{
	TraceHitCount = 0;
	bForceIsOrphan = 0;
	bOrphanedWithSiblings = 0;
	bIsNarrow = 0;
	bIsDestroying = 0;
	bIsDestroyed = 0;

	InitNavPoints();
}

inline void FSurfaceNavGridPoint::InitNavPoints()
{
	// There will never be more than 6 nav points so only reserver that amount
	NavPoints.Reserve(6);
}

void FSurfaceNavGridPoint::FinalizeNavPointsAlt(int Phase)
{
	if (!OwningBlock)
		return;

	GridPointFinishedDel.ExecuteIfBound(this, Phase);

	//#if WITH_EDITOR
	//	if (SurfaceNavProxySettings::ShowDebugNavPointsDuringBuild && World.WorldType != EWorldType::Editor &&
	//		World.WorldType != EWorldType::EditorPreview && World.WorldType != EWorldType::Inactive)
	//	{
	//		for (TPair<FIntVector, FSurfaceNavPoint*>& pair : NavPoints)
	//		{
	//			DrawDebugNavPoint(pair.Key);
	//		}
	//	}
	//#endif
}

//#if WITH_EDITOR
//inline const void FSurfaceNavGridPoint::DrawDebugNavPoint(const FIntVector& NavPointIndex)
//{
//	auto NavPoint = NavPoints.Contains(NavPointIndex) ? NavPoints[NavPointIndex] : nullptr;
//	if (!NavPoint || !NavPoint->IsValid())
//		return;
//
//	auto NavPointWorldLocation = NavPoint->WorldLocation;
//	auto DrawColor = NavPoint->NavArea->DrawColor;
//
//	AsyncTask(ENamedThreads::GameThread, [this, NavPointWorldLocation, DrawColor]()
//		{
//			FColor DrawColorNormal = FLinearColor(0.f, 0.8f, 0.f, 1.0f).ToFColor(true);
//			float DrawDuration = 30.0f;
//			bool DrawShadow = false;
//			if (this->IsValid() && this->OwningBlock && this->OwningBlock->IsValid() && this->OwningBlock->GetWorld())
//			{
//				DrawDebugPoint
//				(
//					this->OwningBlock->GetWorld(),
//					NavPointWorldLocation,
//					10.f,
//					DrawColor,
//					false,
//					2.f
//				);
//			}
//		});
//}
//#endif

inline const bool FSurfaceNavGridPoint::IsBoxedIn()
{
	return TraceHitCount == 6;
}

inline const bool FSurfaceNavGridPoint::HasNoSurface()
{
	return TraceHitCount == 0;
}

void FSurfaceNavGridPoint::Reset()
{
	// Our indexes are actually from -1 to +1, but the array start at zero so offset everything

	for (TPair<FIntVector, TSharedPtr<FSurfaceNavPointBase>>& pair : NavPoints)
	{
		NavPointPool.Release(pair.Value);
	}

	NavPoints.Empty();

	ResetAllBitFlags();

	TraceHitCount = 0;

	// Need to decrement this so the block can be deleted
	OwningBlock->LivingGridPointCount.Decrement();

	// Not responsible for deleting this so just set to nullptr
	OwningBlock = nullptr;

	bOrphanedWithSiblings = false;
	bForceIsOrphan = false;

	bIsNarrow = false;

	//delete WorldLocation;
	//delete VectorIndex;

	//WorldLocation = nullptr;
	//VectorIndex = nullptr;
}

void FSurfaceNavGridPoint::ResetAllBitFlags()
{
	// Reset all sibling bit flags
	// If we don't do this, we could get false orphaned grid points
	ResetSiblingBitFlag(0);
	ResetSiblingBitFlag(1);
	ResetSiblingBitFlag(2);
	ResetSiblingBitFlag(3);
	ResetSiblingBitFlag(4);
	ResetSiblingBitFlag(5);

	LocalAxisHits = 0;
	SiblingAxisHits = 0;

	/*OrphanedSiblings[0] = 0;
	OrphanedSiblings[1] = 0;
	OrphanedSiblings[2] = 0;
	OrphanedSiblings[3] = 0;
	OrphanedSiblings[4] = 0;
	OrphanedSiblings[5] = 0;*/

	/*SiblingGridPoints[0] = 0;
	SiblingGridPoints[1] = 0;
	SiblingGridPoints[2] = 0;
	SiblingGridPoints[3] = 0;
	SiblingGridPoints[4] = 0;
	SiblingGridPoints[5] = 0;*/
}

void FSurfaceNavGridPoint::ResetSiblingBitFlag(int AxisIndex)
{
	FSurfaceNavGridPoint* SiblingGridPoint = GetSiblingGridPoint(SurfaceNavGridPointConsts::AxisVectors[AxisIndex]);

	if (SiblingGridPoint)
	{
		int8 BitFlag = 0x1 << AxisIndex;

		int8 SiblingBitFlag = AxisIndex & 0x1 ? BitFlag >> 1 : BitFlag << 1;

		SiblingGridPoint->UnsetSiblingAxisHit(SiblingBitFlag);
	}
}

void FSurfaceNavGridPoint::SetLocation(const FVector location)
{
	WorldLocation = FVector(location);
}

bool FSurfaceNavGridPoint::IsOrphaned(bool IncludeSiblings) const
{
	if (IncludeSiblings)
	{
		return bForceIsOrphan || bOrphanedWithSiblings || ((LocalAxisHits ^ SiblingAxisHits) & SiblingAxisHits) > 0;
	}
	else
	{
		return bForceIsOrphan || ((LocalAxisHits ^ SiblingAxisHits) & SiblingAxisHits) > 0;
	}
}

inline FSurfaceNavGridPoint* FSurfaceNavGridPoint::GetSiblingGridPoint(FIntVector SiblingDirection) //, FSurfaceNavGridPoint* SiblingGridPoint)
{
	if (!OwningBlock || !OwningBlock->IsActive() || !OwningBlock->IsValid() || !OwningBlock->BlockKey)
		return nullptr;

	int MaxIndex = OwningBlock->GetSingleAxisVoxelCount() - 1;
	FIntVector SiblingGridPointIndex = VectorIndex + SiblingDirection;

	if (SiblingGridPointIndex.X < 0 || SiblingGridPointIndex.Y < 0 || SiblingGridPointIndex.Z < 0)
	{
		USurfaceNavigationSystem* const& NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(&World);
		const FSurfaceNavBlock* OtherBlock = NavSys->FindBlockByIndex(*OwningBlock->BlockKey + SiblingDirection);

		if (OtherBlock && OtherBlock->IsActive())
		{
			if (SiblingGridPointIndex.X < 0)
				SiblingGridPointIndex.X = MaxIndex;
			else if (SiblingGridPointIndex.Y < 0)
				SiblingGridPointIndex.Y = MaxIndex;
			else if (SiblingGridPointIndex.Z < 0)
				SiblingGridPointIndex.Z = MaxIndex;

			return OtherBlock->SurfaceNavGridPoints[SiblingGridPointIndex.X][SiblingGridPointIndex.Y][SiblingGridPointIndex.Z];
		}
		else
			return nullptr;
	}
	else if (SiblingGridPointIndex.X > MaxIndex || SiblingGridPointIndex.Y > MaxIndex || SiblingGridPointIndex.Z > MaxIndex)
	{
		USurfaceNavigationSystem* const& NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(&World);
		const FSurfaceNavBlock* OtherBlock = NavSys->FindBlockByIndex(*OwningBlock->BlockKey + SiblingDirection);

		if (OtherBlock && OtherBlock->IsActive())
		{
			if (SiblingGridPointIndex.X > MaxIndex)
				SiblingGridPointIndex.X = 0;
			else if (SiblingGridPointIndex.Y > MaxIndex)
				SiblingGridPointIndex.Y = 0;
			else if (SiblingGridPointIndex.Z > MaxIndex)
				SiblingGridPointIndex.Z = 0;

			return OtherBlock->SurfaceNavGridPoints[SiblingGridPointIndex.X][SiblingGridPointIndex.Y][SiblingGridPointIndex.Z];
		}
		else
			return nullptr;
	}

	return OwningBlock->SurfaceNavGridPoints[SiblingGridPointIndex.X][SiblingGridPointIndex.Y][SiblingGridPointIndex.Z];
}

//inline void FSurfaceNavGridPoint::TestForOrphanedSibling(FIntVector SiblingVectorIndexOffset, int LocalIndex, int SiblingIndex)
//{
//	FSurfaceNavGridPoint* SiblingGridPoint = GetSiblingGridPoint(SiblingVectorIndexOffset);
//	if (SiblingGridPoint)
//	{
//		bool HasSurface = !HasNoSurface();
//		bool SiblingHasSurface = !SiblingGridPoint->HasNoSurface();
//
//		OrphanedSiblings[LocalIndex] = SiblingHasSurface && SiblingGridPoint->IsOrphaned(false);
//		SiblingGridPoints[LocalIndex] = SiblingHasSurface;
//
//		SiblingGridPoint->OrphanedSiblings[SiblingIndex] = HasSurface && IsOrphaned(false);
//		SiblingGridPoint->SiblingGridPoints[SiblingIndex] = HasSurface;
//
//		SiblingGridPoint->UpdateOrphanedWithSiblings();
//	}
//	else
//	{
//		SiblingGridPoints[LocalIndex] = false;
//		OrphanedSiblings[LocalIndex] = false;
//	}
//}

//inline void FSurfaceNavGridPoint::UpdateOrphanedWithSiblings()
//{
//	int OrphanedSiblingCount = CountSetBits_6(OrphanedSiblings); // .CountSetBits();
//	bOrphanedWithSiblings = OrphanedSiblingCount > 0 && OrphanedSiblingCount > (CountSetBits_6(SiblingGridPoints) / 2);
//}

//inline int FSurfaceNavGridPoint::CountSetBits_6(TStaticBitArray<6>& MyBitArray)
//{
//	int result = MyBitArray[0] + MyBitArray[1] + MyBitArray[2] + MyBitArray[3] + MyBitArray[4] + MyBitArray[5];
//
//	return result;
//}

inline void FSurfaceNavGridPoint::DetermineIfNarrowCorridor(FSurfaceNavGridPointTraceResult& TraceResult, int HitResultIndex1, int HitResultIndex2)
{
	if (!bIsNarrow && TraceResult.SingleTraceResults[HitResultIndex1].Hit && TraceResult.SingleTraceResults[HitResultIndex2].Hit)
	{
		auto HitNormal1 = TraceResult.SingleTraceResults[HitResultIndex1].ImpactNormal;
		auto HitNormal2 = TraceResult.SingleTraceResults[HitResultIndex2].ImpactNormal;

		// TODO: Add test for distance between hits
		// The distance should be configurable as a percent of the grid distance

		bool bIsWithinDist = true;

		if (SurfaceNavProxySettings::NarrowCorridorPercentToFlagSq < SurfaceNavProxySettings::GridPointDistanceSq)
		{
			float DistanceBetweenHitsSq = FVector::DistSquared(
				TraceResult.SingleTraceResults[HitResultIndex1].ImpactLocation,
				TraceResult.SingleTraceResults[HitResultIndex2].ImpactLocation);

			bIsWithinDist = DistanceBetweenHitsSq <= SurfaceNavProxySettings::NarrowCorridorPercentToFlagSq;
		}

		bIsNarrow = bIsWithinDist && FVector::DotProduct(HitNormal1, HitNormal2) < -0.3f;
	}
}

void FSurfaceNavGridPoint::ConfigNavPoints(FSurfaceNavGridPointTraceResult& TraceResult)
{
	auto CurGridPointIndex = TraceResult.GridPointIndex;
	int MaxIndex = OwningBlock->GetSingleAxisVoxelCount() - 1;

	bIsNarrow = false;
	TraceHitCount = TraceResult.TraceHitCount;

	// Test if checking for narrow is enabled
	if (SurfaceNavProxySettings::FlagNarrowCorridors)
	{
		// Only check opposing axis directions to see if they are
		// pointing towards each other.
		DetermineIfNarrowCorridor(TraceResult, 0, 1);
		DetermineIfNarrowCorridor(TraceResult, 2, 3);
		DetermineIfNarrowCorridor(TraceResult, 4, 5);
	}

	// ************************************************************
	// ------------------------------------------------------------
	// TODO: May need to add this back
	// 
	//TestForOrphanedSibling(FIntVector(-1, 0, 0), 0, 1);
	//TestForOrphanedSibling(FIntVector(1, 0, 0), 1, 0);
	//TestForOrphanedSibling(FIntVector(0, -1, 0), 2, 3);
	//TestForOrphanedSibling(FIntVector(0, 1, 0), 3, 2);
	//TestForOrphanedSibling(FIntVector(0, 0, -1), 4, 5);
	//TestForOrphanedSibling(FIntVector(0, 0, 1), 5, 4);
	//
	//// If we have too many orphaned siblings, then we're going to treat this as an
	//// orphaned gridpoint as well
	//UpdateOrphanedWithSiblings();
	//
	// ------------------------------------------------------------
	// ************************************************************

	//if (!World.IsValid() || World->bIsTearingDown)
	//{
	//	//DeleteTraceResultsData(TraceResults);
	//	return;
	//}

	//USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(World.Get());

	// Start index at -1, it will be incremented to zero on first loop iteration
	int AxisIndex = -1;

	for (auto& SingleTraceResult : TraceResult.SingleTraceResults)
	{
		AxisIndex++;

		FIntVector TraceIndex = SingleTraceResult.TraceIndex;

		FSurfaceNavGridPoint* SiblingGridPoint = GetSiblingGridPoint(TraceIndex); // nullptr;

		if (SingleTraceResult.Hit)
		{
			//TraceHitCount++;

			// Determine bit flag from axis index
			int8 BitFlag = 0x1 << AxisIndex;

			// Set this axis as a hit
			SetLocalAxisHit(BitFlag);

			// If a sibling gridpoint was found then set the axis as a hit
			if (SiblingGridPoint)
			{
				// Get the bit flag for a hit for a sibling grid point in my direction
				int8 SiblingBitFlag = AxisIndex & 0x1 ? BitFlag >> 1 : BitFlag << 1;

				// Add 1 to index if odd, subtract 1 if even.
				// This will give us the inverse hits for the siblings which is what we want for testing.
				SiblingGridPoint->SetSiblingAxisHit(SiblingBitFlag);

				// If the sibling grid point has a hit in my direction then set the bit flag
				// for the hit in the sibling's direction.
				if (SiblingGridPoint->LocalAxisHits & SiblingBitFlag)
					SetSiblingAxisHit(BitFlag);
				else // Otherwise, there was no hit so unset it
					UnsetSiblingAxisHit(BitFlag);
			}
			else
				UnsetSiblingAxisHit(BitFlag);

			auto HitLocation = SingleTraceResult.ImpactLocation;
			auto HitNormal = SingleTraceResult.ImpactNormal;

			// Get a free point from the pool
			TSharedPtr<FSurfaceNavPointBase> NavPoint = NavPointPool.Pop(
				TraceIndex,
				HitLocation,
				HitNormal);
			
			check(this->OwningBlock->IsActive());

			NavPoint->SetOwningGridPoint(this);

			// Add it to this grid point's nav point container
			NavPoints.Add(TraceIndex, NavPoint);

			NavPoint->SetGroundNormal(SingleTraceResult.GroundNormal);

			NavPoint->NavArea = SingleTraceResult.NavArea;

			// ***************************************************
			// ***************************************************
			// TODO: May want to move this to Builder and add at least a read lock
			// since we're changing the child collections

			USurfaceNavigationSystem* const& NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(&World);
			FReadScopeLock rNavLinkLock(NavSys->GetNavLinkLock());

			for (auto& PointLink : SingleTraceResult.PointALinks)
			{
				if (!PointLink.LinkNavPointA->IsValid())
					continue;

				NavPoint->CustomLinkNeighbors.Emplace(PointLink.LinkNavPointA);
				PointLink.LinkNavPointA->CustomLinkNeighbors.Emplace(NavPoint);
			}

			for (auto& PointLink : SingleTraceResult.PointBLinks)
			{
				if (!PointLink.LinkNavPointB->IsValid())
					continue;

				NavPoint->CustomLinkNeighbors.Emplace(PointLink.LinkNavPointB);
				PointLink.LinkNavPointB->CustomLinkNeighbors.Emplace(NavPoint);
			}

			// ***************************************************
			// ***************************************************

		}
		else
		{
			//// Determine bit flag from axis index
			//int8 BitFlag = 0x1 << AxisIndex;

			//// If a sibling gridpoint was found then set the axis as a hit
			//UnsetLocalAxisHit(BitFlag);

			//// If a sibling gridpoint was found then unset the axis to indicate not hit
			//if (SiblingGridPoint)
			//{
			//	int8 SiblingBitFlag = AxisIndex & 0x1 ? BitFlag >> 1 : BitFlag << 1;

			//	SiblingGridPoint->UnsetSiblingAxisHit(SiblingBitFlag);

			//	if (SiblingGridPoint->LocalAxisHits & SiblingBitFlag)
			//		SetSiblingAxisHit(BitFlag);
			//	else
			//		UnsetSiblingAxisHit(BitFlag);
			//}
			//else
			//	UnsetSiblingAxisHit(BitFlag);

			//if (NavPoint)
			//{
			//	NavPoints.Remove(TraceIndex);
			//	NavPointPool.Release(NavPoint);
			//}
		}
	}
}

void FSurfaceNavGridPoint::FindOpposingNavPoint(FSurfaceNavPoint* const& CurrentNavPoint, TSharedPtr<FSurfaceNavPointBase>& OpposingNavPoint)
{
	FIntVector OpposingIndex = CurrentNavPoint->VectorIndex * -1;

	TSharedPtr<FSurfaceNavPointBase>* OpposingNavPointPtr = NavPoints.Find(OpposingIndex);

	if (OpposingNavPointPtr && *OpposingNavPointPtr)
		OpposingNavPoint = *OpposingNavPointPtr;
}

FVector FSurfaceNavGridPoint::CalcAvgLocation()
{
	if (CachedAvgLocation != FVector::ZeroVector)
		return CachedAvgLocation;

	FVector AvgLoc(0.f);
	float count = 0.f;
	for (TPair<FIntVector, TSharedPtr<FSurfaceNavPointBase>> NavPointPair : NavPoints)
	{
		if (!NavPointPair.Value->IsValid())
			continue;

		count++;
		AvgLoc += NavPointPair.Value->WorldLocation;
	}
	if (count > 0)
		CachedAvgLocation = AvgLoc / count;

	return CachedAvgLocation;
}

FVector FSurfaceNavGridPoint::CalcAvgNormal()
{
	if (CachedAvgNormal != FVector::ZeroVector)
		return CachedAvgNormal;

	FVector AvgNormal(0.f);
	float count = 0.f;
	for (TPair<FIntVector, TSharedPtr<FSurfaceNavPointBase>> NavPointPair : NavPoints)
	{
		if (!NavPointPair.Value->IsValid())
			continue;

		count++;
		AvgNormal += NavPointPair.Value->GetNormal();
	}
	if (count > 0)
		CachedAvgNormal = AvgNormal.GetSafeNormal();

	return CachedAvgNormal;
}
