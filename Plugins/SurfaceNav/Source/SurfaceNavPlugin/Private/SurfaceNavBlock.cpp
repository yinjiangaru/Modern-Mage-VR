// Copyright BMK Games LLC. All Rights Reserved.


#include "SurfaceNavBlock.h"

#include "Async/Async.h"
#include "NavLinkCustomInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "LandscapeProxy.h"
#include "GameFramework/Pawn.h"
#include "Components/ModelComponent.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
#include "Engine/OverlapResult.h"
#endif

#include "SurfaceNavigationSystem.h"
#include "SurfaceNavGridPointPool.h"
#include "SurfaceNavGridPoint.h"


namespace SurfaceNavBlockConsts
{
	const FString VoxelPluginCollisionCompStr("VoxelCollisionComponent");
	const FString WorldScapeActorStr("WorldScapeRoot");
	const FString WorldScapeMeshCompStr("WorldScapeMeshComponent");
	const FQuat DefaultFQuat = FQuat();
	const FColor GridPointDrawColorNormal = FLinearColor(0.f, 0.0f, 0.8f, 1.0f).ToFColor(true);
	const float GridPointDrawSize = 10.0f;
	const float GridPointDrawDuration = 1.0f;
}

// Sets default values
FSurfaceNavBlock::FSurfaceNavBlock(UWorld* InWorld) :
	IsDestroyed(0), IsFlatWorld(1), bIsActive(0), bIsProcessingGrid(0), bIsDirty(1), bIsPendingCleanup(0), FoundSurfaceCount(0)
{
	World = InWorld;

	//ensure(World.Get());

	CollisionQueryParams = FCollisionQueryParams();


	SingleAxisVoxelCount = SurfaceNavProxySettings::SingleAxisVoxelCount;

	GridPointDist = SurfaceNavProxySettings::GridPointDistance;

	BlockOverlapDel.BindLambda([this](const FTraceHandle& TraceHandle, FOverlapDatum& TraceData)
		{
			BlockOverlapCheckResponse(TraceHandle, TraceData);
		});

	IsFlatWorld = SurfaceNavProxySettings::IsFlatWorld;
}

FSurfaceNavBlock::~FSurfaceNavBlock()
{
	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::BeginDestroy(): Write Lock - Begin"));

		//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::BeginDestroy(): Write Lock - NavBlockLock"));

	bIsActive = false;
	ResetAll();

	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::BeginDestroy(): Write Lock - End"));
}

void FSurfaceNavBlock::Init()
{
}

inline void FSurfaceNavBlock::PreInitGridPoints()
{
	float startOffset = GridPointDist / 2.f;

	float actorExtentOffset = SurfaceNavProxySettings::BlockExtent;

	AxisStart = startOffset - actorExtentOffset;

	GridPointStartOffset = AxisStart + GridPointDist;

	int voxelCount = SingleAxisVoxelCount;

	SurfaceNavGridPoints.Init(TArray<TArray<FSurfaceNavGridPoint*>>(), voxelCount);
	for (int i = 0; i < voxelCount; i++)
	{
		SurfaceNavGridPoints[i].Init(TArray<FSurfaceNavGridPoint*>(), voxelCount);

		for (int j = 0; j < voxelCount; j++)
		{
			SurfaceNavGridPoints[i][j].Init(nullptr, voxelCount);
		}
	}

}

inline void FSurfaceNavBlock::ReleaseAllGridPoints()
{
	if (SurfaceNavGridPoints.Num() == 0)
		return;

	int voxelCount = SingleAxisVoxelCount;
	//FWriteScopeLock wNavPointLock(NavPointLock);	

	for (int i = 0; i < voxelCount; i++)
	{
		for (int j = 0; j < voxelCount; j++)
		{
			for (int k = 0; k < voxelCount; k++)
			{
				GridPointCanBeReleased(FIntVector(i, j, k));
			}
		}
	}
}

void FSurfaceNavBlock::ClearGridPoints()
{
	ReleaseAllGridPoints();

	for (int i = 0; i < SurfaceNavGridPoints.Num(); i++)
	{
		for (int j = 0; j < SurfaceNavGridPoints[i].Num(); j++)
		{
			SurfaceNavGridPoints[i][j].Empty();
		}

		SurfaceNavGridPoints[i].Empty();
	}
	SurfaceNavGridPoints.Empty();
}

void FSurfaceNavBlock::MarkDirty()
{
	bIsDirty = true;

	/*if (bIsActive && HandleBlockMarkedDirtyDel)
		HandleBlockMarkedDirtyDel->ExecuteIfBound(*BlockKey);*/
}

inline void FSurfaceNavBlock::ClearDirty()
{
	bIsDirty = false;
}

void FSurfaceNavBlock::ResetAll()
{
	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::ResetAll_MainThread(): Write Lock - NavBlockLock"));
	FWriteScopeLock wLock(NavBlockLock);


	bIsActive = false;
	bIsProcessingGrid = false;

	{
		FWriteScopeLock wNavPointLock(NavPointLock);
		ClearGridPoints();
	}

	delete PlanetLoc;
	PlanetLoc = nullptr;

	delete BlockKey;
	BlockKey = nullptr;

	World.Reset();
}

void FSurfaceNavBlock::Activate()
{
	FWriteScopeLock wLock(NavBlockLock);


	ActorLoc = GetLocation();

	PreInitGridPoints();

	DeterminePlanet();

	bIsActive = true;

	MarkDirty();
}

inline void FSurfaceNavBlock::DeterminePlanet()
{
	if (!World.Get() || !World.IsValid() || World->bIsTearingDown)
		return;

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(World.Get());

	if (!NavSys || NavSys->bIsCleaningUp || !NavSys->IsInitDone())
		return;

	AActor* PlanetActor = nullptr;
	NavSys->FindClosestPlanet(GetLocation(), PlanetActor);

	if (PlanetActor)
		PlanetLoc = new FVector(PlanetActor->GetActorLocation());
}

bool FSurfaceNavBlock::Refresh(UWorld* MyWorld)
{
	//ensure(World.Get());
	bool result = false;

	FWriteScopeLock wLock(NavBlockLock);

	// Don't continue unless
	// - block is dirty
	// - not processing grid
	// - is active
	//if (!bIsDirty || bIsProcessingGrid || bIsPendingCleanup || !bIsActive || !World.IsValid(false, true) || World.IsStale(true, true) || World->bIsTearingDown)
	if (!bIsDirty || bIsProcessingGrid || bIsPendingCleanup || !bIsActive || MyWorld->bIsTearingDown)
	{
		return false;
	}

	//bIsPendingCleanup = false;

	// Must use non-async version for now becase the Async version doesn't always
	// return overlaps that I can clearly see.
	result = CheckForOverlappingActors(MyWorld);

	return result;


	// TODO: Can't use the Async version yet because it doesn't alwasy detect the same overlaps
	// that the non-async version gives...research, maybe something I'm doing wrong.
	//AsyncCheckForOverlappingActors();
}

void FSurfaceNavBlock::InvalidateBlockProcessing(bool TriggerRemoval)
{
	FWriteScopeLock wLock(NavBlockLock);


	bIsProcessingGrid = false;
	bIsPendingCleanup = true;

	//if (TriggerRemoval)
	CheckIfShouldRemove();
	//SetCheckIfShouldRemoveTimer();
}

bool FSurfaceNavBlock::IsReadyToRelease()
{
	return !bIsProcessingGrid &&
		(bIsPendingCleanup || !World.Get() || !World.IsValid());
}

void FSurfaceNavBlock::CheckIfShouldRemove()
{
	if (!bIsActive || !ReleaseBlockDel)
		return;

	if (IsReadyToRelease())
	{
		ReleaseBlockDel->ExecuteIfBound(*BlockKey);
	}
}

inline void FSurfaceNavBlock::AsyncCheckForOverlappingActors()
{
	// NOTE: If we use this function again, needs to be updated

		// It may seem strange to use a sphere to test overlap for a cube, but the cube overlap test
		// seemed inconsistent and the sphere test should be a small bit faster

	// Don't continue unless
	// - block is dirty
	// - not processing grid
	// - is active
	if (!bIsDirty || bIsProcessingGrid || !bIsActive || !World.Get() || !World.IsValid() || World->bIsTearingDown) // || bIsReleasing)
		return;

	bIsProcessingGrid = true;
	ClearDirty();

	// For some reason the box shape does not always pickup overlaps that are very clearly there so
	// have to use a sphere shape which is padded to include the corners of where the box would be.
	World->AsyncOverlapByChannel(
		ActorLoc,
		SurfaceNavBlockConsts::DefaultFQuat,
		ECC_Camera, //ECC_Visibility,
		SurfaceNavProxySettings::BlockTestShape, //*BlockTestBox,
		CollisionQueryParams, //BlockOverlapSphereQueryParams,
		FCollisionResponseParams::DefaultResponseParam, //BlockOverlapCollisionResponseParams,
		&BlockOverlapDel
	);
}

