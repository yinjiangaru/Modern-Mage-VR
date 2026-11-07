// Copyright BMK Games LLC. All Rights Reserved.

#include "BTTask_SurfaceNavMoveToRandomLocation.h"
#include "BTTask_SurfaceNavMoveTo.h"
#include "GameFramework/Actor.h"
#include "AISystem.h"
#include "SurfaceNavPathFollowingComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "VisualLogger/VisualLogger.h"
#include "AIController.h"
#include "SurfaceNavAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AITask_SurfaceNavMoveTo.h"

UBTTask_SurfaceNavMoveToRandomLocation::UBTTask_SurfaceNavMoveToRandomLocation(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "SurfaceNav Move To Random Location";

#if ENGINE_MAJOR_VERSION == 5
	INIT_TASK_NODE_NOTIFY_FLAGS();
#else
	bNotifyTick = !GET_AI_CONFIG_VAR(bEnableBTAITasks);
	bNotifyTaskFinished = true;
#endif

	AcceptableRadius = GET_AI_CONFIG_VAR(AcceptanceRadius);
	bReachTestIncludesGoalRadius = bReachTestIncludesAgentRadius = bStopOnOverlap = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap);
	bAllowStrafe = GET_AI_CONFIG_VAR(bAllowStrafing);
	//bAllowPartialPath = GET_AI_CONFIG_VAR(bAcceptPartialPaths);
	//bTrackMovingGoal = true;
	//bProjectGoalLocation = true;
	bUsePathfinding = true;

	bStopOnOverlapNeedsUpdate = true;

	ObservedBlackboardValueTolerance = AcceptableRadius * 0.95f;

	// accept only actors and vectors
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SurfaceNavMoveToRandomLocation, BlackboardKey), AActor::StaticClass());
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SurfaceNavMoveToRandomLocation, BlackboardKey));

	// accept only float
	SearchRadiusKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SurfaceNavMoveToRandomLocation, SearchRadiusKey));

	// accept only actors and vectors
	TetherPointKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SurfaceNavMoveToRandomLocation, TetherPointKey), AActor::StaticClass());
	TetherPointKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SurfaceNavMoveToRandomLocation, TetherPointKey));

}

void UBTTask_SurfaceNavMoveToRandomLocation::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	UBlackboardData* BBAsset = GetBlackboardAsset();
	if (BBAsset)
	{
		SearchRadiusKey.ResolveSelectedKey(*BBAsset);
		TetherPointKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		UE_LOG(LogBehaviorTree, Warning, TEXT("Can't initialize task: %s, make sure that behavior tree specifies blackboard asset!"), *GetName());
	}
}

EBTNodeResult::Type UBTTask_SurfaceNavMoveToRandomLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	EBTNodeResult::Type NodeResult = EBTNodeResult::InProgress;

	FBTSurfaceNavMoveToTaskMemory* MyMemory = CastInstanceNodeMemory<FBTSurfaceNavMoveToTaskMemory>(NodeMemory);
	MyMemory->PreviousGoalLocation = FAISystem::InvalidLocation;
	MyMemory->MoveRequestID = FAIRequestID::InvalidRequest;

	AAIController* MyController = OwnerComp.GetAIOwner();
	if (MyController == nullptr)
	{
		UE_VLOG(OwnerComp.GetOwner(), LogBehaviorTree, Error, TEXT("UBTTask_SurfaceNavMoveToRandomLocation::ExecuteTask failed since AIController is missing."));
		NodeResult = EBTNodeResult::Failed;
	}
	else
	{
		NodeResult = PerformMoveTask(OwnerComp, NodeMemory);
	}

	if (NodeResult == EBTNodeResult::InProgress && bObserveBlackboardValue)
	{
		UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
		if (ensure(BlackboardComp))
		{
			if (MyMemory->BBObserverDelegateHandle.IsValid())
			{
				UE_VLOG(MyController, LogBehaviorTree, Warning, TEXT("UBTTask_SurfaceNavMoveToRandomLocation::ExecuteTask \'%s\' Old BBObserverDelegateHandle is still valid! Removing old Observer."), *GetNodeName());
				BlackboardComp->UnregisterObserver(BlackboardKey.GetSelectedKeyID(), MyMemory->BBObserverDelegateHandle);
			}
			MyMemory->BBObserverDelegateHandle = BlackboardComp->RegisterObserver(BlackboardKey.GetSelectedKeyID(), this, FOnBlackboardChangeNotification::CreateUObject(this, &UBTTask_SurfaceNavMoveToRandomLocation::OnBlackboardValueChange));
		}
	}

	return NodeResult;
}

