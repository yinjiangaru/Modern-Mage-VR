// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavLinkPoint.h"
#include "SurfaceNavigationSystem.h"
#include "SurfaceNavGridPoint.h"
#include "Kismet/KismetMathLibrary.h"

FSurfaceNavLinkPoint::FSurfaceNavLinkPoint()
{
	Init();
}

FSurfaceNavLinkPoint::~FSurfaceNavLinkPoint()
{
}

inline void FSurfaceNavLinkPoint::Init()
{
	Super::Init();

	bIsDestroying = 0;
	bIsDestroyed = 0;
	CalcAngleFromGroundCached = 0;
	CalcNormalCached = 0;

	ExpenseModifier = 0.f;

	OtherLinkPoint = nullptr;
}

inline void FSurfaceNavLinkPoint::Reset()
{
	Super::Reset();

	OtherLinkPoint = nullptr;
}

void FSurfaceNavLinkData::Destroy()
{
	ensure(LinkNavPointA.GetSharedReferenceCount() > 0);
	ensure(LinkNavPointB.GetSharedReferenceCount() > 0);

	ensure(LinkNavPointA.IsValid());
	ensure(LinkNavPointB.IsValid());

	// Not sure why, but destroying these points as shared pointers will sometimes cause
	// a crash. It seems to be fine when using the shared reference.
	auto SharedRefLinkNavPointA = LinkNavPointA.ToSharedRef();
	auto SharedRefLinkNavPointB = LinkNavPointB.ToSharedRef();

	// Now call destroy on the shared refs
	SharedRefLinkNavPointA->Destroy();
	SharedRefLinkNavPointB->Destroy();

	USurfaceNavigationSystem::QueueNavPointToDelete(LinkNavPointA);
	USurfaceNavigationSystem::QueueNavPointToDelete(LinkNavPointB);
}