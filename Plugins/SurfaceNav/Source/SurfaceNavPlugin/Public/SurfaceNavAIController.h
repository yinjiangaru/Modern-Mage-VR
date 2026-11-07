// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "EngineDefines.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AITypes.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavigationSystemTypes.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionListenerInterface.h"
#include "GameFramework/Actor.h"
#include "AIController.h"

#include "SurfaceNavCommon.h"
#include "SurfaceNavPath.h"
#include "SurfaceNavAIController.generated.h"


class USurfaceNavPathFollowingComponent;
class UAIPerceptionComponent;
class UBehaviorTree;
class UBlackboardComponent;
class UBlackboardData;
class UBrainComponent;
class UCanvas;
class UPawnAction;
class UPawnActionsComponent;



/** Actor which traces for world objects with collisions */
UCLASS(ClassGroup = AI, BlueprintType, Blueprintable, Abstract)
class SURFACENAVPLUGIN_API ASurfaceNavAIController :
	public AAIController	
{
	GENERATED_BODY()

public:

	ASurfaceNavAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	
	/** Gets a random location within the provided radius. While the name implies that the returned location will be an 
	 * actual DSN nav point, it will simply be a random location in the world using the radius and tether if provided. 
	 * The surface nav move to functions will take this location and find an actual nearby nav point to navigate to. 
	 * The reason for this function not returning a nav point is because it's an expensive process to do once for a 
	 * random point then once again for the move to so this was a design decision for the sake of performance. 
	 * @param SearchRadius - How far out to find a random location.
	 * @param TetherPoint - The center point to use for the radius, if not provided then the pawn's location is used.
	 * @param FilterClass - Not currently used as finding actual nav point was too expensive at this time.
	 * @return A random location based on radius and tether point, if tether point not provided then pawn's location
	 */
	UFUNCTION(BlueprintCallable, Category = "Surface Nav Controller|AI")
		FVector GetRandomSurfaceNavPointNearby(float SearchRadius = 1500.f, const FVector TetherPoint = FVector::ZeroVector, 
			TSubclassOf<UNavigationQueryFilter> FilterClass = NULL) const;

	/** Gets a random reachable surface nav point location within the provided radius. This is a somewhat expensive
	 * task since the system must perform a pathing operation to find a reachable point, but this may be needed
	 * in some cases.
	 * @param SearchRadius - How far out to find a random location.
	 * @param TetherPoint - The center point to use for the radius, if not provided then the pawn's location is used.
	 * @param FilterClass - Not currently used as finding actual nav point was too expensive at this time.
	 * @return A random location based on radius and tether point, if tether point not provided then pawn's location
	 */
	UFUNCTION(BlueprintCallable, Category = "Surface Nav Controller|AI")
		FVector GetRandomReachableSurfaceNavPointNearby(float SearchRadius = 1500.f, const FVector TetherPoint = FVector::ZeroVector,
			TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Finds closest planet to the controlled pawn and return the distance between the two.
	 * @param OutPlanetActor - Closest planet to pawn found.
	 * @return Distance between planet and pawn.
	 */
	UFUNCTION(BlueprintCallable, Category = "Surface Nav Controller|AI")
		float GetClosestPlanetActor(AActor*& OutPlanetActor);

	/** Get the current planet for this pawn. Will do nothing if IsFlatWorld is enabled.
	 * If the planet is not already set, it will use the closest planet.
	 * @param OutPlanetActor - Will be set to the current planet for this pawn if set, otherwise will use closest planet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Surface Nav Controller|AI")
		virtual void GetCurrentPlanet(AActor*& OutPlanetActor) 
	{ 
		if (SurfaceNavProxySettings::IsFlatWorld)
			return;

		if (CurrentPlanet.IsValid())
		{
			OutPlanetActor = CurrentPlanet.Get();
			return;
		}

		// If we're not on a flat world and the current planet is not set
		// then we need to determine the closest planet.
		GetClosestPlanetActor(OutPlanetActor);
	}

	/** Set the current planet for this pawn.
	 * @param InPlanetActor - The current planet to set for this pawn.
	 */
	UFUNCTION(BlueprintCallable, Category = "Surface Nav Controller|AI")
		virtual void SetCurrentPlanet(AActor* InPlanetActor) 
	{ 
		if (InPlanetActor)
		{
			// TODO: Need to make sure actor has planet component

			CurrentPlanet = InPlanetActor;
		
			return;
		}

		CurrentPlanet = nullptr;
	}

	/** Makes AI go toward specified Goal actor (destination will be continuously updated), aborts any active path following
	 *  @param Goal - The goal actor to move towards
	 *  @param AcceptanceRadius - finish move if pawn gets close enough
	 *  @param bCanStrafe - set focus related flag: bAllowStrafe
	 *  @param FilterClass - navigation filter for pathfinding adjustments. If none specified DefaultNavigationFilterClass will be used
	 *  @param bAllowPartialPath - use incomplete path when goal can't be reached
	 *  @note AcceptanceRadius has default value of -1 due to Header Parser not being able to recognize UPathFollowingComponent::DefaultAcceptanceRadius
	 */
	UFUNCTION(BlueprintCallable, Category = "Surface Nav Controller|AI", Meta = (AdvancedDisplay = "bCanStrafe,bAllowPartialPath"))
		EPathFollowingRequestResult::Type SurfaceNavMoveToActor(AActor* Goal, float AcceptanceRadius = -1, bool bCanStrafe = true,
			TSubclassOf<UNavigationQueryFilter> FilterClass = NULL, bool bAllowPartialPath = true);

	/** Makes AI go toward specified Dest location, aborts any active path following
	 *  @param Dest - The goal location to move towards
	 *  @param AcceptanceRadius - finish move if pawn gets close enough
	 *  @param bCanStrafe - set focus related flag: bAllowStrafe
	 *  @param FilterClass - navigation filter for pathfinding adjustments. If none specified DefaultNavigationFilterClass will be used
	 *  @param bAllowPartialPath - use incomplete path when goal can't be reached
	 *	@note AcceptanceRadius has default value of -1 due to Header Parser not being able to recognize UPathFollowingComponent::DefaultAcceptanceRadius
	 */
	UFUNCTION(BlueprintCallable, Category = "Surface Nav Controller|AI", Meta = (AdvancedDisplay = "bCanStrafe,bAllowPartialPath"))
		EPathFollowingRequestResult::Type SurfaceNavMoveToLocation(const FVector& Dest, float AcceptanceRadius = -1, bool bCanStrafe = true,
			TSubclassOf<UNavigationQueryFilter> FilterClass = NULL, bool bAllowPartialPath = true);

	/** Makes AI go toward random location, aborts any active path following
	*  @param SearchRadius - How far out to find a random location.
	*  @param TetherPoint - The center point to use for the radius, if not provided then the pawn's location is used.
	*  @param AcceptanceRadius - finish move if pawn gets close enough
	*  @param FilterClass - navigation filter for pathfinding adjustments. If none specified DefaultNavigationFilterClass will be used
	*  @note AcceptanceRadius has default value of -1 due to Header Parser not being able to recognize UPathFollowingComponent::DefaultAcceptanceRadius
	*/
	UFUNCTION(BlueprintCallable, Category = "Surface Nav Controller|AI", Meta = (AdvancedDisplay = "bCanStrafe,bAllowPartialPath"))
	EPathFollowingRequestResult::Type SurfaceNavMoveToRandomLocation(const float SearchRadius = 1500.f,
		const FVector TetherPoint = FVector::ZeroVector, float AcceptanceRadius = -1, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Makes AI go toward specified destination
	 *  @param MoveRequest - details about move
	 *  @param OutPath - optional output param, filled in with assigned path
	 *  @return struct holding MoveId and enum code
	 */
	virtual FPathFollowingRequestResult SurfaceNavMoveTo(const FAIMoveRequest& MoveRequest, FSurfaceNavPathSharedPtr* OutPath = nullptr);
	
	/** Finds path for given move request
	 *  @param MoveRequest - details about move
	 *  @param OutPath - generated path
	 */
	virtual void SurfaceNavFindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FSurfaceNavPathFindingQueryData& Query, FSurfaceNavPathSharedPtr& OutPath) const;

	virtual void FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query, FNavPathSharedPtr& OutPath) const override { };

	/** Helper function for creating pathfinding query for this agent from move request data */
	bool SN_BuildPathfindingQuery(const FAIMoveRequest& MoveRequest, FSurfaceNavPathFindingQueryData& Query) const;


	/** Passes move request and path object to path following */
	virtual FAIRequestID SurfaceNavRequestMove(const FAIMoveRequest& MoveRequest, FSurfaceNavPathSharedPtr Path);

	virtual FAIRequestID RequestMove(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr Path) override
	{
		return FAIRequestID::InvalidRequest;
	}

	/** Returns this controller's path following component as a SurfaceNavPathFollowingComponent. **/
	UFUNCTION(BlueprintCallable, Category = "Surface Nav Controller|AI|Navigation")
	USurfaceNavPathFollowingComponent* GetSurfaceNavPathFollowingComponent() const;

	/** Returns the index number of the current target point for the path. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav Controller|AI|Path Data")
	int GetCurrentTargetPointIndex() const;

	/** Return a copy of the path point data that can be used to perform custom actions as the pawn
	*   moves along the path. Changing this copy of the path data will not affect path following.
	*   @param Success - True if currently has valid data to return, false otherwise.
	*   @param PathData - Copy of the path data. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav Controller|AI|Path Data")
	void GetPathData(bool& Success, TArray<FSurfaceNavPathPointData>& PathData) const;

	/** Returns data for the path point at the specified index.
	*   @param Index - Index of the path point to get data for.
	*   @param Success - True if currently has valid data to return, false otherwise.
	*   @param PathData - Copy of the path data. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav Controller|AI|Path Data")
	void GetPathPointDataAtIndex(int Index, bool& Success, FSurfaceNavPathPointData& PointData) const;

	/** Returns data for the current target path point. 
	*   @param Success - True if currently has valid data to return, false otherwise.
	*   @param PathData - Copy of the path data. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav Controller|AI|Path Data")
	void GetCurrentTargetPathPointData(bool& Success, FSurfaceNavPathPointData& PointData) const;

	/** Returns data for the prior target path point. 
	*   @param Success - True if currently has valid data to return, false otherwise.
	*   @param PathData - Copy of the path data. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav Controller|AI|Path Data")
	void GetPreviousTargetPathPointData(bool& Success, FSurfaceNavPathPointData& PointData) const;

	virtual bool IsFollowingAPath() const override;

protected:

	UPROPERTY(BlueprintReadOnly, Category = "Surface Nav Controller|AI")
		TWeakObjectPtr<AActor> CurrentPlanet = nullptr;
};