inline void FSurfaceNavBlock::BlockOverlapCheckResponse(const FTraceHandle& TraceHandle, FOverlapDatum& TraceData)
{
	// Only continue if overlaps > 0
	if (TraceData.OutOverlaps.Num() > 0)
	{
		BlockOverlapsFoundDel->ExecuteIfBound(*BlockKey);
		return;
	}

	bIsProcessingGrid = false;

	ReleaseAllGridPoints();
}

inline bool FSurfaceNavBlock::CheckForOverlappingActors(UWorld* MyWorld)
{
	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::CheckForOverlappingActors(): Write Lock - NavBlockLock"));


	// It may seem strange to use a sphere to test overlap for a cube, but the cube overlap test
	// seemed inconsistent and the sphere test should be a small bit faster

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(MyWorld);

	if (!NavSys || NavSys->bIsCleaningUp || !NavSys->IsInitDone())
		return false;

	bIsProcessingGrid = true;
	ClearDirty();

	OverlappingComponents.Empty();

	TArray<struct FOverlapResult> OutOverlaps;

	// For some reason the box shape does not always pickup overlaps that are very clearly there so
	// have to use a sphere shape which is padded to include the corners of where the box would be.
	//if (World.IsValid(false, true) && !World.IsStale(true, true) && !World->bIsTearingDown &&
	if (!MyWorld->bIsTearingDown &&
		BlockOverlapTest(OutOverlaps, MyWorld))
	{
		for (FOverlapResult& Overlap : OutOverlaps)
		{
			OverlappingComponents.Emplace(Overlap.Component);
		}

		BlockOverlapsFoundDel->ExecuteIfBound(*BlockKey);
		return true;
	}

	bIsProcessingGrid = false;

	{
		FReadScopeLock rPathingLock(NavSys->PathingLock);

		FWriteScopeLock wNavPointLock(NavPointLock);
		ReleaseAllGridPoints();
	}

	return true;
}

inline bool FSurfaceNavBlock::BlockOverlapTest(TArray<struct FOverlapResult>& OutOverlaps, UWorld* MyWorld)
{
	if (SurfaceNavProxySettings::TraceMethod == ESurfaceNavTraceMethod::Profile)
		return MyWorld->OverlapMultiByProfile(
			OutOverlaps,
			ActorLoc,
			SurfaceNavBlockConsts::DefaultFQuat,
			FName(SurfaceNavProxySettings::TraceProfile),
			SurfaceNavProxySettings::BlockTestShape);
	else
		return MyWorld->OverlapMultiByChannel(
			OutOverlaps,
			ActorLoc,
			SurfaceNavBlockConsts::DefaultFQuat,
			SurfaceNavProxySettings::TraceChannel,
			SurfaceNavProxySettings::BlockTestShape);
}

inline void FSurfaceNavBlock::GridPointCanBeReleased(const FIntVector& GridPointIndex)
{
	if (SurfaceNavGridPoints.Num() > 0)
	{
		ReleaseGridPoint(SurfaceNavGridPoints[GridPointIndex.X][GridPointIndex.Y][GridPointIndex.Z]);
		SurfaceNavGridPoints[GridPointIndex.X][GridPointIndex.Y][GridPointIndex.Z] = nullptr;
	}
	else
	{
		// We would expect that if there are no more grid points in the array that our tracked count would also be zero
		ensure(LivingGridPointCount.GetValue() == 0);
	}
}

inline void FSurfaceNavBlock::ReleaseGridPoint(FSurfaceNavGridPoint*& GridPoint)
{
	if (!GridPoint)
		return;

	GridPoint->BeginDestroy();

	if (FoundSurfaceCount > 0)
		FoundSurfaceCount--;

	FSurfaceNavGridPoint* GridPointPtrCpy = GridPoint;
	GridPoint = nullptr;

	if (!IsValid())
	{
		GridPointPool->Release(GridPointPtrCpy);
		return;
	}

	UWorld* MyWorld = World.Get();

	if (!MyWorld || !World.IsValid(false, true) || MyWorld->bIsTearingDown)
	{
		GridPointPool->Release(GridPointPtrCpy);
		return;
	}

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(World.Get());

	// Sanity check, this shouldn't happen due to earlier checks
	if (!NavSys || !NavSys->IsInitDone() || NavSys->bIsCleaningUp)
	{
		GridPointPool->Release(GridPointPtrCpy);
		return;
	}

	NavSys->ReleaseGridPointCallback(GridPointPtrCpy);
}

bool FSurfaceNavBlock::HasFoundSurface()
{
	return FoundSurfaceCount > 0;
}

