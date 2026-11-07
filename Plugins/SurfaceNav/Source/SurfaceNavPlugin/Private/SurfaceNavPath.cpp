// Copyright BMK Games LLC. All Rights Reserved.


#include "SurfaceNavPath.h"
#include "SurfaceNavCommon.h"
#include "SurfaceNavigationSystem.h"
#include "SurfaceNavMovementComponent.h"

#include "GameFramework/Actor.h"

//#include "SurfaceNavMovementComponent.h";

//FSurfaceNavPath

//int SurfaceNavigation::MaxRandomRepathOffset;

//----------------------------------------------------------------------//
// FSurfaceNavPath
//----------------------------------------------------------------------//
const FNavPathType FSurfaceNavPath::Type;

FSurfaceNavPath::FSurfaceNavPath()
	: GoalActorAsNavAgent(nullptr)
	, SourceActorAsNavAgent(nullptr)
	, PathType(FSurfaceNavPath::Type)
	, bDoAutoUpdateOnInvalidation(true)
	, bIgnoreInvalidation(false)
	, bUpdateStartPointOnRepath(true)
	, bUpdateEndPointOnRepath(true)
	, bWaitingForRepath(false)
	, bUseOnPathUpdatedNotify(false)
	, LastUpdateTimeStamp(-1.f)	// indicates that it has not been set
	, GoalActorLocationTetherDistanceSq(-1.f)
	, GoalActorLastLocation(FVector::ZeroVector)
{
	InternalResetNavigationPath();
}

FSurfaceNavPath::FSurfaceNavPath(const TArray<FVector>& Points, const TArray<float>& Expenses, const TArray<FVector>& Normals, TArray<int64>& CustomLinkIDs, AActor* InBase)
	: GoalActorAsNavAgent(nullptr)
	, SourceActorAsNavAgent(nullptr)
	, PathType(FSurfaceNavPath::Type)
	, bDoAutoUpdateOnInvalidation(true)
	, bIgnoreInvalidation(false)
	, bUpdateStartPointOnRepath(true)
	, bUpdateEndPointOnRepath(true)
	, bWaitingForRepath(false)
	, bUseOnPathUpdatedNotify(false)
	, LastUpdateTimeStamp(-1.f)	// indicates that it has not been set
	, GoalActorLocationTetherDistanceSq(-1.f)
	, GoalActorLastLocation(FVector::ZeroVector)
{
	InternalResetNavigationPath();
	//MarkReady();

	Base = InBase;

	UpdatePathData(Points, Expenses, Normals, CustomLinkIDs);

	/*PathPoints.AddZeroed(Points.Num());
	PointNormals.AddZeroed(Normals.Num());
	for (int32 i = 0; i < Points.Num(); i++)
	{
		FBasedPosition BasedPoint(InBase, Points[i]);
		PathPoints[i] = FNavPathPoint(*BasedPoint);
		PointNormals[i] = Normals[i];
	}

	PathPointsBox = FBox(Points);*/
}

void FSurfaceNavPath::InternalResetNavigationPath()
{
	PathPoints.Reset();
	PointNormals.Reset();
	PointExpenses.Reset();
	Base.Reset();

	bUpToDate = true;
	bIsReady = false;
	bIsPartial = false;
	bReachedSearchLimit = false;
	bObservingGoalActor = GoalActor.IsValid();
}

FVector FSurfaceNavPath::GetGoalLocation() const
{
	if (GoalActor.IsValid())
	{
		USurfaceNavMovementComponent* MoveComp = GoalActor->FindComponentByClass<USurfaceNavMovementComponent>();
		if (MoveComp)
			return MoveComp->GetActorSurfaceLocation();

		return GoalActorAsNavAgent != NULL ? GoalActorAsNavAgent->GetNavAgentLocation() : GoalActor->GetActorLocation();
	}

	return GetEndLocation();
}

