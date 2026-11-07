// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavPathFollowingComponent.h"

#include "Navigation/PathFollowingComponent.h"
#include "UObject/Package.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "AISystem.h"
#include "BrainComponent.h"
#include "Engine/Canvas.h"
#include "AIController.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "AbstractNavData.h"
#include "NavLinkCustomInterface.h"
//#include "Navigation/MetaNavMeshPath.h"
#include "SurfaceNavigationSystem.h"

#include "SurfaceNavMetaPath.h"
#include "SurfaceNavPath.h"
#include "Kismet/KismetMathLibrary.h"
#include "AIConfig.h"

#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 4
#include "GameFramework/NavMovementInterface.h"
#endif

// Temporary log category, generally you should not check things in that use this
//CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSurfaceNavPath, Log, All);

#if UE_BUILD_TEST || UE_BUILD_SHIPPING
#define SHIPPING_STATIC static
#else
#define SHIPPING_STATIC
#endif // UE_BUILD_TEST || UE_BUILD_SHIPPING


namespace
{
	FORCEINLINE FVector FindGoalLocation(const USurfaceNavPathFollowingComponent& Component, const AActor& GoalActor, const INavAgentInterface* GoalNavAgent, float& GoalRadius, float& GoalHalfHeight)
	{
		if (GoalNavAgent)
		{
			const AActor* OwnerActor = Component.GetOwner();
			FVector GoalOffset;
			GoalNavAgent->GetMoveGoalReachTest(OwnerActor, Component.GetMoveGoalLocationOffset(), GoalOffset, GoalRadius, GoalHalfHeight);

			return FQuatRotationTranslationMatrix(GoalActor.GetActorQuat(), GoalNavAgent->GetNavAgentLocation()).TransformPosition(GoalOffset);
		}
		else
		{
			return GoalActor.GetActorLocation();
		}
	}
}

void LogBlockHelper(AActor* LogOwner, ISurfaceNavMovementInterface* SurfaceMovementComp, UNavMovementComponent* MoveComp, float RadiusPct, float HeightPct, const FVector& SegmentStart, const FVector& SegmentEnd)
{
#if ENABLE_VISUAL_LOG
	if (MoveComp && LogOwner)
	{
		const FVector AgentLocation = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : MoveComp->GetActorFeetLocation();
		const FVector ToTarget = (SegmentEnd - AgentLocation);
		const float SegmentDot = FVector::DotProduct(ToTarget.GetSafeNormal(), (SegmentEnd - SegmentStart).GetSafeNormal());
		//UE_VLOG(LogOwner, LogPathFollowing, Verbose, TEXT("[agent to segment end] dot [segment dir]: %f"), SegmentDot);

		float AgentRadius = 0.0f;
		float AgentHalfHeight = 0.0f;
		AActor* MovingAgent = MoveComp->GetOwner();
		MovingAgent->GetSimpleCollisionCylinder(AgentRadius, AgentHalfHeight);

		const float Dist2D = ToTarget.Size2D();
		//UE_VLOG(LogOwner, LogPathFollowing, Verbose, TEXT("dist 2d: %f (agent radius: %f [%f])"), Dist2D, AgentRadius, AgentRadius * (1 + RadiusPct));

		const float ZDiff = FMath::Abs(ToTarget.Z);
		//UE_VLOG(LogOwner, LogPathFollowing, Verbose, TEXT("Z diff: %f (agent halfZ: %f [%f])"), ZDiff, AgentHalfHeight, AgentHalfHeight * (1 + HeightPct));
	}
#endif // ENABLE_VISUAL_LOG
}

void USurfaceNavPathFollowingComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Status == EPathFollowingStatus::Moving)
	{
		DrawCurrentPath();
	}
}

void USurfaceNavPathFollowingComponent::DrawCurrentPath() const
{
#if WITH_EDITOR
	if (SurfaceNavProxySettings::ShowDebug_AI_Paths && Path.IsValid() && Path->IsValid())
	{
		int LastIndex = Path->GetPathPoints().Num() - 1;
		int CurIndex = 0;
		for (auto& PathPoint : Path->GetPathPoints())
		{
			DrawDebugPoint
			(
				GetWorld(),
				PathPoint,
				CurIndex == 0 || CurIndex == LastIndex ? 25.f : 15.f,
				CurIndex == GetCurrentTargetPointIndex() ? FColor::Yellow : CurIndex == 0 ? FColor::Green : CurIndex == LastIndex ? FColor::Red : FColor::Purple,
				false,
				-1
			);

			CurIndex++;
		}
	}
#endif
}


USurfaceNavPathFollowingComponent::USurfaceNavPathFollowingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//PrimaryComponentTick.bCanEverTick = false;
}

FAIRequestID USurfaceNavPathFollowingComponent::SurfaceNavRequestMove(const FAIMoveRequest& RequestData, FSurfaceNavPathSharedPtr InPath)
{
	//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("RequestMove: Path(%s) %s"), *GetPathDescHelper(InPath), *RequestData.ToString());
	//LogPathHelper(GetOwner(), InPath, RequestData.GetGoalActor());

	if (ResourceLock.IsLocked())
	{
		//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("Rejecting move request due to resource lock by %s"), *ResourceLock.GetLockPriorityName());
		return FAIRequestID::InvalidRequest;
	}

	const float UseAcceptanceRadius = (RequestData.GetAcceptanceRadius() == UPathFollowingComponent::DefaultAcceptanceRadius) ? MyDefaultAcceptanceRadius : RequestData.GetAcceptanceRadius();
	if (!InPath.IsValid() || UseAcceptanceRadius < 0.0f)
	{
		//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("RequestMove: invalid request"));
		return FAIRequestID::InvalidRequest;
	}

	// try to grab movement component
	if (!UpdateMovementComponent())
	{
		//UE_VLOG(GetOwner(), LogPathFollowing, Warning, TEXT("RequestMove: missing movement component"));
		return FAIRequestID::InvalidRequest;
	}

	FAIRequestID MoveId = GetCurrentRequestId();

	//bool test = InPath.Get() == Path.Get();

	// abort previous movement
	if (Status == EPathFollowingStatus::Paused && Path.IsValid() && InPath.Get() == Path.Get() && DestinationActor == RequestData.GetGoalActor())
	{
		ResumeMove();
	}
	else
	{
		if (Status != EPathFollowingStatus::Idle)
		{
			// setting to false to make sure OnPathFinished won't result in stoping 
			bStopMovementOnFinish = false;
			OnSurfaceNavPathFinished(EPathFollowingResult::Aborted, FPathFollowingResultFlags::NewRequest);
		}

		// force-setting this to true since this is the default state, and gets modified only 
		// when aborting movement.
		bStopMovementOnFinish = true;

		Reset();
		StoreRequestId();

		// store new Id, using CurrentRequestId directly at the end of function is not safe with chained moves
		MoveId = GetCurrentRequestId();

		// store new data
		Path = InPath;
		Path->AddObserver(FSurfaceNavPath::FSurfaceNavPathObserverDelegate::FDelegate::CreateUObject(this, &USurfaceNavPathFollowingComponent::OnPathEvent));
		if (GetMoveCompIsValid() && GetMoveCompOwnerAsActor())
		{
			Path->SetSourceActor(*(GetMoveCompOwnerAsActor()));
		}

		OriginalMoveRequestGoalLocation = RequestData.GetGoalActor() ? RequestData.GetGoalActor()->GetActorLocation() : RequestData.GetGoalLocation();

		// update meta path data
		FSurfaceNavMetaPath* MetaNavPath = Path->CastPath<FSurfaceNavMetaPath>();
		//FMetaNavMeshPath t;
		//FSurfaceNavMetaPath* MetaNavPath = static_cast<FSurfaceNavMetaPath*>(Path.Get()); // Path->CastPath<FSurfaceNavMetaPath>();

		if (MetaNavPath)
		{
			bIsUsingMetaPath = true;

			const FVector CurrentLocation = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : GetMoveCompIsValid() ? GetMoveCompFeetLocation() : FAISystem::InvalidLocation;
			MetaNavPath->Initialize(CurrentLocation);
		}

		PathTimeWhenPaused = 0.0f;
		OnPathUpdated();

		// make sure that OnPathUpdated didn't change current move request
		// otherwise there's no point in starting it
		if (GetCurrentRequestId() == MoveId)
		{
			AcceptanceRadius = UseAcceptanceRadius;
			bReachTestIncludesAgentRadius = RequestData.IsReachTestIncludingAgentRadius();
			bReachTestIncludesGoalRadius = RequestData.IsReachTestIncludingGoalRadius();
			GameData = RequestData.GetUserData();
			SetDestinationActor(RequestData.GetGoalActor());
			SetAcceptanceRadius(UseAcceptanceRadius);
			UpdateDecelerationData();

#if ENABLE_VISUAL_LOG
			const FVector CurrentLocation = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : GetMoveCompIsValid() ? GetMoveCompFeetLocation() : FVector::ZeroVector;
			const FVector DestLocation = InPath->GetDestinationLocation();
			const FVector ToDest = DestLocation - CurrentLocation;
			//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("RequestMove: accepted, ID(%u) dist2D(%.0f) distZ(%.0f)"), MoveId, ToDest.Size2D(), FMath::Abs(ToDest.Z));
#endif // ENABLE_VISUAL_LOG

			// with async pathfinding paths can be incomplete, movement will start after receiving path event 
			if (!bIsUsingMetaPath && Path.IsValid() && Path->IsValid())
			{
				Status = EPathFollowingStatus::Moving;

				// determine with path segment should be followed
				const uint32 CurrentSegment = SurfaceNavDetermineStartingPathPoint(InPath.Get());
				SetMoveSegment(CurrentSegment);
			}
			else
			{
				Status = EPathFollowingStatus::Waiting;
				GetWorld()->GetTimerManager().SetTimer(WaitingForPathTimer, this, &USurfaceNavPathFollowingComponent::OnWaitingPathTimeout, WaitingTimeout);
			}
		}
	}

	return MoveId;
}