void FSurfaceNavBlock::CheckForSurface()
{
	// The calling function should be locking the nav sys destroy lock so
	// don't do that again in this thread

	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::CheckForSurface(): Read Lock - NavBlockLock"));

	if (bIsDirty || !bIsActive || bIsPendingCleanup)
	{
		bIsProcessingGrid = false;
		return;
	}

	FWriteScopeLock wNavBlockLock(NavBlockLock);

	// Don't continue if the block has been marked as dirty since we started
	if (bIsDirty || !bIsActive || bIsPendingCleanup)
	{
		bIsProcessingGrid = false;
		return;
	}

	// TODO: Some of this can be moved to class level or even global variables
	float blockSize = SurfaceNavProxySettings::BlockSize;
	float voxelCount = SingleAxisVoxelCount;
	float lastIndex = voxelCount - 1.f;
	FVector actorLoc = ActorLoc;
	float axisStart = AxisStart;
	float gridPointDist = GridPointDist;
	float halfGridPointDist = gridPointDist * 0.5f; // / 2.f;
	float halfGridPointDistSq = halfGridPointDist * halfGridPointDist;

	// Get padded bounds to use so we can accurately get any components actors that may be
	// just outside this block's bounds.
	FBox PaddedBounds = GetBounds().ExpandBy(halfGridPointDist + 0.1f);

	FVector GridPointExtent(halfGridPointDist);

	FCollisionQueryParams collisionQueryParams = FCollisionQueryParams();

	TraceResults.Empty(SurfaceNavProxySettings::GridPointsPerBlock);

	bool hasAnyOverlaps = false;

	auto GridPointOverlapSphereQueryParams = FCollisionQueryParams();
	GridPointOverlapSphereQueryParams.bTraceComplex = false;
	GridPointOverlapSphereQueryParams.bReturnPhysicalMaterial = false;

	UWorld* MyWorld = World.Get();

	if (!MyWorld || !World.IsValid(false, true) || MyWorld->bIsTearingDown)
	{
		bIsProcessingGrid = false;
		bIsDirty = true;
		return;
	}

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(World.Get());

	// Sanity check, this shouldn't happen due to earlier checks
	if (!NavSys || !NavSys->IsInitDone() || NavSys->bIsCleaningUp)
	{
		bIsProcessingGrid = false;
		bIsDirty = true;
		return;
	}

	int i = 0;
	int j = 0;
	int k = 0;
	bool bCanIterateGridPoints = true;


	while (bCanIterateGridPoints)
	{
		double x = axisStart + gridPointDist * i + actorLoc.X;
		double y = axisStart + gridPointDist * j + actorLoc.Y;
		double z = axisStart + gridPointDist * k + actorLoc.Z;

		FIntVector GridPointIndex(i, j, k);
		FVector WorldLocation(x, y, z);
		bool bShouldCheckForSurface = false;

		// Check if this grid point intersects a nav mesh bounds volume
		{
			FReadScopeLock rVolumeBoundLock(NavSys->VolumeBoundLock);

			if (NavSys->NavVolumeBounds.Num() > 0)
			{
				for (const auto& vBounds : NavSys->NavVolumeBounds)
				{
					if (vBounds.AreaBox.Intersect(FBox::BuildAABB(WorldLocation, FVector(halfGridPointDist))))
					{
						bShouldCheckForSurface = true;

						// We only care if it intersects any nav mesh bounds volume so stop processing
						// once we find one
						break;
					}
				}
			}
		}

#pragma region CANT USE - CAUSING CRASH

#if false
		/***********************************************************************************************************/
		/*** CAN'T USE THIS HERE, HIGH CHANCE OF CAUSING CRASH RELATED TO WRONG THREAD WORKING WITH PHYSICS ***/
		/* If we want to use the below, then need a different place on game thread. */

		// If the grid point meets the required conditions above and there are components overlapping
		// this block, then check if this grid point is inside the collision of any of those components.
		// If it is then it becomes an invalid grid point.
		if (bShouldCheckForSurface && OverlappingComponents.Num() > 0)
		{
			for (TWeakObjectPtr<UPrimitiveComponent>& OverlappingCompPtr : OverlappingComponents)
			{
				UPrimitiveComponent* OverlappingComp = OverlappingCompPtr.IsValid() ? OverlappingCompPtr.Get() : nullptr;

				if (!OverlappingComp || OverlappingComp->IsBeingDestroyed() || OverlappingComp->bHiddenInGame)
					continue;

				AActor* CompOwner = OverlappingComp->GetOwner();
				if (!CompOwner // || CompOwner->IsA<FSurfaceNavBlock>()
					|| CompOwner->IsA<APawn>() || CompOwner->IsActorBeingDestroyed())
					continue;

				// Test the distance from the component's bounding box to the grid point location. If the grid point is
				// outside the bounds then we know it's not inside this components collision so skip the heavier processing.
				FBoxSphereBounds CompBound = OverlappingComp->Bounds;
				float DistSq = CompBound.ComputeSquaredDistanceFromBoxToPoint(WorldLocation);
				if (DistSq > 0.f)
					continue;


				//// ***********************************************
				//// TESTING ONLY
				//FBodyInstance* BodyInst = OverlappingComp->GetBodyInstance();
				//if (BodyInst != nullptr)
				//{
				//	bool test = BodyInst->GetSquaredDistanceToBody(WorldLocation, DistSq, ClosestPointOnCollision);
				//}
				//// ***********************************************

				// Let's do a quick test to see if the grid point is inside the collision. If it is,
				// then we don't need to do any other testing for this gridpoint as it's considered invalid.
				FVector ClosestPointOnCollision;
				// *************************************************
				// *************************************************
				// Below is the code causing the crash due to the body instance sometimes being
				// null or invalid.
				// *************************************************
				// *************************************************
				bool result = OverlappingComp->GetSquaredDistanceToCollision(WorldLocation, DistSq, ClosestPointOnCollision);
				if (result)
				{
					if (DistSq == 0.f) //OverlappingComp->GetSquaredDistanceToCollision(WorldLocation, DistSq, ClosestPointOnCollision))
					{
						bShouldCheckForSurface = false;
						break;
					}
				}
				//else
				//{
				//  // ************************************************
				//  // ************************************************
				//	// Tests a static mesh if the point is inside or not.
				//	// Not using this as it is very expensive as is.
				//  // ************************************************
				//  // ************************************************
				// 
				//	auto StaticMeshComp = Cast<UStaticMeshComponent>(OverlappingComp);
				//	if (StaticMeshComp)
				//	{
				//		auto BoxMax = CompBound.GetBox().Max - StaticMeshComp->GetComponentLocation();
				//		auto OuterPoint = FVector(BoxMax.X * 1.1f, 0.f, 0.f);
				//		int InterssectCount = RayMeshIntersectionCount(WorldLocation, OuterPoint, StaticMeshComp->GetStaticMesh(), StaticMeshComp->GetComponentTransform());
				// 
				//		// If inside mesh (count is odd) then do not check surfaces
				//		if (InterssectCount & 1)
				//			bShouldCheckForSurface = false;
				//	}
				//}
			}
		}

		/***********************************************************************************************************/

#endif

#pragma endregion


		if (bShouldCheckForSurface)
		{
			bool result = CheckPointForSurface(
				GridPointIndex,
				WorldLocation,
				SurfaceNavGridPoints[i][j][k],
				GridPointOverlapSphereQueryParams);

			if (result)
				hasAnyOverlaps = true;

			TraceResults.Emplace(FSurfaceNavGridPointTraceResult(GridPointIndex, WorldLocation, result));
		}
		else
		{
			// Still need to add an entry for this grid point even if it has not surfaces. This is needed
			// To cleanup the old grid point if there is one.
			TraceResults.Emplace(FSurfaceNavGridPointTraceResult(GridPointIndex, WorldLocation, false));
		}

		k++;

		if (k < voxelCount)
			continue;

		k = 0;
		j++;

		if (j < voxelCount)
			continue;

		j = 0;
		i++;

		if (i < voxelCount)
			continue;

		bCanIterateGridPoints = false;

	}

	// Don't continue if the block has been marked as dirty since we started
	if (bIsDirty)
	{
		bIsProcessingGrid = false;
		return;
	}

	if (hasAnyOverlaps)
	{
		// We know we have some overlaps so let find any nav areas overlapping this block.
		// We could do this when we initially place the block, but nav areas can dynamically change so get them now

		// Container for nav modifiers which intersect this block
		TArray<TTuple<INavRelevantInterface*, FBox>> IntersectingNavMods;

		if (NavSys->bIsCleaningUp)
		{
			bIsProcessingGrid = false;
			bIsDirty = true;
			return;
		}

		{
			FReadScopeLock rModVolumesLock(NavSys->NavModVolumesLock);

			TSet<ANavModifierVolume*>& AllNavModVolumes = NavSys->GetNavModVolumes();

			//if (AllNavModVolumes.Num() == 0)
			//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::CheckForSurface(): Nav mods found = %d"), AllNavModVolumes.Num());

			// Let's get all the nav mod volumes that intersect this block
			for (auto It = AllNavModVolumes.CreateConstIterator(); It; ++It)
			{
				auto& NavModVolume = *It;

				if (FBoxSphereBounds::BoxesIntersect(PaddedBounds, NavModVolume->GetBounds()))
				{
					IntersectingNavMods.Emplace(NavModVolume, NavModVolume->GetBounds().GetBox());
				}
			}
		}

		{
			FReadScopeLock rModCompsLock(NavSys->NavModComponentsLock);

			TSet<UNavModifierComponent*>& AllNavModComponents = NavSys->GetNavModComponents();

			// Let's get all the nav mod components that intersect this block
			for (auto It = AllNavModComponents.CreateConstIterator(); It; ++It)
			{
				auto& NavModComponent = *It;

				if (!NavModComponent->IsNavigationRelevant() || !NavModComponent->CanEverAffectNavigation() ||
					NavModComponent->IsBeingDestroyed())
					continue;

				// TODO: Fix ensure failure due to array num changing
				FBox NavBounds = NavModComponent->GetNavigationBounds();
				if (FBoxSphereBounds::BoxesIntersect(PaddedBounds, NavBounds))
				{
					IntersectingNavMods.Emplace(NavModComponent, NavBounds);
				}
			}
		}


		// Container for nav links which intersect this block
		TArray<FSurfaceNavLinkData> IntersectingNavLinks;
		{
			// This lock may not have a large enough scope...may need to wrap more of the below code
			// to prevent changes while using these objects.
			FReadScopeLock rNavLinkLock(NavSys->GetNavLinkLock());

			TMap<int64, FSurfaceNavLinkData>& LocalCustomLinks = NavSys->SurfaceNavCustomLinksMap; // GetSurfaceNavCustomLinks();

			// Let's get all the nav links that intersect this block
			for (const TPair<int64, FSurfaceNavLinkData>& CustomLinkKeyPair : LocalCustomLinks)
			{
				FSurfaceNavLinkData const& CustomLink = CustomLinkKeyPair.Value;

				// Don't bother with it if it's not relevant
				if (!CustomLink.bIsRelevant)
					continue;

				FBox LinkBounds(ForceInitToZero);
				LinkBounds += CustomLink.WorldPtA;
				LinkBounds += CustomLink.WorldPtB;

				LinkBounds = LinkBounds.ExpandBy(CustomLink.Radius);

				if (FBoxSphereBounds::BoxesIntersect(PaddedBounds, LinkBounds))
				{
					// Add to collection on intersecting nav links
					IntersectingNavLinks.Emplace(CustomLink);
				}
			}
		}

		auto GridPointTraceQueryParams = FCollisionQueryParams();
		GridPointTraceQueryParams.bTraceComplex = false;
		GridPointTraceQueryParams.bReturnPhysicalMaterial = false;

		const float Distance = SurfaceNavProxySettings::GridPointDistance;
		//auto& LocalTraceResults = TraceResults;
		/*ParallelFor(TraceResults.Num(), [&LocalTraceResults, &IntersectingNavMods, IntersectingNavLinks](int32 Index)
			{
			});*/




		for (auto& TraceResult : TraceResults)
		{
			/*ParallelFor(TraceResults.Num(), [this, &Distance, &GridPointTraceQueryParams, &IntersectingNavMods, &IntersectingNavLinks](int32 Index)
				{
					auto& TraceResult = TraceResults[Index];*/

			if (TraceResult.OverlapFound)
			{
				int TraceHitCount = 0;

				const FVector startLoc = TraceResult.WorldLocation;
				FVector endLoc;

				double LowerEndLocX = startLoc.X - Distance;
				double UpperEndLocX = startLoc.X + Distance;

				double LowerEndLocY = startLoc.Y - Distance;
				double UpperEndLocY = startLoc.Y + Distance;

				double LowerEndLocZ = startLoc.Z - Distance;
				double UpperEndLocZ = startLoc.Z + Distance;

				/*****************************/
				/* Perform traces for x axis */
				endLoc = FVector(LowerEndLocX, startLoc.Y, startLoc.Z); // startLoc + FVector(-Distance, 0, 0); // *Distance;
				TraceFromGridPoint(startLoc, endLoc, GridPointTraceQueryParams, TraceResult, 0, IntersectingNavMods, IntersectingNavLinks);
				//continue;

				endLoc = FVector(UpperEndLocX, startLoc.Y, startLoc.Z); // startLoc + FVector(Distance, 0, 0); // *Distance;
				TraceFromGridPoint(startLoc, endLoc, GridPointTraceQueryParams, TraceResult, 1, IntersectingNavMods, IntersectingNavLinks);
				//continue;
			/*****************************/

			/*****************************/
			/* Perform traces for y axis */
				endLoc = FVector(startLoc.X, LowerEndLocY, startLoc.Z); // startLoc + FVector(0, -Distance, 0); // *Distance;
				TraceFromGridPoint(startLoc, endLoc, GridPointTraceQueryParams, TraceResult, 2, IntersectingNavMods, IntersectingNavLinks);
				//continue;

				endLoc = FVector(startLoc.X, UpperEndLocY, startLoc.Z); // startLoc + FVector(0, Distance, 0); // *Distance;
				TraceFromGridPoint(startLoc, endLoc, GridPointTraceQueryParams, TraceResult, 3, IntersectingNavMods, IntersectingNavLinks);
				//continue;
			/*****************************/

			/*****************************/
			/* Perform traces for z axis */
				endLoc = FVector(startLoc.X, startLoc.Y, LowerEndLocZ); // startLoc + FVector(0, 0, -Distance); // *Distance;
				TraceFromGridPoint(startLoc, endLoc, GridPointTraceQueryParams, TraceResult, 4, IntersectingNavMods, IntersectingNavLinks);
				//continue;

				endLoc = FVector(startLoc.X, startLoc.Y, UpperEndLocZ); // startLoc + FVector(0, 0, Distance); // *Distance;
				TraceFromGridPoint(startLoc, endLoc, GridPointTraceQueryParams, TraceResult, 5, IntersectingNavMods, IntersectingNavLinks);
				//continue;
			/*****************************/
			}
		}
	}

	// Don't continue if the block has been marked as dirty since we started
	if (bIsDirty || !bIsActive || bIsPendingCleanup)
	{
		bIsProcessingGrid = false;
		return;
	}

	NavSys->QueueFinishProcessingBlockGridPoint(*this->BlockKey);

	//AsyncTask(ENamedThreads::GameThread, [this]() //, CurBlockIndex]()
	//	{
	//		FinishProcessingGridPoints();


	//	});

}