void FSurfaceNavPath::GetGoalLocations(FVector& Loc, FVector& OffsetLoc) const
{
	if (GoalActor.IsValid())
	{
		USurfaceNavMovementComponent* MoveComp = GoalActor->FindComponentByClass<USurfaceNavMovementComponent>();
		if (MoveComp)
		{
			Loc = GoalActorAsNavAgent != NULL ? GoalActorAsNavAgent->GetNavAgentLocation() : GoalActor->GetActorLocation();
			OffsetLoc = MoveComp->GetActorSurfaceLocation();
			return;
		}

		Loc = OffsetLoc = GoalActorAsNavAgent != NULL ? GoalActorAsNavAgent->GetNavAgentLocation() : GoalActor->GetActorLocation();
		return;
	}

	Loc = OffsetLoc = GetEndLocation();
}

void FSurfaceNavPath::GetPathFindingStartLocations(FVector& Loc, FVector& OffsetLoc) const
{
	if (SourceActor.IsValid())
	{
		USurfaceNavMovementComponent* MoveComp = SourceActor->FindComponentByClass<USurfaceNavMovementComponent>();
		if (MoveComp)
		{
			Loc = SourceActor->GetActorLocation();
			OffsetLoc = MoveComp->GetActorSurfaceLocation();
			return;
		}

		Loc = OffsetLoc = SourceActor->GetActorLocation();
		return;
	}

	Loc = OffsetLoc = GetStartLocation();
}

void FSurfaceNavPath::SetGoalActorObservation(const AActor& ActorToObserve, float TetherDistance)
{
	//if (NavigationDataUsed.IsValid() == false)
	//{
	//	// this mechanism is available only for navigation-generated paths
	//	UE_LOG(LogNavigation, Warning, TEXT("Updating navigation path on goal actor's location change is available only for navigation-generated paths. Called for %s")
	//		, *GetNameSafe(&ActorToObserve));
	//	return;
	//}

	// register for path observing only if we weren't registered already
	const bool RegisterForPathUpdates = (GoalActor.IsValid() == false);
	GoalActor = &ActorToObserve;
	checkSlow(GoalActor.IsValid());
	GoalActorAsNavAgent = Cast<INavAgentInterface>(&ActorToObserve);
	GoalActorLocationTetherDistanceSq = FMath::Square(TetherDistance);
	bObservingGoalActor = true;
	UpdateLastRepathGoalLocation();

	if (RegisterForPathUpdates)
	{
		/*ASurfaceNavGridBuilder* GridBuilder = ASurfaceNavGridBuilder::GetCurrent();
		if (GridBuilder)
			GridBuilder->RegisterObservedPath(AsShared());*/

		USurfaceNavigationSystem* const& CurNavSys = USurfaceNavigationSystem::CurrentSurfaceNavSystem.Get();
		if (CurNavSys)
			CurNavSys->RegisterObservedPath(AsShared());
	}
}

void FSurfaceNavPath::SetSourceActor(const AActor& InSourceActor)
{
	SourceActor = &InSourceActor;
	SourceActorAsNavAgent = Cast<INavAgentInterface>(&InSourceActor);
}

void FSurfaceNavPath::UpdateLastRepathGoalLocation()
{
	if (GoalActor.IsValid())
	{
		USurfaceNavMovementComponent* MoveComp = GoalActor->FindComponentByClass<USurfaceNavMovementComponent>();
		if (MoveComp)
			GoalActorLastLocation = MoveComp->GetActorSurfaceLocation();
		else
			GoalActorLastLocation = GoalActorAsNavAgent ? GoalActorAsNavAgent->GetNavAgentLocation() : GoalActor->GetActorLocation();

		//GoalActorLastLocation = GoalActorAsNavAgent ? GoalActorAsNavAgent->GetNavAgentLocation() : GoalActor->GetActorLocation();
	}
}

EPathObservationResult::Type FSurfaceNavPath::TickPathObservation()
{
	if (bObservingGoalActor == false || GoalActor.IsValid() == false)
	{
		return EPathObservationResult::NoLongerObserving;
	}

	FVector GoalLocation = FVector::ZeroVector;
	
	if (GoalActor.IsValid())
	{
		USurfaceNavMovementComponent* MoveComp = GoalActor->FindComponentByClass<USurfaceNavMovementComponent>();
		if (MoveComp)
			GoalLocation = MoveComp->GetActorSurfaceLocation();
		else
			GoalLocation = GoalActorAsNavAgent != NULL ? GoalActorAsNavAgent->GetNavAgentLocation() : GoalActor->GetActorLocation();
	}
	else
		GoalLocation = GoalActor->GetActorLocation();

	//return FVector::DistSquared(GoalLocation, GoalActorLastLocation) <= GoalActorLocationTetherDistanceSq || !bCanChange ? EPathObservationResult::NoChange : EPathObservationResult::RequestRepath;
	return FVector::DistSquared(GoalLocation, GoalActorLastLocation) <= GoalActorLocationTetherDistanceSq ? EPathObservationResult::NoChange : EPathObservationResult::RequestRepath;
}

