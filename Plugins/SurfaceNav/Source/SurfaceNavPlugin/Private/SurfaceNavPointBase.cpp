// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavPointBase.h"
#include "SurfaceNavigationSystem.h"
#include "SurfaceNavGridPoint.h"
#include "Kismet/KismetMathLibrary.h"

FSurfaceNavPointBase::FSurfaceNavPointBase()
{
	Init();
}

FSurfaceNavPointBase::~FSurfaceNavPointBase()
{
}

inline void FSurfaceNavPointBase::Init()
{
	bIsDestroying = 0;
	bIsDestroyed = 0;
	CalcAngleFromGroundCached = 0;
	CalcNormalCached = 0;

	ExpenseModifier = 0.f;
}

inline void FSurfaceNavPointBase::ResetLinkNeighbor()
{
	// Let's iterate over the custom link neighbors so we can remove them
	// Start from the end and work to the start so we can remove them without
	// causing looping issues.
	for (int i = CustomLinkNeighbors.Num() - 1; i >= 0; i--)
	{
		if (TSharedPtr<FSurfaceNavPointBase> LinkNeighbor = CustomLinkNeighbors[i].Pin())
		{
			// If our link neighbor is valid then remove this instance from the link
			// neighbor's collection
			if (LinkNeighbor->IsValid())
			{
				for (int j = LinkNeighbor->CustomLinkNeighbors.Num() - 1; j >= 0; j--)
				{
					auto& LinkNeighborHeighbor = LinkNeighbor->CustomLinkNeighbors[j];
				
					if (auto PinnedLinkNeighborHeighbor = LinkNeighborHeighbor.Pin())
					{
						if (PinnedLinkNeighborHeighbor.Get() == this)
						{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
							LinkNeighbor->CustomLinkNeighbors.RemoveAtSwap(j, 1, false);
#else
							LinkNeighbor->CustomLinkNeighbors.RemoveAtSwap(j, 1, EAllowShrinking::No);
#endif
							break;
						}
					}
				}
			}

			// Now remove the link neighbor from this instance's collection
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
			CustomLinkNeighbors.RemoveAtSwap(i, 1, false);
#else
			CustomLinkNeighbors.RemoveAtSwap(i, 1, EAllowShrinking::No);
#endif
		}
	}

	// Make sure these are empty now
	CustomLinkNeighbors.Empty();
}

inline void FSurfaceNavPointBase::Reset()
{
	ResetLinkNeighbor();

	CalcAngleFromGroundCached = false;
	CalcNormalCached = false;

	ExpenseModifier = 0.f;
}

void FSurfaceNavPointBase::Destroy()
{
	bIsDestroying = true;

	Reset();
	bIsDestroyed = true;
}
