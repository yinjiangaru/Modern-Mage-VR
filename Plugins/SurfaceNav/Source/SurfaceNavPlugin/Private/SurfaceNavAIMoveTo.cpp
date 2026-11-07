// Copyright BMK Games LLC. All Rights Reserved.


#include "SurfaceNavAIMoveTo.h"
#include "AIController.h"
#include "Async/Async.h"
//#include "Engine.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "SurfaceNavAIController.h"
#include "SurfaceNavigationSystem.h"

//#include "Tasks/Task.h"
#include "Tasks/AITask.h"
#include "AITask_SurfaceNavMoveTo.h"
#include "GameplayTasksComponent.h"

//#include "SurfaceNavAIBlueprintHelperLibrary.h"
//#include "SurfaceNavAIAsyncTaskBlueprintProxy.h"



USurfaceNavAIMoveTo::USurfaceNavAIMoveTo(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
WorldContextObject(nullptr), Pawn(nullptr), Destination(FVector::ZeroVector), TargetActor(nullptr), AcceptanceRadius(0.f),
bStopOnOverlap(false)
{
}

USurfaceNavAIMoveTo* USurfaceNavAIMoveTo::SurfaceNavAIMoveTo(const UObject* WorldContextObject, const APawn* Pawn,
	const FVector Destination, const AActor* TargetActor, const float AcceptanceRadius,
	const TSubclassOf<UNavigationQueryFilter> FilterClass) //, const bool StopOnOverlap = false)
{
	USurfaceNavAIMoveTo* BlueprintNode = NewObject<USurfaceNavAIMoveTo>();
	BlueprintNode->WorldContextObject = WorldContextObject;
	BlueprintNode->Pawn = Pawn;
	BlueprintNode->Destination = Destination;
	BlueprintNode->TargetActor = TargetActor;
	BlueprintNode->AcceptanceRadius = AcceptanceRadius;
	BlueprintNode->FilterClass = FilterClass;
	//BlueprintNode->bCanStrafe = bCanStrafe;
	//BlueprintNode->bAllowPartialPath = bAllowPartialPath;
	//BlueprintNode->bStopOnOverlap = StopOnOverlap;

	return BlueprintNode;
}

USurfaceNavAIMoveTo* USurfaceNavAIMoveTo::SurfaceNavAIMoveToRandomLocation(const UObject* WorldContextObject, const APawn* Pawn,
	const float SearchRadius, const FVector TetherPoint,
	const float AcceptanceRadius, const TSubclassOf<UNavigationQueryFilter> FilterClass) //, const bool StopOnOverlap = false)
{
	USurfaceNavAIMoveTo* BlueprintNode = NewObject<USurfaceNavAIMoveTo>();

	const ASurfaceNavAIController* Controller = nullptr;

	if (Pawn && Pawn->GetController()->IsA<ASurfaceNavAIController>())
		Controller = Cast<ASurfaceNavAIController>(Pawn->GetController());
	else if (WorldContextObject->IsA<ASurfaceNavAIController>())
		Controller = Cast<const ASurfaceNavAIController>(WorldContextObject);

	//ASurfaceNavAIController* Controller = Cast<ASurfaceNavAIController>(Pawn->GetController());

	if (!Controller)
		return BlueprintNode;

	BlueprintNode->WorldContextObject = WorldContextObject;
	BlueprintNode->Pawn = Pawn;
	BlueprintNode->Destination = Controller->GetRandomSurfaceNavPointNearby(SearchRadius, TetherPoint, Controller->GetDefaultNavigationFilterClass());
	BlueprintNode->AcceptanceRadius = AcceptanceRadius;
	BlueprintNode->FilterClass = FilterClass;
	//BlueprintNode->bCanStrafe = bCanStrafe;
	BlueprintNode->bAllowPartialPath = true;
	//BlueprintNode->bStopOnOverlap = StopOnOverlap;

	return BlueprintNode;
}

void USurfaceNavAIMoveTo::Activate()
{
	// Any safety checks should be performed here. Check here validity of all your pointers etc.
	// You can log any errors using FFrame::KismetExecutionMessage, like that:
	// FFrame::KismetExecutionMessage(TEXT("Valid Player Controller reference is needed for ... to start!"), ELogVerbosity::Error);

	if (WorldContextObject == nullptr)
	{
		if (Pawn != nullptr)
		{
			WorldContextObject = Pawn;
		}
		else
		{
			//UE_LOG(LogAIBlueprint, Warning, TEXT("Empty (None) world context as well as Pawn passed in while trying to create a MoveTo proxy"));
			OnFail.Broadcast(EPathFollowingResult::Aborted);
			return;
		}
	}

	if (Pawn == nullptr)
	{
		// maybe we can extract the pawn from the world context
		const AAIController* AsController = Cast<const AAIController>(WorldContextObject);
		if (AsController)
		{
			Pawn = AsController->GetPawn();
		}
	}

	if (!Pawn)
	{
		OnFail.Broadcast(EPathFollowingResult::Aborted);
		return;
	}

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(Pawn->GetWorld());

	if (!NavSys || !NavSys->CanAcceptPathingRequest())
	{
		OnFail.Broadcast(EPathFollowingResult::Aborted);
		return;
	}

	ASurfaceNavAIController* SurfaceNavAIController = Cast<ASurfaceNavAIController>(Pawn->GetController());
	if (SurfaceNavAIController)
	{
		AsyncTask(ENamedThreads::GameThread, [this, SurfaceNavAIController]()
			{

				//UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);

				FAIMoveRequest MoveReq;
				MoveReq.SetUsePathfinding(true);
				MoveReq.SetAcceptanceRadius(AcceptanceRadius);
				MoveReq.SetReachTestIncludesAgentRadius(bStopOnOverlap);
				MoveReq.SetAllowPartialPath(bAllowPartialPath);
				if (TargetActor)
				{
					MoveReq.SetGoalActor(TargetActor);
				}
				else
				{
					MoveReq.SetGoalLocation(Destination);
				}
				MoveReq.SetNavigationFilter(*FilterClass ? FilterClass : SurfaceNavAIController->GetDefaultNavigationFilterClass());

				FPathFollowingRequestResult ResultData = SurfaceNavAIController->SurfaceNavMoveTo(MoveReq);

				switch (ResultData.Code)
				{
				case EPathFollowingRequestResult::RequestSuccessful:
					SurfaceNavAIController->ReceiveMoveCompleted.AddDynamic(this, &USurfaceNavAIMoveTo::OnSurfaceNavMoveCompleted);
					break;

				case EPathFollowingRequestResult::AlreadyAtGoal:
					OnSuccess.Broadcast(EPathFollowingResult::Success);
					break;

				case EPathFollowingRequestResult::Failed:
				default:
					OnFail.Broadcast(EPathFollowingResult::Aborted);
					break;
				}
			});

	}
}

void USurfaceNavAIMoveTo::OnSurfaceNavMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type MovementResult)
{
	ASurfaceNavAIController* SurfaceNavAIController = Cast<ASurfaceNavAIController>(Pawn->GetController());
	if (SurfaceNavAIController)
		SurfaceNavAIController->ReceiveMoveCompleted.RemoveDynamic(this, &USurfaceNavAIMoveTo::OnSurfaceNavMoveCompleted);

	if (MovementResult == EPathFollowingResult::Success)
	{
		OnSuccess.Broadcast(MovementResult);
	}
	else
	{
		OnFail.Broadcast(MovementResult);
	}
}
