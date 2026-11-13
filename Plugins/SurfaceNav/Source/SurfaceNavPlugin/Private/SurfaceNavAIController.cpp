// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavAIController.h"
#include "SurfaceNavPathFollowingComponent.h"
#include "SurfaceNavigationSystem.h"
#include "SurfaceNavPath.h"

#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "NavigationData.h"
#include "Kismet/GameplayStatics.h"
#include "DisplayDebugHelpers.h"

//#include "Tasks/Task.h"
#include "Tasks/AITask.h"
#include "AITask_SurfaceNavMoveTo.h"
#include "GameplayTasksComponent.h"

struct FSurfaceNavPoint;

//DEFINE_LOG_CATEGORY(LogSurfaceNavAI);

//DEFINE_LOG_CATEGORY(LogAISurfaceNav);


ASurfaceNavAIController::ASurfaceNavAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USurfaceNavPathFollowingComponent>(TEXT("PathFollowingComponent")))
{

}

FVector ASurfaceNavAIController::GetRandomSurfaceNavPointNearby(float SearchRadius, const FVector TetherPoint, TSubclassOf<UNavigationQueryFilter> FilterClass) const
{
	// We don't know where we're going yet so just set to zero
	FVector Dest(0);

	FVector NewTetherPoint = TetherPoint.IsZero() ? GetPawn()->GetActorLocation() : TetherPoint;

	// Get a random location in the range of the search radius
	Dest = FVector(
		NewTetherPoint.X + FMath::FRandRange(-SearchRadius, SearchRadius),
		NewTetherPoint.Y + FMath::FRandRange(-SearchRadius, SearchRadius),
		NewTetherPoint.Z + FMath::FRandRange(-SearchRadius, SearchRadius));

	return Dest;
}

FVector ASurfaceNavAIController::GetRandomReachableSurfaceNavPointNearby(float SearchRadius, const FVector TetherPoint, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	// We don't know where we're going yet so just set to zero
	FVector Dest(0.f);

	// Just build a move request so we can pass to SN_BuildPathfindingQuery for now.
	// TODO: Create a dedicated function to build query without move request using
	// controller and pawn values.
	FAIMoveRequest MoveReq(Dest);
	MoveReq.SetUsePathfinding(true);
	MoveReq.SetAllowPartialPath(true);
	MoveReq.SetProjectGoalLocation(false);
	MoveReq.SetNavigationFilter(*FilterClass ? FilterClass : DefaultNavigationFilterClass);
	MoveReq.SetAcceptanceRadius(0.f);
	MoveReq.SetReachTestIncludesAgentRadius(false);
	MoveReq.SetCanStrafe(false);

	FSurfaceNavPathFindingQueryData PFQuery;
	const bool bValidQuery = SN_BuildPathfindingQuery(MoveReq, PFQuery);

	if (!bValidQuery)
		return Dest;

	// Get ref to nav system and make sure it's valid
	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());
	if (!NavSys)
		return Dest;

	// Make the query for testing only so it doesn't use up iterations
	PFQuery.bIsForTesting = true;

	// Let's get a basic random vector using our params
	FVector DestTemp = GetRandomSurfaceNavPointNearby(SearchRadius, TetherPoint, FilterClass);

	// Update the ned locations
	PFQuery.EndLocation = DestTemp;
	PFQuery.NonOffsetEndLocation = DestTemp;

	// Attempt to find a path to the random location
	FSurfaceNavPathResult PathResult = NavSys->FindPath(PFQuery);

	// If the path finding failed, no need to continue
	if (!PathResult.IsSuccessful())
		return Dest;

	// Get the last location of the path finding. 
	// This will be our random reachable location.
	Dest = PathResult.Path->GetPathPoints().Last().Location;

	return Dest;
}

float ASurfaceNavAIController::GetClosestPlanetActor(AActor*& PlanetActor)
{
	float result = MAX_flt;

	auto NavSystem = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	if (!NavSystem)
		return result;

	auto MyPawn = GetPawn();

	if (!MyPawn)
		return result;

	//AActor* PlanetActor = nullptr;
	result = NavSystem->FindClosestPlanet(GetPawn(), PlanetActor);

	return result;
}

