// Copyright BMK Games LLC. All Rights Reserved.

#include "EQS/Generators/EnvQueryGenerator_DSN_ProjectedPoints.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

#include "NavigationSystem.h"
#include "SurfaceNavPoint.h"
#include "SurfaceNavGridPoint.h"

//#include "EnvironmentQuery/EnvQueryTraceHelpers.h"

UEnvQueryGenerator_DSN_ProjectedPoints::UEnvQueryGenerator_DSN_ProjectedPoints(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ProjectionData.TraceMode = ESurfaceNavEnvQueryTrace::Navigation;

	ItemType = UEnvQueryItemType_Point::StaticClass();
}

FVector UEnvQueryGenerator_DSN_ProjectedPoints::DetermineProjectedLocation(FSurfaceNavGridPoint*& GridPoint) const
{
	FVector ProjectedLocation;

	if (ProjectionData.TraceMode == ESurfaceNavEnvQueryTrace::Navigation)
		ProjectedLocation = GridPoint->GetAvgLocation();
	else if (ProjectionData.TraceMode == ESurfaceNavEnvQueryTrace::ClosestSurface)
	{
		FVector ClosestLoc(INFINITY);
		float ClosestDistSq = INFINITY;
		FVector CompareTo = GridPoint->WorldLocation;

		for (TPair<FIntVector, TSharedPtr<FSurfaceNavPointBase>>& NavPointPair : GridPoint->NavPoints)
		{
			TSharedPtr<FSurfaceNavPointBase>& NavPoint = NavPointPair.Value;
			if (!NavPoint || !NavPoint->IsValid())
				continue;

			FVector CurLoc = NavPoint->WorldLocation;
			float CurDistSq = FVector::DistSquared(CurLoc, CompareTo);
			if (CurDistSq < ClosestDistSq)
			{
				ClosestDistSq = CurDistSq;
				ClosestLoc = CurLoc;
			}
		}
		if (ClosestDistSq == INFINITY)
			ClosestLoc = CompareTo;

		ProjectedLocation = FNavLocation(ClosestLoc);
	}
	else if (ProjectionData.TraceMode == ESurfaceNavEnvQueryTrace::CheapestSurface)
	{
		FVector CheapestLoc(INFINITY);
		float CheapestCost = INFINITY;

		//UNavArea* DefaultNavArea = FNavigationSystem::GetDefaultWalkableArea().GetDefaultObject()->StaticClass();
		const UNavArea* DefaultAreaOb = static_cast<const UNavArea*>(FNavigationSystem::GetDefaultWalkableArea().GetDefaultObject());

		if (GridPoint->NavPoints.Num() == 1)
		{
			TArray<FIntVector> NavPointKeys;
			GridPoint->NavPoints.GetKeys(NavPointKeys);
			TSharedPtr<FSurfaceNavPointBase>& NavPoint = GridPoint->NavPoints[NavPointKeys[0]];
			if (NavPoint && NavPoint->IsValid())
				CheapestLoc = GridPoint->NavPoints[NavPointKeys[0]]->WorldLocation;
		}
		else
		{
			for (TPair<FIntVector, TSharedPtr<FSurfaceNavPointBase>>& NavPointPair : GridPoint->NavPoints)
			{
				TSharedPtr<FSurfaceNavPointBase>& NavPoint = NavPointPair.Value;
				if (!NavPoint || !NavPoint->IsValid())
					continue;

				FVector CurLoc = NavPoint->WorldLocation;
				float CurCost = INFINITY;

				// This should already be the default object
				UNavArea* CurNavArea = NavPoint->NavArea;

				if (CurNavArea)
				{
					CurCost = CurNavArea->DefaultCost;

					if (ProjectionData.NavigationFilter)
					{
						for (auto ProjectionArea : ProjectionData.NavigationFilter.GetDefaultObject()->Areas)
						{
							if (ProjectionArea.AreaClass.GetDefaultObject() == CurNavArea)
							{
								CurCost = ProjectionArea.TravelCostOverride;
								break;
							}
						}
					}
				}
				else
				{
					CurCost = DefaultAreaOb->DefaultCost;
				}

				if (CurCost < CheapestCost)
				{
					CheapestCost = CurCost;
					CheapestLoc = CurLoc;
				}
			}
		}

		ProjectedLocation = FNavLocation(CheapestLoc);
	}
	else
		ProjectedLocation = GridPoint->WorldLocation;

	if (ProjectionData.OffsetAlongNavNormal != 0.f)
		ProjectedLocation += GridPoint->GetAvgNormal() * ProjectionData.OffsetAlongNavNormal;

	return ProjectedLocation;
}

void UEnvQueryGenerator_DSN_ProjectedPoints::StoreNavPoints(const TArray<FNavLocation>& Points, FEnvQueryInstance& QueryInstance) const
{
	const int32 InitialElementsCount = QueryInstance.Items.Num();
	QueryInstance.ReserveItemData(InitialElementsCount + Points.Num());
	for (int32 Idx = 0; Idx < Points.Num(); Idx++)
	{
		// store using default function to handle creating new data entry 
		QueryInstance.AddItemData<UEnvQueryItemType_Point>(Points[Idx]);
	}

	FEnvQueryOptionInstance& OptionInstance = QueryInstance.Options[QueryInstance.OptionIndex];
	OptionInstance.bHasNavLocations = (ProjectionData.TraceMode == ESurfaceNavEnvQueryTrace::Navigation);
}

void UEnvQueryGenerator_DSN_ProjectedPoints::PostLoad()
{
	Super::PostLoad();
	ProjectionData.OnPostLoad();
}