EBTNodeResult::Type UBTTask_SurfaceNavMoveToRandomLocation::PerformMoveTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const UBlackboardComponent* MyBlackboard = OwnerComp.GetBlackboardComponent();
	FBTSurfaceNavMoveToTaskMemory* MyMemory = CastInstanceNodeMemory<FBTSurfaceNavMoveToTaskMemory>(NodeMemory);
	AAIController* MyController = OwnerComp.GetAIOwner();

	EBTNodeResult::Type NodeResult = EBTNodeResult::Failed;
	if (MyController && MyBlackboard)
	{
		FAIMoveRequest MoveReq;
		MoveReq.SetNavigationFilter(*FilterClass ? FilterClass : MyController->GetDefaultNavigationFilterClass());
		MoveReq.SetAllowPartialPath(true);
		MoveReq.SetAcceptanceRadius(AcceptableRadius);
		MoveReq.SetCanStrafe(bAllowStrafe);
		MoveReq.SetReachTestIncludesAgentRadius(bReachTestIncludesAgentRadius);
		MoveReq.SetReachTestIncludesGoalRadius(bReachTestIncludesGoalRadius);
		MoveReq.SetProjectGoalLocation(false);
		MoveReq.SetUsePathfinding(bUsePathfinding);

		FVector TetherPoint(0.f, 0.f, 0.f);
		ASurfaceNavAIController* SNController = Cast<ASurfaceNavAIController>(MyController);

		if (TetherPointKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
		{
			UObject* KeyValue = MyBlackboard->GetValue<UBlackboardKeyType_Object>(TetherPointKey.GetSelectedKeyID());
			AActor* TargetActor = Cast<AActor>(KeyValue);
			if (!TargetActor)
			{
				UE_LOG(LogTemp, Error, TEXT("UBTTask_SurfaceNavMoveToRandomLocation::PerformMoveTask: The provided Tether Point is not an actor or a vector."));
				return NodeResult;
			}

			TetherPoint = TargetActor->GetActorLocation();
		}
		else if (TetherPointKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
		{
			TetherPoint = MyBlackboard->GetValue<UBlackboardKeyType_Vector>(TetherPointKey.GetSelectedKeyID());
		}

		float SearchRadius = 3000.f;
		if (SearchRadiusKey.SelectedKeyType == UBlackboardKeyType_Float::StaticClass())
		{
			//SearchRadius = MyBlackboard->GetValueAsFloat(SearchRadiusKey.SelectedKeyName);
			SearchRadius = MyBlackboard->GetValue<UBlackboardKeyType_Float>(SearchRadiusKey.GetSelectedKeyID());
			SearchRadius = FMath::Clamp(SearchRadius, 100.f, 10000.f);
		}

		const FVector TargetLocation = SNController->GetRandomSurfaceNavPointNearby(SearchRadius, TetherPoint, SNController->GetDefaultNavigationFilterClass());
		MoveReq.SetGoalLocation(TargetLocation);

		MyMemory->PreviousGoalLocation = TargetLocation;

		if (MoveReq.IsValid())
		{
#if ENGINE_MAJOR_VERSION == 4
			if (GET_AI_CONFIG_VAR(bEnableBTAITasks))
			{
#endif
				UAITask_SurfaceNavMoveTo* MoveTask = MyMemory->Task.Get();
				const bool bReuseExistingTask = (MoveTask != nullptr);

				MoveTask = PrepareMoveTask(OwnerComp, MoveTask, MoveReq);
				if (MoveTask)
				{
					MyMemory->bObserverCanFinishTask = false;

					if (bReuseExistingTask)
					{
						if (MoveTask->IsActive())
						{
							UE_VLOG(MyController, LogBehaviorTree, Verbose, TEXT("\'%s\' reusing AITask %s"), *GetNodeName(), *MoveTask->GetName());
							MoveTask->ConditionalPerformMove();
						}
						else
						{
							UE_VLOG(MyController, LogBehaviorTree, Verbose, TEXT("\'%s\' reusing AITask %s, but task is not active - handing over move performing to task mechanics"), *GetNodeName(), *MoveTask->GetName());
						}
					}
					else
					{
						MyMemory->Task = MoveTask;
						UE_VLOG(MyController, LogBehaviorTree, Verbose, TEXT("\'%s\' task implementing move with task %s"), *GetNodeName(), *MoveTask->GetName());
						MoveTask->ReadyForActivation();
					}

					MyMemory->bObserverCanFinishTask = true;
					NodeResult = (MoveTask->GetState() != EGameplayTaskState::Finished) ? EBTNodeResult::InProgress :
						MoveTask->WasMoveSuccessful() ? EBTNodeResult::Succeeded :
						EBTNodeResult::Failed;
				}
#if ENGINE_MAJOR_VERSION == 4
			}
			else
			{
				//auto SNController = Cast<ASurfaceNavAIController>(MyController);
				const FPathFollowingRequestResult RequestResult = SNController->SurfaceNavMoveTo(MoveReq);
				if (RequestResult.Code == EPathFollowingRequestResult::RequestSuccessful)
				{
					MyMemory->MoveRequestID = RequestResult.MoveId;
					WaitForMessage(OwnerComp, UBrainComponent::AIMessage_MoveFinished, RequestResult.MoveId);
					WaitForMessage(OwnerComp, UBrainComponent::AIMessage_RepathFailed);

					NodeResult = EBTNodeResult::InProgress;
				}
				else if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
				{
					NodeResult = EBTNodeResult::Succeeded;
				}
			}
#endif
		}
	}

	return NodeResult;
}

UAITask_SurfaceNavMoveTo* UBTTask_SurfaceNavMoveToRandomLocation::PrepareMoveTask(UBehaviorTreeComponent& OwnerComp, UAITask_SurfaceNavMoveTo* ExistingTask, FAIMoveRequest& MoveRequest)
{
	UAITask_SurfaceNavMoveTo* MoveTask = ExistingTask ? ExistingTask : NewBTAITask<UAITask_SurfaceNavMoveTo>(OwnerComp);
	if (MoveTask)
	{
		MoveTask->SetUp(MoveTask->GetAIController(), MoveRequest);
	}

	return MoveTask;
}

EBlackboardNotificationResult UBTTask_SurfaceNavMoveToRandomLocation::OnBlackboardValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID)
{
	UBehaviorTreeComponent* BehaviorComp = Cast<UBehaviorTreeComponent>(Blackboard.GetBrainComponent());
	if (BehaviorComp == nullptr)
	{
		return EBlackboardNotificationResult::RemoveObserver;
}

	const AAIController* MyController = BehaviorComp->GetAIOwner();
	uint8* RawMemory = BehaviorComp->GetNodeMemory(this, BehaviorComp->FindInstanceContainingNode(this));
	FBTSurfaceNavMoveToTaskMemory* MyMemory = CastInstanceNodeMemory<FBTSurfaceNavMoveToTaskMemory>(RawMemory);

	const EBTTaskStatus::Type TaskStatus = BehaviorComp->GetTaskStatus(this);
	if (TaskStatus != EBTTaskStatus::Active)
	{
		UE_VLOG(MyController, LogBehaviorTree, Error, TEXT("BT Move To Random Location \'%s\' task observing BB entry while no longer being active!"), *GetNodeName());

		// resetting BBObserverDelegateHandle without unregistering observer since 
		// returning EBlackboardNotificationResult::RemoveObserver here will take care of that for us
		MyMemory->BBObserverDelegateHandle.Reset(); //-V595

		return EBlackboardNotificationResult::RemoveObserver;
	}

	// this means the move has already started. 
	if (MyMemory != nullptr && BehaviorComp->GetAIOwner() != nullptr)
	{
		check(BehaviorComp->GetAIOwner()->GetPathFollowingComponent());

		bool bUpdateMove = true;
		// check if new goal is almost identical to previous one
		if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
		{
			const FVector TargetLocation = Blackboard.GetValue<UBlackboardKeyType_Vector>(BlackboardKey.GetSelectedKeyID());

			bUpdateMove = (FVector::DistSquared(TargetLocation, MyMemory->PreviousGoalLocation) > FMath::Square(ObservedBlackboardValueTolerance));
		}

		if (bUpdateMove)
		{
			// don't abort move if using AI tasks - it will mess things up
			if (MyMemory->MoveRequestID.IsValid())
			{
				UE_VLOG(MyController, LogBehaviorTree, Log, TEXT("Blackboard value for goal has changed, aborting current move request"));
				StopWaitingForMessages(*BehaviorComp);
				BehaviorComp->GetAIOwner()->GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::NewRequest, MyMemory->MoveRequestID, EPathFollowingVelocityMode::Keep);
			}

			const EBTNodeResult::Type NodeResult = PerformMoveTask(*BehaviorComp, RawMemory);
			if (NodeResult != EBTNodeResult::InProgress)
			{
				FinishLatentTask(*BehaviorComp, NodeResult);
			}
		}
	}

	return EBlackboardNotificationResult::ContinueObserving;
}

