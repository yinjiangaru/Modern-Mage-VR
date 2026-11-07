// Copyright BMK Games LLC. All Rights Reserved.

#include "AITask_SurfaceNavMoveTo.h"
#include "UObject/Package.h"
#include "TimerManager.h"
#include "AISystem.h"
#include "AIController.h"
#include "VisualLogger/VisualLogger.h"
#include "AIResources.h"
#include "GameplayTasksComponent.h"

#include "SurfaceNavAIController.h"
#include "SurfaceNavigationSystem.h"

UAITask_SurfaceNavMoveTo::UAITask_SurfaceNavMoveTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPausable = true;
	MoveRequestID = FAIRequestID::InvalidRequest;

	MoveRequest.SetAcceptanceRadius(GET_AI_CONFIG_VAR(AcceptanceRadius));
	MoveRequest.SetReachTestIncludesAgentRadius(GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap));
	MoveRequest.SetAllowPartialPath(GET_AI_CONFIG_VAR(bAcceptPartialPaths));
	MoveRequest.SetUsePathfinding(true);

	AddRequiredResource(UAIResource_Movement::StaticClass());
	AddClaimedResource(UAIResource_Movement::StaticClass());

	MoveResult = EPathFollowingResult::Invalid;
	bUseContinuousTracking = false;
}

UAITask_SurfaceNavMoveTo* UAITask_SurfaceNavMoveTo::AIMoveTo(ASurfaceNavAIController* Controller, FVector InGoalLocation, AActor* InGoalActor,
	float AcceptanceRadius, EAIOptionFlag::Type StopOnOverlap, EAIOptionFlag::Type AcceptPartialPath,
	bool bUsePathfinding, bool bLockAILogic, bool bUseContinuosGoalTracking, EAIOptionFlag::Type ProjectGoalOnNavigation)
{
	UAITask_SurfaceNavMoveTo* MyTask = Controller ? UAITask::NewAITask<UAITask_SurfaceNavMoveTo>(*Controller, EAITaskPriority::High) : nullptr;
	if (MyTask)
	{
		FAIMoveRequest MoveReq;
		if (InGoalActor)
		{
			MoveReq.SetGoalActor(InGoalActor);
		}
		else
		{
			MoveReq.SetGoalLocation(InGoalLocation);
		}

		MoveReq.SetAcceptanceRadius(AcceptanceRadius);
		MoveReq.SetReachTestIncludesAgentRadius(FAISystem::PickAIOption(StopOnOverlap, MoveReq.IsReachTestIncludingAgentRadius()));
		MoveReq.SetAllowPartialPath(FAISystem::PickAIOption(AcceptPartialPath, MoveReq.IsUsingPartialPaths()));
		MoveReq.SetUsePathfinding(bUsePathfinding);
		MoveReq.SetProjectGoalLocation(FAISystem::PickAIOption(ProjectGoalOnNavigation, MoveReq.IsProjectingGoal()));
		if (Controller)
		{
			MoveReq.SetNavigationFilter(Controller->GetDefaultNavigationFilterClass());
		}

		MyTask->SetUp(Controller, MoveReq);
		MyTask->SetContinuousGoalTracking(bUseContinuosGoalTracking);

		if (bLockAILogic)
		{
			MyTask->RequestAILogicLocking();
		}
	}

	return MyTask;
}

