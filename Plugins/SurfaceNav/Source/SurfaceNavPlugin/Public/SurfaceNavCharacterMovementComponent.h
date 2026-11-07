// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#if false

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "WorldCollision.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/RootMotionSource.h"
#include "AI/Navigation/NavigationAvoidanceTypes.h"
#include "AI/RVOAvoidanceInterface.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/CharacterMovementReplication.h"
#include "Interfaces/NetworkPredictionInterface.h"
#include "CharacterMovementComponentAsync.h"

#include "SurfaceNavCharacterMovementComponent.generated.h"




///** Surface Nav Movement modes for Characters. */
//UENUM(BlueprintType)
//enum ESurfaceNavMovementMode
//{
//	 /***********************************************/
//	 /***********************************************/
//	 /* Begin Custom Movement Modes */
//	 /*---------------------------------------------*/
//
//	/** Surface Nav movement used when character is navigating on a wall defined by MinClimbingAngle */
//	MOVE_SN_None	UMETA(DisplayName = "None"),
//
//	 /** Surface Nav movement used when character is navigating on a wall defined by MinClimbingAngle */
//	 MOVE_SN_Climbing	UMETA(DisplayName = "SurfaceNav Climbing"),
//
//	 /** Surface Nav movement used when character is navigating on a ceiling defined by MinHangingAngle */
//	 MOVE_SN_Hanging	UMETA(DisplayName = "SurfaceNav Hanging"),
//
//	 /*---------------------------------------------*/
//	 /* End Custom Movement Modes */
//	 /***********************************************/
//	 /***********************************************/
//};

//namespace CharacterMovementCVars
//{
//	extern int32 UseTargetVelocityOnImpact;
//	extern int ForceJumpPeakSubstep;
//}

//namespace MovementComponentCVars
//{
//	extern float PenetrationOverlapCheckInflation;
//}

//namespace CharacterMovementConstants
//{
//	//extern const float MAX_STEP_SIDE_Z;
//	extern const float VERTICAL_SLOPE_NORMAL_Z;
//}

//namespace CharacterMovementCVars
//{
//	// Is Async Character Movement enabled?
//	extern ENGINE_API int32 AsyncCharacterMovement;
//	extern int32 ForceJumpPeakSubstep;
//}