EPathFollowingRequestResult::Type ASurfaceNavAIController::SurfaceNavMoveToActor(AActor* Goal, float AcceptanceRadius, bool bCanStrafe, TSubclassOf<UNavigationQueryFilter> FilterClass, bool bAllowPartialPaths)
{
	bool bStopOnOverlap = true;
	bool bUsePathfinding = true;

	// abort active movement to keep only one request running
	auto pathFollowingComponent = GetPathFollowingComponent();
	if (pathFollowingComponent && pathFollowingComponent->GetStatus() != EPathFollowingStatus::Idle)
	{
		pathFollowingComponent->AbortMove(*this, FPathFollowingResultFlags::ForcedScript | FPathFollowingResultFlags::NewRequest
			, FAIRequestID::CurrentRequest, EPathFollowingVelocityMode::Keep);
	}

	FAIMoveRequest MoveReq(Goal);
	MoveReq.SetUsePathfinding(bUsePathfinding);
	MoveReq.SetAllowPartialPath(bAllowPartialPaths);
	MoveReq.SetNavigationFilter(*FilterClass ? FilterClass : DefaultNavigationFilterClass);
	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetReachTestIncludesAgentRadius(bStopOnOverlap);
	MoveReq.SetCanStrafe(bCanStrafe);

	return SurfaceNavMoveTo(MoveReq);
}

EPathFollowingRequestResult::Type ASurfaceNavAIController::SurfaceNavMoveToLocation(const FVector& Dest, float AcceptanceRadius, bool bCanStrafe, TSubclassOf<UNavigationQueryFilter> FilterClass, bool bAllowPartialPaths)
{
	bool bStopOnOverlap = true;
	bool bUsePathfinding = true;
	bool bProjectDestinationToNavigation = false;

	// abort active movement to keep only one request running
	auto pathFollowingComponent = GetPathFollowingComponent();
	if (pathFollowingComponent && pathFollowingComponent->GetStatus() != EPathFollowingStatus::Idle)
	{
		pathFollowingComponent->AbortMove(*this, FPathFollowingResultFlags::ForcedScript | FPathFollowingResultFlags::NewRequest
			, FAIRequestID::CurrentRequest, EPathFollowingVelocityMode::Keep);
	}

	FAIMoveRequest MoveReq(Dest);
	MoveReq.SetUsePathfinding(bUsePathfinding);
	MoveReq.SetAllowPartialPath(bAllowPartialPaths);
	MoveReq.SetProjectGoalLocation(bProjectDestinationToNavigation);
	MoveReq.SetNavigationFilter(*FilterClass ? FilterClass : DefaultNavigationFilterClass);
	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetReachTestIncludesAgentRadius(bStopOnOverlap);
	MoveReq.SetCanStrafe(bCanStrafe);

	return SurfaceNavMoveTo(MoveReq);
}

EPathFollowingRequestResult::Type ASurfaceNavAIController::SurfaceNavMoveToRandomLocation(const float SearchRadius, const FVector TetherPoint, 
	float AcceptanceRadius, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	bool bStopOnOverlap = true;
	bool bUsePathfinding = true;
	bool bProjectDestinationToNavigation = false;

	// abort active movement to keep only one request running
	auto pathFollowingComponent = GetPathFollowingComponent();
	if (pathFollowingComponent && pathFollowingComponent->GetStatus() != EPathFollowingStatus::Idle)
	{
		pathFollowingComponent->AbortMove(*this, FPathFollowingResultFlags::ForcedScript | FPathFollowingResultFlags::NewRequest
			, FAIRequestID::CurrentRequest, EPathFollowingVelocityMode::Keep);
	}

	FVector GoalLocation = GetRandomSurfaceNavPointNearby(SearchRadius, TetherPoint, GetDefaultNavigationFilterClass());

	FAIMoveRequest MoveReq(GoalLocation);
	MoveReq.SetUsePathfinding(bUsePathfinding);
	MoveReq.SetAllowPartialPath(true);
	MoveReq.SetProjectGoalLocation(bProjectDestinationToNavigation);
	MoveReq.SetNavigationFilter(*FilterClass ? FilterClass : DefaultNavigationFilterClass);
	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetReachTestIncludesAgentRadius(bStopOnOverlap);
	MoveReq.SetCanStrafe(false);

	return SurfaceNavMoveTo(MoveReq);
}

