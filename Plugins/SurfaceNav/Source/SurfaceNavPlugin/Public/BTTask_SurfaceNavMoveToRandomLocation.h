// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AITypes.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_SurfaceNavMoveToRandomLocation.generated.h"

class UAITask_SurfaceNavMoveTo;
class UBlackboardComponent;

/**
 * Move To task node.
 * Moves the AI pawn toward a random location using the navigation system.
 */
UCLASS(config = Game)
class UBTTask_SurfaceNavMoveToRandomLocation : public UBTTask_BlackboardBase
{
	GENERATED_UCLASS_BODY()

	/** initialize any asset related data */
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

		/** fixed distance added to threshold between AI and goal location in destination reach test */
		UPROPERTY(config, Category = Node, EditAnywhere, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float AcceptableRadius;

	/** "None" will result in default filter being used */
	UPROPERTY(Category = Node, EditAnywhere)
		TSubclassOf<UNavigationQueryFilter> FilterClass;

	/** if task is expected to react to changes to location represented by BB key
	 *	this property can be used to tweak sensitivity of the mechanism. Value is
	 *	recommended to be less than AcceptableRadius */
	UPROPERTY(Category = Blackboard, EditAnywhere, meta = (ClampMin = "1", UIMin = "1", EditCondition = "bObserveBlackboardValue", DisplayAfter = "bObserveBlackboardValue"))
		float ObservedBlackboardValueTolerance;

	// Blackboard key selector
	UPROPERTY(Category = Node, EditAnywhere)
	FBlackboardKeySelector SearchRadiusKey;

	UPROPERTY(Category = Node, EditAnywhere)
	FBlackboardKeySelector TetherPointKey;

	/** if move goal in BB changes the move will be redirected to new location */
	UPROPERTY(Category = Blackboard, EditAnywhere)
		uint32 bObserveBlackboardValue : 1;

	UPROPERTY(Category = Node, EditAnywhere)
		uint32 bAllowStrafe : 1;


	///** if set, radius of AI's capsule will be added to threshold between AI and goal location in destination reach test  */
	/** if set, radius of AI's bounding sphere will be added to threshold between AI and goal location in destination reach test  */
	//UPROPERTY(Category = Node, EditAnywhere)
		uint32 bReachTestIncludesAgentRadius : 1;

	///** if set, radius of goal's capsule will be added to threshold between AI and goal location in destination reach test  */
	/** if set, radius of goal's bounding sphere will be added to threshold between AI and goal location in destination reach test  */
	//UPROPERTY(Category = Node, EditAnywhere)
		uint32 bReachTestIncludesGoalRadius : 1;

	/** DEPRECATED, please use combination of bReachTestIncludes*Radius instead */
	//UPROPERTY(Category = Node, VisibleInstanceOnly)
		uint32 bStopOnOverlap : 1;

	UPROPERTY()
		uint32 bStopOnOverlapNeedsUpdate : 1;

	/** if set, move will use pathfinding. Not exposed on purpose, please use BTTask_MoveDirectlyToward */
	uint32 bUsePathfinding : 1;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void PostLoad() override;

	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;
	virtual void OnMessage(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, FName Message, int32 RequestID, bool bSuccess) override;
	EBlackboardNotificationResult OnBlackboardValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID);

	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
	virtual void OnNodeCreated() override;
#endif // WITH_EDITOR

protected:

	virtual EBTNodeResult::Type PerformMoveTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);

	/** prepares move task for activation */
	virtual UAITask_SurfaceNavMoveTo* PrepareMoveTask(UBehaviorTreeComponent& OwnerComp, UAITask_SurfaceNavMoveTo* ExistingTask, FAIMoveRequest& MoveRequest);
};
