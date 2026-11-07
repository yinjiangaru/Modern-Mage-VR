// Copyright BMK Games LLC. All Rights Reserved.


#include "SurfaceNavPointPool.h"
#include "SurfaceNavigationSystem.h"

FSurfaceNavPointPool::FSurfaceNavPointPool()
{
}

inline TSharedPtr<FSurfaceNavPointBase> FSurfaceNavPointPool::CreateInstance()
{
	TSharedPtr<FSurfaceNavPointBase> navPoint = TSharedPtr<FSurfaceNavPointBase>(new FSurfaceNavPoint());

	return navPoint;
}

void FSurfaceNavPointPool::Release(TSharedPtr<FSurfaceNavPointBase>& navPoint)
{
	if (!navPoint) // || !navPoint->IsValid())
		return;

	// This shouldn't happen as that would mean this nav point
	// is being cleaned up twice
	if (!navPoint->IsValid()) 
		UE_LOG(LogTemp, Fatal, TEXT("navPoint is not valid"));

	navPoint->Destroy();

	USurfaceNavigationSystem::QueueNavPointToDelete(navPoint);
}

/** Pull a block off the pool */
TSharedPtr<FSurfaceNavPointBase> FSurfaceNavPointPool::Pop(
	const FIntVector& VectorIndex,
	const FVector& WorldLoc,
	const FVector& Normal)
{
	TSharedPtr<FSurfaceNavPointBase> navPoint = CreateInstance();
	navPoint->SetVectorIndex(VectorIndex);
	navPoint->WorldLocation = WorldLoc;
	navPoint->HitNormal = Normal;

	return navPoint;
}