void USurfaceNavPathFollowingComponent::FollowPathSegment(float DeltaTime)
{
	if (!Path.IsValid() || !GetMoveCompIsValid())
	{
		return;
	}

	const FVector CurrentLocation = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : GetMoveCompFeetLocation();

	FVector UpVector = GetPointNormal(MoveSegmentEndIndex);
	const FVector CurrentTarget = GetCurrentTargetLocation(); // +UpVector * SurfaceMovementComp->CalcSurfaceHeightFromActor(); // -UpVector * SurfaceMovementComp->PawnMovementHeightOffset;

	//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavPathFollowingComponent::FollowPathSegment: MoveSegmentEndIndex = %d"), MoveSegmentEndIndex);

	/*DrawDebugPoint
	(
		GetWorld(),
		CurrentLocation,
		10.f,
		FColor::Orange,
		false,
		5.f
	);*/

	FVector ForwardVector = CurrentTarget - CurrentLocation;
	FVector ForwardNormal = ForwardVector.GetSafeNormal();
	bool IsCurSegmentNavLink = IsCurrentSegmentNavigationLink();

	if (!IsCurSegmentNavLink)
		// If we're not on a nav link then let's see if we need to corner boost
		ApplyCornerBoost(CurrentTarget, ForwardNormal, UpVector);
	else
		// If we are on a nav link then let's avg the up normal between the start and end.
		// If we don't do this the pawn may keep trying to make rotation adjustments along the way
		// which can look odd.
		UpVector = (UpVector + GetPointNormal(MoveSegmentStartIndex)).GetSafeNormal();

	AActor* ActorOwner = GetOwner();
	FVector ActorForward = ActorOwner->GetActorForwardVector();
	FVector ActorRight = ActorOwner->GetActorRightVector();

	// set to false by default, we will set set this back to true if appropriate
	bIsDecelerating = false;

	const bool bAccelerationBased = GetMoveComp()->UseAccelerationForPathFollowing();
	if (bAccelerationBased)
	{
		CurrentMoveInput = ForwardNormal;

		const AActor* PathGoalActor = Path->GetGoalActor();

		if (bStopMovementOnFinish && (MoveSegmentStartIndex >= DecelerationSegmentIndex))
		{
			const FVector PathEnd = Path->GetEndLocation();

			if (PathGoalActor)
			{
				const FVector GoalActorLoc = PathGoalActor->GetActorLocation();

				// If the pawn is closer to the goal than the end point then we may be headed
				// outside of a nav mesh volume so let's stop here.
				if (FVector::DistSquared(PathEnd, GoalActorLoc) > FVector::DistSquared(CurrentLocation, GoalActorLoc))
				{
					CurrentMoveInput *= 0.f;
					bIsDecelerating = true;
				}
			}

			if (!bIsDecelerating)
			{
				const float DistToEnd = FVector::Dist(CurrentLocation, PathEnd);
				const bool bShouldDecelerate = DistToEnd < CachedBrakingDistance;
				if (bShouldDecelerate)
				{
					bIsDecelerating = true;

					const float SpeedPct = FMath::Clamp(DistToEnd / CachedBrakingDistance, 0.0f, 1.0f);
					CurrentMoveInput *= SpeedPct;
				}
			}
		}

		// Make sure corner deceleration is enabled.
		// Don't attempt corner deceleration if the pawn is on a nav link segment.
		// Make sure the pawn isn't already decelerating.
		if (SurfaceMovementComp->GetEnableCornerDeceleration() && !IsCurSegmentNavLink && !bIsDecelerating)
		{
			float ActorVelocitySq = GetMoveCompVelocity().SizeSquared();
			float DecelMinSpeed = SurfaceMovementComp->GetCornerDecelerationMinSpeedSq();

			// Make sure the current actor velocity is greater than the min speed required to apply deceleration
			if (ActorVelocitySq > DecelMinSpeed)
			{
				float MinSpeedPct = 1.f;
				int NavPointsToTest = SurfaceMovementComp->GetCornerDecelerationTestNavPointCount();

				// Get the current nav point target...it's the start of the next segment
				int CurrentIndex = GetNextPathIndex();

				for (int i = CurrentIndex, NavPointCounter = 0; i < Path->GetPathPoints().Num() && NavPointCounter < NavPointsToTest; i++, NavPointCounter++)
				{
					FVector PathDir = Path->GetSegmentDirection(i);
					float CornerDecelerationThreshold = SurfaceMovementComp->GetCornerDecelerationThreshold();
					FVector MoveVector = GetMoveCompVelocity().GetSafeNormal();
					float ActorForwardToSegmentDelta = FVector::DotProduct(MoveVector, PathDir);

					if (ActorForwardToSegmentDelta < CornerDecelerationThreshold)
					{
						const float DistToPoint = FVector::Dist(CurrentLocation, Path->GetPathPointLocation(i).Position);
						if (DistToPoint > SurfaceMovementComp->GetCornerDecelerationTestDistance())
							continue;

						// Use the dot product of the the pawns current forward velocity vector and the segment vector
						// being checked to determine our starting braking value
						float SpeedPctTest = 1.f - ActorForwardToSegmentDelta;
						SpeedPctTest /= (CornerDecelerationThreshold + 1.f);

						// Use the distance to get a percentage of the braking value.
						// The further away, the lower the the braking value should be.
						SpeedPctTest *= ((SurfaceMovementComp->GetCornerDecelerationTestDistance() - DistToPoint) / SurfaceMovementComp->GetCornerDecelerationTestDistance());

						// Apply the braking multiplier to the final calculated value
						SpeedPctTest *= SurfaceMovementComp->GetCornerDecelerationBrakingMultiplier();

						// Flip the braking value so it's a percentage of the speed and then clamp it
						SpeedPctTest = FMath::Clamp(1.f - SpeedPctTest, SurfaceMovementComp->GetCornerDecelerationMinSpeedPercent(), 1.f);

						if (SpeedPctTest < MinSpeedPct)
							MinSpeedPct = SpeedPctTest;
					}
				}

				CurrentMoveInput *= MinSpeedPct;
			}
		}

		PostProcessMove.ExecuteIfBound(this, CurrentMoveInput);
		if (SurfaceMovementComp)
			SurfaceMovementComp->HandleRotation(CurrentTarget, UpVector, ForwardNormal); // , IsCurrentSegmentNavigationLink());

		GetMoveComp()->RequestPathMove(CurrentMoveInput);
	}
	else
	{
		/*DrawDebugPoint
		(
			GetWorld(),
			CurrentTarget,
			10.f,
			FColor::Magenta,
			false,
			10.f
		);*/

		FVector MoveVelocity = ForwardVector / DeltaTime;

		const int32 LastSegmentStartIndex = Path->GetPathPoints().Num() - 2;
		const bool bNotFollowingLastSegment = (MoveSegmentStartIndex < LastSegmentStartIndex);

		PostProcessMove.ExecuteIfBound(this, MoveVelocity);
		if (SurfaceMovementComp)
			SurfaceMovementComp->HandleRotation(CurrentTarget, UpVector, ForwardNormal); // , IsCurrentSegmentNavigationLink());

		GetMoveComp()->RequestDirectMove(MoveVelocity, bNotFollowingLastSegment);
	}
}

void USurfaceNavPathFollowingComponent::ApplyCornerBoost(FVector CurrentTarget, FVector& ForwardNormal, FVector& UpVector)
{
	if (!SurfaceMovementComp)
		return;

	bool EnableCornerAdjustment = SurfaceMovementComp->GetEnableCornerBoost() && SurfaceMovementComp->GetCornerBoostMultiplier() > 0.f;
	if (!EnableCornerAdjustment)
		return;

	if (Path->GetPathPoints().Num() < 2)
		return;

	int CurrentIndex = GetNextPathIndex();

	// Our logic below assumes CurrentIndex is 1 or more
	if (CurrentIndex == 0)
		CurrentIndex++;

	FVector PointNormal = GetPointNormal(CurrentIndex);
	FVector PrevPointNormal = GetPointNormal(CurrentIndex - 1);
	float PointNormalsDelta = FVector::DotProduct(PrevPointNormal, PointNormal);

	// If corner angle is less than min threshold then end.
	if (PointNormalsDelta > SurfaceMovementComp->GetOutsideCornerAssistThreshold())
		return;

	// This basically gets us the steepness of the corner
	FVector PrevToCurDir = Path->GetSegmentDirection(CurrentIndex).GetSafeNormal();
	float OutsideCornerSteepness = FVector::DotProduct(PrevToCurDir, PointNormal);

	FVector ActorNavToPointDir = (SurfaceMovementComp->GetActorSurfaceLocation() - CurrentTarget).GetSafeNormal();

	float AcceptDistSq = FVector::DistSquared(SurfaceMovementComp->GetActorSurfaceLocation(), CurrentTarget);

	// This holds the angle (Dot) between the agent location and the current point normal.
	float ActorToCorner = FVector::DotProduct(ActorNavToPointDir, PointNormal);

	// If this is an outside corner and is steep enough and the actor is not within the boost shutoff
	// threshold for the corner then lets try to boost.
	if (ActorToCorner < SurfaceMovementComp->GetOutsideCornerAcceptanceDelta() &&
		AcceptDistSq > SurfaceMovementComp->GetOutsideCornerAcceptanceDistance())
	{
		// Get a cross product vector to use as a rotation axis.
		FVector RotationAxis = FVector::CrossProduct(PrevPointNormal, PrevToCurDir);

		// Now, rotate the impact point normal by 90 to get our new foward direction
		FVector TempForwardNormal((ForwardNormal + (PrevPointNormal.RotateAngleAxis(90.f, RotationAxis) * SurfaceMovementComp->GetCornerBoostMultiplier())).GetSafeNormal());

		// Need to make sure adjusted forward is not too far off the default forward, this could cause strange
		// behavior like hopping if the new forward is in the up direction.
		if (FVector::DotProduct(TempForwardNormal, ForwardNormal) > 0.f)
		{
			/*DrawDebugDirectionalArrow(
				GetWorld(),
				SurfaceMovementComp->GetActorSurfaceLocation(),
				CurrentTarget,
				30.f,
				FColor::Green,
				true,
				-1.f,
				0,
				1.f);*/

			ForwardNormal = TempForwardNormal;
		}
	}

}