FPathFollowingRequestResult ASurfaceNavAIController::SurfaceNavMoveTo(const FAIMoveRequest& MoveRequest, FSurfaceNavPathSharedPtr* OutPath)
{
	// both MoveToActor and MoveToLocation can be called from blueprints/script and should keep only single movement request at the same time.
	// this function is entry point of all movement mechanics - do NOT abort in here, since movement may be handled by AITasks, which support stacking 

	/*SCOPE_CYCLE_COUNTER(STAT_MoveTo);
	UE_VLOG(this, LogAINavigation, Log, TEXT("MoveTo: %s"), *MoveRequest.ToString());*/

	FPathFollowingRequestResult ResultData;
	ResultData.Code = EPathFollowingRequestResult::Failed;

	if (MoveRequest.IsValid() == false)
	{
		//UE_VLOG(this, LogSurfaceNavAI, Error, TEXT("MoveTo request failed due MoveRequest not being valid. Most probably desired Goal Actor not longer exists. MoveRequest: '%s'"), *MoveRequest.ToString());
		return ResultData;
	}

	auto pathFollowingComponent = GetPathFollowingComponent();
	auto SurfaceNavPathFollowingComponent = Cast<USurfaceNavPathFollowingComponent>(pathFollowingComponent);
	if (pathFollowingComponent == nullptr)
	{
		//	UE_VLOG(this, LogAINavigation, Error, TEXT("MoveTo request failed due missing PathFollowingComponent"));
		return ResultData;
	}

	ensure(MoveRequest.GetNavigationFilter() || !DefaultNavigationFilterClass);

	bool bCanRequestMove = true;
	bool bAlreadyAtGoal = false;

	if (!MoveRequest.IsMoveToActorRequest())
	{
		if (MoveRequest.GetGoalLocation().ContainsNaN() || FAISystem::IsValidLocation(MoveRequest.GetGoalLocation()) == false)
		{
			//UE_VLOG(this, LogAINavigation, Error, TEXT("AAIController::MoveTo: Destination is not valid! Goal(%s)"), TEXT_AI_LOCATION(MoveRequest.GetGoalLocation()));
			bCanRequestMove = false;
		}

		// fail if projection to navigation is required but it failed
		if (bCanRequestMove && MoveRequest.IsProjectingGoal())
		{
			USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());
			//const FNavAgentProperties& AgentProps = GetNavAgentPropertiesRef();
			TSharedPtr<FSurfaceNavPointBase> ProjectedLocation = nullptr;
			//FNavLocation ProjectedLocation;

			float MinAngleFromGround = 0.f;
			float MaxAngleFromGround = 180.f;
			float AcceptanceRadius = 100.f;

			auto MoveComp = GetPawn()->FindComponentByClass<USurfaceNavMovementComponent>();
			if (MoveComp)
			{
				MinAngleFromGround = MoveComp->MinAngleFromGround;
				MaxAngleFromGround = MoveComp->MaxAngleFromGround;
				AcceptanceRadius = MoveComp->GetPathPointAcceptanceRadius();
			}

			UNavigationQueryFilter* FilterClass = MoveRequest.GetNavigationFilter() ? MoveRequest.GetNavigationFilter().GetDefaultObject() : DefaultNavigationFilterClass ? DefaultNavigationFilterClass.GetDefaultObject() : nullptr;

			if (NavSys
				&& !NavSys->ProjectPointToNavigation(
					MoveRequest.GetGoalLocation(), GetPawn()->GetActorLocation(), false, ProjectedLocation, MinAngleFromGround, MaxAngleFromGround, AcceptanceRadius, FilterClass)
				)
			{
				if (MoveRequest.IsUsingPathfinding())
				{
					//UE_VLOG_LOCATION(this, LogAINavigation, Error, MoveRequest.GetGoalLocation(), 30.f, FColor::Red, TEXT("ASurfaceNavAIController::SurfaceNavMoveTo failed to project destination location to navmesh"));
				}
				else
				{
					//UE_VLOG_LOCATION(this, LogAINavigation, Error, MoveRequest.GetGoalLocation(), 30.f, FColor::Red, TEXT("ASurfaceNavAIController::SurfaceNavMoveTo failed to project destination location to navmesh, path finding is disabled perhaps disable goal projection ?"));
				}
				bCanRequestMove = false;
			}

			if (bCanRequestMove)
				MoveRequest.UpdateGoalLocation(ProjectedLocation->WorldLocation);
		}

		bAlreadyAtGoal = bCanRequestMove && pathFollowingComponent->HasReached(MoveRequest);
	}
	else
	{
		bAlreadyAtGoal = bCanRequestMove && pathFollowingComponent->HasReached(MoveRequest);
	}

	if (bAlreadyAtGoal)
	{
		//	UE_VLOG(this, LogAINavigation, Log, TEXT("MoveTo: already at goal!"));
		ResultData.MoveId = pathFollowingComponent->RequestMoveWithImmediateFinish(EPathFollowingResult::Success);
		ResultData.Code = EPathFollowingRequestResult::AlreadyAtGoal;
	}
	else if (bCanRequestMove)
	{
		FSurfaceNavPathFindingQueryData PFQuery;

		const bool bValidQuery = SN_BuildPathfindingQuery(MoveRequest, PFQuery);
		if (bValidQuery)
		{
			//PFQuery.SetNavigationFilter(MoveRequest.GetNavigationFilter() ? MoveRequest.GetNavigationFilter() : DefaultNavigationFilterClass);
			//PFQuery.SetHeightOffset(SurfaceNavPathFollowingComponent ? SurfaceNavPathFollowingComponent->GetMovementOffset() : 0.f);

			FSurfaceNavPathSharedPtr Path;
			SurfaceNavFindPathForMoveRequest(MoveRequest, PFQuery, Path);

			const FAIRequestID RequestID = Path.IsValid() ? SurfaceNavRequestMove(MoveRequest, Path) : FAIRequestID::InvalidRequest;
			if (RequestID.IsValid())
			{
				bAllowStrafe = MoveRequest.CanStrafe();
				ResultData.MoveId = RequestID;
				ResultData.Code = EPathFollowingRequestResult::RequestSuccessful;

				if (OutPath)
				{
					*OutPath = Path;
				}
			}
		}
	}

	if (ResultData.Code == EPathFollowingRequestResult::Failed)
	{
		ResultData.MoveId = pathFollowingComponent->RequestMoveWithImmediateFinish(EPathFollowingResult::Invalid);
	}

	return ResultData;
}