//
//
//
////DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FSurfaceNavMoveCompOnDetermineRotationSignature, USurfaceNavMovementComponent, OnDetermineRotation, USurfaceNavMovementComponent*, Component, bool, bReset);
//
//
////DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnPawnNeedsRotationUpdatedSignature, USurfaceNavMovementComponent, OnPawnNeedsRotationUpdated, FVector, TargetLocation, FVector, TargetUpVector, FVector, TargetForwardVector);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnCharacterNeedsPathMoveRotationSignature, USurfaceNavCharacterMovementComponent, OnNeedsPathMoveRotation, FVector, TargetLocation, FVector, TargetUpVector, FVector, TargetForwardVector);
//
////UDELEGATE(BlueprintAuthorityOnly)
////DECLARE_DELEGATE(FSurfaceNavMoveCompOnDetermineRotationSignature); //(USurfaceNavMovementComponent, OnDetermineRotation, FVector)
//
U CLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class SURFACENAVPLUGIN_API USurfaceNavCharacterMovementComponent : public UCharacterMovementComponent, public ISurfaceNavMovementInterface
{
	GENERATED_UCLASS_BODY()

		//typedef UCharacterMovementComponent Super;

		//Begin UActorComponent Interface
		virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav")
		float RotationSpeed;

	/** Min angle from ground this pawn is allowed to navigate on with 0 degrees
	being the ground below the pawn and 180 degrees being the ceiling. Must be less
	than MaxAngleFromGround. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav",
		meta = (ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "180.0"))
		float MinAngleFromGround = 0.f;

	/** Max angle from ground this pawn is allowed to navigate on with 0 degrees
	being the ground below the pawn and 180 degrees being the ceiling. Must be greater
	than MinAngleFromGround. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav",
		meta = (ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "180.0"))
		float MaxAngleFromGround = 180.f;



	/********************************************************************/
	/********************************************************************/
	/* Begin ISurfaceNavMovementInterface */
	/*------------------------------------------------------------------*/

	inline virtual float GetPawnMovementHeightOffset() const override { return GetOwner()->GetActorScale().Z * PawnMovementHeightOffset; }
	inline virtual float GetPathPointAcceptanceRadius() const override { return GetOwner()->GetActorScale().X * PathPointAcceptanceRadius; }
	inline virtual float GetNavLinkPointAcceptanceRadius() const override { return GetOwner()->GetActorScale().X * NavLinkPointAcceptanceRadius; }

	inline virtual float GetMinAngleFromGround() const override { return MinAngleFromGround; }
	inline virtual float GetMaxAngleFromGround() const override { return MaxAngleFromGround; }

	inline virtual FVector GetActorSurfaceLocation() const override
	{
		FVector CompUp = UpdatedComponent->GetUpVector();
		float ZExtent = UpdatedComponent->CalcLocalBounds().BoxExtent.Z;

		return UpdatedComponent ? (UpdatedComponent->GetComponentLocation() - (CompUp * (ZExtent + PawnMovementHeightOffset))) : FNavigationSystem::InvalidLocation;
		//return UpdatedComponent ? (UpdatedComponent->GetComponentLocation() - (UpdatedComponent->GetUpVector() * UpdatedComponent->CalcLocalBounds().BoxExtent.Z + PawnMovementHeightOffset)) : FNavigationSystem::InvalidLocation;
	}

	virtual void HandleRotation(FVector TargetLocation, FVector TargetUpVector, FVector TargetFowardVector) override; // , bool ForceDefault = false);

	/*------------------------------------------------------------------*/
	/* End ISurfaceNavMovementInterface */
	/********************************************************************/
	/********************************************************************/




//	/** Returns location of controlled actor's "feet" meaning center of bottom of collision bounding box */
//	//FORCEINLINE FVector GetActorSurfaceLocation() const { return UpdatedComponent ? (UpdatedComponent->GetComponentLocation() - (UpdatedComponent->GetUpVector() * UpdatedComponent->CalcLocalBounds().BoxExtent.Z)) : FNavigationSystem::InvalidLocation; }
//	//FORCEINLINE FVector GetActorSurfaceLocation() const { return UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FNavigationSystem::InvalidLocation; }
//	FVector GetActorSurfaceLocation() const
//	{
//		FVector CompUp = UpdatedComponent->GetUpVector();
//		float ZExtent = UpdatedComponent->CalcLocalBounds().BoxExtent.Z;
//
//		return UpdatedComponent ? (UpdatedComponent->GetComponentLocation() - (CompUp * (ZExtent + PawnMovementHeightOffset))) : FNavigationSystem::InvalidLocation;
//		//return UpdatedComponent ? (UpdatedComponent->GetComponentLocation() - (UpdatedComponent->GetUpVector() * UpdatedComponent->CalcLocalBounds().BoxExtent.Z + PawnMovementHeightOffset)) : FNavigationSystem::InvalidLocation;
//	}
//
//	float CalcSurfaceHeightFromActor() const
//	{
//		float ZExtent = UpdatedComponent->CalcLocalBounds().BoxExtent.Z;
//
//		return ZExtent + PawnMovementHeightOffset;
//	}
//
//	virtual void HandleRotation(FVector TargetLocation, FVector TargetUpVector, FVector TargetFowardVector); // , bool ForceDefault = false);

	UPROPERTY(BlueprintAssignable, Category = "Surface Nav")
		FOnCharacterNeedsPathMoveRotationSignature OnNeedsPathMoveRotation;

	/** Sets the next rotator to lerp the character to. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav")
		void SetNextRotator(FRotator Rot) { NextRotator = Rot; };

	/** Skips applying a rotation lerp for a single tick. Useful if there's something special you
	need your character to do for a few frames. If you plan on handling rotations yourself on every tick,
	then use DisableDefaultRotationLerp instead. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav")
		void SkipNextRotationLerp() { bSkipNextRotationLerp = true; };

	/** Disables the default rotation lerp handling. This is useful if you know you're
	going to always handle rotations and don't want to call SkipNextRotationLerp on
	every tick. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav")
		void DisableDefaultRotationLerp() { bDisableDefaultRotation = true; };

	/** Enables the default rotation lerp handling. This is enabled by default, so calling
	this is only required if it's been disabled and you'd like to re-enabled it. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav")
		void EnableDefaultRotationLerp() { bDisableDefaultRotation = false; };

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav")
		void SkipNextMovementInput() { bSkipNextMovementInput = true; };

	//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav")
	//		float RotationSpeed;
	//
	//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav")
	//		float MinAngleFromGround = 0.f;
	//
	//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav")
	//		float MaxAngleFromGround = 180.f;

		///** path following: request new velocity */
		//virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;

		///** path following: request new move input (normal vector = full strength) */
		//virtual void RequestPathMove(const FVector& MoveInput) override;


	//	/** Stops applying further movement (usually zeros acceleration). */
	//	virtual void StopActiveMovement() override;
	//
	//	// Overridden to also call StopActiveMovement().
	//	virtual void StopMovementImmediately() override;
	//
	//	//void SetUseRepathAccelBoost() {	bUseRepathAccelBoost = true; }
	//
	//	FORCEINLINE virtual float GetPawnMovementHeightOffset() { return GetOwner()->GetActorScale().Z * PawnMovementHeightOffset; }
	//	FORCEINLINE virtual float GetPathPointAcceptanceRadius() { return GetOwner()->GetActorScale().X * PathPointAcceptanceRadius; }
	//	FORCEINLINE virtual float GetNavLinkPointAcceptanceRadius() { return GetOwner()->GetActorScale().X * NavLinkPointAcceptanceRadius; }
	//
	//
	//	/**************************************/
	//	/**************************************/
	//	/* Begin Character Movement Component */
	//	/* ---------------------------------- */
	//
	//	/**
	//	 * If ShouldPerformAirControlForPathFollowing() returns true, it will update Velocity and Acceleration to air control in the desired Direction for character using path following.
	//	 * @param Direction is the desired direction of movement
	//	 * @param ZDiff is the height difference between the destination and the Pawn's current position
	//	 * @see RequestDirectMove()
	//	*/
	//	virtual void PerformAirControlForPathFollowing(FVector Direction, float ZDiff);
	//
	//	/**
	//	 * Constrain components of root motion velocity that may not be appropriate given the current movement mode (e.g. when falling Z may be ignored).
	//	 */
	//	virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const;
	//
	//	/**
	//	 * Compute new falling velocity from given velocity and gravity. Applies the limits of the current Physics Volume's TerminalVelocity.
	//	 */
	//	virtual FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const;
	//
	//	/** Simulate movement on a non-owning client. Called by SimulatedTick(). */
	//	virtual void SimulateMovement(float DeltaTime);
	//
	//	/** @note Movement update functions should only be called through StartNewPhysics()*/
	//	virtual void PhysFlying(float deltaTime, int32 Iterations);
	//
	//	/** @note Movement update functions should only be called through StartNewPhysics()*/
	//	virtual void PhysSwimming(float deltaTime, int32 Iterations);
	//
	//	/** Handle falling movement. */
	//	virtual void PhysFalling(float deltaTime, int32 Iterations);
	//
	//	/** Returns true if there is a suitable floor SideStep from current position. */
	//	virtual bool CheckLedgeDirection(const FVector& OldLocation, const FVector& SideStep, const FVector& GravDir) const;
	//
	//	/**
	//	 * @param Delta is the current move delta (which ended up going over a ledge).
	//	 * @return new delta which moves along the ledge
	//	 */
	//	virtual FVector GetLedgeMove(const FVector& OldLocation, const FVector& Delta, const FVector& GravDir) const;
	//
	//	/**
	//	 * Move along the floor, using CurrentFloor and ComputeGroundMovementDelta() to get a movement direction.
	//	 * If a second walkable surface is hit, it will also be moved along using the same approach.
	//	 *
	//	 * @param InVelocity:			Velocity of movement
	//	 * @param DeltaSeconds:			Time over which movement occurs
	//	 * @param OutStepDownResult:	[Out] If non-null, and a floor check is performed, this will be updated to reflect that result.
	//	 */
	//	virtual void MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult = NULL);
	//
	//	/** @note Movement update functions should only be called through StartNewPhysics()*/
	//	virtual void PhysWalking(float deltaTime, int32 Iterations);
	//
	//	/** Use new physics after landing. Defaults to swimming if in water, walking otherwise. */
	//	virtual void SetPostLandedPhysics(const FHitResult& Hit);
	//
	//	/**
	//	 * Moves along the given movement direction using simple movement rules based on the current movement mode (usually used by simulated proxies).
	//	 *
	//	 * @param InVelocity:			Velocity of movement
	//	 * @param DeltaSeconds:			Time over which movement occurs
	//	 * @param OutStepDownResult:	[Out] If non-null, and a floor check is performed, this will be updated to reflect that result.
	//	 */
	//	virtual void MoveSmooth(const FVector& InVelocity, const float DeltaSeconds, FStepDownResult* OutStepDownResult = NULL);
	//
	//	/**
	//	 * Move up steps or slope. Does nothing and returns false if CanStepUp(Hit) returns false.
	//	 *
	//	 * @param GravDir			Gravity vector direction (assumed normalized or zero)
	//	 * @param Delta				Requested move
	//	 * @param Hit				[In] The hit before the step up.
	//	 * @param OutStepDownResult	[Out] If non-null, a floor check will be performed if possible as part of the final step down, and it will be updated to reflect this result.
	//	 * @return true if the step up was successful.
	//	 */
	//	virtual bool StepUp(const FVector& GravDir, const FVector& Delta, const FHitResult& Hit, FStepDownResult* OutStepDownResult = NULL);
	//
	//	/** Handle a blocking impact. Calls ApplyImpactPhysicsForces for the hit, if bEnablePhysicsInteraction is true. */
	//	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;
	//
	//	/**
	//	 * Applies downward force when walking on top of physics objects.
	//	 * @param DeltaSeconds Time elapsed since last frame.
	//	 */
	//	virtual void ApplyDownwardForce(float DeltaSeconds);
	//
	//	/** Applies momentum accumulated through AddImpulse() and AddForce(), then clears those forces. Does *not* use ClearAccumulatedForces() since that would clear pending launch velocity as well. */
	//	virtual void ApplyAccumulatedForces(float DeltaSeconds);
	//
	//
	//	// TODO: How do we handle this function, is it only internal so we can override the function which call it
	//	/** Applies root motion from root motion sources to velocity (override and additive) */
	//	void ApplyRootMotionToVelocity(float deltaTime);
	//
	//
	//	/* ---------------------------------- */
	//	/* End Character Movement Component */
	//	/**************************************/
	//	/**************************************/





	virtual bool IsClimbing() const override;
	virtual bool IsHanging() const override;

	virtual bool CanClimb() const override;
	virtual bool CanHang() const override;

	virtual float GetStartClimbingAngle() const override { return StartClimbingAngle; }
	virtual float GetStartHangingAngle() const override { return StartHangingAngle; }

	virtual ESurfaceNavOrientation GetClimbingOrientation() const override { return ClimbingOrientation; }
	virtual ESurfaceNavOrientation GetHangingOrientation() const override { return HangingOrientation; }

	/**
		 * Handle start swimming functionality
		 * @param OldLocation - Location on last tick
		 * @param OldVelocity - velocity at last tick
		 * @param timeTick - time since at OldLocation
		 * @param remainingTime - DeltaTime to complete transition to swimming
		 * @param Iterations - physics iteration count
		 */
	void StartClimbing(FVector OldLocation, FVector OldVelocity, float timeTick, float remainingTime, int32 Iterations);

	/* Swimming uses gravity - but scaled by (1.f - buoyancy) */
	float Climb(FVector Delta, FHitResult& Hit);

	/**
		 * Handle start swimming functionality
		 * @param OldLocation - Location on last tick
		 * @param OldVelocity - velocity at last tick
		 * @param timeTick - time since at OldLocation
		 * @param remainingTime - DeltaTime to complete transition to swimming
		 * @param Iterations - physics iteration count
		 */
	void StartHanging(FVector OldLocation, FVector OldVelocity, float timeTick, float remainingTime, int32 Iterations);

	/* Swimming uses gravity - but scaled by (1.f - buoyancy) */
	float Hang(FVector Delta, FHitResult& Hit);


	/** @note Movement update functions should only be called through StartNewPhysics()*/
	virtual void PhysClimbing(float deltaTime, int32 Iterations);

	virtual void PhysNavClimbing(float deltaTime, int32 Iterations);

	/** @note Movement update functions should only be called through StartNewPhysics()*/
	virtual void PhysHanging(float deltaTime, int32 Iterations);



	/** changes physics based on MovementMode */
	virtual void StartNewPhysics(float deltaTime, int32 Iterations) override;

	virtual float GetMaxSpeed() const override;

	/** Returns maximum deceleration for the current state when braking (ie when there is no acceleration). */
	virtual float GetMaxBrakingDeceleration() const override;

	/** Handle falling movement. */
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;

	/*TODO: May need to override this function */
	/** Use new physics after landing. Defaults to swimming if in water, walking otherwise. */
	virtual void SetPostLandedPhysics(const FHitResult& Hit) override;

	/** Called by owning Character upon successful teleport from AActor::TeleportTo(). */
	virtual void OnTeleported() override;

	/** Delegate when PhysicsVolume of UpdatedComponent has been changed **/
	virtual void PhysicsVolumeChanged(class APhysicsVolume* NewVolume) override;

	/** Returns MovementMode string */
	virtual FString GetMovementName() const override;

	/** Set movement mode to the default based on the current physics volume. */
	virtual void SetDefaultMovementMode() override;

	// TODO: Need to customize this further to pull dynamic gravity vector if not flat world
	virtual FVector GetGravityNormal() const;
	/*{ 
		if (SurfaceNavigation::IsFlatWorld)
			return FVector::DownVector; 
		else
		{
			if (CharacterOwner)
			{
				AActor* CurrentPlanet = nullptr;
				GetCurrentPlanet(CurrentPlanet);
				if (CurrentPlanet)
				{
					auto GravityNormal = (UpdatedComponent->GetComponentLocation() - CurrentPlanet->GetActorLocation());
					GravityNormal.Normalize();
					return GravityNormal;
				}
			}
		}

		return FVector::DownVector;
	}*/

	virtual FVector GetGroundNormal() const;
	/*{
		return GetGravityNormal() * -1.f;
	}*/

	// TODO: Need to customize this further to pull dynamic gravity vector if not flat world
	virtual FVector GetGravity() const; // { return GetGravityNormal() * GetGravityZ(); }

	virtual void GetCurrentPlanet(AActor*& OutPlanetActor) const;

	/*{
		ASurfaceNavAIController* AiController = Cast<ASurfaceNavAIController>(CharacterOwner->GetController());
		if (AiController)
		{
			AiController->GetCurrentPlanet(OutPlanetActor);
		}
	}*/

	/**
	 * Move up steps or slope. Does nothing and returns false if CanStepUp(Hit) returns false.
	 *
	 * @param GravDir			Gravity vector direction (assumed normalized or zero)
	 * @param Delta				Requested move
	 * @param Hit				[In] The hit before the step up.
	 * @param OutStepDownResult	[Out] If non-null, a floor check will be performed if possible as part of the final step down, and it will be updated to reflect this result.
	 * @return true if the step up was successful.
	 */
	virtual bool StepUp(const FVector& GravDir, const FVector& Delta, const FHitResult& Hit, FStepDownResult* OutStepDownResult = NULL) override;


	/**
	 * Compute distance to the floor from bottom sphere of capsule and store the result in OutFloorResult.
	 * This distance is the swept distance of the capsule to the first point impacted by the lower hemisphere, or distance from the bottom of the capsule in the case of a line trace.
	 * This function does not care if collision is disabled on the capsule (unlike FindFloor).
	 * @see FindFloor
	 *
	 * @param CapsuleLocation:	Location of the capsule used for the query
	 * @param LineDistance:		If non-zero, max distance to test for a simple line check from the capsule base. Used only if the sweep test fails to find a walkable floor, and only returns a valid result if the impact normal is a walkable normal.
	 * @param SweepDistance:	If non-zero, max distance to use when sweeping a capsule downwards for the test. MUST be greater than or equal to the line distance.
	 * @param OutFloorResult:	Result of the floor check. The HitResult will contain the valid sweep or line test upon success, or the result of the sweep upon failure.
	 * @param SweepRadius:		The radius to use for sweep tests. Should be <= capsule radius.
	 * @param DownwardSweepResult:	If non-null and it contains valid blocking hit info, this will be used as the result of a downward sweep test instead of doing it as part of the update.
	 */
	virtual void ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FFindFloorResult& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult = NULL) const override;

	/**
	 * Return true if the 2D distance to the impact point is inside the edge tolerance (CapsuleRadius minus a small rejection threshold).
	 * Useful for rejecting adjacent hits when finding a floor or landing spot.
	 */
	virtual bool IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, const float CapsuleRadius) const override;

	/** Return true if the hit result should be considered a walkable surface for the character. */
	virtual bool IsWalkable(const FHitResult& Hit) const override;

	/** Returns true if we can step up on the actor in the given FHitResult. */
	virtual bool CanStepUp(const FHitResult& Hit) const override;

	/**
	 * Constrain components of root motion velocity that may not be appropriate given the current movement mode (e.g. when falling Z may be ignored).
	 */
	virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;

	/**
	 * Check if the result of a sweep test (passed in InHit) might be a valid location to perch, in which case we should use ComputePerchResult to validate the location.
	 * @see ComputePerchResult
	 * @param InHit:			Result of the last sweep test before this query.
	 * @param bCheckRadius:		If true, only allow the perch test if the impact point is outside the radius returned by GetValidPerchRadius().
	 * @return Whether perching may be possible, such that ComputePerchResult can return a valid result.
	 */
	virtual bool ShouldComputePerchResult(const FHitResult& InHit, bool bCheckRadius = true) const override;

	/**
	 * Compute the sweep result of the smaller capsule with radius specified by GetValidPerchRadius(),
	 * and return true if the sweep contacts a valid walkable normal within InMaxFloorDist of InHit.ImpactPoint.
	 * This may be used to determine if the capsule can or cannot stay at the current location if perched on the edge of a small ledge or unwalkable surface.
	 * Note: Only returns a valid result if ShouldComputePerchResult returned true for the supplied hit value.
	 *
	 * @param TestRadius:			Radius to use for the sweep, usually GetValidPerchRadius().
	 * @param InHit:				Result of the last sweep test before the query.
	 * @param InMaxFloorDist:		Max distance to floor allowed by perching, from the supplied contact point (InHit.ImpactPoint).
	 * @param OutPerchFloorResult:	Contains the result of the perch floor test.
	 * @return True if the current location is a valid spot at which to perch.
	 */
	virtual bool ComputePerchResult(const float TestRadius, const FHitResult& InHit, const float InMaxFloorDist, FFindFloorResult& OutPerchFloorResult) const override;

	/**
	 * @param Delta is the current move delta (which ended up going over a ledge).
	 * @return new delta which moves along the ledge
	 */
	virtual FVector GetLedgeMove(const FVector& OldLocation, const FVector& Delta, const FVector& GravDir) const override;

	/** Transition from walking to falling */
	virtual void StartFalling(int32 Iterations, float remainingTime, float timeTick, const FVector& Delta, const FVector& subLoc) override;

	/** Adjust distance from floor, trying to maintain a slight offset from the floor when walking (based on CurrentFloor). */
	virtual void AdjustFloorHeight() override;

	/**
	 * Handle start swimming functionality
	 * @param OldLocation - Location on last tick
	 * @param OldVelocity - velocity at last tick
	 * @param timeTick - time since at OldLocation
	 * @param remainingTime - DeltaTime to complete transition to swimming
	 * @param Iterations - physics iteration count
	 */
	void StartSwimming(FVector OldLocation, FVector OldVelocity, float timeTick, float remainingTime, int32 Iterations);

	/**
	 * Get the lateral acceleration to use during falling movement. The Z component of the result is ignored.
	 * Default implementation returns current Acceleration value modified by GetAirControl(), with Z component removed,
	 * with magnitude clamped to GetMaxAcceleration().
	 * This function is used internally by PhysFalling().
	 *
	 * @param DeltaTime Time step for the current update.
	 * @return Acceleration to use during falling movement.
	 */
	virtual FVector GetFallingLateralAcceleration(float DeltaTime) override;

	/**
	 * Returns true if falling movement should limit air control. Limiting air control prevents input acceleration during falling movement
	 * from allowing velocity to redirect forces upwards while falling, which could result in slower falling or even upward boosting.
	 *
	 * @see GetFallingLateralAcceleration(), BoostAirControl(), GetAirControl(), LimitAirControl()
	 */
	virtual bool ShouldLimitAirControl(float DeltaTime, const FVector& FallAcceleration) const override;

	/**
	* Unrotate a vector using the difference between the current locations gravity for planets and the
	* basic flat world gravity (0,0,-1) and return a vector which you can zero out Z or get size 2D on and then
	* call RotateVectorUsingGravity if you need the vector rotated back to work with the planets gravity.
	*/
	virtual FVector UnrotateVectorUsingGravity(const FVector SourceVector) const
	{
		
		//SourceVector.RotateAngleAxis(1.f, SourceVector);

		if (SurfaceNavigation::IsFlatWorld)
			return SourceVector;
		
		return UpdatedComponent->GetComponentRotation().UnrotateVector(SourceVector);
	}

	/**
	* Rotate a vector using the difference between the basic flat world gravity (0,0,-1) and current 
	* locations gravity for planets and return a vector which works with the planets gravity. Call
	* UnrotateVectorUsingGravity if you need to work with a vector where you can zero out the Z axis
	* or get size 2D.
	*/
	virtual FVector RotateVectorUsingGravity(const FVector SourceVector) const
	{
		if (SurfaceNavigation::IsFlatWorld)
			return SourceVector;
			
		return UpdatedComponent->GetComponentRotation().RotateVector(SourceVector);
	}

	/** Verify that the supplied hit result is a valid landing spot when falling. */
	virtual bool IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const override;

	/** Force this pawn to bounce off its current base, which isn't an acceptable base for it. */
	virtual void JumpOff(AActor* MovementBaseActor) override;

	/**
	 * If ShouldPerformAirControlForPathFollowing() returns true, it will update Velocity and Acceleration to air control in the desired Direction for character using path following.
	 * @param Direction is the desired direction of movement
	 * @param ZDiff is the height difference between the destination and the Pawn's current position
	 * @see RequestDirectMove()
	*/
	virtual void PerformAirControlForPathFollowing(FVector Direction, float ZDiff) override;

	virtual void RequestPathMove(const FVector& MoveInput) override;

	/** Applies repulsion force to all touched components. */
	virtual void ApplyRepulsionForce(float DeltaSeconds) override;

	/**
	 * Project a location to navmesh to find adjusted height.
	 * @param TestLocation		Location to project
	 * @param NavFloorLocation	Location on navmesh
	 * @return True if projection was performed (successfully or not)
	 */
	virtual bool FindNavFloor(const FVector& TestLocation, FNavLocation& NavFloorLocation) const override;

	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;