void USurfaceNavPathFollowingComponent::UpdateDecelerationData()
{
	// no point in updating if we don't have a MovementComp
	if (!GetMoveCompIsValid())
	{
		return;
	}

	if (!bStopMovementOnFinish)
	{
		return;
	}

	const float CurrentMaxSpeed = GetMoveCompMaxSpeed();
	bool bUpdatePathSegment = (DecelerationSegmentIndex == INDEX_NONE);
	if (CurrentMaxSpeed != CachedBrakingMaxSpeed)
	{
		const float PrevBrakingDistance = CachedBrakingDistance;
		CachedBrakingDistance = GetMoveComp()->GetPathFollowingBrakingDistance(CurrentMaxSpeed);
		CachedBrakingMaxSpeed = CurrentMaxSpeed;

		bUpdatePathSegment = bUpdatePathSegment || (PrevBrakingDistance != CachedBrakingDistance);
	}

	if (bUpdatePathSegment && Path.IsValid())
	{
		DecelerationSegmentIndex = 0;

		const TArray<FNavPathPoint>& PathPoints = Path->GetPathPoints();
		float PathLengthFromEnd = 0.0f;

		for (int32 Idx = PathPoints.Num() - 1; Idx > 0; Idx--)
		{
			const float PathSegmentLength = FVector::Dist(PathPoints[Idx].Location, PathPoints[Idx - 1].Location);
			PathLengthFromEnd += PathSegmentLength;

			if (PathLengthFromEnd > CachedBrakingDistance)
			{
				DecelerationSegmentIndex = Idx - 1;
				break;
			}
		}

		//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("Updated deceleration segment: %d (MaxSpeed:%.2f, BrakingDistance:%.2f"), DecelerationSegmentIndex, CachedBrakingMaxSpeed, CachedBrakingDistance);
	}
}

void USurfaceNavPathFollowingComponent::PauseMove(FAIRequestID RequestID, EPathFollowingVelocityMode VelocityMode)
{
	//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("PauseMove: RequestID(%u)"), RequestID);
	if (Status == EPathFollowingStatus::Paused)
	{
		return;
	}

	if (RequestID.IsEquivalent(GetCurrentRequestId()))
	{
		if ((VelocityMode == EPathFollowingVelocityMode::Reset) && GetMoveCompIsValid() && HasMovementAuthority())
		{
			GetMoveComp()->StopMovementKeepPathing();
		}

		LocationWhenPaused = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : GetMoveCompIsValid() ? GetMoveCompFeetLocation() : FVector::ZeroVector;
		PathTimeWhenPaused = Path.IsValid() ? Path->GetTimeStamp() : 0.0f;
		Status = EPathFollowingStatus::Paused;

		UpdateMoveFocus();
	}
}

void USurfaceNavPathFollowingComponent::ResumeMove(FAIRequestID RequestID)
{
	if (RequestID.IsEquivalent(GetCurrentRequestId()) && RequestID.IsValid())
	{
		//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("ResumeMove: RequestID(%u)"), RequestID);

		const bool bMovedDuringPause = ShouldCheckPathOnResume();
		const bool bIsOnPath = IsOnPath();
		if (bIsOnPath)
		{
			Status = EPathFollowingStatus::Moving;

			const bool bWasPathUpdatedRecently = Path.IsValid() ? (Path->GetTimeStamp() > PathTimeWhenPaused) : false;
			if (bMovedDuringPause || bWasPathUpdatedRecently)
			{
				const int32 CurrentSegment = SurfaceNavDetermineStartingPathPoint(Path.Get());
				SetMoveSegment(CurrentSegment);
			}
			else
			{
				UpdateMoveFocus();
			}
		}
		else if (Path.IsValid() && Path->IsValid()) // && Path->GetNavigationDataUsed() == NULL)
		{
			// this means it's a scripted path, just resume
			Status = EPathFollowingStatus::Moving;
			UpdateMoveFocus();
		}
		else
		{
			OnSurfaceNavPathFinished(EPathFollowingResult::OffPath, FPathFollowingResultFlags::None);
		}
	}
	else
	{
		//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("ResumeMove: RequestID(%u) is neither \'AnyRequest\' not CurrentRequestId(%u). Ignoring."), RequestID, GetCurrentRequestId());
	}
}

void USurfaceNavPathFollowingComponent::OnPathFinished(const FPathFollowingResult& Result)
{
	//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("OnPathFinished: %s"), *Result.ToString());

	// Handle rotation one last time
	/*if (SurfaceMovementComp && Path)
	{
		const FVector CurrentLocation = SurfaceMovementComp->GetActorSurfaceLocation();
		FVector UpVector = GetPointNormal(MoveSegmentEndIndex);
		const FVector CurrentTarget = GetCurrentTargetLocation();
		FVector ForwardVector = CurrentTarget - CurrentLocation;
		FVector ForwardNormal = ForwardVector.GetSafeNormal();

		SurfaceMovementComp->HandleRotation(CurrentTarget, UpVector, ForwardNormal);
	}*/

	INavLinkCustomInterface* CustomNavLink = Cast<INavLinkCustomInterface>(CurrentCustomLinkOb.Get());
	if (CustomNavLink)
	{
		CustomNavLink->OnLinkMoveFinished(this);
		CurrentCustomLinkOb.Reset();
	}

	// update meta path if needed
	if (bIsUsingMetaPath && Result.IsSuccess() && GetMoveCompIsValid())
	{
		//FMetaNavMeshPath* MetaNavPath = Path->CastPath<FMetaNavMeshPath>();
		FSurfaceNavMetaPath* MetaNavPath = static_cast<FSurfaceNavMetaPath*>(Path.Get());
		const bool bNeedPathUpdate = MetaNavPath && MetaNavPath->ConditionalMoveToNextSection(SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : GetMoveCompFeetLocation(), EMetaPathUpdateReason::PathFinished);
		if (bNeedPathUpdate)
		{
			// keep move request active
			return;
		}
	}

	// save move status
	bLastMoveReachedGoal = Result.IsSuccess() && !HasPartialPath();

	// save data required for observers before reseting temporary variables
	const FAIRequestID FinishedMoveId = GetCurrentRequestId();

	Reset();
	UpdateMoveFocus();

	if (bStopMovementOnFinish && GetMoveCompIsValid() && HasMovementAuthority() && !GetMoveComp()->UseAccelerationForPathFollowing())
	{
		GetMoveComp()->StopMovementKeepPathing();
	}

	// notify observers after state was reset (they can request another move)
	OnRequestFinished.Broadcast(FinishedMoveId, Result);

	FAIMessage Msg(UBrainComponent::AIMessage_MoveFinished, this, FinishedMoveId, Result.IsSuccess());
	Msg.SetFlag(Result.Flags & 0xff);
	FAIMessage::Send(Cast<AController>(GetOwner()), Msg);
}

void USurfaceNavPathFollowingComponent::OnPathUpdated()
{
	if (OnSurfaceNavPathUpdated.IsBound())
	{
		bool Success = false;
		TArray<FSurfaceNavPathPointData> PathData;
		GetPathData(Success, PathData);
		OnSurfaceNavPathUpdated.Broadcast(PathData);
	}
}