FAIRequestID ASurfaceNavAIController::SurfaceNavRequestMove(const FAIMoveRequest& MoveRequest, FSurfaceNavPathSharedPtr Path)
{
	uint32 RequestID = FAIRequestID::InvalidRequest;

	auto SNPathFollowingComponent = Cast<USurfaceNavPathFollowingComponent>(GetPathFollowingComponent());
	if (SNPathFollowingComponent)
	{
		RequestID = SNPathFollowingComponent->SurfaceNavRequestMove(MoveRequest, Path);
	}

	return RequestID;
}

void ASurfaceNavAIController::SurfaceNavFindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FSurfaceNavPathFindingQueryData& Query, FSurfaceNavPathSharedPtr& OutPath) const
{
	//SCOPE_CYCLE_COUNTER(STAT_AI_Overall);

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());
	if (NavSys)
	{
		FPathFindingResult PathResultTest;
		FSurfaceNavPathResult PathResult = NavSys->FindSurfacePathSync(Query);
		if (PathResult.Result != ENavigationQueryResult::Error)
		{
			if (PathResult.IsSuccessful() && PathResult.Path.IsValid())
				//if (PathResult.IsSuccessful() && !PathResult.NavPoints.IsEmpty())
			{
				if (MoveRequest.IsMoveToActorRequest())
				{
					PathResult.Path->SetGoalActorObservation(*MoveRequest.GetGoalActor(), 100.0f);
				}

				PathResult.Path->EnableRecalculationOnInvalidation(true);

				OutPath = PathResult.Path;
			}
		}
		else
		{
			/*UE_VLOG(this, LogAINavigation, Error, TEXT("Trying to find path to %s resulted in Error")
				, MoveRequest.IsMoveToActorRequest() ? *GetNameSafe(MoveRequest.GetGoalActor()) : *MoveRequest.GetGoalLocation().ToString());
			UE_VLOG_SEGMENT(this, LogAINavigation, Error, GetPawn() ? GetPawn()->GetActorLocation() : FAISystem::InvalidLocation
				, MoveRequest.GetGoalLocation(), FColor::Red, TEXT("Failed move to %s"), *GetNameSafe(MoveRequest.GetGoalActor()));*/
		}
	}
}


