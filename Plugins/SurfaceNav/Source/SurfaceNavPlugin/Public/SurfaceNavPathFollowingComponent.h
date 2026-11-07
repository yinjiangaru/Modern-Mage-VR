// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "Navigation/PathFollowingComponent.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavigationData.h"
#include "AITypes.h"
#include "AIResourceInterface.h"
#include "GameFramework/NavMovementComponent.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"

#include "SurfaceNavPath.h"
#include "SurfaceNavPoint.h"
#include "SurfaceNavMovementComponent.h"
#include "SurfaceNavPathFollowingComponent.generated.h"

//SURFACENAVPLUGIN_API DECLARE_LOG_CATEGORY_EXTERN(LogSurfaceNavPathFollowing, Warning, All);

#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 4
class INavMovementInterface;
#endif

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnSurfaceNavCurrentTargetPointUpdatedSignature, USurfaceNavPathFollowingComponent, OnSurfaceNavCurrentTargetPointUpdated, int, NewTargetPointIndex, FSurfaceNavPathPointData, NewTargetPointData, FSurfaceNavPathPointData, PreviousTargetPointData);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnSurfaceNavPathUpdatedSignature, USurfaceNavPathFollowingComponent, OnSurfaceNavPathUpdated, TArray<FSurfaceNavPathPointData>, NewPathData);


/** This component provides pathing capability for AI using the DSN plugin’s navigation. */
UCLASS(config = Engine)
class SURFACENAVPLUGIN_API USurfaceNavPathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_UCLASS_BODY()


	//typedef UPathFollowingComponent Super;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void DrawCurrentPath() const;

	/** start movement along path
		* @return MoveId of requested move
		*/
	virtual FAIRequestID SurfaceNavRequestMove(const FAIMoveRequest& RequestData, FSurfaceNavPathSharedPtr InPath);

	virtual FAIRequestID RequestMove(const FAIMoveRequest& RequestData, FNavPathSharedPtr InPath) override
	{
		//UE_VLOG(GetOwner(), LogSurfaceNavPathFollowing, Log, TEXT("Rejecting move request, use SurfaceNavRequestMove instead!"), *ResourceLock.GetLockPriorityName());
		return FAIRequestID::InvalidRequest;
	};


	// need to override since it uses feet location
	/** pause path following
	*  @param RequestID - request to pause, FAIRequestID::CurrentRequest means pause current request, regardless of its ID */
	virtual void PauseMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest, EPathFollowingVelocityMode VelocityMode = EPathFollowingVelocityMode::Reset) override;

	/** resume path following
	*  @param RequestID - request to resume, FAIRequestID::CurrentRequest means restor current request, regardless of its ID */
	virtual void ResumeMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest);

	// * Refs nav links, what needs to be done for that to work
	// * Use custom meta class where needed
	/** notify about finished movement */
	virtual void OnPathFinished(const FPathFollowingResult& Result) override;

	/** notify about changing current path: new pointer or update from path event */
	virtual void OnPathUpdated() override;

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
	// need to override and comment out section using UE nav
	/** set associated movement component */
	virtual void SetMovementComponent(UNavMovementComponent* MoveComp) override;
#else
	/** set associated nav movement interface */
	virtual void SetNavMovementInterface(INavMovementInterface* NavMoveInterface) override;
#endif

	// Unsure at this time
	/** call when moving agent finishes using custom nav link, returns control back to path following */
	virtual void FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink) override;

	// override and leave empty for now
	virtual void GetDebugStringTokens(TArray<FString>& Tokens, TArray<EPathFollowingDebugTokens::Type>& Flags) const override;

	// override and leave empty for now
	virtual FString GetDebugString() const override;

	// This exact function does already exist in the base path following component, but it's private so
	// I need my own here as well.
	FORCEINLINE void OnSurfaceNavPathFinished(EPathFollowingResult::Type ResultCode, uint16 ExtraResultFlags) { OnPathFinished(FPathFollowingResult(ResultCode, ExtraResultFlags)); }

	// May want to change the return type to something else
	//FSurfaceNavPoint GetCurrentSurfaceNavLocation() const;

	/** path observer */
	void OnPathEvent(FSurfaceNavPath* InPath, ENavPathEvent::Type Event);

	void SetAcceptanceRadius(const float InAcceptanceRadius);

	FORCEINLINE const FSurfaceNavPathSharedPtr GetPath() const { return Path; }

	//FORCEINLINE const float GetMovementOffset() const { return SurfaceMovementComp ? SurfaceMovementComp->GetPawnMovementOffset() : 0.f; }

	//FORCEINLINE const FVector GetDestinationNavAgentLocation() const { return DestinationAgent ? DestinationAgent->GetNavAgentLocation() + DestinationActor->GetActorUpVector() * GetMovementHeightOffset() : DestinationActor->GetActorLocation(); }
	FORCEINLINE const FVector GetDestinationNavAgentLocation() const { return DestinationAgent ? DestinationAgent->GetNavAgentLocation() : DestinationActor->GetActorLocation(); }

	FVector GetDestinationActorMoveToLocation(FBox& DestLocalBox, bool& DestLocalBoxSet) const;

	ISurfaceNavMovementInterface* SurfaceMovementComp = nullptr;
	//TWeakObjectPtr<ISurfaceNavMovementInterface> SurfaceMovementComp;
	//TObjectPtr<ISurfaceNavMovementInterface> SurfaceMovementComp;

	bool IsCurrentSegmentNavigationLink() const;