bool USurfaceNavPathFollowingComponent::UpdateMovementComponent(bool bForce)
{
	if (!GetMoveCompIsValid() || bForce == true)
	{
		APawn* MyPawn = Cast<APawn>(GetOwner());
		if (MyPawn == NULL)
		{
			AController* MyController = Cast<AController>(GetOwner());
			if (MyController)
			{
				MyPawn = MyController->GetPawn();
			}
		}

		if (MyPawn)
		{
			MyPawn->OnActorHit.AddUniqueDynamic(this, &UPathFollowingComponent::OnActorBump);

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
			SetMovementComponent(MyPawn->FindComponentByClass<USurfaceNavMovementComponent>());
#else
			SetNavMovementInterface(MyPawn->FindComponentByClass<USurfaceNavMovementComponent>());
#endif
		}
	}

	return GetMoveCompIsValid();
}

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
void USurfaceNavPathFollowingComponent::SetMovementComponent(UNavMovementComponent* MoveComp)
{
	if (!MoveComp && MovementComp && MovementComp->GetPathFollowingAgent() == this)
	{
		MovementComp->SetPathFollowingAgent(nullptr);
	}

	MovementComp = MoveComp;
	SurfaceMovementComp = Cast<ISurfaceNavMovementInterface>(MoveComp);
	MyNavData = nullptr;

	if (MoveComp)
	{
		const FNavAgentProperties& NavAgentProps = MoveComp->GetNavAgentPropertiesRef();
		MyDefaultAcceptanceRadius = NavAgentProps.AgentRadius * 0.1f;
		MoveComp->SetPathFollowingAgent(Cast<IPathFollowingAgentInterface>(this));

		// *****************************************************
		// *****************************************************
		// TODO: Need to replace this with surface nav system and the builder data
		// *****************************************************
		/*UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

		if (NavSys)
		{
			MyNavData = NavSys->GetNavDataForProps(NavAgentProps, MoveComp->GetActorLocation());
			if (MyNavData == nullptr)
			{
				if (NavSys->IsInitialized() == false)
				{
					NavSys->OnNavigationInitDone.AddUObject(this, &UPathFollowingComponent::OnNavigationInitDone);
				}
				else
				{
					NavSys->OnNavDataRegisteredEvent.AddUniqueDynamic(this, &UPathFollowingComponent::OnNavDataRegistered);
				}
			}
		}*/
		// *****************************************************
		// *****************************************************
	}
}
#else
void USurfaceNavPathFollowingComponent::SetNavMovementInterface(INavMovementInterface* NavMoveInterface)
{
	if (!NavMoveInterface && NavMovementInterface.IsValid() && NavMovementInterface->GetPathFollowingAgent() == this)
	{
		NavMovementInterface->SetPathFollowingAgent(nullptr);
	}

	NavMovementInterface = NavMoveInterface;
	SurfaceMovementComp = GetMoveCompIsValid() ? Cast<ISurfaceNavMovementInterface>(NavMovementInterface.Get()) : nullptr;
	MyNavData = nullptr;

	if (NavMovementInterface.IsValid())
	{
		const FNavAgentProperties& NavAgentProps = NavMovementInterface->GetNavAgentPropertiesRef();
		MyDefaultAcceptanceRadius = NavAgentProps.AgentRadius * 0.1f;
		NavMovementInterface->SetPathFollowingAgent(Cast<IPathFollowingAgentInterface>(this));

		// *****************************************************
		// *****************************************************
		// TODO: Need to replace this with surface nav system and the builder data
		// *****************************************************
		/*UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

		if (NavSys)
		{
			MyNavData = NavSys->GetNavDataForProps(NavAgentProps, NavMovementInterface->GetLocation());
			if (MyNavData == nullptr)
			{
				if (NavSys->IsInitialized() == false)
				{
					NavSys->OnNavigationInitDone.AddUObject(this, &UPathFollowingComponent::OnNavigationInitDone);
				}
				else
				{
					NavSys->OnNavDataRegisteredEvent.AddUniqueDynamic(this, &UPathFollowingComponent::OnNavDataRegistered);
				}
			}
		}*/
		// *****************************************************
		// *****************************************************
	}
}
#endif

//FVector USurfaceNavPathFollowingComponent::GetMoveFocus(bool bAllowStrafe) const
//{
//	FVector MoveFocus = FVector::ZeroVector;
//	if (bAllowStrafe && DestinationActor.IsValid())
//	{
//		MoveFocus = DestinationActor->GetActorLocation();
//	}
//	else
//	{
//		const FVector CurrentMoveDirection = GetCurrentDirection();
//
//		/*UE_LOG(LogTemp, Warning, TEXT("USurfaceNavPathFollowingComponent::GetMoveFocus: CurrentDestination = %s | CurrentMoveDirection = %s"), 
//			*CurrentDestination.Position.ToString(),
//			*CurrentMoveDirection.ToString());*/
//
//		MoveFocus = *CurrentDestination + (CurrentMoveDirection * FAIConfig::Navigation::FocalPointDistance);
//	}
//
//	return MoveFocus;
//}
//
//void USurfaceNavPathFollowingComponent::UpdateMoveFocus()
//{
//	AAIController* AIOwner = Cast<AAIController>(GetOwner());
//	if (AIOwner != NULL)
//	{
//		if (Status == EPathFollowingStatus::Moving)
//		{
//			const FVector MoveFocus = GetMoveFocus(AIOwner->bAllowStrafe);
//
//			APawn* MyPawn = AIOwner->GetPawn();
//			if (MyPawn)
//			{
//				FVector CurrentLocation = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : MovementComp->GetActorFeetLocation();
//				//MyPawn->GetActorRotation().UnrotateVector()
//				MyPawn->GetPawnViewLocation().Z;
//			}
//			
//			AIOwner->SetFocalPoint(MoveFocus, EAIFocusPriority::Move);
//		}
//		else if (Status == EPathFollowingStatus::Idle)
//		{
//			AIOwner->ClearFocus(EAIFocusPriority::Move);
//		}
//	}
//}

void USurfaceNavPathFollowingComponent::FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink)
{
	if (CustomNavLink)
	{
		UObject* NavLinkOb = Cast<UObject>(CustomNavLink);
		if (CurrentCustomLinkOb == NavLinkOb)
		{
			//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("Finish custom move using navlink: %s"), *GetPathNameSafe(NavLinkOb));

			CustomNavLink->OnLinkMoveFinished(this);
			CurrentCustomLinkOb.Reset();
		}
	}
}

void USurfaceNavPathFollowingComponent::GetDebugStringTokens(TArray<FString>& Tokens, TArray<EPathFollowingDebugTokens::Type>& Flags) const
{
	//Tokens.Add(GetStatusDesc());
	//Flags.Add(EPathFollowingDebugTokens::Description);

	//if (Status != EPathFollowingStatus::Moving)
	//{
	//	return;
	//}

	//FString& StatusDesc = Tokens[0];
	//if (Path.IsValid())
	//{
	//	const int32 NumMoveSegments = (Path.IsValid() && Path->IsValid()) ? Path->GetPathPoints().Num() : -1;
	//	const bool bIsDirect = (Path->CastPath<FAbstractNavigationPath>() != NULL);
	//	const bool bIsCustomLink = CurrentCustomLinkOb.IsValid();

	//	if (!bIsDirect)
	//	{
	//		StatusDesc += FString::Printf(TEXT(" (%d..%d/%d)%s"), MoveSegmentStartIndex + 1, MoveSegmentEndIndex + 1, NumMoveSegments,
	//			bIsCustomLink ? TEXT(" (custom NavLink)") : TEXT(""));
	//	}
	//	else
	//	{
	//		StatusDesc += TEXT(" (direct)");
	//	}
	//}
	//else
	//{
	//	StatusDesc += TEXT(" (invalid path)");
	//}

	//// add debug params
	//float CurrentDot = 0.0f, CurrentDistance = 0.0f, CurrentHeight = 0.0f;
	//uint8 bFailedDot = 0, bFailedDistance = 0, bFailedHeight = 0;
	//DebugReachTest(CurrentDot, CurrentDistance, CurrentHeight, bFailedHeight, bFailedDistance, bFailedHeight);

	//Tokens.Add(TEXT("dot"));
	//Flags.Add(EPathFollowingDebugTokens::ParamName);
	//Tokens.Add(FString::Printf(TEXT("%.2f"), CurrentDot));
	//Flags.Add(bFailedDot ? EPathFollowingDebugTokens::FailedValue : EPathFollowingDebugTokens::PassedValue);

	//Tokens.Add(TEXT("dist2D"));
	//Flags.Add(EPathFollowingDebugTokens::ParamName);
	//Tokens.Add(FString::Printf(TEXT("%.0f"), CurrentDistance));
	//Flags.Add(bFailedDistance ? EPathFollowingDebugTokens::FailedValue : EPathFollowingDebugTokens::PassedValue);

	//Tokens.Add(TEXT("distZ"));
	//Flags.Add(EPathFollowingDebugTokens::ParamName);
	//Tokens.Add(FString::Printf(TEXT("%.0f"), CurrentHeight));
	//Flags.Add(bFailedHeight ? EPathFollowingDebugTokens::FailedValue : EPathFollowingDebugTokens::PassedValue);
}