EBTNodeResult::Type UBTTask_SurfaceNavMoveToRandomLocation::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTSurfaceNavMoveToTaskMemory* MyMemory = CastInstanceNodeMemory<FBTSurfaceNavMoveToTaskMemory>(NodeMemory);
	if (MyMemory->MoveRequestID.IsValid())
	{
		AAIController* MyController = OwnerComp.GetAIOwner();
		if (MyController && MyController->GetPathFollowingComponent())
		{
			MyController->GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MyMemory->MoveRequestID);
		}
	}
	else
	{
		MyMemory->bObserverCanFinishTask = false;
		UAITask_SurfaceNavMoveTo* MoveTask = MyMemory->Task.Get();
		if (MoveTask)
		{
			MoveTask->ExternalCancel();
		}
	}

	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UBTTask_SurfaceNavMoveToRandomLocation::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	FBTSurfaceNavMoveToTaskMemory* MyMemory = CastInstanceNodeMemory<FBTSurfaceNavMoveToTaskMemory>(NodeMemory);
	MyMemory->Task.Reset();

	if (bObserveBlackboardValue)
	{
		UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
		if (ensure(BlackboardComp) && MyMemory->BBObserverDelegateHandle.IsValid())
		{
			BlackboardComp->UnregisterObserver(BlackboardKey.GetSelectedKeyID(), MyMemory->BBObserverDelegateHandle);
		}

		MyMemory->BBObserverDelegateHandle.Reset();
	}

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_SurfaceNavMoveToRandomLocation::OnMessage(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, FName Message, int32 SenderID, bool bSuccess)
{
	// AIMessage_RepathFailed means task has failed
	bSuccess &= (Message != UBrainComponent::AIMessage_RepathFailed);
	Super::OnMessage(OwnerComp, NodeMemory, Message, SenderID, bSuccess);
}