void FSurfaceNavPath::DisableGoalActorObservation()
{
	GoalActor = NULL;
	GoalActorAsNavAgent = NULL;
	GoalActorLocationTetherDistanceSq = -1.f;
	bObservingGoalActor = false;
}

void FSurfaceNavPath::Invalidate()
{
	if (!bIgnoreInvalidation)
	{
		bUpToDate = false;
		ObserverDelegate.Broadcast(this, ENavPathEvent::Invalidated);
		//ASurfaceNavGridBuilder* GridBuilder = ASurfaceNavGridBuilder::GetCurrent();
		USurfaceNavigationSystem* const& CurNavSys = USurfaceNavigationSystem::CurrentSurfaceNavSystem.Get();
		if (bDoAutoUpdateOnInvalidation && CurNavSys) // && NavigationDataUsed.IsValid())
		{
			bWaitingForRepath = true;
			//GridBuilder->RequestRePath(AsShared(), ENavPathUpdateType::NavigationChanged);
			CurNavSys->RequestRePath(AsShared(), ENavPathUpdateType::NavigationChanged);

			//NavigationDataUsed->RequestRePath(AsShared(), ENavPathUpdateType::NavigationChanged);
		}
	}
}

void FSurfaceNavPath::RePathFailed()
{
	ObserverDelegate.Broadcast(this, ENavPathEvent::RePathFailed);
	bWaitingForRepath = false;
}

void FSurfaceNavPath::ResetForRepath()
{
	InternalResetNavigationPath();
}

void FSurfaceNavPath::DebugDraw(const ANavigationData* NavData, FColor PathColor, UCanvas* Canvas, bool bPersistent, float LifeTime, const uint32 NextPathPointIndex) const
{
#if ENABLE_DRAW_DEBUG

	static const FColor Grey(100, 100, 100);
	const int32 NumPathVerts = PathPoints.Num();

	UWorld* World = NavData->GetWorld();

	for (int32 VertIdx = 0; VertIdx < NumPathVerts - 1; ++VertIdx)
	{
		// draw box at vert
		FVector const VertLoc = PathPoints[VertIdx].Location + NavigationDebugDrawing::PathOffset;
		DrawDebugSolidBox(World, VertLoc, NavigationDebugDrawing::PathNodeBoxExtent, VertIdx < int32(NextPathPointIndex) ? Grey : PathColor, bPersistent, LifeTime);

		// draw line to next loc
		FVector const NextVertLoc = PathPoints[VertIdx + 1].Location + NavigationDebugDrawing::PathOffset;
		DrawDebugLine(World, VertLoc, NextVertLoc, VertIdx < int32(NextPathPointIndex) - 1 ? Grey : PathColor, bPersistent
			, LifeTime, /*DepthPriority*/0
			, /*Thickness*/NavigationDebugDrawing::PathLineThickness);
	}

	// draw last vert
	if (NumPathVerts > 0)
	{
		DrawDebugBox(World, PathPoints[NumPathVerts - 1].Location + NavigationDebugDrawing::PathOffset, FVector(15.f), PathColor, bPersistent, LifeTime);
	}

	// if observing goal actor draw a radius and a line to the goal
	if (GoalActor.IsValid())
	{
		const FVector GoalLocation = GetGoalLocation() + NavigationDebugDrawing::PathOffset;
		const FVector EndLocation = GetEndLocation() + NavigationDebugDrawing::PathOffset;
		static const FVector CylinderHalfHeight = FVector::UpVector * 10.f;
		DrawDebugCylinder(World, EndLocation - CylinderHalfHeight, EndLocation + CylinderHalfHeight, FMath::Sqrt(GoalActorLocationTetherDistanceSq), 16, PathColor, bPersistent, LifeTime);
		DrawDebugLine(World, EndLocation, GoalLocation, Grey, bPersistent, LifeTime);
	}

#endif
}