FString USurfaceNavPathFollowingComponent::GetDebugString() const
{
	TArray<FString> Tokens;
	TArray<EPathFollowingDebugTokens::Type> Flags;

	GetDebugStringTokens(Tokens, Flags);

	FString Desc;
	for (int32 TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
	{
		Desc += Tokens[TokenIndex];
		Desc += (Flags.IsValidIndex(TokenIndex) && Flags[TokenIndex] == EPathFollowingDebugTokens::ParamName) ? TEXT(": ") : TEXT(", ");
	}

	return Desc;
}

bool USurfaceNavPathFollowingComponent::HandlePathUpdateEvent()
{
	//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("HandlePathUpdateEvent Path(%s) Status(%s)"), *GetPathDescHelper(Path), *GetStatusDesc());
	//LogPathHelper(GetOwner(), Path, DestinationActor.Get());

	if (Status == EPathFollowingStatus::Waiting && Path.IsValid() && !Path->IsValid())
	{
		/*UE_VLOG(GetOwner(), LogPathFollowing, Error, TEXT("Received unusable path in Waiting state! (ready:%d upToDate:%d pathPoints:%d)"),
			Path->IsReady() ? 1 : 0,
			Path->IsUpToDate() ? 1 : 0,
			Path->GetPathPoints().Num());*/
	}

	/*bool isReady = Path->IsReady();
	bool hasPoints = Path->GetPathPoints().Num() > 1;
	bool upToDate = Path->IsUpToDate();*/

	if (Status == EPathFollowingStatus::Idle || !Path.IsValid() || !Path->IsValid())
	{
		//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT(">> invalid request"));
		return false;
	}

	const AActor* PathGoalActor = Path->GetGoalActor();
	if (PathGoalActor)
	{
		float IgnoreGoalRadius, IgnoreGoalHalfHeight;
		OriginalMoveRequestGoalLocation = FindGoalLocation(*this, *PathGoalActor, Path->GetGoalActorAsNavAgent(), IgnoreGoalRadius, IgnoreGoalHalfHeight);
	}
	if (FAISystem::IsValidLocation(OriginalMoveRequestGoalLocation))
	{
		PreciseAcceptanceRadiusCheckStartNodeIndex = FindPreciseAcceptanceRadiusTestsStartNodeIndex(*Path, OriginalMoveRequestGoalLocation);
	}

	OnPathUpdated();
	GetWorld()->GetTimerManager().ClearTimer(WaitingForPathTimer);
	UpdateDecelerationData();

	if (Status == EPathFollowingStatus::Waiting || Status == EPathFollowingStatus::Moving)
	{
		Status = EPathFollowingStatus::Moving;

		const int32 CurrentSegment = SurfaceNavDetermineStartingPathPoint(Path.Get());
		SetMoveSegment(CurrentSegment);
	}

	return true;
}

bool USurfaceNavPathFollowingComponent::ShouldCheckPathOnResume() const
{
	bool bCheckPath = true;
	if (GetMoveCompIsValid())
	{
		float AgentRadius = 0.0f, AgentHalfHeight = 0.0f;
		GetMoveCompOwnerAsActor()->GetSimpleCollisionCylinder(AgentRadius, AgentHalfHeight);

		const FVector CurrentLocation = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : GetMoveCompFeetLocation();
		const float DeltaMove2DSq = (CurrentLocation - LocationWhenPaused).SizeSquared2D();
		const float DeltaZ = FMath::Abs(CurrentLocation.Z - LocationWhenPaused.Z);
		if (DeltaMove2DSq < FMath::Square(AgentRadius) && DeltaZ < (AgentHalfHeight * 0.5f))
		{
			bCheckPath = false;
		}
	}

	return bCheckPath;
}

void USurfaceNavPathFollowingComponent::OnNavigationInitDone()
{
	UWorld* MyWorld = GetWorld();
	if (GetMoveCompIsValid() && MyWorld)
	{
		// *****************************************************
		// *****************************************************
		// TODO: Need to replace this with surface nav system and the builder data
		// *****************************************************
		/*UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(MyWorld);
		check(NavSys);
		const FNavAgentProperties& NavAgentProps = MovementComp->GetNavAgentPropertiesRef();
		MyNavData = NavSys->GetNavDataForProps(NavAgentProps, MovementComp->GetActorLocation());
		NavSys->OnNavigationInitDone.RemoveAll(this);*/
		// *****************************************************
		// *****************************************************
	}
}

void USurfaceNavPathFollowingComponent::Reset()
{
	Super::Reset();
}

int32 USurfaceNavPathFollowingComponent::SurfaceNavDetermineStartingPathPoint(const FSurfaceNavPath* ConsideredPath) const
{
	int32 PickedPathPoint = INDEX_NONE;

	if (ConsideredPath && ConsideredPath->IsValid())
	{
		// if we already have some info on where we were on previous path
		// we can find out if there's a segment on new path we're currently at
		if (MoveSegmentStartRef != INVALID_NAVNODEREF &&
			MoveSegmentEndRef != INVALID_NAVNODEREF) //&&	ConsideredPath->GetNavigationDataUsed() != NULL)
		{
			// iterate every new path node and see if segment match
			for (int32 PathPoint = 0; PathPoint < ConsideredPath->GetPathPoints().Num() - 1; ++PathPoint)
			{
				if (ConsideredPath->GetPathPoints()[PathPoint].NodeRef == MoveSegmentStartRef &&
					ConsideredPath->GetPathPoints()[PathPoint + 1].NodeRef == MoveSegmentEndRef)
				{
					PickedPathPoint = PathPoint;
					break;
				}
			}
		}

		if (GetMoveCompIsValid() && PickedPathPoint == INDEX_NONE)
		{
			if (ConsideredPath->GetPathPoints().Num() > 2)
			{
				// check if is closer to first or second path point (don't assume AI's standing)
				const FVector CurrentLocation = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : GetMoveCompFeetLocation();
				const FVector PathPt0 = *ConsideredPath->GetPathPointLocation(0);
				const FVector PathPt1 = *ConsideredPath->GetPathPointLocation(1);
				// making this test in 2d to avoid situation where agent's Z location not being in "navmesh plane"
				// would influence the result
				/*const float SqDistToFirstPoint = (CurrentLocation - PathPt0).SizeSquared2D();
				const float SqDistToSecondPoint = (CurrentLocation - PathPt1).SizeSquared2D();*/

				const float DistToFirstPoint = FVector::DistSquared(CurrentLocation, PathPt0);
				const float DistToSecondPoint = FVector::DistSquared(CurrentLocation, PathPt1);

				PickedPathPoint = (DistToFirstPoint < DistToSecondPoint) ? 0 : 1;
				/*PickedPathPoint = FMath::IsNearlyEqual(SqDistToFirstPoint, SqDistToSecondPoint) ?
					((FMath::Abs(CurrentLocation.Z - PathPt0.Z) < FMath::Abs(CurrentLocation.Z - PathPt1.Z)) ? 0 : 1) :
					((SqDistToFirstPoint < SqDistToSecondPoint) ? 0 : 1);*/
			}
			else
			{
				// If there are only two point we probably should start from the beginning
				PickedPathPoint = 0;
			}
		}
	}

	return PickedPathPoint;
}

void USurfaceNavPathFollowingComponent::SetMoveSegment(int32 SegmentStartIndex)
{
	const float NavLinkAcceptanceRadius = SurfaceMovementComp && SurfaceMovementComp->GetNavLinkPointAcceptanceRadius() > 0.f ? SurfaceMovementComp->GetNavLinkPointAcceptanceRadius() : GET_AI_CONFIG_VAR(PathfollowingNavLinkAcceptanceRadius);
	const float PathPointAcceptanceRadius = SurfaceMovementComp && SurfaceMovementComp->GetPathPointAcceptanceRadius() > 0.f ? SurfaceMovementComp->GetPathPointAcceptanceRadius() : GET_AI_CONFIG_VAR(PathfollowingRegularPathPointAcceptanceRadius);

	/*bool ShouldFindNextSegment = true;
	while (ShouldFindNextSegment)
	{*/
	int32 EndSegmentIndex = SegmentStartIndex + 1;
	const FSurfaceNavPath* PathInstance = Path.Get();
	if (PathInstance != nullptr && PathInstance->GetPathPoints().IsValidIndex(SegmentStartIndex) && PathInstance->GetPathPoints().IsValidIndex(EndSegmentIndex))
	{
		EndSegmentIndex = DetermineCurrentTargetPathPoint(SegmentStartIndex);

		MoveSegmentStartIndex = SegmentStartIndex;
		MoveSegmentEndIndex = EndSegmentIndex;
		const FNavPathPoint& PathPt0 = PathInstance->GetPathPoints()[MoveSegmentStartIndex];
		const FNavPathPoint& PathPt1 = PathInstance->GetPathPoints()[MoveSegmentEndIndex];

		MoveSegmentStartRef = PathPt0.NodeRef;
		MoveSegmentEndRef = PathPt1.NodeRef;

		CurrentDestination = PathInstance->GetPathPointLocation(MoveSegmentEndIndex);
		const FVector SegmentStart = *PathInstance->GetPathPointLocation(MoveSegmentStartIndex);
		FVector SegmentEnd = *CurrentDestination;

		// make sure we have a non-zero direction if still following a valid path
		if (SegmentStart.Equals(SegmentEnd) && PathInstance->GetPathPoints().IsValidIndex(MoveSegmentEndIndex + 1))
		{
			MoveSegmentEndIndex++;

			CurrentDestination = PathInstance->GetPathPointLocation(MoveSegmentEndIndex);
			SegmentEnd = *CurrentDestination;
		}

		CurrentAcceptanceRadius = (PathInstance->GetPathPoints().Num() == (MoveSegmentEndIndex + 1))
			? GetFinalAcceptanceRadius(*PathInstance, OriginalMoveRequestGoalLocation)
			// pick appropriate value base on whether we're going to nav link or not
			: (PathInstance->PathPointIsNavLink(PathPt1) == false ? PathPointAcceptanceRadius : NavLinkAcceptanceRadius);
		//: (FNavMeshNodeFlags(PathPt1.Flags).IsNavLink() == false ? PathPointAcceptanceRadius : NavLinkAcceptanceRadius);

		MoveSegmentDirection = (SegmentEnd - SegmentStart).GetSafeNormal();

		//bWalkingNavLinkStart = PathInstance->PathPointIsNavLink(FNavMeshNodeFlags(PathPt0.Flags).IsNavLink();
		bWalkingNavLinkStart = PathInstance->PathPointIsNavLink(PathPt0);

		// handle moving through custom nav links
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
		if (PathPt0.CustomLinkId)
		{
			// TODO: Need to do something else here
			UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			INavLinkCustomInterface* CustomNavLink = NavSys->GetCustomLink(PathPt0.CustomLinkId);
			StartUsingCustomLink(CustomNavLink, SegmentEnd);

			//// We're following a nav link so don't loop
			//ShouldFindNextSegment = false;
		}
#else
		if (PathPt0.CustomNavLinkId.GetId())
		{
			// TODO: Need to do something else here
			UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			INavLinkCustomInterface* CustomNavLink = NavSys->GetCustomLink(PathPt0.CustomNavLinkId);
			StartUsingCustomLink(CustomNavLink, SegmentEnd);

			//// We're following a nav link so don't loop
			//ShouldFindNextSegment = false;
		}
#endif


		//else
		//{
		//	const FVector CurrentLocation = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : MovementComp->GetActorFeetLocation();
		//	auto SqDistToEnd = FVector::DistSquared(CurrentLocation, PathPt1.Location);
		//	auto SqAcceptRadius = CurrentAcceptanceRadius * CurrentAcceptanceRadius;

		//	// Are we already within the acceptance radius of the segment's end point?
		//	if (SqDistToEnd <= SqAcceptRadius)
		//	{
		//		// We are so move to the next point
		//		SegmentStartIndex++;
		//		continue;
		//	}

		//	// Set to false so the loop will stop processing
		//	ShouldFindNextSegment = false;
		//}

		// update move focus in owning AI
		UpdateMoveFocus();

		if (OnSurfaceNavCurrentTargetPointUpdated.IsBound())
		{
			bool Success = false;
			FSurfaceNavPathPointData NewTargetPointData;
			FSurfaceNavPathPointData PreviousTargetPointData;
			GetPathPointDataAtIndex(MoveSegmentStartIndex, Success, PreviousTargetPointData);
			GetPathPointDataAtIndex(MoveSegmentEndIndex, Success, NewTargetPointData);
			OnSurfaceNavCurrentTargetPointUpdated.Broadcast(MoveSegmentEndIndex, NewTargetPointData, PreviousTargetPointData);
		}
	}
	//else
	//{
	//	// No more valid pathing options so don't loop
	//	ShouldFindNextSegment = false;
	//}
//}
}

bool USurfaceNavPathFollowingComponent::IsCurrentSegmentNavigationLink() const
{
	return Path.IsValid() && Path->GetPathPoints().IsValidIndex(MoveSegmentStartIndex) && Path->PathPointIsNavLink(MoveSegmentStartIndex);
}

void USurfaceNavPathFollowingComponent::UpdatePathSegment()
{
#if !UE_BUILD_SHIPPING
	DEBUG_bMovingDirectlyToGoal = false;
#endif // !UE_BUILD_SHIPPING

	if ((Path.IsValid() == false) || !GetMoveCompIsValid())
	{
		UE_CVLOG(Path.IsValid() == false, this, LogPathFollowing, Log, TEXT("Aborting move due to not having a valid path object"));
		OnSurfaceNavPathFinished(EPathFollowingResult::Aborted, FPathFollowingResultFlags::InvalidPath);
		// bCanSeeNextPoint = false;
		return;
	}

	const FVector CurrentLocation = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : GetMoveCompFeetLocation();
	const bool bCanUpdateState = HasMovementAuthority();

	if (!Path->IsValid())
	{
		if (!Path->IsWaitingForRepath())
		{
			//UE_VLOG(this, LogPathFollowing, Log, TEXT("Aborting move due to path being invalid and not waiting for repath"));
			OnSurfaceNavPathFinished(EPathFollowingResult::Aborted, FPathFollowingResultFlags::InvalidPath);
			// bCanSeeNextPoint = false;
			return;
		}
		else if (HasStartedNavLinkMove() && bCanUpdateState && Status == EPathFollowingStatus::Moving)
		{
			// pawn needs to get off navlink to unlock path updates (AAIController::ShouldPostponePathUpdates)
			if (HasReachedCurrentTarget(CurrentLocation))
			{
				OnSegmentFinished();
				SetNextMoveSegment();
				// bCanSeeNextPoint = false;
			}
		}
		else
		{
			// continue with execution, if navigation is being rebuild constantly AI will get stuck with current waypoint
			// path updates should be still coming in, even though they get invalidated right away
			//UE_VLOG(this, LogPathFollowing, Log, TEXT("Updating path points in invalid & pending path!"));
		}
	}

	FSurfaceNavMetaPath* MetaNavPath = bIsUsingMetaPath ? static_cast<FSurfaceNavMetaPath*>(Path.Get()) : nullptr;

	/** it's possible that finishing this move request will result in another request
	 *	which won't be easily detectable from this function. This simple local
	 *	variable gives us this knowledge. */
	const FAIRequestID MoveRequestId = GetCurrentRequestId();

	// if agent has control over its movement, check finish conditions
	if (bCanUpdateState && Status == EPathFollowingStatus::Moving)
	{
		const int32 LastSegmentEndIndex = Path->GetPathPoints().Num() - 1;
		const bool bFollowingLastSegment = (MoveSegmentEndIndex >= LastSegmentEndIndex);
		const bool bLastPathChunk = (MetaNavPath == nullptr || MetaNavPath->IsLastSection());

		if (bCollidedWithGoal)
		{
			// check if collided with goal actor
			OnSegmentFinished();
			OnSurfaceNavPathFinished(EPathFollowingResult::Success, FPathFollowingResultFlags::None);
			// bCanSeeNextPoint = false;
		}
		else if (MoveSegmentEndIndex > PreciseAcceptanceRadiusCheckStartNodeIndex && HasReachedDestination(CurrentLocation))
		{
			//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavPathFollowingComponent::UpdatePathSegment: MoveSegmentEndIndex > PreciseAcceptanceRadiusCheckStartNodeIndex && HasReachedDestination(CurrentLocation)"));

			OnSegmentFinished();
			OnSurfaceNavPathFinished(EPathFollowingResult::Success, FPathFollowingResultFlags::None);
			// bCanSeeNextPoint = false;
		}
		else if (bFollowingLastSegment && bMoveToGoalOnLastSegment && bLastPathChunk)
		{
			//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavPathFollowingComponent::UpdatePathSegment: bFollowingLastSegment && bMoveToGoalOnLastSegment && bLastPathChunk"));

			// use goal actor for end of last path segment
			// UNLESS it's partial path (can't reach goal)
			if (DestinationActor.IsValid() && Path->IsPartial() == false)
			{
				//const FVector AgentLocation = GetDestinationNavAgentLocation(); // DestinationAgent ? DestinationAgent->GetNavAgentLocation() : DestinationActor->GetActorLocation();
				// note that the condition below requires GoalLocation to be in world space.

				FBox DestLocalBox;
				bool DestLocalBoxSet = false;
				const FVector GoalLocation = GetDestinationActorMoveToLocation(DestLocalBox, DestLocalBoxSet); // FQuatRotationTranslationMatrix(DestinationActor->GetActorQuat(), AgentLocation).TransformPosition(MoveOffset);

				const float AcceptanceRadiusSq = FMath::Square(SurfaceMovementComp->GetPathPointAcceptanceRadius());
				if (FVector::DistSquared(GoalLocation, CurrentLocation) <= AcceptanceRadiusSq)
				{
					// Let's only do this if the goal actor is within the acceptance radius

					CurrentDestination.Set(NULL, GoalLocation);

					//UE_VLOG(this, LogPathFollowing, Log, TEXT("Moving directly to move goal rather than following last path segment"));
					//UE_VLOG_LOCATION(this, LogPathFollowing, VeryVerbose, GoalLocation, 30, FColor::Green, TEXT("Last-segment-to-actor"));
					//UE_VLOG_SEGMENT(this, LogPathFollowing, VeryVerbose, CurrentLocation, GoalLocation, FColor::Green, TEXT_EMPTY);
				}
			}

			UpdateMoveFocus();

#if !UE_BUILD_SHIPPING
			DEBUG_bMovingDirectlyToGoal = true;
#endif // !UE_BUILD_SHIPPING
		}
		// check if current move segment is finished
		else if (HasReachedCurrentTarget(CurrentLocation))
		{
			//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavPathFollowingComponent::UpdatePathSegment: HasReachedCurrentTarget(CurrentLocation)"));

			OnSegmentFinished();
			SetNextMoveSegment();
			// bCanSeeNextPoint = false;
		}
		else
		{
			//UE_LOG(LogTemp, Error, TEXT("USurfaceNavPathFollowingComponent::UpdatePathSegment: No condition met"));
		}
	}

	if (bCanUpdateState
		&& Status == EPathFollowingStatus::Moving
		// still the same move request
		&& MoveRequestId == GetCurrentRequestId())
	{
		// check waypoint switch condition in meta paths
		if (MetaNavPath)
		{
			MetaNavPath->ConditionalMoveToNextSection(CurrentLocation, EMetaPathUpdateReason::MoveTick);
		}

		// gather location samples to detect if moving agent is blocked
		const bool bHasNewSample = UpdateBlockDetection();
		if (bHasNewSample && IsBlocked())
		{
			if (Path->GetPathPoints().IsValidIndex(MoveSegmentEndIndex) && Path->GetPathPoints().IsValidIndex(MoveSegmentStartIndex))
			{
				/*LogBlockHelper(GetOwner(), SurfaceMovementComp, GetMoveComp(), MinAgentRadiusPct, MinAgentHalfHeightPct,
					*Path->GetPathPointLocation(MoveSegmentStartIndex),
					*Path->GetPathPointLocation(MoveSegmentEndIndex));*/
			}
			else
			{
				if ((GetOwner() != NULL) && GetMoveCompIsValid())
				{
					//UE_VLOG(GetOwner(), LogPathFollowing, Verbose, TEXT("Path blocked, but move segment indices are not valid: start %d, end %d of %d"), MoveSegmentStartIndex, MoveSegmentEndIndex, Path->GetPathPoints().Num());
				}
			}
			OnSurfaceNavPathFinished(EPathFollowingResult::Blocked, FPathFollowingResultFlags::None);
			// bCanSeeNextPoint = false;
		}
	}
}

bool USurfaceNavPathFollowingComponent::HasReachedDestination(const FVector& CurrentLocation) const
{
	// get cylinder at goal location
	FVector GoalLocation = *Path->GetPathPointLocation(Path->GetPathPoints().Num() - 1);
	float GoalRadius = 0.0f;
	float GoalHalfHeight = 0.0f;

	// take goal's current location, unless path is partial or last segment doesn't reach goal actor (used by tethered AI)
	if (DestinationActor.IsValid() && !Path->IsPartial() && bMoveToGoalOnLastSegment)
	{
		/*if (DestinationAgent)
		{*/
		const AActor* OwnerActor = GetOwner();

		FBox DestLocalBox;
		bool DestLocalBoxSet = false;
		GoalLocation = GetDestinationActorMoveToLocation(DestLocalBox, DestLocalBoxSet);
		if (!DestLocalBoxSet)
			DestLocalBox = DestinationActor->CalculateComponentsBoundingBoxInLocalSpace();

		//GoalRadius = LocalBox.Max.Size();
		GoalRadius = FMath::Sqrt(FMath::Square(DestLocalBox.GetExtent().X) + FMath::Square(DestLocalBox.GetExtent().Y));

		//float GoalHalfHeight = LocalBox.GetExtent().Z;

		//DestinationAgent->GetMoveGoalReachTest(OwnerActor, MoveOffset, GoalOffset, GoalRadius, GoalHalfHeight);

		/*DrawDebugCapsule
		(
			GetWorld(),
			DestinationActor->GetActorLocation(),
			CylinderHalfHeight,
			CylinderRadius,
			FQuat(),
			FColor::Red,
			false,
			2.f
		);*/

		/*DrawDebugPoint
		(
			GetWorld(),
			DestinationActor->GetActorLocation(),
			10.f,
			FColor::Green,
			false,
			2.f
		);*/

		// Get nav agent location
		// This will be offset on the Z axis
		// 

		// As far as I can tell, the nav agent location is always in the down direction.
		// Use the actor rotation to rotate the offset from nav agent loc to actor location and then
		// add the actor loc back to it. Hopefully this works in most/all cases.
		//GoalLocation = DestinationActor->GetActorRotation().RotateVector(DestinationAgent->GetNavAgentLocation() - DestinationActor->GetActorLocation()) + DestinationActor->GetActorLocation();

		//float GoalActorHalfHeight = LocalBox.GetExtent().Z;
		//GoalLocation = DestinationActor->GetActorRotation().RotateVector(FVector(0.f, 0.f, -GoalActorHalfHeight)) + DestinationActor->GetActorLocation();

		//GoalLocation = DestinationActor->GetActorLocation();

		//GoalLocation = FQuatRotationTranslationMatrix(DestinationActor->GetActorQuat(), DestinationAgent->GetNavAgentLocation()).TransformPosition(GoalOffset);

		/*UE_LOG(LogTemp, Warning, TEXT("*************************"));
		UE_LOG(LogTemp, Warning, TEXT("GoalOffset = %s"), *GoalOffset.ToCompactString());
		UE_LOG(LogTemp, Warning, TEXT("ActorLoc = %s"), *DestinationActor->GetActorLocation().ToCompactString());
		UE_LOG(LogTemp, Warning, TEXT("GoalLoc = %s"), *GoalLocation.ToCompactString());
		UE_LOG(LogTemp, Warning, TEXT("*************************"));*/

		/*DrawDebugPoint
		(
			GetWorld(),
			GoalLocation,
			10.f,
			FColor::Red,
			false,
			2.f
		);*/

		/*}
		else
		{
			GoalLocation = DestinationActor->GetActorLocation();
		}*/
	}

	const float AcceptanceRangeToUse = GetFinalAcceptanceRadius(*Path, OriginalMoveRequestGoalLocation);
	return HasReachedInternal(GoalLocation, bReachTestIncludesGoalRadius ? GoalRadius : 0.0f, GoalHalfHeight, CurrentLocation
		, AcceptanceRangeToUse, bReachTestIncludesAgentRadius ? MinAgentRadiusPct : 0.0f);
}

bool USurfaceNavPathFollowingComponent::HasReachedCurrentTarget(const FVector& CurrentLocation) const
{
	if (!GetMoveCompIsValid())
	{
		return false;
	}

	const FVector CurrentTarget = GetCurrentTargetLocation();
	const FVector CurrentDirection = GetCurrentDirection();

	// check if moved too far
	const FVector ToTarget = (CurrentTarget - (SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : GetMoveCompFeetLocation()));
	const float SegmentDot = FVector::DotProduct(ToTarget, CurrentDirection);
	if (SegmentDot < 0.0)
	{
		return true;
	}

	// or standing at target position
	// don't use acceptance radius here, it has to be exact for moving near corners (2D test < 5% of agent radius)
	const float GoalRadius = 0.0f;
	const float GoalHalfHeight = 0.0f;

	return HasReachedInternal(CurrentTarget, GoalRadius, GoalHalfHeight, CurrentLocation, CurrentAcceptanceRadius, 0.05f);
}

bool USurfaceNavPathFollowingComponent::HasReachedInternal(const FVector& GoalLocation, float GoalRadius, float GoalHalfHeight, const FVector& AgentLocation, float RadiusThreshold, float AgentRadiusMultiplier) const
{
	if (!SurfaceMovementComp)
		return false;

	// get cylinder of moving agent
	float AgentRadius = 0.0f;
	float AgentHalfHeight = 0.0f;
	AActor* MovingAgent = GetMoveCompOwnerAsActor();

	FBox LocalBox = MovingAgent->CalculateComponentsBoundingBoxInLocalSpace();
	AgentRadius = FMath::Sqrt(FMath::Square(LocalBox.GetExtent().X) + FMath::Square(LocalBox.GetExtent().Y));

	// check if they overlap (with added AcceptanceRadius)
	const FVector ToGoal = GoalLocation - AgentLocation;

	// Make sure we're within the "reached" distance configured
	const float DistSq = ToGoal.SizeSquared();
	float UseRadius = FMath::Square(RadiusThreshold + GoalRadius + (AgentRadius * AgentRadiusMultiplier));


	if (DistSq > UseRadius)
		return false;

	bool bEnableOutsideCornerAssist = SurfaceMovementComp->GetEnableOutsideCornerAssist();
	bool bEnableInsideCornerAssist = SurfaceMovementComp->GetEnableInsideCornerAssist();

	if ((!bEnableOutsideCornerAssist && !bEnableInsideCornerAssist) || IsCurrentSegmentNavigationLink())
		return true;

	if (Path->GetPathPoints().Num() < 2)
		return true;

	int CurrentIndex = GetNextPathIndex();

	// Our logic below assumes CurrentIndex is 1 or more
	if (CurrentIndex == 0)
		CurrentIndex++;

	FVector PointNormal = GetPointNormal(CurrentIndex);
	FVector PrevPointNormal = GetPointNormal(CurrentIndex - 1);

	// Need to do this because the Dot of the normal will be the opposite
	// value of the angle of the corner.
	float PointNormalsDelta = FVector::DotProduct(PrevPointNormal, PointNormal);

	bool bCanTestOutsideCorner = bEnableOutsideCornerAssist && PointNormalsDelta < SurfaceMovementComp->GetOutsideCornerAssistThreshold();
	bool bCanTestInsideCorner = bEnableInsideCornerAssist && PointNormalsDelta < SurfaceMovementComp->GetInsideCornerAssistThreshold();

	if (!bCanTestOutsideCorner && !bCanTestInsideCorner)
		return true;

	FVector ActorNavToPointDir = (SurfaceMovementComp->GetActorSurfaceLocation() - GoalLocation).GetSafeNormal();
	float AcceptDistSq = FVector::DistSquared(SurfaceMovementComp->GetActorSurfaceLocation(), GoalLocation);
	float ActorToCorner = FVector::DotProduct(ActorNavToPointDir, PointNormal);

	// If this is an outside corner, is steep enough, and angle is greater than the threshold then we return
	// false because we consider the point as not being reached yet.
	if (bCanTestOutsideCorner && ActorToCorner < SurfaceMovementComp->GetOutsideCornerAcceptanceDelta() && AcceptDistSq > SurfaceMovementComp->GetOutsideCornerAcceptanceDistance())
	{
#if WITH_EDITOR
		if (SurfaceMovementComp->GetShowOutsideCornerAssistDebugData())
		{
			float ShowTime = SurfaceMovementComp->GetShowOutsideCornerAssistDebugTime();
			FVector ActorOffsetLoc = SurfaceMovementComp->GetActorSurfaceLocation();

			DrawDebugPoint
			(
				GetWorld(),
				GoalLocation,
				30.f,
				FColorList::Red,
				false,
				ShowTime
			);

			DrawDebugPoint
			(
				GetWorld(),
				Path->GetPathPoints()[CurrentIndex - 1],
				30.f,
				FColorList::Green,
				false,
				ShowTime
			);

			DrawDebugLine(
				GetWorld(),
				SurfaceMovementComp->GetActorSurfaceLocation(),
				GoalLocation,
				FColor::Blue,
				false,
				ShowTime,
				0,
				1.f
			);
		}
#endif

		return false;
	}

	// If this is an inside corner, is steep enough, and angle is greater than the threshold then we return
	// false because we consider the point as not being reached yet.
	if (bCanTestInsideCorner && ActorToCorner > SurfaceMovementComp->GetInsideCornerAcceptanceDelta() && AcceptDistSq > SurfaceMovementComp->GetInsideCornerAcceptanceDistance())
	{
#if WITH_EDITOR
		if (SurfaceMovementComp->GetShowInsideCornerAssistDebugData())
		{
			float ShowTime = SurfaceMovementComp->GetShowInsideCornerAssistDebugTime();
			FVector ActorOffsetLoc = SurfaceMovementComp->GetActorSurfaceLocation();

			DrawDebugPoint
			(
				GetWorld(),
				GoalLocation,
				30.f,
				FColorList::Red,
				false,
				ShowTime
			);

			DrawDebugPoint
			(
				GetWorld(),
				Path->GetPathPoints()[CurrentIndex - 1],
				30.f,
				FColorList::Green,
				false,
				ShowTime
			);

			DrawDebugLine(
				GetWorld(),
				SurfaceMovementComp->GetActorSurfaceLocation(),
				GoalLocation,
				FColor::Blue,
				false,
				ShowTime,
				0,
				1.f
			);
		}
#endif

		return false;
	}

	return true;
}

void USurfaceNavPathFollowingComponent::StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint)
{
	INavLinkCustomInterface* PrevNavLink = Cast<INavLinkCustomInterface>(CurrentCustomLinkOb.Get());
	if (PrevNavLink)
	{
		//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("Force finish custom move using navlink: %s"), *GetPathNameSafe(CurrentCustomLinkOb.Get()));

		PrevNavLink->OnLinkMoveFinished(this);
		CurrentCustomLinkOb.Reset();
	}

	UObject* NewNavLinkOb = Cast<UObject>(CustomNavLink);
	if (NewNavLinkOb)
	{
		CurrentCustomLinkOb = NewNavLinkOb;

		const bool bCustomMove = CustomNavLink->OnLinkMoveStarted(this, DestPoint);
		/*UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("%s navlink: %s"),
			bCustomMove ? TEXT("Custom move using") : TEXT("Notify"), *GetPathNameSafe(NewNavLinkOb));*/

		if (!bCustomMove)
		{
			CurrentCustomLinkOb = NULL;
		}
	}
}