public:

	/** This event is triggered when the current path changes and will provide
	the new path point data. */
	UPROPERTY(BlueprintAssignable, Category = "Surface Nav|AI|Path Data")
	FOnSurfaceNavPathUpdatedSignature OnSurfaceNavPathUpdated;

	/** This event is triggered when the current target point for the path changes
	as the pawn moves along the path. */
	UPROPERTY(BlueprintAssignable, Category = "Surface Nav|AI|Path Data")
	FOnSurfaceNavCurrentTargetPointUpdatedSignature OnSurfaceNavCurrentTargetPointUpdated;

	/** Returns the index number of the current target point for the path. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|AI|Path Data")
	int GetCurrentTargetPointIndex() const;

	/** Return a copy of the path point data that can be used to perform custom actions as the pawn
	*   moves along the path. Changing this copy of the path data will not affect path following.
	*   @param Success - True if currently has valid data to return, false otherwise.
	*   @param PathData - Copy of the path data. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|AI|Path Data")
	void GetPathData(bool& Success, TArray<FSurfaceNavPathPointData>& PathData) const;

	/** Returns data for the path point at the specified index.
	*   @param Index - Index of the path point to get data for.
	*   @param Success - True if currently has valid data to return, false otherwise.
	*   @param PathData - Copy of the path data. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|AI|Path Data")
	void GetPathPointDataAtIndex(int Index, bool& Success, FSurfaceNavPathPointData& PointData) const;

	/** Returns data for the current target path point.
	*   @param Success - True if currently has valid data to return, false otherwise.
	*   @param PathData - Copy of the path data. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|AI|Path Data")
	void GetCurrentTargetPathPointData(bool& Success, FSurfaceNavPathPointData& PointData) const;

	/** Returns data for the prior target path point.
	*   @param Success - True if currently has valid data to return, false otherwise.
	*   @param PathData - Copy of the path data. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|AI|Path Data")
	void GetPreviousTargetPathPointData(bool& Success, FSurfaceNavPathPointData& PointData) const;

	/** aborts following path */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|AI|Path Data")
	virtual void UserAbortMove();

	FORCEINLINE bool HasValidPath() const { return Path.IsValid() && Path->IsValid(); }

