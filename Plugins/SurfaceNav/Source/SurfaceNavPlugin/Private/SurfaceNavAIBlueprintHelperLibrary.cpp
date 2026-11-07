// Copyright BMK Games LLC. All Rights Reserved.

//#include "SurfaceNavAIBlueprintHelperLibrary.h"
//
////#include "EngineGlobals.h"
////#include "TimerManager.h"
////#include "Engine/World.h"
////#include "Engine/Engine.h"
////#include "AITypes.h"
//#include "AISystem.h"
////#include "BrainComponent.h"
////#include "Navigation/PathFollowingComponent.h"
////#include "AIController.h"
////#include "BehaviorTree/BlackboardComponent.h"
////#include "Blueprint/AIAsyncTaskBlueprintProxy.h"
////#include "Animation/AnimInstance.h"
////#include "NavigationPath.h"
////#include "NavigationData.h"
////#include "NavigationSystem.h"
////#include "NavMesh/NavMeshPath.h"
////#include "Logging/MessageLog.h"
////#include "Blueprint/AIBlueprintHelperLibrary.h"
//
////#include "C:\Program Files\Epic Games\UE_5.0\Engine\Source\Runtime\AIModule\Private\Blueprint\AIBlueprintHelperLibrary.cpp";
//
//#include "SurfaceNavAIAsyncTaskBlueprintProxy.h"
//#include "SurfaceNavAIController.h"
//#include "SurfaceNavigationSystem.h"
//#include "SurfaceNavPathFollowingComponent.h"
//
//DEFINE_LOG_CATEGORY_STATIC(LogAIBlueprint, Warning, All);
//
//#define LOCTEXT_NAMESPACE "SurfaceNavAIBlueprintHelperLibrary"
//
//
////----------------------------------------------------------------------//
//// UAIAsyncTaskBlueprintProxy
////----------------------------------------------------------------------//
//
//USurfaceNavAIAsyncTaskBlueprintProxy::USurfaceNavAIAsyncTaskBlueprintProxy(const FObjectInitializer& ObjectInitializer)
//	: Super(ObjectInitializer)
//{
//	/*MyWorld = Cast<UWorld>(GetOuter());
//	if (HasAnyFlags(RF_ClassDefaultObject) == false)
//	{
//		SetFlags(RF_StrongRefOnFrame);
//		UAISystem* const AISystem = UAISystem::GetCurrentSafe(MyWorld.Get());
//		if (AISystem)
//		{
//			AISystem->AddReferenceFromProxyObject(this);
//		}
//	}*/
//}
//
//void USurfaceNavAIAsyncTaskBlueprintProxy::OnSurfaceNavMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type MovementResult)
//{
//	if (RequestID.IsEquivalent(MoveRequestId) && AIController.IsValid(true))
//	{
//		AIController->ReceiveMoveCompleted.RemoveDynamic(this, &USurfaceNavAIAsyncTaskBlueprintProxy::OnSurfaceNavMoveCompleted);
//
//		if (MovementResult == EPathFollowingResult::Success)
//		{
//			OnSuccess.Broadcast(MovementResult);
//		}
//		else
//		{
//			OnFail.Broadcast(MovementResult);
//		}
//
//		UAISystem* const AISystem = UAISystem::GetCurrentSafe(MyWorld.Get());
//		if (AISystem)
//		{
//			AISystem->RemoveReferenceToProxyObject(this);
//		}
//	}
//}
//
//void USurfaceNavAIAsyncTaskBlueprintProxy::OnSurfaceNavNoPath()
//{
//	OnFail.Broadcast(EPathFollowingResult::Aborted);
//	UAISystem* const AISystem = UAISystem::GetCurrentSafe(MyWorld.Get());
//	if (AISystem)
//	{
//		AISystem->RemoveReferenceToProxyObject(this);
//	}
//}
//
//void USurfaceNavAIAsyncTaskBlueprintProxy::OnSurfaceNavAtGoal()
//{
//	OnSuccess.Broadcast(EPathFollowingResult::Success);
//	UAISystem* const AISystem = UAISystem::GetCurrentSafe(MyWorld.Get());
//	if (AISystem)
//	{
//		AISystem->RemoveReferenceToProxyObject(this);
//	}
//}
//
//void USurfaceNavAIAsyncTaskBlueprintProxy::BeginDestroy()
//{
//	UAISystem* const AISystem = UAISystem::GetCurrentSafe(MyWorld.Get());
//	if (AISystem)
//	{
//		AISystem->RemoveReferenceToProxyObject(this);
//	}
//
//	// We can't call the parent BeginDestroy without causing an error, but
//	// we can call the parent's parent BeginDestroy...so let's just duplicate
//	// what's in the parent to handle that part.
//	Super::Super::BeginDestroy();
//}
//
////----------------------------------------------------------------------//
//// UAIAsyncTaskBlueprintProxy
////----------------------------------------------------------------------//
//
//
//USurfaceNavAIBlueprintHelperLibrary::USurfaceNavAIBlueprintHelperLibrary(const FObjectInitializer& ObjectInitializer)
//	: Super(ObjectInitializer)
//{
//}
//
//USurfaceNavAIAsyncTaskBlueprintProxy* USurfaceNavAIBlueprintHelperLibrary::CreateSurfaceNavMoveToProxyObject(UObject* WorldContextObject, APawn* Pawn, FVector Destination, AActor* TargetActor, float AcceptanceRadius, bool bStopOnOverlap)
//{
//	if (WorldContextObject == nullptr)
//	{
//		if (Pawn != nullptr)
//		{
//			WorldContextObject = Pawn;
//		}
//		else
//		{
//			UE_LOG(LogAIBlueprint, Warning, TEXT("Empty (None) world context as well as Pawn passed in while trying to create a Surface Nav MoveTo proxy"));
//			return nullptr;
//		}
//	}
//
//	if (Pawn == nullptr)
//	{
//		// maybe we can extract the pawn from the world context
//		AAIController* AsController = Cast<AAIController>(WorldContextObject);
//		if (AsController)
//		{
//			Pawn = AsController->GetPawn();
//		}
//	}
//
//	if (!Pawn)
//	{
//		return NULL;
//	}
//
//	USurfaceNavAIAsyncTaskBlueprintProxy* MyObj = NULL;
//	//AAIController* AIController = Cast<AAIController>(Pawn->GetController());
//	ASurfaceNavAIController* AIController = Cast<ASurfaceNavAIController>(Pawn->GetController());
//	if (AIController)
//	{
//		UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
//		MyObj = NewObject<USurfaceNavAIAsyncTaskBlueprintProxy>(World);
//
//		FAIMoveRequest MoveReq;
//		MoveReq.SetUsePathfinding(true);
//		MoveReq.SetAcceptanceRadius(AcceptanceRadius);
//		MoveReq.SetReachTestIncludesAgentRadius(bStopOnOverlap);
//		if (TargetActor)
//		{
//			MoveReq.SetGoalActor(TargetActor);
//		}
//		else
//		{
//			MoveReq.SetGoalLocation(Destination);
//		}
//		MoveReq.SetNavigationFilter(AIController->GetDefaultNavigationFilterClass());
//
//		//FPathFollowingRequestResult ResultData = AIController->MoveTo(MoveReq);
//		const FPathFollowingRequestResult ResultData = AIController->SurfaceNavMoveTo(MoveReq);
//
//		//MyObj->OnAtGoal();
//
//		switch (ResultData.Code)
//		{
//		case EPathFollowingRequestResult::RequestSuccessful:
//			MyObj->AIController = AIController;
//			MyObj->AIController->ReceiveMoveCompleted.AddDynamic(MyObj, &USurfaceNavAIAsyncTaskBlueprintProxy::OnSurfaceNavMoveCompleted);
//			MyObj->MoveRequestId = ResultData.MoveId;
//			break;
//
//		case EPathFollowingRequestResult::AlreadyAtGoal:
//			World->GetTimerManager().SetTimer(MyObj->TimerHandle_OnInstantFinish, MyObj, &USurfaceNavAIAsyncTaskBlueprintProxy::OnSurfaceNavAtGoal, 0.1f, false);
//			break;
//
//		case EPathFollowingRequestResult::Failed:
//		default:
//			World->GetTimerManager().SetTimer(MyObj->TimerHandle_OnInstantFinish, MyObj, &USurfaceNavAIAsyncTaskBlueprintProxy::OnSurfaceNavNoPath, 0.1f, false);
//			break;
//		}
//	}
//	return MyObj;
//}
//
//namespace
//{
//	USurfaceNavPathFollowingComponent* InitSurfaceNavControl(AController& Controller)
//	{
//		AAIController* AsAIController = Cast<AAIController>(&Controller);
//		USurfaceNavPathFollowingComponent* PathFollowingComp = nullptr;
//
//		if (AsAIController)
//		{
//			UPathFollowingComponent* TempPathFollowingComp = AsAIController->GetPathFollowingComponent();
//			PathFollowingComp = Cast<USurfaceNavPathFollowingComponent>(TempPathFollowingComp);
//		}
//		else
//		{
//			PathFollowingComp = Controller.FindComponentByClass<USurfaceNavPathFollowingComponent>();
//			if (PathFollowingComp == nullptr)
//			{
//				PathFollowingComp = NewObject<USurfaceNavPathFollowingComponent>(&Controller);
//				PathFollowingComp->RegisterComponentWithWorld(Controller.GetWorld());
//				PathFollowingComp->Initialize();
//			}
//		}
//
//		return PathFollowingComp;
//	}
//}
//
////void USurfaceNavAIBlueprintHelperLibrary::SurfaceNavSimpleMoveToActor(AController* Controller, const AActor* Goal)
////{
////	USurfaceNavigationSystem* NavSys = Controller ? FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(Controller->GetWorld()) : nullptr;
////	if (NavSys == nullptr || Goal == nullptr || Controller == nullptr || Controller->GetPawn() == nullptr)
////	{
////		UE_LOG(LogNavigation, Warning, TEXT("USurfaceNavigationSystem::SurfaceNavSimpleMoveToActor called for NavSys:%s Controller:%s controlling Pawn:%s with goal actor %s (if any of these is None then there's your problem"),
////			*GetNameSafe(NavSys), *GetNameSafe(Controller), Controller ? *GetNameSafe(Controller->GetPawn()) : TEXT("NULL"), *GetNameSafe(Goal));
////		return;
////	}
////
////	USurfaceNavPathFollowingComponent* PFollowComp = InitSurfaceNavControl(*Controller);
////
////	if (PFollowComp == nullptr)
////	{
////		FMessageLog("PIE").Warning(FText::Format(
////			LOCTEXT("SurfaceNavSimpleMoveErrorNoComp", "SurfaceNavSimpleMove failed for {0}: missing components"),
////			FText::FromName(Controller->GetFName())
////		));
////		return;
////	}
////
////	if (!PFollowComp->IsPathFollowingAllowed())
////	{
////		FMessageLog("PIE").Warning(FText::Format(
////			LOCTEXT("SurfaceNavSimpleMoveErrorMovement", "SurfaceNavSimpleMove failed for {0}: movement not allowed"),
////			FText::FromName(Controller->GetFName())
////		));
////		return;
////	}
////
////	const bool bAlreadyAtGoal = PFollowComp->HasReached(*Goal, EPathFollowingReachMode::OverlapAgentAndGoal);
////
////	// script source, keep only one move request at time
////	if (PFollowComp->GetStatus() != EPathFollowingStatus::Idle)
////	{
////		PFollowComp->AbortMove(*NavSys, FPathFollowingResultFlags::ForcedScript | FPathFollowingResultFlags::NewRequest
////			, FAIRequestID::AnyRequest, bAlreadyAtGoal ? EPathFollowingVelocityMode::Reset : EPathFollowingVelocityMode::Keep);
////	}
////
////	if (bAlreadyAtGoal)
////	{
////		PFollowComp->RequestMoveWithImmediateFinish(EPathFollowingResult::Success);
////	}
////	else
////	{
////		const FVector AgentNavLocation = Controller->GetNavAgentLocation();
////		//const ANavigationData* NavData = NavSys->GetNavDataForProps(Controller->GetNavAgentPropertiesRef(), AgentNavLocation);
////		/*if (NavData)
////		{*/
////		//FPathFindingQuery Query(Controller, *NavData, AgentNavLocation, Goal->GetActorLocation());
////		FSurfaceNavPathFindingQueryData Query(Controller, AgentNavLocation, Goal->GetActorLocation());
////
////		//FPathFindingResult Result = NavSys->FindPathSync(Query);
////		FSurfaceNavPathResult Result = NavSys->FindSurfacePathSync(Query);
////
////		if (Result.IsSuccessful())
////		{
////			Result.Path->SetGoalActorObservation(*Goal, 100.0f);
////			PFollowComp->SurfaceNavRequestMove(FAIMoveRequest(Goal), Result.Path);
////		}
////		else if (PFollowComp->GetStatus() != EPathFollowingStatus::Idle)
////		{
////			PFollowComp->RequestMoveWithImmediateFinish(EPathFollowingResult::Invalid);
////		}
////		//}
////	}
////}
////
////void USurfaceNavAIBlueprintHelperLibrary::SurfaceNavSimpleMoveToLocation(AController* Controller, const FVector& GoalLocation)
////{
////	USurfaceNavigationSystem* NavSys = Controller ? FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(Controller->GetWorld()) : nullptr;
////	if (NavSys == nullptr || Controller == nullptr || Controller->GetPawn() == nullptr)
////	{
////		UE_LOG(LogNavigation, Warning, TEXT("USurfaceNavigationSystem::SurfaceNavSimpleMoveToActor called for NavSys:%s Controller:%s controlling Pawn:%s (if any of these is None then there's your problem"),
////			*GetNameSafe(NavSys), *GetNameSafe(Controller), Controller ? *GetNameSafe(Controller->GetPawn()) : TEXT("NULL"));
////		return;
////	}
////
////	USurfaceNavPathFollowingComponent* PFollowComp = InitSurfaceNavControl(*Controller);
////
////	if (PFollowComp == nullptr)
////	{
////		FMessageLog("PIE").Warning(FText::Format(
////			LOCTEXT("SurfaceNavSimpleMoveErrorNoComp", "SurfaceNavSimpleMove failed for {0}: missing components"),
////			FText::FromName(Controller->GetFName())
////		));
////		return;
////	}
////
////	if (!PFollowComp->IsPathFollowingAllowed())
////	{
////		FMessageLog("PIE").Warning(FText::Format(
////			LOCTEXT("SurfaceNavSimpleMoveErrorMovement", "SurfaceNavSimpleMove failed for {0}: movement not allowed"),
////			FText::FromName(Controller->GetFName())
////		));
////		return;
////	}
////
////	const bool bAlreadyAtGoal = PFollowComp->HasReached(GoalLocation, EPathFollowingReachMode::OverlapAgent);
////
////	// script source, keep only one move request at time
////	if (PFollowComp->GetStatus() != EPathFollowingStatus::Idle)
////	{
////		PFollowComp->AbortMove(*NavSys, FPathFollowingResultFlags::ForcedScript | FPathFollowingResultFlags::NewRequest
////			, FAIRequestID::AnyRequest, bAlreadyAtGoal ? EPathFollowingVelocityMode::Reset : EPathFollowingVelocityMode::Keep);
////	}
////
////	// script source, keep only one move request at time
////	if (PFollowComp->GetStatus() != EPathFollowingStatus::Idle)
////	{
////		PFollowComp->AbortMove(*NavSys, FPathFollowingResultFlags::ForcedScript | FPathFollowingResultFlags::NewRequest);
////	}
////
////	if (bAlreadyAtGoal)
////	{
////		PFollowComp->RequestMoveWithImmediateFinish(EPathFollowingResult::Success);
////	}
////	else
////	{
////		const FVector AgentNavLocation = Controller->GetNavAgentLocation();
////		/*const ANavigationData* NavData = NavSys->GetNavDataForProps(Controller->GetNavAgentPropertiesRef(), AgentNavLocation);
////		if (NavData)
////		{*/
////			//FPathFindingQuery Query(Controller, *NavData, AgentNavLocation, GoalLocation);
////			FSurfaceNavPathFindingQueryData Query(Controller, AgentNavLocation, GoalLocation);
////
////			//FPathFindingResult Result = NavSys->FindPathSync(Query);
////			FSurfaceNavPathResult Result = NavSys->FindSurfacePathSync(Query);
////
////			if (Result.IsSuccessful())
////			{
////				//PFollowComp->RequestMove(FAIMoveRequest(GoalLocation), Result.Path);
////				PFollowComp->SurfaceNavRequestMove(FAIMoveRequest(GoalLocation), Result.Path);
////			}
////			else if (PFollowComp->GetStatus() != EPathFollowingStatus::Idle)
////			{
////				PFollowComp->RequestMoveWithImmediateFinish(EPathFollowingResult::Invalid);
////			}
////		//}
////	}
////}
//
//#undef LOCTEXT_NAMESPACE