bool USurfaceNavPathFollowingComponent::IsOnPath() const
{
	bool bOnPath = false;

	if (Path.IsValid() && Path->IsValid() && GetMoveCompIsValid())
	{
		const float NavLinkAcceptanceRadius = SurfaceMovementComp && SurfaceMovementComp->GetNavLinkPointAcceptanceRadius() > 0.f ? SurfaceMovementComp->GetNavLinkPointAcceptanceRadius() : GET_AI_CONFIG_VAR(PathfollowingNavLinkAcceptanceRadius);
		const float PathPointAcceptanceRadius = SurfaceMovementComp && SurfaceMovementComp->GetPathPointAcceptanceRadius() > 0.f ? SurfaceMovementComp->GetPathPointAcceptanceRadius() : GET_AI_CONFIG_VAR(PathfollowingRegularPathPointAcceptanceRadius);

		// Find the largest acceptance radius we can test with and add a little padding
		const float LargestAcceptanceRadius = FMath::Square(FMath::Max(NavLinkAcceptanceRadius, PathPointAcceptanceRadius) * 1.2f);

		const FVector CurrentLocation = SurfaceMovementComp ? SurfaceMovementComp->GetActorSurfaceLocation() : GetMoveCompFeetLocation();

		Path->GetPathPointLocation(MoveSegmentEndIndex);

		// We'll consider the pawn on the path if it's within the largest acceptance radius of either the segment's start or end point
		if (FVector::DistSquared(CurrentLocation, Path->GetPathPointLocation(MoveSegmentStartIndex).Position) < LargestAcceptanceRadius ||
			FVector::DistSquared(CurrentLocation, Path->GetPathPointLocation(MoveSegmentEndIndex).Position) < LargestAcceptanceRadius)
		{
			bOnPath = true;
		}
	}

	return bOnPath;
}