protected:

	FSurfaceNavPathSharedPtr Path;

	// Need to override since it uses node index
	/** called after receiving update event from current path
	 *  @return false if path was not accepted and move request needs to be aborted */
	virtual bool HandlePathUpdateEvent() override;

	// need to override since it uses 2D dist and feet loc
	/** should verify if agent if still on path ater movement has been resumed? */
	virtual bool ShouldCheckPathOnResume() const override;

	// need to override
	/** called when NavigationSystem finishes initial navigation data registration.
	 *	This is usually required by AI agents hand-placed on levels to find MyNavData */
	virtual void OnNavigationInitDone() override;

	// Unsure if need to override or add to at this point
	/** reset path following data */
	virtual void Reset() override;

	// need to override, ref nav data
	/** Checks if this PathFollowingComponent is already on path, and
	*	if so determines index of next path point
	*	@return what PathFollowingComponent thinks should be next path point. INDEX_NONE if given path is invalid
	*	@note this function does not set MoveSegmentEndIndex */
	virtual int32 SurfaceNavDetermineStartingPathPoint(const FSurfaceNavPath* ConsideredPath) const;

	virtual int32 DetermineStartingPathPoint(const FNavigationPath* ConsideredPath) const override
	{
		return -1;
	}

	// need to override since it refs nodes
	/** sets variables related to current move segment */
	virtual void SetMoveSegment(int32 SegmentStartIndex) override;

	// Unsure if need to override
	/** check state of path following, update move segment if needed */
	virtual void UpdatePathSegment() override;

	// May need to override, uses feet location
	/** check if segment is completed */
	virtual bool HasReachedCurrentTarget(const FVector& CurrentLocation) const override;

	/** check if move is completed */
	virtual bool HasReachedDestination(const FVector& CurrentLocation) const override;

	// Unsure at this time
	/** next path segment if custom nav link, try passing control to it */
	virtual void StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint) override;

	// May need to override, uses nav data
	/** check if agent is on path */
	virtual bool IsOnPath() const override;

	/** For given path finds a path node at which
	 *	PathfollowingComponent should start doing
	 *	precise is-goal-in-acceptance-radius  tests */
	int32 FindPreciseAcceptanceRadiusTestsStartNodeIndex(const FSurfaceNavPath& PathInstance, const FVector& GoalLocation) const;

	/** Based on Path's properties, original move goal location and requested AcceptanceRadius
	 *	this function calculates actual acceptance radius to apply when testing if the agent
	 *	has successfully reached requested goal's vicinity */
	float GetFinalAcceptanceRadius(const FSurfaceNavPath& PathInstance, const FVector OriginalGoalLocation, const FVector* PathEndOverride = nullptr) const;

	/** follow current path segment */
	virtual void FollowPathSegment(float DeltaTime) override;

	/** Adjust movement input vector based on whether the actor has line of site to the next
	target. */
	virtual void ApplyCornerBoost(FVector CurrentTarget, FVector& ForwardNormal, FVector& UpVector);

	/** check if moving agent has reached goal defined by cylinder */
	bool HasReachedInternal(const FVector& GoalLocation, float GoalRadius, float GoalHalfHeight, const FVector& AgentLocation, float RadiusThreshold, float AgentRadiusMultiplier) const;

	FVector GetPointNormal(int32 index) const { return Path->GetPointNormal(index); };
	FVector GetPointLocation(int32 index) const { return Path->GetPointLocation(index); };

	/** updates braking distance and deceleration segment */
	virtual void UpdateDecelerationData() override;

	virtual bool UpdateMovementComponent(bool bForce = false) override;

#if ENGINE_MAJOR_VERSION < 5
	FORCEINLINE virtual UNavMovementComponent* GetMoveComp() const { return MovementComp; };
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
	FORCEINLINE virtual TObjectPtr<UNavMovementComponent> GetMoveComp() const { return MovementComp; };
#else
	FORCEINLINE virtual TWeakInterfacePtr<INavMovementInterface> GetMoveComp() const { return NavMovementInterface; };
#endif

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
	//FORCEINLINE virtual TObjectPtr<UNavMovementComponent> GetMoveComp() const { return MovementComp; };
	FORCEINLINE virtual bool GetMoveCompIsValid() const { return GetMoveComp() != NULL && GetMoveComp() != nullptr; };
	FORCEINLINE virtual AActor* GetMoveCompOwnerAsActor() const { return GetMoveComp()->GetOwner(); };
	FORCEINLINE virtual FVector GetMoveCompFeetLocation() const { return GetMoveComp()->GetActorFeetLocation(); };
	FORCEINLINE virtual FVector GetMoveCompVelocity() { return GetMoveComp()->Velocity; };
	FORCEINLINE virtual float GetMoveCompMaxSpeed() { return GetMoveComp()->GetMaxSpeed(); };
#else
	//FORCEINLINE virtual TWeakInterfacePtr<INavMovementInterface> GetMoveComp() const { return NavMovementInterface; };
	FORCEINLINE virtual bool GetMoveCompIsValid() const { return GetMoveComp().IsValid(); };
	FORCEINLINE virtual AActor* GetMoveCompOwnerAsActor() const { return Cast<AActor>(GetMoveComp()->GetOwnerAsObject()); };
	FORCEINLINE virtual FVector GetMoveCompFeetLocation() const { return GetMoveComp()->GetFeetLocation(); };
	FORCEINLINE virtual FVector GetMoveCompVelocity() { return GetMoveComp()->GetVelocityForNavMovement(); };
	FORCEINLINE virtual float GetMoveCompMaxSpeed() { return GetMoveComp()->GetMaxSpeedForNavMovement(); };
#endif
	
	
private:
	/** Current location on navigation data.  Lazy-updated, so read this via GetCurrentNavLocation().
	 *	Since it makes conceptual sense for GetCurrentNavLocation() to be const but we may
	 *	need to update the cached value, CurrentNavLocation is mutable. */
	mutable FSurfaceNavPoint CurrentNavLocation;

	int LastTestedIndex = -1;
	int ForceNextIndex = -1;
};