bool ASurfaceNavAIController::SN_BuildPathfindingQuery(const FAIMoveRequest& MoveRequest, FSurfaceNavPathFindingQueryData& Query) const
{
	bool bResult = false;

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	if (NavSys && NavSys->IsReady())
	{
		FVector GoalLocation = MoveRequest.GetGoalLocation();
		FVector NonOffsetGoalLocation = GoalLocation;
		// TODO: Need to look at this a bit more in case it's needed
		if (MoveRequest.IsMoveToActorRequest())
		{
			NonOffsetGoalLocation = MoveRequest.GetGoalActor()->GetActorLocation();

			UNavMovementComponent* GoalMoveComp = MoveRequest.GetGoalActor()->FindComponentByClass<UNavMovementComponent>();
			USurfaceNavMovementComponent* GoalSurfaceNavMoveComp = Cast<USurfaceNavMovementComponent>(GoalMoveComp);
			if (GoalSurfaceNavMoveComp)
			{
				GoalLocation = GoalSurfaceNavMoveComp->GetActorSurfaceLocation();
			}
			else if (GoalMoveComp)
			{
#if ENGINE_MAJOR_VERSION < 5
				FBoxSphereBounds UpdatedCompBounds = GoalMoveComp->UpdatedComponent->CalcLocalBounds();
#else
				FBoxSphereBounds UpdatedCompBounds = GoalMoveComp->UpdatedComponent->GetLocalBounds();
#endif
				GoalLocation = MoveRequest.GetGoalActor()->GetActorRotation().RotateVector(FVector(0.f, 0.f, -UpdatedCompBounds.BoxExtent.Z)) + GoalMoveComp->UpdatedComponent->GetComponentLocation();
			}
			else if (MoveRequest.GetGoalActor()->IsA<APawn>())
			{
				FBox LocalBox = MoveRequest.GetGoalActor()->CalculateComponentsBoundingBoxInLocalSpace();
				float GoalActorHalfHeight = LocalBox.GetExtent().Z;
				GoalLocation = MoveRequest.GetGoalActor()->GetActorRotation().RotateVector(FVector(0.f, 0.f, -GoalActorHalfHeight)) + MoveRequest.GetGoalActor()->GetActorLocation();
			}
			else
			{
				const INavAgentInterface* NavGoal = Cast<const INavAgentInterface>(MoveRequest.GetGoalActor());
				if (NavGoal)
				{
					//const FVector Offset = NavGoal->GetMoveGoalOffset(this);
					//GoalLocation = FQuatRotationTranslationMatrix(MoveRequest.GetGoalActor()->GetActorQuat(), NavGoal->GetNavAgentLocation()).TransformPosition(Offset);

					GoalLocation = MoveRequest.GetGoalActor()->GetActorRotation().RotateVector(NavGoal->GetNavAgentLocation() - MoveRequest.GetGoalActor()->GetActorLocation()) + MoveRequest.GetGoalActor()->GetActorLocation();
				}
				else
				{
					GoalLocation = MoveRequest.GetGoalActor()->GetActorLocation();
				}
			}
		}

		auto MyPawn = GetPawn();

		// Find the location at the base of this actor in the center
		FVector LocAtBase = MyPawn->GetActorLocation();
		FVector NonOffsetLocAtBase = LocAtBase;

		USurfaceNavMovementComponent* MoveComp = GetPawn()->FindComponentByClass<USurfaceNavMovementComponent>();

		if (MoveComp)
			LocAtBase = MoveComp->GetActorSurfaceLocation();

		TSubclassOf<UNavigationQueryFilter> NavFilter = MoveRequest.GetNavigationFilter() ? MoveRequest.GetNavigationFilter() : DefaultNavigationFilterClass;

		//FSurfaceNavPathFindingQueryData(const UObject* InOwner, const FVector& InStartLocation, const FVector& InEndLocation, TSubclassOf<UNavigationQueryFilter> InFilterClass, FSurfaceNavPathSharedPtr InPathInstanceToFill, int32 InNavDataFlags = 0, bool bInAllowPartialPaths = true, const float InCostLimit = FLT_MAX)
		Query = FSurfaceNavPathFindingQueryData(this, LocAtBase, GoalLocation, NonOffsetLocAtBase, NonOffsetGoalLocation, NavFilter, NULL, 0, MoveRequest.IsUsingPartialPaths());

		//USurfaceNavMovementComponent

		if (MoveComp)
		{
			Query.MinAngleFromGround = MoveComp->MinAngleFromGround;
			Query.MaxAngleFromGround = MoveComp->MaxAngleFromGround;
			Query.AcceptanceRadius = MoveComp->GetPathPointAcceptanceRadius();
			Query.MaxPathingIterations = MoveComp->GetMaxPathingIterations();
			Query.PawnAngleExpenseMultipliers = MoveComp->PawnAngleExpenseMultipliers;
		}
		else
		{
			Query.MinAngleFromGround = 0.f;
			Query.MaxAngleFromGround = 180.f;
			Query.AcceptanceRadius = 100.f;
		}

		//FPathFindingQueryData(InOwner, Start, End, SourceQueryFilter, 0 /*InNavDataFlags*/, true /*bInAllowPartialPaths*/, CostLimit)
		// 
		//Query.SetAllowPartialPaths(MoveRequest.IsUsingPartialPaths());

		bResult = true;
	}
	else
	{
		//	/*if (NavSys == nullptr)
		//	{
		//		UE_VLOG(this, LogAINavigation, Warning, TEXT("Unable AAIController::BuildPathfindingQuery due to no NavigationSystem present. Note that even pathfinding-less movement requires presence of NavigationSystem."));
		//	}
		//	else
		//	{
		//		UE_VLOG(this, LogAINavigation, Warning, TEXT("Unable to find NavigationData instance while calling AAIController::BuildPathfindingQuery"));
		//	}*/
	}

	return bResult;
}