void FSurfaceNavBlock::FinishProcessingGridPoints()
{
	UWorld* MyWorld = World.Get();

	// Test before we get the current nav sys
	if (!bIsActive || bIsPendingCleanup || !MyWorld || !World.IsValid(false, true) || MyWorld->bIsTearingDown)
	{
		bIsProcessingGrid = false;
		bIsDirty = true;
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::CheckForSurface(): Getting Read Lock - DestroyLock"));

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(World.Get());

	//USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	MyWorld = World.Get();

	// Check again after getting nav sys to make sure nothing changed
	if (!NavSys || NavSys->bIsCleaningUp || !bIsActive || bIsPendingCleanup || !MyWorld || !World.IsValid(false, true) || MyWorld->bIsTearingDown)
	{
		bIsProcessingGrid = false;
		bIsDirty = true;
		return;
	}

	//USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());
	//FReadScopeLock rDestroyLock(NavSys->DestroyLock);

	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::CheckForSurface(): Read Lock Acquired - DestroyLock"));

	//check(bIsProcessingGrid);

	// Check yet again in case something changed since we got the lock
	if (!bIsProcessingGrid || !NavSys || NavSys->bIsCleaningUp || !bIsActive || bIsPendingCleanup) // || !GetWorld() || GetWorld()->bIsTearingDown)
	{
		bIsProcessingGrid = false;
		bIsDirty = true;
		return;
	}

	FSurfaceNavBlock* Myself = NavSys->FindBlockByIndex(*BlockKey);

	check(this == Myself);

	// Not me so stop
	// This really shouldn't happen, but does occasionally so handle anyway.
	// NOTE: This may no longer be an issue based on above check never breaking
	if (!Myself || this != Myself)
	{
		bIsProcessingGrid = false;
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::CheckForSurface()->AsyncTask: Read Lock - NavBlockLock"));
	//FReadScopeLock rNavBlockLock(NavBlockLock);

	if (!bIsActive || bIsPendingCleanup)
	{
		bIsProcessingGrid = false;
		bIsDirty = true;
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::CheckForSurface(): 2"));

	FWriteScopeLock wNavPointLock(NavPointLock);

	TArray<FSurfaceNavGridPoint*> ValidGridPoints;

	FReadScopeLock rPathingLock(NavSys->PathingLock);

	for (auto& TraceResult : TraceResults)
	{
		if (!NavSys || NavSys->bIsCleaningUp || !bIsActive || bIsPendingCleanup || !MyWorld || !World.IsValid(false, true) || MyWorld->bIsTearingDown)
			break;

		FIntVector GridPointIndex = TraceResult.GridPointIndex;

		check(TraceResult.TraceHitCount >= 0 && TraceResult.TraceHitCount <= 6);

		FSurfaceNavGridPoint*& GridPoint = SurfaceNavGridPoints[GridPointIndex.X][GridPointIndex.Y][GridPointIndex.Z];

		if (GridPoint != nullptr)
			GridPointCanBeReleased(GridPointIndex);

		if (TraceResult.TraceHitCount > 0 && !TraceResult.IsBoxedIn())
		{
			// The grid point was nullptr so get an instance from the pool
			GridPoint = GridPointPool->Pop(
				TraceResult.WorldLocation,
				GridPointIndex);

			GridPoint->OwningBlock = this;
			FoundSurfaceCount++;

			LivingGridPointCount.Increment();

			GridPoint->SetForceIsOrphaned(TraceResult.IsOrphaned);
			GridPoint->ConfigNavPoints(TraceResult);

			ValidGridPoints.Emplace(GridPoint);
			//GridPoint->FinalizeNavPointsAlt();
		}
	}

	// Only finalize after all grid points are in place.
	// Phase 1 is to build adjacent neighbors
	for (auto& GridPoint : ValidGridPoints)
	{
		if (!NavSys || NavSys->bIsCleaningUp || !bIsActive || bIsPendingCleanup || !MyWorld || !World.IsValid(false, true) || MyWorld->bIsTearingDown)
			break;

		GridPoint->FinalizeNavPointsAlt(1);
	}

	int MaxIndex = SurfaceNavProxySettings::SingleAxisVoxelCount - 1;

	// Phase 2 is to build diagonal neighbors
	for (auto& GridPoint : ValidGridPoints)
	{
		//int X = GridPoint->VectorIndex.X;
		//int Y = GridPoint->VectorIndex.Y;
		//int Z = GridPoint->VectorIndex.Z;

		//// Make sure we only let edge grid points process a second time
		//if (X != 0 && X != MaxIndex && Y != 0 && Y != MaxIndex && Z != 0 && Z != MaxIndex)
		//	continue;

		if (!NavSys || NavSys->bIsCleaningUp || !bIsActive || bIsPendingCleanup || !MyWorld || !World.IsValid(false, true) || MyWorld->bIsTearingDown)
			break;

		GridPoint->FinalizeNavPointsAlt(2);
	}


#if WITH_EDITOR
	DrawDebugNavPoints();
#endif

	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::CheckForSurface(): 3"));

	bIsProcessingGrid = false;

	// Let builder know the block's nav has updated
	BlockNavUpdatedDel->ExecuteIfBound(*BlockKey);

	//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::CheckForSurface(): 4"));

}

inline bool FSurfaceNavBlock::TraceFromGridPoint(const FVector& startLoc, const FVector& endLoc, const FCollisionQueryParams& GridPointTraceQueryParams,
	FSurfaceNavGridPointTraceResult& TraceResult, const int SingleTraceIndex, TArray<TTuple<INavRelevantInterface*, FBox>> IntersectingNavMods,
	TArray<FSurfaceNavLinkData> IntersectingNavLinks)
{
	// ***********************************

	auto TraceDir = endLoc - startLoc;
	TraceDir.Normalize();

	TraceResult.SingleTraceResults[SingleTraceIndex] = FSurfaceNavGridPointSingleTraceResult();
	auto& SingleTraceResult = TraceResult.SingleTraceResults[SingleTraceIndex];

	SingleTraceResult.TraceIndex = FIntVector(FMath::RoundToInt(TraceDir.X), FMath::RoundToInt(TraceDir.Y), FMath::RoundToInt(TraceDir.Z));

	// If we already determined this gridpoint to be orphaned then don't
	// waste time doing other line traces
	if (TraceResult.IsOrphaned)
		return true;

	FHitResult Hit;

	FCollisionShape TraceSphere = FCollisionShape::MakeSphere(SurfaceNavProxySettings::TraceSphereRadius);

	// TODO: Test without any offset as this may not even be needed
	FVector StartOffset = TraceDir * SurfaceNavProxySettings::TraceSphereStartOffset;
	FVector EndOffset = TraceDir * SurfaceNavProxySettings::TraceSphereEndOffset;

	if (TraceForNav
	(
		Hit,
		startLoc + StartOffset,
		endLoc + EndOffset,
		TraceSphere,
		GridPointTraceQueryParams
	))
	{
		auto HitActor = Hit.GetActor();
		auto HitComponent = Hit.GetComponent();

		// Some types of components don't have actors, such as BSP brushes, so handle accordingly using this flag
		bool HasHitActor = HitActor != nullptr;

		if (HitComponent == nullptr || HitComponent->IsBeingDestroyed() || //!HitComponent->HasBegunPlay() || 
			(HasHitActor && (HitActor->IsActorBeingDestroyed() || HitActor->IsA<APawn>()))) // || !HitComponent->IsNavigationRelevant())
			return true; // return true to allow tracing other directions to continue

		{
			// Make sure this component is relevant to navigation.
			// HACK: Since the Voxel Plugin 2.0 has the bCanEverAffectNavigation flag turned off for UVoxelCollisionComponent let's just
			// assume it's navigable if it is that type. This may not always be the case and may need a better implementation
			// down the line.
			bool IsNavigationRelevant =
				HitComponent->CanEverAffectNavigation() ||
				//HitComponent->IsNavigationRelevant() ||
				HitComponent->GetClass()->GetName().Equals(SurfaceNavBlockConsts::VoxelPluginCollisionCompStr) ||
				(!HasHitActor && HitComponent->IsA<UModelComponent>());

			if (!IsNavigationRelevant)
				return true;
		}

		// We only want to ignore self if it's a landscape, because landscapes cause us some issues
		// with the reverse trace.
		bool ignoreSelf = false;

		// Is the impact normal in generally the same direction of the down vector?
		// If so, check if the hit actor is landscape as well as we might be under it.
		if (HasHitActor && HitActor->IsA<ALandscapeProxy>())
		{
			if ((Hit.ImpactNormal | FVector::DownVector) > 0.f)
			{
				// We're under the landscape so this is an orphaned grid point
				TraceResult.IsOrphaned = true;
				return false; // return false to prevent tracing other directions
			}

			ignoreSelf = true;
		}
		// HACK: We need to do certain normal tests if this is a WorldScape mesh component for proper
		// navigation.
		// TODO: May want a flag in proxy to indicate if using WorldScape to speed up this test.
		else if (HitComponent->GetClass()->GetName().Equals(SurfaceNavBlockConsts::WorldScapeMeshCompStr))
		{
			// If we're working on a flat world then we're just checking against down normal
			if (IsFlatWorld)
			{
				// On a flat world so always compare normal to down vector
				if ((Hit.ImpactNormal | FVector::DownVector) > 0.f)
				{
					// We're under the landscape so this is an orphaned grid point
					TraceResult.IsOrphaned = true;
					return false; // return false to prevent tracing other directions
				}
			}
			else // Else we're on something like a planet and need to check against normal towards center of actor
			{
				// On a planet so compare normal to direction to center of planet
				if ((Hit.ImpactNormal | (HitActor->GetActorLocation() - Hit.ImpactPoint).GetSafeNormal()) > 0.f)
				{
					// We're under the landscape so this is an orphaned grid point
					TraceResult.IsOrphaned = true;
					return false; // return false to prevent tracing other directions
				}
			}

			ignoreSelf = true;
		}

		FVector HitLoc(Hit.ImpactPoint);


		// *******************************
		// -------------------------------
		// It's possible this hit was behind/inside another collision object
		// so do another trace in reverse to test for this.
		// If that's the case then we don't want to process it.
		{
			FHitResult ReverseHit;

			FCollisionQueryParams ReverseGridPointTraceQueryParams = FCollisionQueryParams();
			ReverseGridPointTraceQueryParams.bTraceComplex = false;
			ReverseGridPointTraceQueryParams.bReturnPhysicalMaterial = false;
			if (ignoreSelf && HasHitActor)
				ReverseGridPointTraceQueryParams.AddIgnoredActor(HitActor);

			if (TraceForNav
			(
				ReverseHit,
				HitLoc,
				startLoc,
				TraceSphere,
				ReverseGridPointTraceQueryParams
			))
			{
				if (ReverseHit.IsValidBlockingHit())
				{
					auto ReverseHitActor = ReverseHit.GetActor();
					auto ReverseHitComponent = ReverseHit.GetComponent();

					bool HasReverseHitActor = ReverseHitActor != nullptr;

					bool InvalidHit = ReverseHitComponent == nullptr || (HasReverseHitActor && ReverseHitActor->IsA<APawn>()); // || !ReverseHitComponent->IsNavigationRelevant();

					// If the component is valid for navigation up to this point, then let's also make sure it's
					// navigation relevant
					if (!InvalidHit)
					{
						// Make sure this component is relevant to navigation.
						// HACK: Since the Voxel Plugin 2.0 has the bCanEverAffectNavigation flag turned off for UVoxelCollisionComponent let's just
						// assume it's navigable if it is that type. This may not always be the case and may need a better implementation
						// down the line.
						//InvalidHit = !(ReverseHitComponent->IsNavigationRelevant() || ReverseHitComponent->GetClass()->GetName().Equals(SurfaceNavBlockConsts::VoxelPluginCollisionCompStr)); // ((UVoxelCollisionComponent*)ReverseHitComponent));
						InvalidHit = !(ReverseHitComponent->CanEverAffectNavigation() || ReverseHitComponent->GetClass()->GetName().Equals(SurfaceNavBlockConsts::VoxelPluginCollisionCompStr)); // ((UVoxelCollisionComponent*)ReverseHitComponent));
					}

					//// If the component is valid for navigation up to this point, then check to see if we need to prevent a false positive
					//// on detecting orphans with Voxel Plugin 1.2.
					//if (!InvalidHit && (UVoxelProceduralMeshComponent*)ReverseHitComponent && (UVoxelProceduralMeshComponent*)HitComponent)
					//	// For some reason, when making changes to VP 1.2 terrain, it sometimes picks up reverse hits even when nothing shows
					//	// there...could be some kind of timing or race condition issue.
					//	InvalidHit = true;

						//InvalidHit = (UVoxelProceduralMeshComponent*)ReverseHitComponent && (UVoxelProceduralMeshComponent*)HitComponent;


					//// The landscape sometimes detects reverse hits at the same location, or close to it, so if our reverse hit is
					//// also on the landscape and is the same component as the original hit and VERY close by then mark it as an
					//// invalid hit.
					//if (!InvalidHit && ReverseHitActor->IsA<ALandscapeProxy>() && HitComponent == ReverseHitComponent &&
					//	ReverseHit.ImpactPoint.Equals(Hit.ImpactPoint, KINDA_SMALL_NUMBER))
					//	//FVector::DistSquared(Hit.ImpactPoint, ReverseHit.ImpactPoint) < KINDA_SMALL_NUMBER)
					//	InvalidHit = true;


					if (!InvalidHit)
					{
						TraceResult.IsOrphaned = true;
						return false; // return false to prevent tracing other directions
					}
				}
			}
		}

		// -------------------------------
		// *******************************


		bool IsInNavModVolume = false;
		TSubclassOf<UNavArea> AreaClass = nullptr;
		UNavArea* NavArea = nullptr;

		UWorld* MyWorld = World.Get();
		if (!MyWorld || !World.IsValid(false, true) || World.IsStale(true, true) || MyWorld->bIsTearingDown)
		{
			TraceResult.IsOrphaned = true;
			return false; // return false to prevent tracing other directions
		}

		USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(MyWorld);
		if (!NavSys || NavSys->bIsCleaningUp)
		{
			// This shouldn't normally happen, but could if using level streaming and unloading at just the right time

			TraceResult.IsOrphaned = true;
			return false; // return false to prevent tracing other directions
		}

		{ // Begin NavSys destroy lock
			//FReadScopeLock rDestroyLock(NavSys->DestroyLock);

			// Now that we have the lock let's test again
			if (!MyWorld || !World.IsValid(false, true) || World.IsStale(true, true) || MyWorld->bIsTearingDown || NavSys->bIsCleaningUp)
			{
				// This shouldn't normally happen, but could if using level streaming and unloading at just the right time
				TraceResult.IsOrphaned = true;
				return false; // return false to prevent tracing other directions
			}

			// Iterate through all the nav modifiers that intersected this block to
			// see if the point is inside the nav modifier
			//for (auto& NavMod : IntersectingNavMods)
			for (auto IntersectingNavModsIt = IntersectingNavMods.CreateIterator(); IntersectingNavModsIt; ++IntersectingNavModsIt)
			{
				INavRelevantInterface*& NavMod = IntersectingNavModsIt->Key;

				ANavModifierVolume* NavModVolume = Cast<ANavModifierVolume>(NavMod);
				bool NavModContainsPoint = false;
				TSubclassOf<UNavArea> NewAreaClass = NULL;

				if (NavModVolume)
				{
					FReadScopeLock rModVolumesLock(NavSys->NavModVolumesLock);

					if (!NavMod)
						continue;

					NewAreaClass = NavModVolume->GetAreaClass();

					auto VolumeRotation = NavModVolume->GetActorRotation();
					auto VolumeOrigin = NavModVolume->GetActorLocation();

					auto RelativePointVector = HitLoc - VolumeOrigin;


					//****************************************************
					//****************************************************
					// Do a faster test here to see if the point is in the volume's adjusted bounds
					// before doing the more expensive tests
					//----------------------------------------------------

					auto AdjustedPointVector = VolumeRotation.UnrotateVector(RelativePointVector) + VolumeOrigin;
					auto VolumeTransform = NavModVolume->GetBrushComponent()->GetComponentTransform();
					VolumeTransform.SetRotation(FQuat::MakeFromEuler(FVector::ZeroVector));
					auto BrushCompBounds = NavModVolume->GetBrushComponent()->CalcBounds(VolumeTransform);
					auto DistToPoint = BrushCompBounds.ComputeSquaredDistanceFromBoxToPoint(AdjustedPointVector);
					if (DistToPoint > 1.f)
					{
						// We're not inside the volume's bounds so skip the rest and go to the next volume
						continue;
					}
					else
					{
						// TODO: Currently we can only support cubes until we find a way to test if a point
						// is inside a volume without the editor so just rely on the simple test for now.
						NavModContainsPoint = true;
					}

					// Make sure enhanced tests are on as the next bit is more expensive
					if (SurfaceNavProxySettings::EnhancedNavModBrushTest)
					{
						//****************************************************
						//****************************************************
						// Let's do some complex stuff to find out if the point is inside the complex volume
						//----------------------------------------------------
						// Get the volume's scale and bounds to use below
						FVector Scale = NavModVolume->GetActorScale();
						FBoxSphereBounds VolumeBounds = NavModVolume->GetBounds();

						// Now apply the opposite of the volumes rotation to the point so we can get the location of
						// the point relative to the unrotated volume
						AdjustedPointVector = VolumeRotation.UnrotateVector(RelativePointVector); // +VolumeOrigin;

						// Let's de-scale the point
						AdjustedPointVector /= Scale;

						auto BoxMax = VolumeBounds.GetBox().Max - VolumeOrigin;

						// Going out to a corner of the box is too "clean" and can result in the line/ray going exactly through a
						// point or edge on the poly and giving odd results. To fix this, first scale up a bit based on multiple 
						// of current scale. Then divide by scale to get the de-scaled corner point and finally add some randomness.
						FVector OuterPoint = FVector(BoxMax.X, 0.f, 0.f) / Scale * 1.1f;

						// Find insersections from this point to a point known to be outside of the volume
						int IntersectionCount = RayMeshIntersectionCount(AdjustedPointVector, OuterPoint, NavModVolume->Brush);
						int IsInsideCount = IntersectionCount > 0 && IntersectionCount % 2 == 1 ? 1 : 0;

						OuterPoint = FVector(0.f, BoxMax.Y, 0.f) / Scale * 1.1f;
						IntersectionCount = RayMeshIntersectionCount(AdjustedPointVector, OuterPoint, NavModVolume->Brush);
						IsInsideCount += IntersectionCount > 0 && IntersectionCount % 2 == 1 ? 1 : 0;

						OuterPoint = FVector(0.f, 0.f, BoxMax.Z) / Scale * 1.1f;
						IntersectionCount = RayMeshIntersectionCount(AdjustedPointVector, OuterPoint, NavModVolume->Brush);
						IsInsideCount += IntersectionCount > 0 && IntersectionCount % 2 == 1 ? 1 : 0;

						NavModContainsPoint = IsInsideCount == 3;
					}
				}
				else
				{
					FReadScopeLock rModCompsLock(NavSys->NavModComponentsLock);

					auto NavModComp = Cast<UNavModifierComponent>(NavMod);
					if (!NavModComp)
						continue;

					AActor* NavModCompActor = NavModComp->GetOwner();
					NewAreaClass = NavModComp->AreaClass;

					//FVector ActorOrigin = NavModCompActor->GetActorLocation();
					//FRotator ActorRotation = NavModCompActor->GetActorRotation();

					FBox NavModCompBounds = IntersectingNavModsIt->Value; // NavModComp->GetNavigationBounds();

					if (!HasHitActor || HitActor != NavModCompActor)
					{
						double DistToPoint = NavModCompBounds.ComputeSquaredDistanceToPoint(HitLoc);
						if (DistToPoint > 40.f)
							continue;
					}

					if (NavModCompActor->IsPendingKillPending())
						continue;

					const TSet<UActorComponent*>& ActorComponents = NavModCompActor->GetComponents();

					for (auto ActorComponentsIt = ActorComponents.CreateConstIterator(); ActorComponentsIt; ++ActorComponentsIt)
					{
						auto& ActorComponent = *ActorComponentsIt;

						if (!ActorComponent->IsNavigationRelevant() || ActorComponent->IsBeingDestroyed())
							continue;

						auto PrimComp = Cast<UPrimitiveComponent>(ActorComponent);
						if (!PrimComp)
							continue;

						// If the hit actor is the same as this nav mod component actor then
						// it the hit point is contained, don't do extra processing below
						if (HitActor == NavModCompActor)
						{
							NavModContainsPoint = true;
							break;
						}

						FVector LocalLoc = PrimComp->GetComponentLocation();
						//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::TraceFromGridPoint: LocalLoc = %f, %f, %f"), LocalLoc.X, LocalLoc.Y, LocalLoc.Z);

							//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::TraceFromGridPoint: *********************"));

							//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::TraceFromGridPoint: LocalLoc = %f, %f, %f"), LocalLoc.X, LocalLoc.Y, LocalLoc.Z);

						auto CompScale = PrimComp->GetComponentScale();
						auto CompRot = PrimComp->GetComponentRotation();
						//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::TraceFromGridPoint: CompScale = %f, %f, %f"), CompScale.X, CompScale.Y, CompScale.Z);

#if ENGINE_MAJOR_VERSION == 5
						FBox CompBox = PrimComp->GetLocalBounds().GetBox().TransformBy(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(CompScale)));
#else
						FBox CompBox = PrimComp->CalcLocalBounds().GetBox().TransformBy(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(CompScale)));
#endif

						FVector RelativePointVector = HitLoc - LocalLoc;

						FVector AdjustedPointVector = CompRot.UnrotateVector(RelativePointVector);

						float DistToPoint = CompBox.ComputeSquaredDistanceToPoint(AdjustedPointVector);

						//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::TraceFromGridPoint: *********************"));

						if (DistToPoint > 40.f)
							continue;

						NavModContainsPoint = true;
						break;
					}
				}


				// Is the point contained within a nav modifier
				if (NavModContainsPoint) //DistanceToBox.IsNearlyZero())
				{
					if (!NewAreaClass)
						continue;

					UNavArea* NewNavArea = NewAreaClass->GetDefaultObject<UNavArea>();

					// Did we already find a volume we're in?
					if (IsInNavModVolume)
					{
						// If so, need to take priority into consideration here.
						// Null > Low Height > Obstacle > Custom > Default
						//UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::TraceFromGridPoint: Multiple nav volumes"));

						bool ReplaceAreaClass = false;

						if (NavArea == nullptr)
						{
							ReplaceAreaClass = true;
						}
						else
						{
							// They're the same so skip
							if (NavArea == NewAreaClass->GetDefaultObject())
								continue;

							// Null always takes priority so just need to test for it
							if (NewNavArea == UNavArea_Null::StaticClass()->GetDefaultObject())
								ReplaceAreaClass = true;

							// LowHeight priority is right below Null...if it's Null we wouldn't make it here so no need to do
							// any other tests
							else if (NewNavArea == UNavArea_LowHeight::StaticClass()->GetDefaultObject())
								ReplaceAreaClass = true;

							// Obstacle only happens if not LowHeight or Null...Null would have already been picked up
							// so don't need to test for it
							else if (NewNavArea == UNavArea_Obstacle::StaticClass()->GetDefaultObject()
								&& NavArea != UNavArea_LowHeight::StaticClass()->GetDefaultObject())
								ReplaceAreaClass = true;

							// Other custom class take priority over default, so if it's not default then it's a custom class
							else if (NewNavArea != UNavArea_Default::StaticClass()->GetDefaultObject()
								&& NavArea != UNavArea_LowHeight::StaticClass()->GetDefaultObject()
								&& NavArea != UNavArea_Obstacle::StaticClass()->GetDefaultObject())
								ReplaceAreaClass = true;

							else {} // Default which is lowest priority so don't replace
						}

						if (!ReplaceAreaClass)
							continue;
					}

					IsInNavModVolume = true;
					AreaClass = NewAreaClass;
					NavArea = NewNavArea;

					// Null is the highest priority Area class, so if that's what we have then stop
					if (NavArea == UNavArea_Null::StaticClass()->GetDefaultObject())
						break;
				}
			}

		} // End NavSys destroy lock

		for (auto& NavLink : IntersectingNavLinks)
		{
			// The current hit point can only be a neighbor to either nav link point A
			// or nav linke point B, not both.
			if (FVector::DistSquared(NavLink.WorldPtA, HitLoc) <= NavLink.RadiusSqrd)
			{
				SingleTraceResult.PointALinks.Emplace(NavLink);
			}
			else if (FVector::DistSquared(NavLink.WorldPtB, HitLoc) <= NavLink.RadiusSqrd)
			{
				SingleTraceResult.PointBLinks.Emplace(NavLink);
			}
		}

		// If no volume was found then let's set it. If it's a UShapeComponent then Unreal currently
		// sets normal navigation to Null so let's do that, otherwise set to Default.
		if (NavArea == nullptr)
		{
			UShapeComponent* ShapeHitComp = Cast<UShapeComponent>(HitComponent);
			if (HitComponent && ShapeHitComp) // HitComponent->IsA<UShapeComponent>())
			{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
				auto ShapeAreaClass = ShapeHitComp->GetDesiredAreaClass();
#else
				auto ShapeAreaClass = ShapeHitComp->AreaClass;
#endif
				if (ShapeAreaClass) // Use the shape's area class
					NavArea = static_cast<UNavArea*>(ShapeAreaClass.GetDefaultObject());
				else // Shapes default to obstacle according to docs
					NavArea = static_cast<UNavArea*>(UNavArea_Obstacle::StaticClass()->GetDefaultObject());
			}
			else
				NavArea = static_cast<UNavArea*>(UNavArea_Default::StaticClass()->GetDefaultObject());
		}

		SingleTraceResult.NavArea = NavArea;

		FVector HitNormal(Hit.ImpactNormal);

		SingleTraceResult.Hit = true;
		SingleTraceResult.ImpactLocation = HitLoc;
		SingleTraceResult.ImpactNormal = HitNormal;

		if (!IsFlatWorld && PlanetLoc != nullptr)
		{
			SingleTraceResult.GroundNormal = (HitLoc - *PlanetLoc).GetSafeNormal();

			/*if (AngleFromGround < 0.f || AngleFromGround > 45.f)
				UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::TraceFromGridPoint: 1 - Invalid Angle - Ground Normal=%s | HitNormal=%s | Dot=%d | Angle=%d"), *GroundNormal.ToString(), *HitNormal.ToString(), DotFromGround, AngleFromGround);*/

		}
		else
		{
			SingleTraceResult.GroundNormal = FVector(0, 0, 1);

			/*if (AngleFromGround < 0.f || AngleFromGround > 180.f)
				UE_LOG(LogTemp, Warning, TEXT("FSurfaceNavBlock::TraceFromGridPoint: 2 - Invalid Angle - Ground Normal=%s | HitNormal=%s | Dot=%d | Angle=%d"), *GroundNormal.ToString(), *HitNormal.ToString(), DotFromGround, AngleFromGround);*/
		}

		TraceResult.TraceHitCount++;
	}

	return true;
}

inline bool FSurfaceNavBlock::TraceForNav(FHitResult& Hit, const FVector& StartLoc, const FVector& EndLoc,
	const FCollisionShape& TraceSphere, const FCollisionQueryParams& GridPointTraceQueryParams)
{
	if (SurfaceNavProxySettings::TraceForNavDel.IsBound())
	{
		bool result = false;
		SurfaceNavProxySettings::TraceForNavDel.ExecuteIfBound(result, Hit, StartLoc, EndLoc, TraceSphere, GridPointTraceQueryParams);
		return result;
	}
	else if (SurfaceNavProxySettings::TraceMethod == ESurfaceNavTraceMethod::Profile)
		return World->SweepSingleByProfile
		(
			Hit,
			StartLoc,
			EndLoc,
			FQuat(),
			FName(SurfaceNavProxySettings::TraceProfile),
			TraceSphere,
			GridPointTraceQueryParams
		);
	else
		return World->SweepSingleByChannel
		(
			Hit,
			StartLoc,
			EndLoc,
			FQuat(),
			SurfaceNavProxySettings::TraceChannel,
			TraceSphere,
			GridPointTraceQueryParams
		);
}

inline bool FSurfaceNavBlock::CheckPointForSurface(
	FIntVector& VectorIndex,
	FVector& GridPointLoc,
	FSurfaceNavGridPoint*& GridPoint,
	FCollisionQueryParams& collisionQueryParams)
{
	if (SurfaceNavProxySettings::CheckPointForSurfaceDel.IsBound())
	{
		bool result = false;
		SurfaceNavProxySettings::CheckPointForSurfaceDel.ExecuteIfBound(result, VectorIndex, GridPointLoc, collisionQueryParams);
		return result;
	}
	else if (SurfaceNavProxySettings::TraceMethod == ESurfaceNavTraceMethod::Profile)
		return World->OverlapBlockingTestByProfile(
			GridPointLoc,
			SurfaceNavBlockConsts::DefaultFQuat,
			FName(SurfaceNavProxySettings::TraceProfile),
			SurfaceNavProxySettings::GridPointTestShape,
			collisionQueryParams);
	else
		return World->OverlapBlockingTestByChannel(
			GridPointLoc,
			SurfaceNavBlockConsts::DefaultFQuat,
			SurfaceNavProxySettings::TraceChannel,
			SurfaceNavProxySettings::GridPointTestShape,
			collisionQueryParams);
}


#if WITH_EDITOR
inline const void FSurfaceNavBlock::DrawDebugNavPoints()
{
	if (SurfaceNavProxySettings::ShowDebugNavPointsDuringBuild && World.Get() && World.IsValid(false, true) && !World->bIsTearingDown &&
		World->WorldType != EWorldType::Editor && World->WorldType != EWorldType::EditorPreview && World->WorldType != EWorldType::Inactive)
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
			{
				if (!this->IsValid() || !bIsActive || bIsPendingCleanup || !World.Get() || !World.IsValid(false) || World->bIsTearingDown)
					return;


				for (int X = 0; X < SurfaceNavGridPoints.Num(); X++)
				{
					for (int Y = 0; Y < SurfaceNavGridPoints[X].Num(); Y++)
					{
						for (int Z = 0; Z < SurfaceNavGridPoints[X][Y].Num(); Z++)
						{
							if (!bIsActive || bIsPendingCleanup || !World.Get() || !World.IsValid(false) || World->bIsTearingDown)
								return;

							FVector GridPointIndex(X, Y, Z);
							auto& GridPoint = SurfaceNavGridPoints[X][Y][Z];
							if (!GridPoint)
								continue;

							for (const TPair<FIntVector, TSharedPtr<FSurfaceNavPointBase>>& NavPointKeyPair : GridPoint->NavPoints)
							{
								//GridPoint->NavPoints
								auto NavPoint = NavPointKeyPair.Value; // GridPoint->NavPoints.Contains(NavPointIndex) ? NavPoints[NavPointIndex] : nullptr;
								if (!NavPoint || !NavPoint->IsValid())
									return;

								auto NavPointWorldLocation = NavPoint->WorldLocation;
								auto DrawColor = NavPoint->NavArea->DrawColor;

								FColor DrawColorNormal = FLinearColor(0.f, 0.8f, 0.f, 1.0f).ToFColor(true);
								float DrawDuration = 30.0f;
								bool DrawShadow = false;

								if (!this->IsValid() || !bIsActive || bIsPendingCleanup || !World.Get() || !World.IsValid(false) || World->bIsTearingDown)
									return;

								DrawDebugPoint
								(
									World.Get(),
									NavPointWorldLocation,
									10.f,
									DrawColor,
									false,
									2.f
								);
							}
						}
					}
				}
			});
	}
}
#endif




