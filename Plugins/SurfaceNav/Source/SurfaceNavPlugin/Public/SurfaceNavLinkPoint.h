// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "SurfaceNavPointBase.h"
#include "SurfaceNavCommon.h"
#include "NavAreas/NavArea.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "Misc/ScopeRWLock.h"
#include "Containers/StaticArray.h"

/** Actor which traces for world objects with collisions */
struct SURFACENAVPLUGIN_API FSurfaceNavLinkPoint : FSurfaceNavPointBase
{
	typedef FSurfaceNavPointBase Super;

public:

public:
	// Sets default values for this actor's properties

	FSurfaceNavLinkPoint();

	virtual ~FSurfaceNavLinkPoint();

	// Determines the normal of the linke point based on first nearby point, not necessarily the closest point
	inline virtual const FVector GetNormal(bool bForceNoRecalc = false) override
	{ 
		FVector LinkPointNormal(0.f);
		int Count = 0;
		for (auto& LinkNeighbor : CustomLinkNeighbors)
		{
			if (auto PinnedLinkNeighbor = LinkNeighbor.Pin())
			{
				if (PinnedLinkNeighbor->IsValid())
				{
					LinkPointNormal += PinnedLinkNeighbor->GetNormal();
					Count++;
				}
			}
		}

		if (Count == 0)
		{
			if (HitNormal != FVector::ZeroVector)
				LinkPointNormal = FVector(HitNormal);
			else
				LinkPointNormal = FVector(0.f, 0.f, 1.f);
		}

		return LinkPointNormal.GetSafeNormal();
	}

	// This will only be used if this nav point is a point for a nav link
	int64 MyLinkId = 0;

	// This will be used for nav points that belong to nav links. This is
	// either point A or point B as a nav point.
	TSharedPtr<FSurfaceNavPointBase> OtherLinkPoint = nullptr;

	virtual void SetLinkId(int64 InLinkId) override { MyLinkId = InLinkId; }
	virtual int64 GetLinkId() const override { return MyLinkId; }
	
	virtual void SetOtherLinkPoint(TSharedPtr<FSurfaceNavPointBase> InOtherLinkPoint) override { OtherLinkPoint = InOtherLinkPoint; }
	virtual TSharedPtr<FSurfaceNavPointBase> GetOtherLinkPoint() const override { return OtherLinkPoint; }

	virtual void Reset();
	virtual void Init();	
};

struct SURFACENAVPLUGIN_API FSurfaceNavLinkData
{
	FSurfaceNavLinkData(FVector& InWorldPtA, FVector& InWorldPtB, ENavLinkDirection::Type InLinkDir, float InRadius, UNavArea* NavArea, bool InIsRelevant = true, uint32 InParentLinkId = 0)
		: ParentLinkId(InParentLinkId), WorldPtA(InWorldPtA), WorldPtB(InWorldPtB), LinkDir(InLinkDir), Radius(InRadius), RadiusSqrd(InRadius* InRadius), bIsRelevant(InIsRelevant)
	{
		LinkNavPointA = TSharedPtr<FSurfaceNavPointBase>(new FSurfaceNavLinkPoint());
		LinkNavPointB = TSharedPtr<FSurfaceNavPointBase>(new FSurfaceNavLinkPoint());

		LinkNavPointA->NavArea = NavArea;
		LinkNavPointB->NavArea = NavArea;

		LinkNavPointA->WorldLocation = WorldPtA;
		LinkNavPointB->WorldLocation = WorldPtB;

		LinkNavPointA->SetOtherLinkPoint(InLinkDir == ENavLinkDirection::BothWays || InLinkDir == ENavLinkDirection::LeftToRight ? LinkNavPointB : nullptr);
		LinkNavPointB->SetOtherLinkPoint(InLinkDir == ENavLinkDirection::BothWays || InLinkDir == ENavLinkDirection::RightToLeft ? LinkNavPointA : nullptr);

		RefreshHash();

		LinkNavPointA->SetLinkId(Hash);
		LinkNavPointB->SetLinkId(Hash);
	}

	FSurfaceNavLinkData(int64 InLinkID, FVector InWorldPtA, FVector InWorldPtB, ENavLinkDirection::Type InLinkDir, float InRadius, UNavArea* NavArea, bool InIsRelevant = true, uint32 InParentLinkId = 0)
		: FSurfaceNavLinkData(InWorldPtA, InWorldPtB, InLinkDir, InRadius, NavArea, InIsRelevant, InParentLinkId)
	{
		LinkID = InLinkID;

		LinkNavPointA->SetLinkId(InLinkID);
		LinkNavPointB->SetLinkId(InLinkID);
	}

	void Destroy();

	void Update(FVector InWorldPtA, FVector InWorldPtB, ENavLinkDirection::Type InLinkDir, float InRadius, UNavArea* NavArea, bool InIsRelevant = true)
	{
		WorldPtA = InWorldPtA;
		WorldPtB = InWorldPtB;
		LinkDir = InLinkDir;
		Radius = InRadius;
		RadiusSqrd = InRadius * InRadius;
		bIsRelevant = InIsRelevant;

		LinkNavPointA->NavArea = NavArea;
		LinkNavPointB->NavArea = NavArea;

		LinkNavPointA->WorldLocation = WorldPtA;
		LinkNavPointB->WorldLocation = WorldPtB;

		LinkNavPointA->SetOtherLinkPoint(InLinkDir == ENavLinkDirection::BothWays || InLinkDir == ENavLinkDirection::LeftToRight ? LinkNavPointB : nullptr);
		LinkNavPointB->SetOtherLinkPoint(InLinkDir == ENavLinkDirection::BothWays || InLinkDir == ENavLinkDirection::RightToLeft ? LinkNavPointA : nullptr);

		RefreshHash();
	}

	void RefreshHash()
	{
		Hash = HashCombine(GetTypeHash(ParentLinkId),
			HashCombine(GetTypeHash(WorldPtA),
				HashCombine(GetTypeHash(WorldPtB),
					HashCombine(GetTypeHash(LinkDir),
						HashCombine(GetTypeHash(LinkNavPointA->NavArea),
							HashCombine(GetTypeHash(bIsRelevant),
								GetTypeHash(Radius)))))));
	}

	uint32 GetHash() { return Hash; }

	// This hold the link ID for custom links. If this is a simple
	// link, then this will hold a negative of this object's hash
	// to be used as the ID.
	int64 LinkID = 0;

	// Holds parent link ID for simple links. This will be zero
	// if this is parent/custom link.
	uint64 ParentLinkId;

	FVector WorldPtA;
	FVector WorldPtB;
	ENavLinkDirection::Type LinkDir;
	float Radius;
	float RadiusSqrd;
	bool bIsRelevant;

	uint32 Hash;

	TSharedPtr<FSurfaceNavPointBase> LinkNavPointA = nullptr;
	TSharedPtr<FSurfaceNavPointBase> LinkNavPointB = nullptr;
};
