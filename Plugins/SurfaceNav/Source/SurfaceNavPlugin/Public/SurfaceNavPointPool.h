// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "SurfaceNavBlock.h"
#include "SurfaceNavGridPoint.h"
#include "SurfaceNavPoint.h"
#include "SurfaceNavCommon.h"

struct FSurfaceNavPoint;
class ASurfaceNavBlock;

struct SURFACENAVPLUGIN_API FSurfaceNavPointPool
{

public:

	FSurfaceNavPointPool();

protected:

	TSharedPtr<FSurfaceNavPointBase> CreateInstance();

public:

	/** Push a block back into the pool */
	void Release(TSharedPtr<FSurfaceNavPointBase>& navPoint);

	/** Pull a block off the pool */
	TSharedPtr<FSurfaceNavPointBase> Pop(
		const FIntVector& VectorIndex,
		const FVector& WorldLoc,
		const FVector& Normal
	);
};