//bool FSurfaceNavPath::ContainsNode(NavNodeRef NodeRef) const
//{
//	for (int32 Index = 0; Index < PathPoints.Num(); Index++)
//	{
//		if (PathPoints[Index].NodeRef == NodeRef)
//		{
//			return true;
//		}
//	}
//
//	return ShortcutNodeRefs.Find(NodeRef) != INDEX_NONE;
//}

float FSurfaceNavPath::GetCostFromIndex(int32 PathPointIndex) const
{
	if (!IsValid())
		return INFINITY;

	const uint32 StartIndex = PathPointIndex + 1;
	if (StartIndex >= (uint32)PathPoints.Num())
		return 0;

	const uint32 PathPointsCount = PathPoints.Num();

	float PathCost = 0.f;

	for (uint32 PathIndex = StartIndex; PathIndex < PathPointsCount; ++PathIndex)
	{
		int PointExpense = PointExpenses[PathIndex];
		PathCost += PointExpense;
	}

	return PathCost;
}

float FSurfaceNavPath::GetLengthFromPosition(FVector SegmentStart, uint32 NextPathPointIndex) const
{
	if (NextPathPointIndex >= (uint32)PathPoints.Num())
		return 0;

	const uint32 PathPointsCount = PathPoints.Num();

	float PathDistance = 0.f;

	for (uint32 PathIndex = NextPathPointIndex; PathIndex < PathPointsCount; ++PathIndex)
	{
		const FVector SegmentEnd = PathPoints[PathIndex].Location;
		PathDistance += FVector::Dist(SegmentStart, SegmentEnd);
		SegmentStart = SegmentEnd;
	}

	return PathDistance; // UE_REAL_TO_FLOAT_CLAMPED_MAX(PathDistance);
}

bool FSurfaceNavPath::ContainsCustomLink(uint32 LinkUniqueId) const
{
	for (int32 i = 0; i < PathPoints.Num(); i++)
	{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
		if (PathPoints[i].CustomLinkId == LinkUniqueId && LinkUniqueId)
#else
		if (PathPoints[i].CustomNavLinkId.GetId() == LinkUniqueId && LinkUniqueId)
#endif
		{
			return true;
		}
	}

	return false;
}

bool FSurfaceNavPath::ContainsAnyCustomLink() const
{
	for (int32 i = 0; i < PathPoints.Num(); i++)
	{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
		if (PathPoints[i].CustomLinkId)
#else
		if (PathPoints[i].CustomNavLinkId.GetId())
#endif
		{
			return true;
		}
	}

	return false;
}

