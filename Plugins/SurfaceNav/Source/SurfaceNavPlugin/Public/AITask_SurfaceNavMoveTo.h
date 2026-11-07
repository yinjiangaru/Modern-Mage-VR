// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "AITypes.h"
#include "SurfaceNavPathFollowingComponent.h"
#include "Tasks/AITask.h"
#include "AITask_SurfaceNavMoveTo.generated.h"

class AAIController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSurfaceNavMoveTaskCompletedSignature, TEnumAsByte<EPathFollowingResult::Type>, Result, AAIController*, AIController);

UCLASS()
class UAITask_SurfaceNavMoveTo : public UAITask
{
	GENERATED_BODY()

public:
	UAITask_SurfaceNavMoveTo(const FObjectInitializer& ObjectInitializer);

	/** tries to start move request and handles retry timer */
	void ConditionalPerformMove();

	/** prepare move task for activation */
	void SetUp(AAIController* Controller, const FAIMoveRequest& InMoveRequest);

	EPathFollowingResult::Type GetMoveResult() const { return MoveResult; }
	bool WasMoveSuccessful() const { return MoveResult == EPathFollowingResult::Success; }
	bool WasMovePartial() const { return Path.IsValid() && Path->IsPartial(); }
	
	/** Makes AI go toward specified goal location or Goal actor (destination will be continuously updated), aborts any active path following. */
	UFUNCTION(BlueprintCallable, Category = "Surface Nav|AI|Tasks", meta = (AdvancedDisplay = "AcceptanceRadius,StopOnOverlap,AcceptPartialPath,bUsePathfinding,bUseContinuosGoalTracking,ProjectGoalOnNavigation", DefaultToSelf = "Controller", BlueprintInternalUseOnly = "TRUE", DisplayName = "Surface Nav Move To Location or Actor"))
		static UAITask_SurfaceNavMoveTo* AIMoveTo(ASurfaceNavAIController* Controller, FVector GoalLocation, AActor* GoalActor = nullptr,
			float AcceptanceRadius = -1.f, EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, EAIOptionFlag::Type AcceptPartialPath = EAIOptionFlag::Default,
			bool bUsePathfinding = true, bool bLockAILogic = true, bool bUseContinuosGoalTracking = false, EAIOptionFlag::Type ProjectGoalOnNavigation = EAIOptionFlag::Default);

	/** Makes AI go toward random location, aborts any active path following. */
	UFUNCTION(BlueprintCallable, Category = "Surface Nav|AI|Tasks", meta = (AdvancedDisplay = "AcceptanceRadius,StopOnOverlap,AcceptPartialPath,bUsePathfinding,bUseContinuosGoalTracking,ProjectGoalOnNavigation", DefaultToSelf = "Controller", BlueprintInternalUseOnly = "TRUE", DisplayName = "Surface Nav Move To Random Location"))
		static UAITask_SurfaceNavMoveTo* AIMoveToRandomLocation(ASurfaceNavAIController* Controller, const float SearchRadius = 1500.f, 
			const FVector TetherPoint = FVector::ZeroVector, const float AcceptanceRadius = -1.f,
			EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, EAIOptionFlag::Type AcceptPartialPath = EAIOptionFlag::Default,
			bool bUsePathfinding = true, bool bLockAILogic = true);


	/** Allows custom move request tweaking. Note that all MoveRequest need to
	 *	be performed before PerformMove is called. */
	FAIMoveRequest& GetMoveRequestRef() { return MoveRequest; }

	/** Switch task into continuous tracking mode: keep restarting move toward goal actor. Only pathfinding failure or external cancel will be able to stop this task. */
	void SetContinuousGoalTracking(bool bEnable);

protected:
	UPROPERTY(BlueprintAssignable)
		FGenericGameplayTaskDelegate OnRequestFailed;

	UPROPERTY(BlueprintAssignable)
		FSurfaceNavMoveTaskCompletedSignature OnMoveFinished;

	/** parameters of move request */
	UPROPERTY()
		FAIMoveRequest MoveRequest;

	/** handle of path following's OnMoveFinished delegate */
	FDelegateHandle PathFinishDelegateHandle;

	/** handle of path's update event delegate */
	FDelegateHandle PathUpdateDelegateHandle;

	/** handle of active ConditionalPerformMove timer  */
	FTimerHandle MoveRetryTimerHandle;

	/** handle of active ConditionalUpdatePath timer */
	FTimerHandle PathRetryTimerHandle;

	/** request ID of path following's request */
	FAIRequestID MoveRequestID;

	/** currently followed path */
	FSurfaceNavPathSharedPtr Path;

	TEnumAsByte<EPathFollowingResult::Type> MoveResult;
	uint8 bUseContinuousTracking : 1;
	/*,
		bIsRandomRequest : 1;

	FVector RandomTetherPoint;
	float RandomSearchDistance;*/

	virtual void Activate() override;
	virtual void OnDestroy(bool bOwnerFinished) override;

	virtual void Pause() override;
	virtual void Resume() override;

	/** finish task */
	void FinishMoveTask(EPathFollowingResult::Type InResult);

	/** stores path and starts observing its events */
	void SetObservedPath(FSurfaceNavPathSharedPtr InPath);

	/** remove all delegates */
	virtual void ResetObservers();

	/** remove all timers */
	virtual void ResetTimers();

	/** tries to update invalidated path and handles retry timer */
	void ConditionalUpdatePath();

	/** start move request */
	virtual void PerformMove();

	/** event from followed path */
	virtual void OnPathEvent(FSurfaceNavPath* InPath, ENavPathEvent::Type Event);

	/** event from path following */
	virtual void OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
};