USurfaceNavPathFollowingComponent* ASurfaceNavAIController::GetSurfaceNavPathFollowingComponent() const
{
	return Cast<USurfaceNavPathFollowingComponent>(GetPathFollowingComponent());
}

int ASurfaceNavAIController::GetCurrentTargetPointIndex() const
{
	USurfaceNavPathFollowingComponent* const& SurfaceNavPathFollowingComp = GetSurfaceNavPathFollowingComponent();
	return SurfaceNavPathFollowingComp->GetCurrentTargetPointIndex();
}

void ASurfaceNavAIController::GetPathData(bool& Success, TArray<FSurfaceNavPathPointData>& PathData) const
{
	USurfaceNavPathFollowingComponent* const& SurfaceNavPathFollowingComp = GetSurfaceNavPathFollowingComponent();
	SurfaceNavPathFollowingComp->GetPathData(Success, PathData);
}

void ASurfaceNavAIController::GetPathPointDataAtIndex(int Index, bool& Success, FSurfaceNavPathPointData& PointData) const
{
	USurfaceNavPathFollowingComponent* const& SurfaceNavPathFollowingComp = GetSurfaceNavPathFollowingComponent();
	SurfaceNavPathFollowingComp->GetPathPointDataAtIndex(Index, Success, PointData);
}

void ASurfaceNavAIController::GetCurrentTargetPathPointData(bool& Success, FSurfaceNavPathPointData& PointData) const
{
	USurfaceNavPathFollowingComponent* const& SurfaceNavPathFollowingComp = GetSurfaceNavPathFollowingComponent();
	SurfaceNavPathFollowingComp->GetCurrentTargetPathPointData(Success, PointData);
}

void ASurfaceNavAIController::GetPreviousTargetPathPointData(bool& Success, FSurfaceNavPathPointData& PointData) const
{
	USurfaceNavPathFollowingComponent* const& SurfaceNavPathFollowingComp = GetSurfaceNavPathFollowingComponent();
	SurfaceNavPathFollowingComp->GetPreviousTargetPathPointData(Success, PointData);
}

bool ASurfaceNavAIController::IsFollowingAPath() const 
{ 
	USurfaceNavPathFollowingComponent* const& SurfaceNavPathFollowingComp = GetSurfaceNavPathFollowingComponent();
	return SurfaceNavPathFollowingComp->HasValidPath();
}