inline FSurfaceNavGridPointTraceResult::FSurfaceNavGridPointTraceResult(
	FIntVector initGridPointIndex, FVector initWorldLocation, bool initOverlapFound
) :
	GridPointIndex(initGridPointIndex), WorldLocation(initWorldLocation), OverlapFound(initOverlapFound), IsOrphaned(0), TraceHitCount(0)
{
}





#pragma region Functions for testing if point is inside complex shapes

bool FSurfaceNavBlock::DoesLineIntersectPoly(TArray<FVector> Verts, FVector Normal, FVector Start, FVector End, FVector& Intersect, bool& ForceInside, bool& ForceOutside)
{
	// If the ray doesn't cross the plane, don't bother going any further.
	const float DistStart = FVector::PointPlaneDist(Start, Verts[0], Normal);
	const float DistEnd = FVector::PointPlaneDist(End, Verts[0], Normal);

	if ((DistStart < 0 && DistEnd < 0) || (DistStart > 0 && DistEnd > 0))
	{
		return 0;
	}

	// Get the intersection of the line and the plane.
	FVector Intersection = FMath::LinePlaneIntersection(Start, End, Verts[0], Normal);
	Intersect = Intersection;
	if (Intersection.Equals(Start) || Intersection.Equals(End))
	{
		return 0;
	}

	ForceInside = DistStart < 0.f;

	return 1;
}