void UBTTask_SurfaceNavMoveToRandomLocation::OnGameplayTaskDeactivated(UGameplayTask& Task)
{
	// AI move task finished
	UAITask_SurfaceNavMoveTo* MoveTask = Cast<UAITask_SurfaceNavMoveTo>(&Task);
	if (MoveTask && MoveTask->GetAIController() && MoveTask->GetState() != EGameplayTaskState::Paused)
	{
		UBehaviorTreeComponent* BehaviorComp = GetBTComponentForTask(Task);
		if (BehaviorComp)
		{
			uint8* RawMemory = BehaviorComp->GetNodeMemory(this, BehaviorComp->FindInstanceContainingNode(this));
			const FBTSurfaceNavMoveToTaskMemory* MyMemory = CastInstanceNodeMemory<FBTSurfaceNavMoveToTaskMemory>(RawMemory);

			if (MyMemory && MyMemory->bObserverCanFinishTask && (MoveTask == MyMemory->Task))
			{
				const bool bSuccess = MoveTask->WasMoveSuccessful();
				FinishLatentTask(*BehaviorComp, bSuccess ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
			}
		}
	}
}

FString UBTTask_SurfaceNavMoveToRandomLocation::GetStaticDescription() const
{
	FString KeyDesc("invalid");
	if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass() ||
		BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
	{
		KeyDesc = BlackboardKey.SelectedKeyName.ToString();
	}

	return FString::Printf(TEXT("%s: %s"), *Super::GetStaticDescription(), *KeyDesc);
}

void UBTTask_SurfaceNavMoveToRandomLocation::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (BlackboardComp)
	{
		const FString KeyValue = BlackboardComp->DescribeKeyValue(BlackboardKey.GetSelectedKeyID(), EBlackboardDescription::OnlyValue);

		const FBTSurfaceNavMoveToTaskMemory* MyMemory = CastInstanceNodeMemory<FBTSurfaceNavMoveToTaskMemory>(NodeMemory);
		const bool bIsUsingTask = MyMemory->Task.IsValid();

		const FString ModeDesc = bIsUsingTask ? TEXT("(task)") : TEXT("");

		Values.Add(FString::Printf(TEXT("move target: %s%s"), *KeyValue, *ModeDesc));
	}
}

uint16 UBTTask_SurfaceNavMoveToRandomLocation::GetInstanceMemorySize() const
{
	return sizeof(FBTSurfaceNavMoveToTaskMemory);
}

void UBTTask_SurfaceNavMoveToRandomLocation::PostLoad()
{
	Super::PostLoad();

	if (bStopOnOverlapNeedsUpdate)
	{
		bStopOnOverlapNeedsUpdate = false;
		bReachTestIncludesAgentRadius = bStopOnOverlap;
		bReachTestIncludesGoalRadius = false;
	}
}

#if WITH_EDITOR

FName UBTTask_SurfaceNavMoveToRandomLocation::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.MoveTo.Icon");
}

void UBTTask_SurfaceNavMoveToRandomLocation::OnNodeCreated()
{
	bStopOnOverlapNeedsUpdate = false;
}

#endif	// WITH_EDITOR
