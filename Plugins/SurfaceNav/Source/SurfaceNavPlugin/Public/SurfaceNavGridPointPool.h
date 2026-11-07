// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "SurfaceNavBlock.h"
#include "SurfaceNavGridPoint.h"
#include "SurfaceNavCommon.h"

struct FSurfaceNavPointPool;
struct FSurfaceNavGridPoint;
class ASurfaceNavBlock;

struct SURFACENAVPLUGIN_API FSurfaceNavGridPointPool
{

public:

	FSurfaceNavGridPointPool(
		const int& initPoolSize,
		UWorld& initWorld,
		const FGridPointFinishedDelegate& initGridPointFinishedDel,
		FSurfaceNavPointPool& initNavPointPool);

protected:

	FSurfaceNavGridPoint* CreateInstance();

	UWorld& World;

	const FGridPointFinishedDelegate& GridPointFinishedDel;

	const int& PoolSize;
	FSurfaceNavPointPool& NavPointPool;

	TArray<FSurfaceNavGridPoint*> AvailableGridPoints = {};

	FCollisionShape GridPointTestSphere;
	FCollisionQueryParams GridPointOverlapSphereQueryParams;
	FCollisionQueryParams GridPointTraceQueryParams;
	
public:

	/** Push a block back into the pool */
	void Release(FSurfaceNavGridPoint*& navGridPoint);

	/** Pull a block off the pool */
	FSurfaceNavGridPoint* Pop(
		const FVector& WorldLoc, 
		const FIntVector& IntLoc
	);
};