#if ENGINE_MAJOR_VERSION < 5
int FSurfaceNavBlock::RayMeshIntersectionCount(FVector rayOrigin, FVector rayVector, UModel* Brush)
#else
int FSurfaceNavBlock::RayMeshIntersectionCount(FVector rayOrigin, FVector rayVector, TObjectPtr<UModel> Brush)
#endif
{
	TArray<FBspNode> Nodes = Brush->Nodes;

	int IntersectionCount = 0;

	FVector ClosestIntersectPoint(0.f);
	double ClosestDistSq = 0.f;
	bool InOrOut = 0;

	for (auto& Node : Nodes)
	{
		int iVertexIndex = Node.iVertexIndex;
		int NumVerts = Node.NumVertices;

		if (iVertexIndex < 0 || NumVerts < 1)
			continue;

		FVector IntersectPoint(0.f);

		int iPoint = Brush->Verts[iVertexIndex].pVertex;
		int iMax = iPoint + NumVerts;

		TArray<FVector> Points;

		for (int i = iPoint; i <= iMax; i++)
			Points.Emplace(FVector(Brush->Points[iPoint]));

		FVector Normal(Node.Plane);
		bool ForceInside = false;
		bool ForceOutside = false;

		if (DoesLineIntersectPoly(Points, Normal, rayOrigin, rayVector, IntersectPoint, ForceInside, ForceOutside))
		{
			double DistSq = FVector::DistSquared(IntersectPoint, rayOrigin);
			if (ClosestDistSq == 0.f || DistSq < ClosestDistSq)
			{
				ClosestDistSq = DistSq;
				ClosestIntersectPoint = IntersectPoint;
				InOrOut = ForceInside;
			}

			IntersectionCount++;
		}
	}

	if (ClosestDistSq > 0.f)
		return InOrOut;
	return IntersectionCount;
}