UAITask_SurfaceNavMoveTo* UAITask_SurfaceNavMoveTo::AIMoveToRandomLocation(ASurfaceNavAIController* Controller, const float SearchRadius,
	const FVector TetherPoint, float AcceptanceRadius, EAIOptionFlag::Type StopOnOverlap, 
	EAIOptionFlag::Type AcceptPartialPath, bool bUsePathfinding, bool bLockAILogic)
{
	UAITask_SurfaceNavMoveTo* MyTask = Controller ? UAITask::NewAITask<UAITask_SurfaceNavMoveTo>(*Controller, EAITaskPriority::High) : nullptr;
	if (MyTask)
	{
		FAIMoveRequest MoveReq;

		FVector GoalLocation = Controller->GetRandomSurfaceNavPointNearby(SearchRadius, TetherPoint, Controller->GetDefaultNavigationFilterClass());

		// Set goal location to tether point for now.
		// If something does go wrong, the pawn will at least stay near where it should
		MoveReq.SetGoalLocation(GoalLocation);

		MoveReq.SetAcceptanceRadius(AcceptanceRadius);
		MoveReq.SetReachTestIncludesAgentRadius(FAISystem::PickAIOption(StopOnOverlap, MoveReq.IsReachTestIncludingAgentRadius()));
		MoveReq.SetAllowPartialPath(true); // FAISystem::PickAIOption(AcceptPartialPath, MoveReq.IsUsingPartialPaths()));
		MoveReq.SetUsePathfinding(bUsePathfinding);
		MoveReq.SetProjectGoalLocation(FAISystem::PickAIOption(EAIOptionFlag::Default, MoveReq.IsProjectingGoal()));
		if (Controller)
		{
			MoveReq.SetNavigationFilter(Controller->GetDefaultNavigationFilterClass());
		}

		/*MyTask->bIsRandomRequest = true;
		MyTask->RandomSearchDistance = SearchRadius;
		MyTask->RandomTetherPoint = TetherPoint;*/
		MyTask->SetUp(Controller, MoveReq);
		MyTask->SetContinuousGoalTracking(false);

		if (bLockAILogic)
		{
			MyTask->RequestAILogicLocking();
		}
	}

	return MyTask;
}

void UAITask_SurfaceNavMoveTo::SetUp(AAIController* Controller, const FAIMoveRequest& InMoveRequest)
{
	OwnerController = Controller;
	MoveRequest = InMoveRequest;
}

void UAITask_SurfaceNavMoveTo::SetContinuousGoalTracking(bool bEnable)
{
	bUseContinuousTracking = bEnable;
}

void UAITask_SurfaceNavMoveTo::FinishMoveTask(EPathFollowingResult::Type InResult)
{
	if (MoveRequestID.IsValid())
	{
		UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PFComp && PFComp->GetStatus() != EPathFollowingStatus::Idle)
		{
			ResetObservers();
			PFComp->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
		}
	}

	MoveResult = InResult;
	EndTask();

	if (InResult == EPathFollowingResult::Invalid)
	{
		OnRequestFailed.Broadcast();
	}
	else
	{
		OnMoveFinished.Broadcast(InResult, OwnerController);
	}
}

void UAITask_SurfaceNavMoveTo::Activate()
{
	Super::Activate();

	//UE_CVLOG(bUseContinuousTracking, GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Continuous goal tracking requested, moving to: %s"),
	//	MoveRequest.IsMoveToActorRequest() ? TEXT("actor => looping successful moves!") : TEXT("location => will NOT loop"));

	MoveRequestID = FAIRequestID::InvalidRequest;
	ConditionalPerformMove();
}

void UAITask_SurfaceNavMoveTo::ConditionalPerformMove()
{
	if (MoveRequest.IsUsingPathfinding() && OwnerController && OwnerController->ShouldPostponePathUpdates())
	{
		//UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> can't path right now, waiting..."), *GetName());
		OwnerController->GetWorldTimerManager().SetTimer(MoveRetryTimerHandle, this, &UAITask_SurfaceNavMoveTo::ConditionalPerformMove, 0.2f, false);
	}
	else
	{
		MoveRetryTimerHandle.Invalidate();
		PerformMove();
	}
}

