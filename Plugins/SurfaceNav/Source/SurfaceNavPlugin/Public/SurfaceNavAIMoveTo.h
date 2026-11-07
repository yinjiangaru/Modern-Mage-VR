// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"

//#include "CoreMinimal.h"
//#include "UObject/ObjectMacros.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameplayTaskOwnerInterface.h"
//#include "K2Node_BaseAsyncTask.h"
#include "SurfaceNavAIMoveTo.generated.h"

SURFACENAVPLUGIN_API DECLARE_LOG_CATEGORY_EXTERN(LogSurfaceNavMoveTo, Display, All);


class UAITask_SurfaceNavMoveTo;

struct FSurfaceNavMoveToTaskMemory
{
	/** Move request ID */
	FAIRequestID MoveRequestID;

	FDelegateHandle ObserverDelegateHandle;
	FVector PreviousGoalLocation;

	TWeakObjectPtr<UAITask_SurfaceNavMoveTo> Task;

	uint8 bObserverCanFinishTask : 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSurfaceNavAIMoveResultOutputPin, EPathFollowingResult::Type, MovementResult); // , float, InputFloatPlusOne, float, InputFloatPlusTwo);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSurfaceNavAIMoveResultOutputPin); // , float, InputFloatPlusOne, float, InputFloatPlusTwo);

UCLASS()
class SURFACENAVPLUGIN_API USurfaceNavAIMoveTo : public UBlueprintAsyncActionBase
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FSurfaceNavAIMoveResultOutputPin OnSuccess;
	UPROPERTY(BlueprintAssignable)
	FSurfaceNavAIMoveResultOutputPin OnFail;

	/** Makes AI go toward specified Goal actor (destination will be continuously updated) or location, aborts any active path following
	 *  @param Pawn - The pawn being moved
	 *  @param Destination - The location to move towards, ignored is TargetActor is set
	 *  @param TargetActor - The goal actor to move towards, if set then Destination is ignored
	 *  @param AcceptanceRadius - finish move if pawn gets close enough
	 *  @param FilterClass - navigation filter for pathfinding adjustments. If none specified DefaultNavigationFilterClass will be used
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"),
		DisplayName = "Surface Nav AI MoveTo", Category = "Surface Nav|AI")
	static USurfaceNavAIMoveTo* SurfaceNavAIMoveTo(const UObject* WorldContextObject, const APawn* Pawn = nullptr,
		const FVector Destination = FVector::ZeroVector, const AActor* TargetActor = nullptr, const float AcceptanceRadius = 5.f,
		const TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Makes AI go toward random location, aborts any active path following
	 *  @param Pawn - The pawn being moved
	 *  @param SearchRadius - How far out to find a random location.
	 *  @param TetherPoint - The center point to use for the radius, if not provided then the pawn's location is used.
	 *  @param AcceptanceRadius - finish move if pawn gets close enough
	 *  @param FilterClass - navigation filter for pathfinding adjustments. If none specified DefaultNavigationFilterClass will be used
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"),
		DisplayName = "Surface Nav AI Move to Random Location", Category = "Surface Nav|AI")
	static USurfaceNavAIMoveTo* SurfaceNavAIMoveToRandomLocation(const UObject* WorldContextObject, const APawn* Pawn = nullptr, 
		const float SearchRadius = 1500.f, const FVector TetherPoint = FVector::ZeroVector,
		const float AcceptanceRadius = 5.f, const TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;
	//~UBlueprintAsyncActionBase interface

	UFUNCTION()
	void OnSurfaceNavMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type MovementResult);

private:

private:
	const UObject* WorldContextObject;

	const APawn* Pawn;

	FVector Destination;

	const AActor* TargetActor;

	float AcceptanceRadius;
	bool bStopOnOverlap;
	bool bAllowPartialPath;
	TSubclassOf<UNavigationQueryFilter> FilterClass;


};