#pragma endregion



#pragma region Functions for testing if point is inside mesh

//// May not want to use as this requires allowing CPU access to the mesh and hurts performance
//// since we need to do so many back to back.
//
//int FSurfaceNavBlock::RayMeshIntersectionCount(FVector rayOrigin, FVector rayVector, UStaticMesh* Mesh, FTransform Transform)
//{
//	if (!Mesh || !Mesh->bAllowCPUAccess)
//		return 0;
//
//	int IntersectionCount = 0;
//
//	TArray<FVector> Vertices;
//	TArray<int32> Triangles;
//	TArray<FVector> Normals;
//	TArray<FVector2D> UVs;
//	TArray<FProcMeshTangent> Tangents;
//
//	UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(Mesh, 0, 0, Vertices, Triangles, Normals, UVs, Tangents);
//
//	int LoopIndex = 0;
//
//	while (LoopIndex < Triangles.Num())
//	{
//		bool TrianglesCanBeIntersected = false;
//
//		int32 Triangle1 = Triangles[LoopIndex];
//		int32 Triangle2 = Triangles[LoopIndex + 1];
//		int32 Triangle3 = Triangles[LoopIndex + 2];
//
//		if (FVector::DotProduct(TransformVector(Transform, Normals[Triangle1]), rayVector) != 0.f
//			&& FVector::DotProduct(TransformVector(Transform, Normals[Triangle2]), rayVector) != 0.f
//			&& FVector::DotProduct(TransformVector(Transform, Normals[Triangle3]), rayVector) != 0.f)
//		{
//			TransformVector(Transform, Vertices[Triangle1]);
//
//			TStaticArray<FVector, 3> TriangleVerts;
//			TriangleVerts[0] = TransformVector(Transform, Vertices[Triangle1]);
//			TriangleVerts[1] = TransformVector(Transform, Vertices[Triangle2]);
//			TriangleVerts[2] = TransformVector(Transform, Vertices[Triangle3]);
//
//			FVector intersectionPoint;
//
//			if (LineTriangleIntersection(rayOrigin, rayVector, TriangleVerts, intersectionPoint))
//				IntersectionCount++;
//
//		}
//
//		LoopIndex += 3;
//	}
//
//	return IntersectionCount;
//}
//
//bool FSurfaceNavBlock::LineTriangleIntersection(FVector rayOrigin, FVector rayVector, TStaticArray<FVector, 3> triangle, FVector& intersectionPoint)
//{
//	if (triangle.Num() < 3) return false;
//	const float EPSILON = 0.0000001f;
//	FVector vertex0 = triangle[0];
//	FVector vertex1 = triangle[1];
//	FVector vertex2 = triangle[2];
//	FVector edge1, edge2, h, s, q;
//	float a, f, u, v;
//	edge1 = vertex1 - vertex0;
//	edge2 = vertex2 - vertex0;
//	h = FVector::CrossProduct(rayVector, edge2);
//	a = FVector::DotProduct(edge1, h);
//	if (a > -EPSILON && a < EPSILON)
//		return false;	// This ray is parallel to the triangle
//
//	f = 1.0f / a;
//	s = rayOrigin - vertex0;
//	u = f * (FVector::DotProduct(s, h));
//	if (u < 0.0f || u > 1.0f)
//		return false;
//	q = FVector::CrossProduct(s, edge1);
//	v = f * FVector::DotProduct(rayVector, q);
//	if (v < 0.0f || u + v > 1.0f)
//		return false;
//
//	// At this stage we can computer t to find out where the intersection point is on the line
//	float t = f * FVector::DotProduct(edge2, q);
//	if (t > EPSILON)	// ray intersection
//	{
//		intersectionPoint = rayOrigin + rayVector * t;
//		return true;
//	}
//
//	// This means that there is a line intersection but not a ray intersection
//	return false;
//}

#pragma endregion