void UAITask_SurfaceNavMoveTo::PerformMove()
{
	UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
	if (PFComp == nullptr)
	{
		FinishMoveTask(EPathFollowingResult::Invalid);
		return;
	}

	ResetObservers();
	ResetTimers();

	// start new move request
	FSurfaceNavPathSharedPtr FollowedPath;
	auto SNController = Cast<ASurfaceNavAIController>(OwnerController);

	const FPathFollowingRequestResult ResultData = SNController->SurfaceNavMoveTo(MoveRequest, &FollowedPath);

	switch (ResultData.Code)
	{
	case EPathFollowingRequestResult::Failed:
		FinishMoveTask(EPathFollowingResult::Invalid);
		break;

	case EPathFollowingRequestResult::AlreadyAtGoal:
		MoveRequestID = ResultData.MoveId;
		OnRequestFinished(ResultData.MoveId, FPathFollowingResult(EPathFollowingResult::Success, FPathFollowingResultFlags::AlreadyAtGoal));
		break;

	case EPathFollowingRequestResult::RequestSuccessful:
		MoveRequestID = ResultData.MoveId;
		PathFinishDelegateHandle = PFComp->OnRequestFinished.AddUObject(this, &UAITask_SurfaceNavMoveTo::OnRequestFinished);
		SetObservedPath(FollowedPath);

		if (IsFinished())
		{
			//UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s> re-Activating Finished task!"), *GetName());
		}
		break;

	default:
		checkNoEntry();
		break;
	}
}

void UAITask_SurfaceNavMoveTo::Pause()
{
	if (OwnerController && MoveRequestID.IsValid())
	{
		OwnerController->PauseMove(MoveRequestID);
	}

	ResetTimers();
	Super::Pause();
}

void UAITask_SurfaceNavMoveTo::Resume()
{
	Super::Resume();

	if (!MoveRequestID.IsValid() || (OwnerController && !OwnerController->ResumeMove(MoveRequestID)))
	{
		//UE_CVLOG(MoveRequestID.IsValid(), GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Resume move failed, starting new one."), *GetName());
		ConditionalPerformMove();
	}
}

void UAITask_SurfaceNavMoveTo::SetObservedPath(FSurfaceNavPathSharedPtr InPath)
{
	if (PathUpdateDelegateHandle.IsValid() && Path.IsValid())
	{
		Path->RemoveObserver(PathUpdateDelegateHandle);
	}

	PathUpdateDelegateHandle.Reset();

	Path = InPath;
	if (Path.IsValid())
	{
		// disable auto repaths, it will be handled by move task to include ShouldPostponePathUpdates condition
		Path->EnableRecalculationOnInvalidation(false);
		PathUpdateDelegateHandle = Path->AddObserver(FSurfaceNavPath::FSurfaceNavPathObserverDelegate::FDelegate::CreateUObject(this, &UAITask_SurfaceNavMoveTo::OnPathEvent));
	}
}

void UAITask_SurfaceNavMoveTo::ResetObservers()
{
	if (Path.IsValid())
	{
		Path->DisableGoalActorObservation();
	}

	if (PathFinishDelegateHandle.IsValid())
	{
		UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PFComp)
		{
			PFComp->OnRequestFinished.Remove(PathFinishDelegateHandle);
		}

		PathFinishDelegateHandle.Reset();
	}

	if (PathUpdateDelegateHandle.IsValid())
	{
		if (Path.IsValid())
		{
			Path->RemoveObserver(PathUpdateDelegateHandle);
		}

		PathUpdateDelegateHandle.Reset();
	}
}

void UAITask_SurfaceNavMoveTo::ResetTimers()
{
	if (OwnerController)
	{
		// Remove all timers including the ones that might have been set with SetTimerForNextTick 
		OwnerController->GetWorldTimerManager().ClearAllTimersForObject(this);
	}
	MoveRetryTimerHandle.Invalidate();
	PathRetryTimerHandle.Invalidate();
}

void UAITask_SurfaceNavMoveTo::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);

	ResetObservers();
	ResetTimers();

	if (MoveRequestID.IsValid())
	{
		UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PFComp && PFComp->GetStatus() != EPathFollowingStatus::Idle)
		{
			PFComp->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
		}
	}

	// clear the shared pointer now to make sure other systems
	// don't think this path is still being used
	Path = nullptr;
}