FORCEINLINE bool FSurfaceNavPath::DoesPathIntersectBoxImplementation(const FBox& Box, const FVector& StartLocation, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const
{
	bool bIntersects = false;

	FVector Start = StartLocation;
	for (int32 PathPointIndex = int32(StartingIndex); PathPointIndex < PathPoints.Num(); ++PathPointIndex)
	{
		const FVector End = PathPoints[PathPointIndex].Location;
		if (FVector::DistSquared(Start, End) > SMALL_NUMBER)
		{
			const FVector Direction = (End - Start);

			FVector HitLocation, HitNormal;
			float HitTime;

			// If we have a valid AgentExtent, then we use an extent box to represent the path
			// Otherwise we use a line to represent the path
			if ((AgentExtent && FMath::LineExtentBoxIntersection(Box, Start, End, *AgentExtent, HitLocation, HitNormal, HitTime)) ||
				(!AgentExtent && FMath::LineBoxIntersection(Box, Start, End, Direction)))
			{
				bIntersects = true;
				if (IntersectingSegmentIndex != NULL)
				{
					*IntersectingSegmentIndex = PathPointIndex;
				}
				break;
			}
		}

		Start = End;
	}

	return bIntersects;
}

bool FSurfaceNavPath::DoesIntersectBox(const FBox& Box, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const
{
	// iterate over all segments and check if any intersects with given box
	bool bIntersects = false;
	int32 PathPointIndex = INDEX_NONE;

	if (PathPoints.Num() > 1 && PathPoints.IsValidIndex(int32(StartingIndex)))
	{
		bIntersects = DoesPathIntersectBoxImplementation(Box, PathPoints[StartingIndex].Location, StartingIndex + 1, IntersectingSegmentIndex, AgentExtent);
	}

	return bIntersects;
}

bool FSurfaceNavPath::DoesIntersectBox(const FBox& Box, const FVector& AgentLocation, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const
{
	// iterate over all segments and check if any intersects with given box
	bool bIntersects = false;
	int32 PathPointIndex = INDEX_NONE;

	if (PathPoints.Num() > 1 && PathPoints.IsValidIndex(int32(StartingIndex)))
	{
		bIntersects = DoesPathIntersectBoxImplementation(Box, AgentLocation, StartingIndex, IntersectingSegmentIndex, AgentExtent);
	}

	return bIntersects;
}

FVector FSurfaceNavPath::GetSegmentDirection(uint32 SegmentEndIndex) const
{
	FVector Result = FNavigationSystem::InvalidLocation;

	// require at least two points
	if (PathPoints.Num() > 1)
	{
		if (PathPoints.IsValidIndex(SegmentEndIndex))
		{
			if (SegmentEndIndex > 0)
			{
				Result = (PathPoints[SegmentEndIndex].Location - PathPoints[SegmentEndIndex - 1].Location).GetSafeNormal();
			}
			else
			{
				// for '0'-th segment returns same as for 1st segment 
				Result = (PathPoints[1].Location - PathPoints[0].Location).GetSafeNormal();
			}
		}
		else if (SegmentEndIndex >= uint32(GetPathPoints().Num()))
		{
			// in this special case return direction of last segment
			Result = (PathPoints[PathPoints.Num() - 1].Location - PathPoints[PathPoints.Num() - 2].Location).GetSafeNormal();
		}
	}

	return Result;
}

FBasedPosition FSurfaceNavPath::GetPathPointLocation(uint32 Index) const
{
	FBasedPosition BasedPt;
	if (PathPoints.IsValidIndex(Index))
	{
		BasedPt.Base = Base.Get();
		BasedPt.Position = PathPoints[Index].Location;
	}

	return BasedPt;
}

void FSurfaceNavPath::GetRawPathData(bool& Success, TArray<FSurfaceNavPathPointData>& PointData) const
{
	if (PathPoints.Num() == 0)
		return;

	check(PathPoints.Num() == PointExpenses.Num() && PathPoints.Num() == PointNormals.Num());

	PointData.Reserve(PathPoints.Num());

	for (int i = 0; i < PathPoints.Num(); i++)
	{
		PointData.Emplace(PathPoints[i].Location, PointNormals[i], PointExpenses[i]);
	}

	Success = true;
}

void FSurfaceNavPath::GetRawPathPointData(int Index, bool& Success, FSurfaceNavPathPointData& PointData) const
{
	if (PathPoints.Num() < Index + 1)
		return;

	check(PathPoints.Num() == PointExpenses.Num() && PathPoints.Num() == PointNormals.Num());

	PointData.Location = PathPoints[Index].Location;
	PointData.Normal = PointNormals[Index];
	PointData.Expense = PointExpenses[Index];

	Success = true;
}

#if ENABLE_VISUAL_LOG

void FSurfaceNavPath::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	/*if (Snapshot == nullptr)
	{
		return;
	}

	const int32 NumPathVerts = PathPoints.Num();
	FVisualLogShapeElement Element(EVisualLoggerShapeElement::Path);
	Element.Category = LogNavigation.GetCategoryName();
	Element.SetColor(FColorList::Green);
	Element.Points.Reserve(NumPathVerts);
	Element.Thicknes = 3.f;

	for (int32 VertIdx = 0; VertIdx < NumPathVerts; ++VertIdx)
	{
		Element.Points.Add(PathPoints[VertIdx].Location + NavigationDebugDrawing::PathOffset);
	}

	Snapshot->ElementsToDraw.Add(Element);*/
}

FString FSurfaceNavPath::GetDescription() const
{
	return FString::Printf(TEXT("NotifyPathUpdate points:%d valid:%s")
		, PathPoints.Num()
		, IsValid() ? TEXT("yes") : TEXT("no"));
}




#endif // ENABLE_VISUAL_LOG