// Copyright BMK Games LLC. All Rights Reserved.


#include "SurfaceNavGridPointPool.h"
#include "SurfaceNavigationSystem.h"
//#include "SurfaceNavPlugin.h"




FSurfaceNavGridPointPool::FSurfaceNavGridPointPool(
	const int& initPoolSize,
	UWorld& initWorld,
	const FGridPointFinishedDelegate& initGridPointFinishedDel,
	FSurfaceNavPointPool& initNavPointPool) :
	World(initWorld),
	GridPointFinishedDel(initGridPointFinishedDel),
	PoolSize(initPoolSize),
	NavPointPool(initNavPointPool)
{
	AvailableGridPoints.Reserve(PoolSize);

	// Configure collision sphere for grid point overlap tests
	float gpDist = SurfaceNavProxySettings::GridPointDistance + 1.f;
	GridPointTestSphere = FCollisionShape::MakeSphere(gpDist);
	// *******************************************************

	// Create query params to reuse for each grid point
	GridPointOverlapSphereQueryParams = FCollisionQueryParams();
	GridPointOverlapSphereQueryParams.bTraceComplex = false;
	GridPointOverlapSphereQueryParams.bReturnPhysicalMaterial = false;
	GridPointOverlapSphereQueryParams.bSkipNarrowPhase = true;
	// *******************************************************

	// Create query params to reuse for each grid point
	GridPointTraceQueryParams = FCollisionQueryParams();
	GridPointTraceQueryParams.bTraceComplex = false;
	GridPointTraceQueryParams.bReturnPhysicalMaterial = false;
	// *******************************************************

	for (int i = 0; i < PoolSize; i++)
	{
		AvailableGridPoints.Emplace(CreateInstance());
	}
}

inline FSurfaceNavGridPoint* FSurfaceNavGridPointPool::CreateInstance()
{
	FSurfaceNavGridPoint* navGridPoint = new FSurfaceNavGridPoint(
		World,
		GridPointTestSphere,
		GridPointOverlapSphereQueryParams,
		GridPointTraceQueryParams,
		GridPointFinishedDel,
		NavPointPool
	);

	return navGridPoint;
}

void FSurfaceNavGridPointPool::Release(FSurfaceNavGridPoint*& navGridPoint)
{
	if (!navGridPoint)
		return;

	navGridPoint->Reset();
	navGridPoint->Destroy();

	check(!navGridPoint->IsValid());

	USurfaceNavigationSystem::QueueNavGridPointToDelete(navGridPoint);
}

/** Pull a block off the pool */
FSurfaceNavGridPoint* FSurfaceNavGridPointPool::Pop(
	const FVector& WorldLoc,
	const FIntVector& IntLoc)
{
	if (this->AvailableGridPoints.Num() != 0)
	{

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
		FSurfaceNavGridPoint* navGridPoint = this->AvailableGridPoints.Pop(false);
#else
		FSurfaceNavGridPoint* navGridPoint = this->AvailableGridPoints.Pop(EAllowShrinking::No);
#endif

		navGridPoint->SetLocation(WorldLoc);
		navGridPoint->VectorIndex = FIntVector(IntLoc);

		return navGridPoint;
	}
	else
	{
		FSurfaceNavGridPoint* navGridPoint = CreateInstance();
		navGridPoint->SetLocation(WorldLoc);
		navGridPoint->VectorIndex = FIntVector(IntLoc);

		return navGridPoint;
	}
}