void UAITask_SurfaceNavMoveTo::OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (RequestID == MoveRequestID)
	{
		if (Result.HasFlag(FPathFollowingResultFlags::UserAbort) && Result.HasFlag(FPathFollowingResultFlags::NewRequest) && !Result.HasFlag(FPathFollowingResultFlags::ForcedScript))
		{
			//UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> ignoring OnRequestFinished, move was aborted by new request"), *GetName());
		}
		else
		{
			// reset request Id, FinishMoveTask doesn't need to update path following's state
			MoveRequestID = FAIRequestID::InvalidRequest;

			if (bUseContinuousTracking && MoveRequest.IsMoveToActorRequest() && Result.IsSuccess())
			{
				//UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> received OnRequestFinished and goal tracking is active! Moving again in next tick"), *GetName());
				GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAITask_SurfaceNavMoveTo::PerformMove);
			}
			else
			{
				FinishMoveTask(Result.Code);
			}
		}
	}
	else if (IsActive())
	{
		//UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Warning, TEXT("%s> received OnRequestFinished with not matching RequestID!"), *GetName());
	}
}

void UAITask_SurfaceNavMoveTo::OnPathEvent(FSurfaceNavPath* InPath, ENavPathEvent::Type Event)
{
	const static UEnum* NavPathEventEnum = StaticEnum<ENavPathEvent::Type>();
	//UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Path event: %s"), *GetName(), *NavPathEventEnum->GetNameStringByValue(Event));

	//UE_LOG(LogTemp, Warning, TEXT("Path event: %s"), *NavPathEventEnum->GetNameStringByValue(Event));

	switch (Event)
	{
	case ENavPathEvent::NewPath:
	case ENavPathEvent::UpdatedDueToGoalMoved:
	case ENavPathEvent::UpdatedDueToNavigationChanged:
		if (InPath && InPath->IsPartial() && !MoveRequest.IsUsingPartialPaths())
		{
			//UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> partial path is not allowed, aborting"));
			//UPathFollowingComponent::LogPathHelper(OwnerController, InPath, MoveRequest.GetGoalActor());
			FinishMoveTask(EPathFollowingResult::Aborted);
		}
//#if ENABLE_VISUAL_LOG
//		else if (!IsActive())
//		{
//			UPathFollowingComponent::LogPathHelper(OwnerController, InPath, MoveRequest.GetGoalActor());
//		}
//#endif // ENABLE_VISUAL_LOG
		break;

	case ENavPathEvent::Invalidated:
		ConditionalUpdatePath();
		break;

	case ENavPathEvent::Cleared:
	case ENavPathEvent::RePathFailed:
		//UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> no path, aborting!"));
		FinishMoveTask(EPathFollowingResult::Aborted);
		break;

	case ENavPathEvent::MetaPathUpdate:
	default:
		break;
	}
}

void UAITask_SurfaceNavMoveTo::ConditionalUpdatePath()
{
	// mark this path as waiting for repath so that PathFollowingComponent doesn't abort the move while we 
	// micro manage repathing moment
	// note that this flag fill get cleared upon repathing end
	if (Path.IsValid())
	{
		Path->SetManualRepathWaiting(true);
	}

	if (MoveRequest.IsUsingPathfinding() && OwnerController && OwnerController->ShouldPostponePathUpdates())
	{
		//UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> can't path right now, waiting..."), *GetName());
		OwnerController->GetWorldTimerManager().SetTimer(PathRetryTimerHandle, this, &UAITask_SurfaceNavMoveTo::ConditionalUpdatePath, 0.2f, false);
	}
	else
	{
		PathRetryTimerHandle.Invalidate();

		USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

		if (NavSys)
		{
			NavSys->RequestRePath(Path, ENavPathUpdateType::NavigationChanged);
		}
		else
		{
			FinishMoveTask(EPathFollowingResult::Aborted);
		}

		//ANavigationData* NavData = Path.IsValid() ? Path->GetNavigationDataUsed() : nullptr;
		//if (NavData)
		//{
		//	NavData->RequestRePath(Path, ENavPathUpdateType::NavigationChanged);
		//}
		//else
		//{
		//	//UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> unable to repath, aborting!"), *GetName());
		//	FinishMoveTask(EPathFollowingResult::Aborted);
		//}
	}
}