protected:

	/**
	 * Ground movement mode to switch to after falling and resuming ground movement.
	 * Only allowed values are: MOVE_Walking, MOVE_NavWalking.
	 * @see SetGroundMovementMode(), GetGroundMovementMode()
	 */
	UPROPERTY(Category = "Character Movement: SurfaceNavMovementMode", BlueprintReadOnly)
		TEnumAsByte<enum ESurfaceNavMovementMode> SurfaceNavMovementMode;

	/** @note Movement update functions should only be called through StartNewPhysics()*/
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;

	/* Prepare inputs for asynchronous simulation on physics thread */
	virtual void FillAsyncInput(const FVector& InputVector, FCharacterMovementComponentAsyncInput& AsyncInput) override;

	FRotator NextRotator;
	bool bSkipNextMovementInput = false;
	bool bSkipNextRotationLerp = false;
	bool bDisableDefaultRotation = false;
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav")
		float PawnMovementHeightOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav")
		float PathPointAcceptanceRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav")
		float NavLinkPointAcceptanceRadius;


	/** Can the character climb? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Movement Capabilities", meta = (DisplayName = "Can Climb"))
		bool bCanClimb = true;

	/** Can the character hang from ceiling? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Movement Capabilities", meta = (DisplayName = "Can Hang"))
		bool bCanHang = true;

	/** Starting angle which the character will switch to climbing mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Movement Capabilities", meta = (DisplayName = "Start Climbing Angle"))
		float StartClimbingAngle = 45.0f;

	/** Starting angle which the character will switch to hanging mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Movement Capabilities", meta = (DisplayName = "Start Hanging Angle"))
		float StartHangingAngle = 135.0f;

	/** Starting angle which the character will switch to hanging mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Movement Capabilities", meta = (DisplayName = "Climbing Orientation"))
		TEnumAsByte<enum ESurfaceNavOrientation> ClimbingOrientation; // = TEnumAsByte<enum ESurfaceNavOrientation>(ESurfaceNavOrientation::ORIENTATION_SN_ForwardVectorFacingSurface);

	/** Starting angle which the character will switch to hanging mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Movement Capabilities", meta = (DisplayName = "Hanging Orientation"))
		TEnumAsByte<enum ESurfaceNavOrientation> HangingOrientation; // = TEnumAsByte<enum ESurfaceNavOrientation>(ESurfaceNavOrientation::ORIENTATION_SN_UpVectorFacingSurface);

	/**
	 * Move along the floor, using CurrentFloor and ComputeGroundMovementDelta() to get a movement direction.
	 * If a second walkable surface is hit, it will also be moved along using the same approach.
	 *
	 * @param InVelocity:			Velocity of movement
	 * @param DeltaSeconds:			Time over which movement occurs
	 * @param OutStepDownResult:	[Out] If non-null, and a floor check is performed, this will be updated to reflect that result.
	 */
	virtual void MoveAlongWall(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult = NULL);

	/** Custom version of SlideAlongSurface that handles different movement modes separately; namely during walking physics we might not want to slide up slopes. */
	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;

	/** Handle a blocking impact. Calls ApplyImpactPhysicsForces for the hit, if bEnablePhysicsInteraction is true. */
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

	/**
	 * Move along the floor, using CurrentFloor and ComputeGroundMovementDelta() to get a movement direction.
	 * If a second walkable surface is hit, it will also be moved along using the same approach.
	 *
	 * @param InVelocity:			Velocity of movement
	 * @param DeltaSeconds:			Time over which movement occurs
	 * @param OutStepDownResult:	[Out] If non-null, and a floor check is performed, this will be updated to reflect that result.
	 */
	virtual void MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult = NULL) override;

	/**
	 * Compute a vector of movement, given a delta and a hit result of the surface we are on.
	 *
	 * @param Delta:				Attempted movement direction
	 * @param RampHit:				Hit result of sweep that found the ramp below the capsule
	 * @param bHitFromLineTrace:	Whether the floor trace came from a line trace
	 *
	 * @return If on a walkable surface, this returns a vector that moves parallel to the surface. The magnitude may be scaled if bMaintainHorizontalGroundVelocity is true.
	 * If a ramp vector can't be computed, this will just return Delta.
	 */
	virtual FVector ComputeGroundMovementDelta(const FVector& Delta, const FHitResult& RampHit, const bool bHitFromLineTrace) const override;

	/**
	 * Adjusts velocity when walking so that Z velocity is zero.
	 * When bMaintainHorizontalGroundVelocity is false, also rescales the velocity vector to maintain the original magnitude, but in the horizontal direction.
	 */
	virtual void MaintainHorizontalGroundVelocity() override;

	/** Applies root motion from root motion sources to velocity (override and additive) */
	void ApplyRootMotionToVelocity(float deltaTime);

	/**
	 * Increase air control if conditions of AirControlBoostMultiplier and AirControlBoostVelocityThreshold are met.
	 * This function is used internally by GetAirControl().
	 *
	 * @param DeltaTime			Time step for the current update.
	 * @param TickAirControl	Current air control value.
	 * @param FallAcceleration	Acceleration used during movement.
	 * @return Modified air control to use during falling movement
	 * @see GetAirControl()
	 */
	virtual float BoostAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration) override;

	/**
	 * Limits the air control to use during falling movement, given an impact while falling.
	 * This function is used internally by PhysFalling().
	 *
	 * @param DeltaTime			Time step for the current update.
	 * @param FallAcceleration	Acceleration used during movement.
	 * @param HitResult			Result of impact.
	 * @param bCheckForValidLandingSpot If true, will use IsValidLandingSpot() to determine if HitResult is a walkable surface. If false, this check is skipped.
	 * @return Modified air control acceleration to use during falling movement.
	 * @see PhysFalling()
	 */
	virtual FVector LimitAirControl(float DeltaTime, const FVector& FallAcceleration, const FHitResult& HitResult, bool bCheckForValidLandingSpot) override;

	/**
	 * Limit the slide vector when falling if the resulting slide might boost the character faster upwards.
	 * @param SlideResult:	Vector of movement for the slide (usually the result of ComputeSlideVector)
	 * @param Delta:		Original attempted move
	 * @param Time:			Amount of move to apply (between 0 and 1).
	 * @param Normal:		Normal opposed to movement. Not necessarily equal to Hit.Normal (but usually is).
	 * @param Hit:			HitResult of the move that resulted in the slide.
	 * @return:				New slide result.
	 */
	virtual FVector HandleSlopeBoosting(const FVector& SlideResult, const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const override;

	/** Custom version that allows upwards slides when walking if the surface is walkable. */
	virtual void TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const override;

	/** @note Movement update functions should only be called through StartNewPhysics()*/
	virtual void PhysNavWalking(float deltaTime, int32 Iterations) override;

	/**
	 * Have the server check if the client is outside an error tolerance, and queue a client adjustment if so.
	 * If either GetPredictionData_Server_Character()->bForceClientUpdate or ServerCheckClientError() are true, the client adjustment will be sent.
	 * RelativeClientLocation will be a relative location if MovementBaseUtility::UseRelativePosition(ClientMovementBase) is true, or a world location if false.
	 * @see ServerCheckClientError()
	 */
	virtual void ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;

	/** Called when the collision capsule touches another primitive component */
	virtual void CapsuleTouched(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	/**
	 * Attempts to better align navmesh walking characters with underlying geometry (sometimes
	 * navmesh can differ quite significantly from geometry).
	 * Updates CachedProjectedNavMeshHitResult, access this for more info about hits.
	 */
	virtual FVector ProjectLocationFromNavMesh(float DeltaSeconds, const FVector& CurrentFeetLocation, const FVector& TargetNavLocation, float UpOffset, float DownOffset);

private:

	/** Remember last server movement base so we can detect mounts/dismounts and respond accordingly. */
	TWeakObjectPtr<UPrimitiveComponent> LastServerMovementBase = nullptr;

};

#endif