void USurfaceNavPathFollowingComponent::OnPathEvent(FSurfaceNavPath* InPath, ENavPathEvent::Type Event)
{
	const static UEnum* NavPathEventEnum = StaticEnum<ENavPathEvent::Type>();
	//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("OnPathEvent: %s"), *NavPathEventEnum->GetNameStringByValue(Event));

	if (InPath && Path.Get() == InPath)
	{
		switch (Event)
		{
		case ENavPathEvent::UpdatedDueToGoalMoved:
		case ENavPathEvent::UpdatedDueToNavigationChanged:
		case ENavPathEvent::MetaPathUpdate:
		{
			const bool bIsPathAccepted = HandlePathUpdateEvent();
			if (!bIsPathAccepted)
			{
				//UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT(">> updated path was not accepcted, aborting move"));
				OnSurfaceNavPathFinished(EPathFollowingResult::Aborted, FPathFollowingResultFlags::InvalidPath);
			}
			else if (InPath->IsPartial() && InPath->GetGoalActor())
			{
				float IgnoreGoalRadius, IgnoreGoalHalfHeight;
				//bRepathAccelBoost = true;
				OriginalMoveRequestGoalLocation = FindGoalLocation(*this, *InPath->GetGoalActor(), InPath->GetGoalActorAsNavAgent(), IgnoreGoalRadius, IgnoreGoalHalfHeight);
			}
		}
		break;

		default:
			break;
		}
	}
}

void USurfaceNavPathFollowingComponent::SetAcceptanceRadius(const float InAcceptanceRadius)
{
	AcceptanceRadius = InAcceptanceRadius;
	if (Path.IsValid() && Path->IsValid() && FAISystem::IsValidLocation(OriginalMoveRequestGoalLocation))
	{
		// figure out when we need to start doing precise end-condition tests
		PreciseAcceptanceRadiusCheckStartNodeIndex = FindPreciseAcceptanceRadiusTestsStartNodeIndex(*Path, OriginalMoveRequestGoalLocation);
	}
}

int32 USurfaceNavPathFollowingComponent::FindPreciseAcceptanceRadiusTestsStartNodeIndex(const FSurfaceNavPath& PathInstance, const FVector& GoalLocation) const
{
	const float DistanceFromEndOfPath = GetFinalAcceptanceRadius(PathInstance, GoalLocation);
	// @todo support based paths

	int32 TestStartIndex = MAX_int32;
	const TArray<FNavPathPoint>& PathPoints = PathInstance.GetPathPoints();

	if (PathPoints.Num() > 1)
	{
		float DistanceSum = 0.f;
		int32 NodeIndex = PathPoints.Num() - 1;
		FVector PrevLocation = PathPoints[NodeIndex].Location;
		--NodeIndex;
		for (; NodeIndex >= 0; --NodeIndex)
		{
			const FVector CurrentLocation = PathPoints[NodeIndex].Location;
			DistanceSum += FVector::Dist(CurrentLocation, PrevLocation);
			if (DistanceSum > DistanceFromEndOfPath)
			{
				TestStartIndex = NodeIndex;
				break;
			}

			PrevLocation = CurrentLocation;
			TestStartIndex = NodeIndex;
		}
	}

	return TestStartIndex;
}

FVector USurfaceNavPathFollowingComponent::GetDestinationActorMoveToLocation(FBox& DestLocalBox, bool& DestLocalBoxSet) const
{
	UNavMovementComponent* GoalMoveComp = DestinationActor->FindComponentByClass<UNavMovementComponent>();
	USurfaceNavMovementComponent* GoalSurfaceNavMoveComp = Cast<USurfaceNavMovementComponent>(GoalMoveComp);
	if (GoalSurfaceNavMoveComp)
	{
		return GoalSurfaceNavMoveComp->GetActorSurfaceLocation();
	}
	else if (GoalMoveComp)
	{
#if ENGINE_MAJOR_VERSION < 5
		FBoxSphereBounds UpdatedCompBounds = GoalMoveComp->UpdatedComponent->CalcLocalBounds();
#else
		FBoxSphereBounds UpdatedCompBounds = GoalMoveComp->UpdatedComponent->GetLocalBounds();
#endif
		return DestinationActor->GetActorRotation().RotateVector(FVector(0.f, 0.f, -UpdatedCompBounds.BoxExtent.Z)) + GoalMoveComp->UpdatedComponent->GetComponentLocation();
	}
	else if (DestinationActor->IsA<APawn>())
	{
		DestLocalBox = DestinationActor->CalculateComponentsBoundingBoxInLocalSpace();
		DestLocalBoxSet = true;
		float GoalActorHalfHeight = DestLocalBox.GetExtent().Z;
		return DestinationActor->GetActorRotation().RotateVector(FVector(0.f, 0.f, -GoalActorHalfHeight)) + DestinationActor->GetActorLocation();
	}
	else
	{
		if (DestinationAgent)
		{
			return DestinationActor->GetActorRotation().RotateVector(DestinationAgent->GetNavAgentLocation() - DestinationActor->GetActorLocation()) + DestinationActor->GetActorLocation();
		}
		else
		{
			return DestinationActor->GetActorLocation();
		}
	}
}

float USurfaceNavPathFollowingComponent::GetFinalAcceptanceRadius(const FSurfaceNavPath& PathInstance, const FVector OriginalGoalLocation, const FVector* PathEndOverride) const
{
	if (PathInstance.IsPartial())
	{
		ensure(FAISystem::IsValidLocation(OriginalGoalLocation));

		const float PathEndToGoalDistance = FVector::Dist(PathEndOverride ? *PathEndOverride : PathInstance.GetEndLocation(), OriginalGoalLocation);
		const float Remaining = AcceptanceRadius - PathEndToGoalDistance;

		// if goal is more than AcceptanceRadius from the end of the path
		// we just need to do regular path following, with default radius value
		return FMath::Max(Remaining, MyDefaultAcceptanceRadius);
	}

	return AcceptanceRadius;
}

int USurfaceNavPathFollowingComponent::GetCurrentTargetPointIndex() const
{
	return GetNextPathIndex();
}

void USurfaceNavPathFollowingComponent::GetPathData(bool& Success, TArray<FSurfaceNavPathPointData>& PathData) const
{
	if (Path.IsValid() && Path->IsValid())
	{
		Path->GetRawPathData(Success, PathData);
	}
}

void USurfaceNavPathFollowingComponent::GetPathPointDataAtIndex(int Index, bool& Success, FSurfaceNavPathPointData& PointData) const
{
	if (Path.IsValid() && Path->IsValid())
	{
		Path->GetRawPathPointData(Index, Success, PointData);
	}
}

void USurfaceNavPathFollowingComponent::GetCurrentTargetPathPointData(bool& Success, FSurfaceNavPathPointData& PointData) const
{
	// The current target index is the next path index, which is the end point of the current segment
	GetPathPointDataAtIndex(GetNextPathIndex(), Success, PointData);
}

void USurfaceNavPathFollowingComponent::GetPreviousTargetPathPointData(bool& Success, FSurfaceNavPathPointData& PointData) const
{
	// The previous target index is the current path index, which is the start point of the current segment
	GetPathPointDataAtIndex(GetCurrentPathIndex(), Success, PointData);
}

void USurfaceNavPathFollowingComponent::UserAbortMove()
{
	AbortMove(*this, FPathFollowingResultFlags::UserAbort);
}
