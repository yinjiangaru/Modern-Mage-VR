// Copyright BMK Games LLC. All Rights Reserved.

#if false

#include "SurfaceNavCharacterMovementComponent.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "EngineStats.h"
#include "Components/PrimitiveComponent.h"
#include "AI/NavigationSystemBase.h"
#include "AI/Navigation/NavigationDataInterface.h"
#include "UObject/Package.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PhysicsVolume.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetDriver.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/Canvas.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"
#include "AI/Navigation/AvoidanceManager.h"
#include "Components/BrushComponent.h"
#include "Misc/App.h"
#include "CharacterMovementComponentAsync.h"
#include "PBDRigidsSolver.h"

#include "Engine/DemoNetDriver.h"
#include "Engine/NetworkObjectList.h"

#include "ProfilingDebugging/CsvProfiler.h"



#pragma region CVars and Constants

// CVars and constants
namespace CharacterMovementCVars
{
	int32 UseTargetVelocityOnImpact = 1;
	int32 ForceJumpPeakSubstep = 1;

	static float ClientAuthorityThresholdOnBaseChange = 0.f;
	static float MaxFallingCorrectionLeash = 0.f;
	static float MaxFallingCorrectionLeashBuffer = 10.f;

	int32 NetShowCorrections = 0;

	float NetCorrectionLifetime = 4.f;
}

namespace MovementComponentCVars
{
	float PenetrationOverlapCheckInflation = 0.100f;
}

namespace CharacterMovementConstants
{
	// MAGIC NUMBERS
	const float MAX_STEP_SIDE_Z = 0.08f;	// maximum z value for the normal on the vertical side of steps
	const float SWIMBOBSPEED = -80.f;
	const float VERTICAL_SLOPE_NORMAL_Z = 0.001f; // Slope is vertical if Abs(Normal.Z) <= this threshold. Accounts for precision problems that sometimes angle normals slightly off horizontal for vertical surface.
}

#pragma endregion



USurfaceNavCharacterMovementComponent::USurfaceNavCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void USurfaceNavCharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);


	if (!PawnOwner || !UpdatedComponent)
	{
		return;
	}

	const AController* Controller = PawnOwner->GetController();
	auto CurRot = UpdatedComponent->GetComponentRotation();
	if (!bSkipNextRotationLerp && !bDisableDefaultRotation && NextRotator != FRotator::ZeroRotator && !NextRotator.Equals(CurRot, 1.f) && Controller && Controller->IsLocalController())
	{

		UpdatedComponent->SetWorldRotation(UKismetMathLibrary::RLerp(CurRot, NextRotator, FMath::Min(1.f, DeltaTime * RotationSpeed), true));
	}

	bSkipNextRotationLerp = false;

}


void USurfaceNavCharacterMovementComponent::HandleRotation(FVector TargetLocation, FVector TargetUpVector, FVector TargetFowardVector)
{
	if (TargetUpVector == FVector::ZeroVector)
		SetNextRotator(FRotator::ZeroRotator);
	else
		SetNextRotator(FRotationMatrix::MakeFromZX(TargetUpVector, TargetFowardVector).Rotator());

	if (OnNeedsPathMoveRotation.IsBound())
	{
		OnNeedsPathMoveRotation.Broadcast(TargetLocation, TargetUpVector, TargetFowardVector);
	}
}



void USurfaceNavCharacterMovementComponent::PhysClimbing(float deltaTime, int32 Iterations)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysWalking);
	//CSV_SCOPED_TIMING_STAT_EXCLUSIVE(CharPhysWalking);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		SetMovementMode(MOVE_Custom, ESurfaceNavMovementMode::MOVE_SN_Climbing);
		return;
	}

	//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysClimbing: Velocity contains NaN before Iteration (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

	bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float remainingTime = deltaTime;

	// Perform the move
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity() || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)))
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Save current values
		UPrimitiveComponent* const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FFindFloorResult OldFloor = CurrentFloor;

		RestorePreAdditiveRootMotionVelocity();

		// Ensure velocity is horizontal.
		//MaintainHorizontalGroundVelocity();
		const FVector OldVelocity = Velocity;
		//Acceleration.Z = 0.f;

		// Apply acceleration
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			CalcVelocity(timeTick, GroundFriction, false, GetMaxBrakingDeceleration());
			//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
		}

		ApplyRootMotionToVelocity(timeTick);
		//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after Root Motion application (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

		if (IsFalling())
		{
			// Root motion could have put us into Falling.
			// No movement has taken place this movement tick so we pass on full time/past iteration count
			StartNewPhysics(remainingTime + timeTick, Iterations - 1);
			return;
		}

		// Compute move parameters
		const FVector MoveVelocity = Velocity;
		const FVector Delta = timeTick * MoveVelocity;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;

		if (bZeroDelta)
		{
			remainingTime = 0.f;
		}
		else
		{
			// try to move along wall
			MoveAlongWall(MoveVelocity, timeTick, &StepDownResult);

			if (IsFalling())
			{
				// pawn decided to jump up
				const float DesiredDist = Delta.Size();
				if (DesiredDist > KINDA_SMALL_NUMBER)
				{
					const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size2D();
					remainingTime += timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));
				}
				StartNewPhysics(remainingTime, Iterations);
				return;
			}
			else if (IsSwimming()) //just entered water
			{
				StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		// Update floor.
		// StepUp might have already done it for us.
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, NULL);
		}

		// check for ledges here
		const bool bCheckLedges = !CanWalkOffLedges();
		if (bCheckLedges && !CurrentFloor.IsWalkableFloor())
		{
			// calculate possible alternate movement
			const FVector GravDir = FVector(0.f, 0.f, -1.f);
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, GravDir);
			if (!NewDelta.IsZero())
			{
				// first revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

				// avoid repeated ledge moves if the first one fails
				bTriedLedgeMove = true;

				// Try new movement direction
				Velocity = NewDelta / timeTick;
				remainingTime += timeTick;
				continue;
			}
			else
			{
				// see if it is OK to jump
				// @todo collision : only thing that can be problem is that oldbase has world collision on
				bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;

				// revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				remainingTime = 0.f;
				break;
			}
		}
		else
		{
			// Validate the floor check
			if (CurrentFloor.IsWalkableFloor())
			{
				if (ShouldCatchAir(OldFloor, CurrentFloor))
				{
					HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
					if (IsMovingOnGround())
					{
						// If still walking, then fall. If not, assume the user set a different mode they want to keep.
						StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation);
					}
					return;
				}

				AdjustFloorHeight();
				SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
			}
			else if (CurrentFloor.HitResult.bStartPenetrating && remainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration
				// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
				FHitResult Hit(CurrentFloor.HitResult);
				Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, MAX_FLOOR_DIST);
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				bForceNextFloorCheck = true;
			}

			// check if just entered water
			if (IsSwimming())
			{
				StartSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}

			// See if we need to start falling.
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				const bool bMustJump = bJustTeleported || bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;
			}
		}


		// Allow overlap events and such to change physics state and velocity
		if (IsMovingOnGround())
		{
			// Make velocity reflect actual move
			if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && timeTick >= MIN_TICK_TIME)
			{
				// TODO-RootMotionSource: Allow this to happen during partial override Velocity, but only set allowed axes?
				Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick;
				MaintainHorizontalGroundVelocity();
			}
		}

		// If we didn't move at all this iteration then abort (since future iterations will also be stuck).
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
	}

	if (IsMovingOnGround())
	{
		MaintainHorizontalGroundVelocity();
	}
}

void USurfaceNavCharacterMovementComponent::PhysNavClimbing(float deltaTime, int32 Iterations)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysNavWalking);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if ((!CharacterOwner || !CharacterOwner->Controller) && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	// Ensure velocity is horizontal.
	MaintainHorizontalGroundVelocity();
	//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysNavWalking: Velocity contains NaN before CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

	//bound acceleration
	Acceleration.Z = 0.f;
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(deltaTime, GroundFriction, false, GetMaxBrakingDeceleration());
		//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysNavWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
	}

	ApplyRootMotionToVelocity(deltaTime);

	if (IsFalling())
	{
		// Root motion could have put us into Falling
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	Iterations++;

	FVector DesiredMove = Velocity;
	DesiredMove.Z = 0.f;

	const FVector OldLocation = GetActorFeetLocation();
	const FVector DeltaMove = DesiredMove * deltaTime;
	const bool bDeltaMoveNearlyZero = DeltaMove.IsNearlyZero();

	FVector AdjustedDest = OldLocation + DeltaMove;
	FNavLocation DestNavLocation;

	bool bSameNavLocation = false;
	if (CachedNavLocation.NodeRef != INVALID_NAVNODEREF)
	{
		if (bProjectNavMeshWalking)
		{
			const float DistSq2D = (OldLocation - CachedNavLocation.Location).SizeSquared2D();
			const float DistZ = FMath::Abs(OldLocation.Z - CachedNavLocation.Location.Z);

			const float TotalCapsuleHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
			const float ProjectionScale = (OldLocation.Z > CachedNavLocation.Location.Z) ? NavMeshProjectionHeightScaleUp : NavMeshProjectionHeightScaleDown;
			const float DistZThr = TotalCapsuleHeight * FMath::Max(0.f, ProjectionScale);

			bSameNavLocation = (DistSq2D <= KINDA_SMALL_NUMBER) && (DistZ < DistZThr);
		}
		else
		{
			bSameNavLocation = CachedNavLocation.Location.Equals(OldLocation);
		}

		if (bDeltaMoveNearlyZero && bSameNavLocation)
		{
			if (const INavigationDataInterface* NavData = GetNavData())
			{
				if (!NavData->IsNodeRefValid(CachedNavLocation.NodeRef))
				{
					CachedNavLocation.NodeRef = INVALID_NAVNODEREF;
					bSameNavLocation = false;
				}
			}
		}
	}

	if (bDeltaMoveNearlyZero && bSameNavLocation)
	{
		DestNavLocation = CachedNavLocation;
		//UE_LOG(LogNavMeshMovement, VeryVerbose, TEXT("%s using cached navmesh location! (bProjectNavMeshWalking = %d)"), *GetNameSafe(CharacterOwner), bProjectNavMeshWalking);
	}
	else
	{
		//SCOPE_CYCLE_COUNTER(STAT_CharNavProjectPoint);

		// Start the trace from the Z location of the last valid trace.
		// Otherwise if we are projecting our location to the underlying geometry and it's far above or below the navmesh,
		// we'll follow that geometry's plane out of range of valid navigation.
		if (bSameNavLocation && bProjectNavMeshWalking)
		{
			AdjustedDest.Z = CachedNavLocation.Location.Z;
		}

		// Find the point on the NavMesh
		const bool bHasNavigationData = FindNavFloor(AdjustedDest, DestNavLocation);
		if (!bHasNavigationData)
		{
			SetMovementMode(MOVE_Walking);
			return;
		}

		CachedNavLocation = DestNavLocation;
	}

	if (DestNavLocation.NodeRef != INVALID_NAVNODEREF)
	{
		FVector NewLocation(AdjustedDest.X, AdjustedDest.Y, DestNavLocation.Location.Z);
		if (bProjectNavMeshWalking)
		{
			//SCOPE_CYCLE_COUNTER(STAT_CharNavProjectLocation);
			const float TotalCapsuleHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
			const float UpOffset = TotalCapsuleHeight * FMath::Max(0.f, NavMeshProjectionHeightScaleUp);
			const float DownOffset = TotalCapsuleHeight * FMath::Max(0.f, NavMeshProjectionHeightScaleDown);
			NewLocation = ProjectLocationFromNavMesh(deltaTime, OldLocation, NewLocation, UpOffset, DownOffset);
		}

		FVector AdjustedDelta = NewLocation - OldLocation;

		if (!AdjustedDelta.IsNearlyZero())
		{
			FHitResult HitResult;
			SafeMoveUpdatedComponent(AdjustedDelta, UpdatedComponent->GetComponentQuat(), bSweepWhileNavWalking, HitResult);
		}

		// Update velocity to reflect actual move
		if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasVelocity())
		{
			Velocity = (GetActorFeetLocation() - OldLocation) / deltaTime;
			MaintainHorizontalGroundVelocity();
		}

		bJustTeleported = false;
	}
	else
	{
		StartFalling(Iterations, deltaTime, deltaTime, DeltaMove, OldLocation);
	}
}



void USurfaceNavCharacterMovementComponent::StartClimbing(FVector OldLocation, FVector OldVelocity, float timeTick, float remainingTime, int32 Iterations)
{
	//if (remainingTime < MIN_TICK_TIME || timeTick < MIN_TICK_TIME)
	//{
	//	return;
	//}

	//if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && !bJustTeleported)
	//{
	//	Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick; //actual average velocity
	//	Velocity = 2.f * Velocity - OldVelocity; //end velocity has 2* accel of avg
	//	Velocity = Velocity.GetClampedToMaxSize(GetPhysicsVolume()->TerminalVelocity);
	//}
	//const FVector End = FindWaterLine(UpdatedComponent->GetComponentLocation(), OldLocation);
	//float waterTime = 0.f;
	//if (End != UpdatedComponent->GetComponentLocation())
	//{
	//	const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size();
	//	if (ActualDist > KINDA_SMALL_NUMBER)
	//	{
	//		waterTime = timeTick * (End - UpdatedComponent->GetComponentLocation()).Size() / ActualDist;
	//		remainingTime += waterTime;
	//	}
	//	MoveUpdatedComponent(End - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true);
	//}
	//if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (Velocity.Z > 2.f * CharacterMovementConstants::SWIMBOBSPEED) && (Velocity.Z < 0.f)) //allow for falling out of water
	//{
	//	Velocity.Z = CharacterMovementConstants::SWIMBOBSPEED - Velocity.Size2D() * 0.7f; //smooth bobbing
	//}
	//if ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	//{
	//	PhysSwimming(remainingTime, Iterations);
	//}
}

// Not sure I need this one
float USurfaceNavCharacterMovementComponent::Climb(FVector Delta, FHitResult& Hit)
{
	FVector Start = UpdatedComponent->GetComponentLocation();
	float airTime = 0.f;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	//if (!GetPhysicsVolume()->bWaterVolume) //then left water
	//{
	//	const FVector End = FindWaterLine(Start, UpdatedComponent->GetComponentLocation());
	//	const float DesiredDist = Delta.Size();
	//	if (End != UpdatedComponent->GetComponentLocation() && DesiredDist > KINDA_SMALL_NUMBER)
	//	{
	//		airTime = (End - UpdatedComponent->GetComponentLocation()).Size() / DesiredDist;
	//		if (((UpdatedComponent->GetComponentLocation() - Start) | (End - UpdatedComponent->GetComponentLocation())) > 0.f)
	//		{
	//			airTime = 0.f;
	//		}
	//		SafeMoveUpdatedComponent(End - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, Hit);
	//	}
	//}
	return airTime;
}


void USurfaceNavCharacterMovementComponent::PhysHanging(float deltaTime, int32 Iterations)
{
	//if (deltaTime < MIN_TICK_TIME)
	//{
	//	return;
	//}

	//RestorePreAdditiveRootMotionVelocity();

	//float NetFluidFriction = 0.f;
	//float Depth = ImmersionDepth();
	//float NetBuoyancy = Buoyancy * Depth;
	//float OriginalAccelZ = Acceleration.Z;
	//bool bLimitedUpAccel = false;

	//if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (Velocity.Z > 0.33f * MaxSwimSpeed) && (NetBuoyancy != 0.f))
	//{
	//	//damp positive Z out of water
	//	Velocity.Z = FMath::Max<FVector::FReal>(0.33f * MaxSwimSpeed, Velocity.Z * Depth * Depth);
	//}
	//else if (Depth < 0.65f)
	//{
	//	bLimitedUpAccel = (Acceleration.Z > 0.f);
	//	Acceleration.Z = FMath::Min<FVector::FReal>(0.1f, Acceleration.Z);
	//}

	//Iterations++;
	//FVector OldLocation = UpdatedComponent->GetComponentLocation();
	//bJustTeleported = false;
	//if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	//{
	//	const float Friction = 0.5f * GetPhysicsVolume()->FluidFriction * Depth;
	//	CalcVelocity(deltaTime, Friction, true, GetMaxBrakingDeceleration());
	//	Velocity.Z += GetGravityZ() * deltaTime * (1.f - NetBuoyancy);
	//}

	//ApplyRootMotionToVelocity(deltaTime);

	//FVector Adjusted = Velocity * deltaTime;
	//FHitResult Hit(1.f);
	//float remainingTime = deltaTime * Swim(Adjusted, Hit);

	////may have left water - if so, script might have set new physics mode
	//if (!IsSwimming())
	//{
	//	StartNewPhysics(remainingTime, Iterations);
	//	return;
	//}

	//if (Hit.Time < 1.f && CharacterOwner)
	//{
	//	HandleSwimmingWallHit(Hit, deltaTime);
	//	if (bLimitedUpAccel && (Velocity.Z >= 0.f))
	//	{
	//		// allow upward velocity at surface if against obstacle
	//		Velocity.Z += OriginalAccelZ * deltaTime;
	//		Adjusted = Velocity * (1.f - Hit.Time) * deltaTime;
	//		Swim(Adjusted, Hit);
	//		if (!IsSwimming())
	//		{
	//			StartNewPhysics(remainingTime, Iterations);
	//			return;
	//		}
	//	}

	//	const FVector GravDir = FVector(0.f, 0.f, -1.f);
	//	const FVector VelDir = Velocity.GetSafeNormal();
	//	const float UpDown = GravDir | VelDir;

	//	bool bSteppedUp = false;
	//	if ((FMath::Abs(Hit.ImpactNormal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) && CanStepUp(Hit))
	//	{
	//		float stepZ = UpdatedComponent->GetComponentLocation().Z;
	//		const FVector RealVelocity = Velocity;
	//		Velocity.Z = 1.f;	// HACK: since will be moving up, in case pawn leaves the water
	//		bSteppedUp = StepUp(GravDir, Adjusted * (1.f - Hit.Time), Hit);
	//		if (bSteppedUp)
	//		{
	//			//may have left water - if so, script might have set new physics mode
	//			if (!IsSwimming())
	//			{
	//				StartNewPhysics(remainingTime, Iterations);
	//				return;
	//			}
	//			OldLocation.Z = UpdatedComponent->GetComponentLocation().Z + (OldLocation.Z - stepZ);
	//		}
	//		Velocity = RealVelocity;
	//	}

	//	if (!bSteppedUp)
	//	{
	//		//adjust and try again
	//		HandleImpact(Hit, deltaTime, Adjusted);
	//		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	//	}
	//}

	//if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && !bJustTeleported && ((deltaTime - remainingTime) > KINDA_SMALL_NUMBER) && CharacterOwner)
	//{
	//	bool bWaterJump = !GetPhysicsVolume()->bWaterVolume;
	//	float velZ = Velocity.Z;
	//	Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / (deltaTime - remainingTime);
	//	if (bWaterJump)
	//	{
	//		Velocity.Z = velZ;
	//	}
	//}

	//if (!GetPhysicsVolume()->bWaterVolume && IsSwimming())
	//{
	//	SetMovementMode(MOVE_Falling); //in case script didn't change it (w/ zone change)
	//}

	////may have left water - if so, script might have set new physics mode
	//if (!IsSwimming())
	//{
	//	StartNewPhysics(remainingTime, Iterations);
	//}
}


void USurfaceNavCharacterMovementComponent::StartHanging(FVector OldLocation, FVector OldVelocity, float timeTick, float remainingTime, int32 Iterations)
{
	//if (remainingTime < MIN_TICK_TIME || timeTick < MIN_TICK_TIME)
	//{
	//	return;
	//}

	//if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && !bJustTeleported)
	//{
	//	Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick; //actual average velocity
	//	Velocity = 2.f * Velocity - OldVelocity; //end velocity has 2* accel of avg
	//	Velocity = Velocity.GetClampedToMaxSize(GetPhysicsVolume()->TerminalVelocity);
	//}
	//const FVector End = FindWaterLine(UpdatedComponent->GetComponentLocation(), OldLocation);
	//float waterTime = 0.f;
	//if (End != UpdatedComponent->GetComponentLocation())
	//{
	//	const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size();
	//	if (ActualDist > KINDA_SMALL_NUMBER)
	//	{
	//		waterTime = timeTick * (End - UpdatedComponent->GetComponentLocation()).Size() / ActualDist;
	//		remainingTime += waterTime;
	//	}
	//	MoveUpdatedComponent(End - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true);
	//}
	//if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (Velocity.Z > 2.f * CharacterMovementConstants::SWIMBOBSPEED) && (Velocity.Z < 0.f)) //allow for falling out of water
	//{
	//	Velocity.Z = CharacterMovementConstants::SWIMBOBSPEED - Velocity.Size2D() * 0.7f; //smooth bobbing
	//}
	//if ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	//{
	//	PhysSwimming(remainingTime, Iterations);
	//}
}

// Not sure I need this one
float USurfaceNavCharacterMovementComponent::Hang(FVector Delta, FHitResult& Hit)
{
	FVector Start = UpdatedComponent->GetComponentLocation();
	float airTime = 0.f;
	//SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	//if (!GetPhysicsVolume()->bWaterVolume) //then left water
	//{
	//	const FVector End = FindWaterLine(Start, UpdatedComponent->GetComponentLocation());
	//	const float DesiredDist = Delta.Size();
	//	if (End != UpdatedComponent->GetComponentLocation() && DesiredDist > KINDA_SMALL_NUMBER)
	//	{
	//		airTime = (End - UpdatedComponent->GetComponentLocation()).Size() / DesiredDist;
	//		if (((UpdatedComponent->GetComponentLocation() - Start) | (End - UpdatedComponent->GetComponentLocation())) > 0.f)
	//		{
	//			airTime = 0.f;
	//		}
	//		SafeMoveUpdatedComponent(End - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, Hit);
	//	}
	//}
	return airTime;
}


void USurfaceNavCharacterMovementComponent::MoveAlongWall(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	// Need to figure out what we need here.

	// Are we going up (opposite gravity) 

	// 

	//

	if (CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	// Move along the current floor
	const FVector Delta = FVector(InVelocity.X, InVelocity.Y, InVelocity.Z) * DeltaSeconds;
	FHitResult Hit(1.f);
	FVector RampVector = ComputeGroundMovementDelta(Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);
	float LastMoveTimeSlice = DeltaSeconds;

	if (Hit.bStartPenetrating)
	{
		// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch.
		HandleImpact(Hit);
		SlideAlongSurface(Delta, 1.f, Hit.Normal, Hit, true);

		if (Hit.bStartPenetrating)
		{
			OnCharacterStuckInGeometry(&Hit);
		}
	}
	else if (Hit.IsValidBlockingHit())
	{
		// We impacted something (most likely another ramp, but possibly a barrier).
		float PercentTimeApplied = Hit.Time;
		if ((Hit.Time > 0.f) && (Hit.Normal.Z > KINDA_SMALL_NUMBER) && IsWalkable(Hit))
		{
			// Another walkable ramp.
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			RampVector = ComputeGroundMovementDelta(Delta * InitialPercentRemaining, Hit, false);
			LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
			SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);

			const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
		}

		if (Hit.IsValidBlockingHit())
		{
			if (CanStepUp(Hit) || (CharacterOwner->GetMovementBase() != nullptr && Hit.HitObjectHandle == CharacterOwner->GetMovementBase()->GetOwner()))
			{
				// hit a barrier, try to step up
				const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
				const FVector GravDir(0.f, 0.f, -1.f);
				if (!StepUp(GravDir, Delta * (1.f - PercentTimeApplied), Hit, OutStepDownResult))
				{
					//UE_LOG(LogCharacterMovement, Verbose, TEXT("- StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					HandleImpact(Hit, LastMoveTimeSlice, RampVector);
					SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
				}
				else
				{
					//UE_LOG(LogCharacterMovement, Verbose, TEXT("+ StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					if (!bMaintainHorizontalGroundVelocity)
					{
						// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments. Only consider horizontal movement.
						bJustTeleported = true;
						const float StepUpTimeSlice = (1.f - PercentTimeApplied) * DeltaSeconds;
						if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && StepUpTimeSlice >= KINDA_SMALL_NUMBER)
						{
							Velocity = (UpdatedComponent->GetComponentLocation() - PreStepUpLocation) / StepUpTimeSlice;
							Velocity.Z = 0;
						}
					}
				}
			}
			else if (Hit.Component.IsValid() && !Hit.Component.Get()->CanCharacterStepUp(CharacterOwner))
			{
				HandleImpact(Hit, LastMoveTimeSlice, RampVector);
				SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
			}
		}
	}
}

inline FVector USurfaceNavCharacterMovementComponent::GetGravityNormal() const
{
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
}

inline FVector USurfaceNavCharacterMovementComponent::GetGroundNormal() const
{
	return GetGravityNormal() * -1.f;
}

inline FVector USurfaceNavCharacterMovementComponent::GetGravity() const
{
	return GetGravityNormal() * GetGravityZ();
}


#pragma region These are under construction

// *****************************************************
// *****************************************************
// These are under construction.
// *****************************************************

void USurfaceNavCharacterMovementComponent::StartNewPhysics(float deltaTime, int32 Iterations)
{
	if ((deltaTime < MIN_TICK_TIME) || (Iterations >= MaxSimulationIterations) || !HasValidData())
	{
		return;
	}

	if (UpdatedComponent->IsSimulatingPhysics())
	{
		//UE_LOG(LogCharacterMovement, Log, TEXT("UCharacterMovementComponent::StartNewPhysics: UpdateComponent (%s) is simulating physics - aborting."), *UpdatedComponent->GetPathName());
		return;
	}

	const bool bSavedMovementInProgress = bMovementInProgress;
	bMovementInProgress = true;

	switch (MovementMode)
	{
	case MOVE_None:
		break;
	case MOVE_Walking:

		switch (SurfaceNavMovementMode)
		{
		case MOVE_SN_Walking:
			PhysWalking(deltaTime, Iterations);
			break;
		case MOVE_SN_Climbing:
			PhysClimbing(deltaTime, Iterations);
			break;
		case MOVE_SN_Hanging:
			// TODO: Change max braking
			PhysWalking(deltaTime, Iterations);
			break;
		default:
			PhysWalking(deltaTime, Iterations);
			break;
		}

		break;
	case MOVE_NavWalking:

		switch (SurfaceNavMovementMode)
		{
		case MOVE_SN_Walking:
			PhysNavWalking(deltaTime, Iterations);
			break;
		case MOVE_SN_Climbing:
			PhysNavClimbing(deltaTime, Iterations);
			break;
		case MOVE_SN_Hanging:
			// TODO: Change max braking
			PhysNavWalking(deltaTime, Iterations);
			break;
		default:
			PhysNavWalking(deltaTime, Iterations);
			break;
		}

		break;
	case MOVE_Falling:
		PhysFalling(deltaTime, Iterations);
		break;
	case MOVE_Flying:
		PhysFlying(deltaTime, Iterations);
		break;
	case MOVE_Swimming:
		PhysSwimming(deltaTime, Iterations);
		break;
	case MOVE_Custom:
		PhysCustom(deltaTime, Iterations);
		break;
	default:
		//UE_LOG(LogCharacterMovement, Warning, TEXT("%s has unsupported movement mode %d"), *CharacterOwner->GetName(), int32(MovementMode));
		SetMovementMode(MOVE_None);
		break;
	}

	bMovementInProgress = bSavedMovementInProgress;
	if (bDeferUpdateMoveComponent)
	{
		SetUpdatedComponent(DeferredUpdatedMoveComponent);
	}
}

void USurfaceNavCharacterMovementComponent::SetDefaultMovementMode()
{
	// check for water volume
	if (CanEverSwim() && IsInWater())
	{
		SetMovementMode(DefaultWaterMovementMode);
	}
	else if (!CharacterOwner || MovementMode != DefaultLandMovementMode)
	{
		const float SavedVelocityZ = Velocity.Z;
		SetMovementMode(DefaultLandMovementMode);

		// Avoid 1-frame delay if trying to walk but walking fails at this location.
		if (MovementMode == MOVE_Walking && GetMovementBase() == NULL)
		{
			Velocity.Z = SavedVelocityZ; // Prevent temporary walking state from zeroing Z velocity.
			SetMovementMode(MOVE_Falling);
		}
	}
}

float USurfaceNavCharacterMovementComponent::GetMaxSpeed() const
{
	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:

		switch (SurfaceNavMovementMode)
		{
		case MOVE_SN_Walking:
			return IsCrouching() ? MaxWalkSpeedCrouched : MaxWalkSpeed;
		case MOVE_SN_Climbing:
			// TODO: Change max braking
			return IsCrouching() ? MaxWalkSpeedCrouched : MaxWalkSpeed;
		case MOVE_SN_Hanging:
			// TODO: Change max braking
			return IsCrouching() ? MaxWalkSpeedCrouched : MaxWalkSpeed;
		default:
			return IsCrouching() ? MaxWalkSpeedCrouched : MaxWalkSpeed;
		}

	case MOVE_Falling:
		return MaxWalkSpeed;
	case MOVE_Swimming:
		return MaxSwimSpeed;
	case MOVE_Flying:
		return MaxFlySpeed;
	case MOVE_Custom:
		return MaxCustomMovementSpeed;
	case MOVE_None:
	default:
		return 0.f;
	}
}

float USurfaceNavCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:

		switch (SurfaceNavMovementMode)
		{
		case MOVE_SN_Walking:
			return BrakingDecelerationWalking;
		case MOVE_SN_Climbing:
			// TODO: Change max braking
			return BrakingDecelerationWalking;
		case MOVE_SN_Hanging:
			// TODO: Change max braking
			return BrakingDecelerationWalking;
		}

	case MOVE_Falling:
		return BrakingDecelerationFalling;
	case MOVE_Swimming:
		return BrakingDecelerationSwimming;
	case MOVE_Flying:
		return BrakingDecelerationFlying;
	case MOVE_Custom:
		return 0.f;
	case MOVE_None:
	default:
		return 0.f;
	}
}




bool USurfaceNavCharacterMovementComponent::FindNavFloor(const FVector& TestLocation, FNavLocation& NavFloorLocation) const
{
	const INavigationDataInterface* NavData = GetNavData();
	if (NavData == nullptr || CharacterOwner == nullptr)
	{
		return false;
	}

	const FNavAgentProperties& AgentProps = CharacterOwner->GetNavAgentPropertiesRef();
	const float SearchRadius = AgentProps.AgentRadius * 2.0f;
	const float SearchHeight = AgentProps.AgentHeight * AgentProps.NavWalkingSearchHeightScale;

	return NavData->ProjectPoint(TestLocation, NavFloorLocation, FVector(SearchRadius, SearchRadius, SearchHeight));
}


void USurfaceNavCharacterMovementComponent::ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& RelativeClientLoc, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	if (!ShouldUsePackedMovementRPCs())
	{
		if (RelativeClientLoc == FVector(1.f, 2.f, 3.f)) // first part of double servermove
		{
			return;
		}
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	// Don't prevent more recent updates from being sent if received this frame.
	// We're going to send out an update anyway, might as well be the most recent one.
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if ((ServerData->LastUpdateTime != GetWorld()->TimeSeconds))
	{
		const AGameNetworkManager* GameNetworkManager = (const AGameNetworkManager*)(AGameNetworkManager::StaticClass()->GetDefaultObject());
		if (GameNetworkManager->WithinUpdateDelayBounds(PC, ServerData->LastUpdateTime))
		{
			return;
		}
	}

	// Offset may be relative to base component
	FVector ClientLoc = RelativeClientLoc;
	if (MovementBaseUtility::UseRelativeLocation(ClientMovementBase))
	{
		FVector BaseLocation;
		FQuat BaseRotation;
		MovementBaseUtility::GetMovementBaseTransform(ClientMovementBase, ClientBaseBoneName, BaseLocation, BaseRotation);
		ClientLoc += BaseLocation;
	}
	else
	{
		ClientLoc = FRepMovement::RebaseOntoLocalOrigin(ClientLoc, this);
	}

	FVector ServerLoc = UpdatedComponent->GetComponentLocation();

	// Client may send a null movement base when walking on bases with no relative location (to save bandwidth).
	// In this case don't check movement base in error conditions, use the server one (which avoids an error based on differing bases). Position will still be validated.
	if (ClientMovementBase == nullptr)
	{
		TEnumAsByte<EMovementMode> NetMovementMode(MOVE_None);
		TEnumAsByte<EMovementMode> NetGroundMode(MOVE_None);
		uint8 NetCustomMode(0);
		UnpackNetworkMovementMode(ClientMovementMode, NetMovementMode, NetCustomMode, NetGroundMode);
		if (NetMovementMode == MOVE_Walking)
		{
			ClientMovementBase = CharacterOwner->GetBasedMovement().MovementBase;
			ClientBaseBoneName = CharacterOwner->GetBasedMovement().BoneName;
		}
	}

	// If base location is out of sync on server and client, changing base can result in a jarring correction.
	// So in the case that the base has just changed on server or client, server trusts the client (within a threshold)
	UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
	FName MovementBaseBoneName = CharacterOwner->GetBasedMovement().BoneName;
	const bool bServerIsFalling = IsFalling();
	const bool bClientIsFalling = ClientMovementMode == MOVE_Falling;
	const bool bServerJustLanded = bLastServerIsFalling && !bServerIsFalling;
	const bool bClientJustLanded = bLastClientIsFalling && !bClientIsFalling;

	FVector RelativeLocation = ServerLoc;
	FVector RelativeVelocity = Velocity;
	bool bUseLastBase = false;
	bool bFallingWithinAcceptableError = false;

	// Potentially trust the client a little when landing
	const float ClientAuthorityThreshold = CharacterMovementCVars::ClientAuthorityThresholdOnBaseChange;
	const float MaxFallingCorrectionLeash = CharacterMovementCVars::MaxFallingCorrectionLeash;
	const bool bDeferServerCorrectionsWhenFalling = ClientAuthorityThreshold > 0.f || MaxFallingCorrectionLeash > 0.f;
	if (bDeferServerCorrectionsWhenFalling)
	{
		// Teleports and other movement modes mean we should just trust the server like we normally would
		if (bTeleportedSinceLastUpdate || (MovementMode != MOVE_Walking && MovementMode != MOVE_Falling))
		{
			MaxServerClientErrorWhileFalling = 0.f;
			bCanTrustClientOnLanding = false;
		}

		// MaxFallingCorrectionLeash indicates we'll use a variable correction size based on the error on take-off and the direction of movement.
		// ClientAuthorityThreshold is an static client-trusting correction upon landing.
		// If both are set, use the smaller of the two. If only one is set, use that. If neither are set, we wouldn't even be inside this block.
		float MaxLandingCorrection = 0.f;
		if (ClientAuthorityThreshold > 0.f && MaxFallingCorrectionLeash > 0.f)
		{
			MaxLandingCorrection = FMath::Min(ClientAuthorityThreshold, MaxServerClientErrorWhileFalling);
		}
		else
		{
			MaxLandingCorrection = FMath::Max(ClientAuthorityThreshold, MaxServerClientErrorWhileFalling);
		}

		if (bCanTrustClientOnLanding && MaxLandingCorrection > 0.f && (bClientJustLanded || bServerJustLanded))
		{
			// no longer falling; server should trust client up to a point to finish the landing as the client sees it
			const FVector LocDiff = ServerLoc - ClientLoc;

			if (!LocDiff.IsNearlyZero(KINDA_SMALL_NUMBER))
			{
				if (LocDiff.SizeSquared() < FMath::Square(MaxLandingCorrection))
				{
					ServerLoc = ClientLoc;
					UpdatedComponent->MoveComponent(ServerLoc - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
					bJustTeleported = true;
				}
				else
				{
					const FVector ClampedDiff = LocDiff.GetSafeNormal() * MaxLandingCorrection;
					ServerLoc -= ClampedDiff;
					UpdatedComponent->MoveComponent(ServerLoc - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
					bJustTeleported = true;
				}
			}

			MaxServerClientErrorWhileFalling = 0.f;
			bCanTrustClientOnLanding = false;
		}

		if (bServerIsFalling && bLastServerIsWalking && !bTeleportedSinceLastUpdate)
		{
			float ClientForwardFactor = 1.f;
			UPrimitiveComponent* LastServerMovementBasePtr = LastServerMovementBase.Get();
			if (IsValid(LastServerMovementBasePtr) && MovementBaseUtility::IsDynamicBase(LastServerMovementBasePtr) && MaxWalkSpeed > KINDA_SMALL_NUMBER)
			{
				const FVector LastBaseVelocity = MovementBaseUtility::GetMovementBaseVelocity(LastServerMovementBasePtr, LastServerMovementBaseBoneName);
				RelativeVelocity = Velocity - LastBaseVelocity;
				const FVector BaseDirection = LastBaseVelocity.GetSafeNormal2D();
				const FVector RelativeDirection = RelativeVelocity * (1.f / MaxWalkSpeed);

				ClientForwardFactor = FMath::Clamp(FVector::DotProduct(BaseDirection, RelativeDirection), 0.f, 1.f);

				// To improve position syncing, use old base for take-off
				if (MovementBaseUtility::UseRelativeLocation(LastServerMovementBasePtr))
				{
					FVector BaseLocation;
					FQuat BaseQuat;
					MovementBaseUtility::GetMovementBaseTransform(LastServerMovementBasePtr, LastServerMovementBaseBoneName, BaseLocation, BaseQuat);

					// Relative Location
					RelativeLocation = UpdatedComponent->GetComponentLocation() - BaseLocation;
					bUseLastBase = true;
				}
			}

			if (ClientAuthorityThreshold > 0.f && ClientForwardFactor < 1.f)
			{
				const float AdjustedClientAuthorityThreshold = ClientAuthorityThreshold * (1.f - ClientForwardFactor);
				const FVector LocDiff = ServerLoc - ClientLoc;

				// Potentially trust the client a little when taking off in the opposite direction to the base (to help not get corrected back onto the base)
				if (!LocDiff.IsNearlyZero(KINDA_SMALL_NUMBER))
				{
					if (LocDiff.SizeSquared() < FMath::Square(AdjustedClientAuthorityThreshold))
					{
						ServerLoc = ClientLoc;
						UpdatedComponent->MoveComponent(ServerLoc - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
						bJustTeleported = true;
					}
					else
					{
						const FVector ClampedDiff = LocDiff.GetSafeNormal() * AdjustedClientAuthorityThreshold;
						ServerLoc -= ClampedDiff;
						UpdatedComponent->MoveComponent(ServerLoc - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
						bJustTeleported = true;
					}
				}
			}

			if (ClientForwardFactor < 1.f)
			{
				MaxServerClientErrorWhileFalling = FMath::Min((ServerLoc - ClientLoc).Size() * (1.f - ClientForwardFactor), MaxFallingCorrectionLeash);
				bCanTrustClientOnLanding = true;
			}
			else
			{
				MaxServerClientErrorWhileFalling = 0.f;
				bCanTrustClientOnLanding = false;
			}
		}
		else if (!bServerIsFalling && bCanTrustClientOnLanding)
		{
			MaxServerClientErrorWhileFalling = 0.f;
			bCanTrustClientOnLanding = false;
		}

		if (MaxServerClientErrorWhileFalling > 0.f && (bServerIsFalling || bClientIsFalling))
		{
			const FVector LocDiff = ServerLoc - ClientLoc;
			if (LocDiff.SizeSquared() <= FMath::Square(MaxServerClientErrorWhileFalling))
			{
				ServerLoc = ClientLoc;
				// Still want a velocity update when we first take off
				bFallingWithinAcceptableError = true;
			}
			else
			{
				// Change ServerLoc to be on the edge of the acceptable error rather than doing a full correction.
				// This is not actually changing the server position, but changing it as far as corrections are concerned.
				// This means we're just holding the client on a longer leash while we're falling.
				ServerLoc = ServerLoc - LocDiff.GetSafeNormal() * FMath::Clamp(MaxServerClientErrorWhileFalling - CharacterMovementCVars::MaxFallingCorrectionLeashBuffer, 0.f, MaxServerClientErrorWhileFalling);
			}
		}
	}

	// Compute the client error from the server's position
	// If client has accumulated a noticeable positional error, correct them.
	bNetworkLargeClientCorrection = ServerData->bForceClientUpdate;
	if (ServerData->bForceClientUpdate || (!bFallingWithinAcceptableError && ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientLoc, RelativeClientLoc, ClientMovementBase, ClientBaseBoneName, ClientMovementMode)))
	{
		ServerData->PendingAdjustment.NewVel = Velocity;
		ServerData->PendingAdjustment.NewBase = MovementBase;
		ServerData->PendingAdjustment.NewBaseBoneName = MovementBaseBoneName;
		ServerData->PendingAdjustment.NewLoc = FRepMovement::RebaseOntoZeroOrigin(ServerLoc, this);
		ServerData->PendingAdjustment.NewRot = UpdatedComponent->GetComponentRotation();

		ServerData->PendingAdjustment.bBaseRelativePosition = (bDeferServerCorrectionsWhenFalling && bUseLastBase) || MovementBaseUtility::UseRelativeLocation(MovementBase);
		if (ServerData->PendingAdjustment.bBaseRelativePosition)
		{
			// Relative location
			if (bDeferServerCorrectionsWhenFalling && bUseLastBase)
			{
				ServerData->PendingAdjustment.NewVel = RelativeVelocity;
				ServerData->PendingAdjustment.NewBase = LastServerMovementBase.Get();
				ServerData->PendingAdjustment.NewBaseBoneName = LastServerMovementBaseBoneName;
				ServerData->PendingAdjustment.NewLoc = RelativeLocation;
			}
			else
			{
				ServerData->PendingAdjustment.NewLoc = CharacterOwner->GetBasedMovement().Location;
			}

			// TODO: this could be a relative rotation, but all client corrections ignore rotation right now except the root motion one, which would need to be updated.
			//ServerData->PendingAdjustment.NewRot = CharacterOwner->GetBasedMovement().Rotation;
		}


#if !UE_BUILD_SHIPPING
		if (CharacterMovementCVars::NetShowCorrections != 0)
		{
			const FVector LocDiff = UpdatedComponent->GetComponentLocation() - ClientLoc;
			const FString BaseString = MovementBase ? MovementBase->GetPathName(MovementBase->GetOutermost()) : TEXT("None");
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("*** Server: Error for %s at Time=%.3f is %3.3f LocDiff(%s) ClientLoc(%s) ServerLoc(%s) Base: %s Bone: %s Accel(%s) Velocity(%s)"),
				*GetNameSafe(CharacterOwner), ClientTimeStamp, LocDiff.Size(), *LocDiff.ToString(), *ClientLoc.ToString(), *UpdatedComponent->GetComponentLocation().ToString(), *BaseString, *ServerData->PendingAdjustment.NewBaseBoneName.ToString(), *Accel.ToString(), *Velocity.ToString());
			const float DebugLifetime = CharacterMovementCVars::NetCorrectionLifetime;
			DrawDebugCapsule(GetWorld(), UpdatedComponent->GetComponentLocation(), CharacterOwner->GetSimpleCollisionHalfHeight(), CharacterOwner->GetSimpleCollisionRadius(), FQuat::Identity, FColor(100, 255, 100), false, DebugLifetime);
			DrawDebugCapsule(GetWorld(), ClientLoc, CharacterOwner->GetSimpleCollisionHalfHeight(), CharacterOwner->GetSimpleCollisionRadius(), FQuat::Identity, FColor(255, 100, 100), false, DebugLifetime);
		}
#endif

		ServerData->LastUpdateTime = GetWorld()->TimeSeconds;
		ServerData->PendingAdjustment.DeltaTime = DeltaTime;
		ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp;
		ServerData->PendingAdjustment.bAckGoodMove = false;
		ServerData->PendingAdjustment.MovementMode = PackNetworkMovementMode();

#if USE_SERVER_PERF_COUNTERS
		//PerfCountersIncrement(PerfCounter_NumServerMoveCorrections);
#endif
	}
	else
	{
		if (ServerShouldUseAuthoritativePosition(ClientTimeStamp, DeltaTime, Accel, ClientLoc, RelativeClientLoc, ClientMovementBase, ClientBaseBoneName, ClientMovementMode))
		{
			const FVector LocDiff = UpdatedComponent->GetComponentLocation() - ClientLoc; //-V595
			if (!LocDiff.IsZero() || ClientMovementMode != PackNetworkMovementMode() || GetMovementBase() != ClientMovementBase || (CharacterOwner && CharacterOwner->GetBasedMovement().BoneName != ClientBaseBoneName))
			{
				// Just set the position. On subsequent moves we will resolve initially overlapping conditions.
				UpdatedComponent->SetWorldLocation(ClientLoc, false); //-V595

				// Trust the client's movement mode.
				ApplyNetworkMovementMode(ClientMovementMode);

				// Update base and floor at new location.
				SetBase(ClientMovementBase, ClientBaseBoneName);
				UpdateFloorFromAdjustment();

				// Even if base has not changed, we need to recompute the relative offsets (since we've moved).
				SaveBaseLocation();

				LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
				LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
				LastUpdateVelocity = Velocity;
			}
		}

		// acknowledge receipt of this successful servermove()
		ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp;
		ServerData->PendingAdjustment.bAckGoodMove = true;
	}

#if USE_SERVER_PERF_COUNTERS
	//PerfCountersIncrement(PerfCounter_NumServerMoves);
#endif

	ServerData->bForceClientUpdate = false;

	LastServerMovementBase = MovementBase;
	LastServerMovementBaseBoneName = MovementBaseBoneName;
	bLastClientIsFalling = bClientIsFalling;
	bLastServerIsFalling = bServerIsFalling;
	bLastServerIsWalking = MovementMode == MOVE_Walking;
}

void USurfaceNavCharacterMovementComponent::ApplyRepulsionForce(float DeltaSeconds)
{
	if (UpdatedPrimitive && RepulsionForce > 0.0f && CharacterOwner != nullptr)
	{
		const TArray<FOverlapInfo>& Overlaps = UpdatedPrimitive->GetOverlapInfos();
		if (Overlaps.Num() > 0)
		{
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMC_ApplyRepulsionForce));
			QueryParams.bReturnFaceIndex = false;
			QueryParams.bReturnPhysicalMaterial = false;

			float CapsuleRadius = 0.f;
			float CapsuleHalfHeight = 0.f;
			CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
			const float RepulsionForceRadius = CapsuleRadius * 1.2f;
			const float StopBodyDistance = 2.5f;
			const FVector MyLocation = UpdatedPrimitive->GetComponentLocation();

			for (int32 i = 0; i < Overlaps.Num(); i++)
			{
				const FOverlapInfo& Overlap = Overlaps[i];

				UPrimitiveComponent* OverlapComp = Overlap.OverlapInfo.Component.Get();
				if (!OverlapComp || OverlapComp->Mobility < EComponentMobility::Movable)
				{
					continue;
				}

				// Use the body instead of the component for cases where we have multi-body overlaps enabled
				FBodyInstance* OverlapBody = nullptr;
				const int32 OverlapBodyIndex = Overlap.GetBodyIndex();
				const USkeletalMeshComponent* SkelMeshForBody = (OverlapBodyIndex != INDEX_NONE) ? Cast<USkeletalMeshComponent>(OverlapComp) : nullptr;
				if (SkelMeshForBody != nullptr)
				{
					OverlapBody = SkelMeshForBody->Bodies.IsValidIndex(OverlapBodyIndex) ? SkelMeshForBody->Bodies[OverlapBodyIndex] : nullptr;
				}
				else
				{
					OverlapBody = OverlapComp->GetBodyInstance();
				}

				if (!OverlapBody)
				{
					//UE_LOG(LogCharacterMovement, Warning, TEXT("%s could not find overlap body for body index %d"), *GetName(), OverlapBodyIndex);
					continue;
				}

				if (!OverlapBody->IsInstanceSimulatingPhysics())
				{
					continue;
				}

				FTransform BodyTransform = OverlapBody->GetUnrealWorldTransform();

				FVector BodyVelocity = OverlapBody->GetUnrealWorldVelocity();
				FVector BodyLocation = BodyTransform.GetLocation();

				// Trace to get the hit location on the capsule
				FHitResult Hit;
				bool bHasHit = UpdatedPrimitive->LineTraceComponent(Hit, BodyLocation,
					FVector(MyLocation.X, MyLocation.Y, BodyLocation.Z),
					QueryParams);

				FVector HitLoc = Hit.ImpactPoint;
				bool bIsPenetrating = Hit.bStartPenetrating || Hit.PenetrationDepth > StopBodyDistance;

				// If we didn't hit the capsule, we're inside the capsule
				if (!bHasHit)
				{
					HitLoc = BodyLocation;
					bIsPenetrating = true;
				}

				const float DistanceNow = (HitLoc - BodyLocation).SizeSquared2D();
				const float DistanceLater = (HitLoc - (BodyLocation + BodyVelocity * DeltaSeconds)).SizeSquared2D();

				if (bHasHit && DistanceNow < StopBodyDistance && !bIsPenetrating)
				{
					OverlapBody->SetLinearVelocity(FVector(0.0f, 0.0f, 0.0f), false);
				}
				else if (DistanceLater <= DistanceNow || bIsPenetrating)
				{
					FVector ForceCenter = MyLocation;

					if (bHasHit)
					{
						ForceCenter.Z = HitLoc.Z;
					}
					else
					{
						ForceCenter.Z = FMath::Clamp(BodyLocation.Z, MyLocation.Z - CapsuleHalfHeight, MyLocation.Z + CapsuleHalfHeight);
					}

					OverlapBody->AddRadialForceToBody(ForceCenter, RepulsionForceRadius, RepulsionForce * Mass, ERadialImpulseFalloff::RIF_Constant);
				}
			}
		}
	}
}


// TODO: This needs more work since it uses nav data
void USurfaceNavCharacterMovementComponent::PhysNavWalking(float deltaTime, int32 Iterations)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysNavWalking);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if ((!CharacterOwner || !CharacterOwner->Controller) && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	// Ensure velocity is horizontal.
	MaintainHorizontalGroundVelocity();
	//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysNavWalking: Velocity contains NaN before CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

	//bound acceleration
	FVector TempUnrotAcceleration = UnrotateVectorUsingGravity(Acceleration);
	TempUnrotAcceleration.Z = 0.f;
	Acceleration = RotateVectorUsingGravity(TempUnrotAcceleration);
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(deltaTime, GroundFriction, false, GetMaxBrakingDeceleration());
		//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysNavWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
	}

	ApplyRootMotionToVelocity(deltaTime);

	if (IsFalling())
	{
		// Root motion could have put us into Falling
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	Iterations++;

	FVector UnrotDesiredMove = UnrotateVectorUsingGravity(Velocity);
	UnrotDesiredMove.Z = 0.f;
	FVector DesiredMove = RotateVectorUsingGravity(UnrotDesiredMove);

	// When dealing with getting feet location, we consider it as the unrotated vector since it's
	// just the location minus a z offset. Then we rotate to get the planet direction if needed.
	const FVector UnrotOldLocation = GetActorFeetLocation();
	const FVector OldLocation = RotateVectorUsingGravity(UnrotOldLocation);
	const FVector DeltaMove = DesiredMove * deltaTime;
	const bool bDeltaMoveNearlyZero = DeltaMove.IsNearlyZero();

	FVector AdjustedDest = OldLocation + DeltaMove;
	FVector UnrotAdjustedDest = UnrotateVectorUsingGravity(AdjustedDest);
	FNavLocation DestNavLocation;

	bool bSameNavLocation = false;

	// TODO: Need to figure out how to handle this
	/*if (CachedNavLocation.NodeRef != INVALID_NAVNODEREF)
	{
		if (bProjectNavMeshWalking)
		{
			const float DistSq2D = (OldLocation - CachedNavLocation.Location).SizeSquared2D();
			const float DistZ = FMath::Abs(OldLocation.Z - CachedNavLocation.Location.Z);

			const float TotalCapsuleHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
			const float ProjectionScale = (OldLocation.Z > CachedNavLocation.Location.Z) ? NavMeshProjectionHeightScaleUp : NavMeshProjectionHeightScaleDown;
			const float DistZThr = TotalCapsuleHeight * FMath::Max(0.f, ProjectionScale);

			bSameNavLocation = (DistSq2D <= KINDA_SMALL_NUMBER) && (DistZ < DistZThr);
		}
		else
		{
			bSameNavLocation = CachedNavLocation.Location.Equals(OldLocation);
		}

		if (bDeltaMoveNearlyZero && bSameNavLocation)
		{
			if (const INavigationDataInterface* NavData = GetNavData())
			{
				if (!NavData->IsNodeRefValid(CachedNavLocation.NodeRef))
				{
					CachedNavLocation.NodeRef = INVALID_NAVNODEREF;
					bSameNavLocation = false;
				}
			}
		}
	}*/

	if (bDeltaMoveNearlyZero && bSameNavLocation)
	{
		DestNavLocation = CachedNavLocation;
		//UE_LOG(LogNavMeshMovement, VeryVerbose, TEXT("%s using cached navmesh location! (bProjectNavMeshWalking = %d)"), *GetNameSafe(CharacterOwner), bProjectNavMeshWalking);
	}
	else
	{
		//SCOPE_CYCLE_COUNTER(STAT_CharNavProjectPoint);

		// Start the trace from the Z location of the last valid trace.
		// Otherwise if we are projecting our location to the underlying geometry and it's far above or below the navmesh,
		// we'll follow that geometry's plane out of range of valid navigation.
		if (bSameNavLocation && bProjectNavMeshWalking)
		{
			//AdjustedDest.Z = CachedNavLocation.Location.Z;
			UnrotAdjustedDest.Z = UnrotateVectorUsingGravity(CachedNavLocation.Location).Z;
			AdjustedDest = RotateVectorUsingGravity(UnrotAdjustedDest);
		}

		// Find the point on the NavMesh
		const bool bHasNavigationData = FindNavFloor(AdjustedDest, DestNavLocation);
		if (!bHasNavigationData)
		{
			SetMovementMode(MOVE_Walking);
			return;
		}

		CachedNavLocation = DestNavLocation;
	}

	if (DestNavLocation.NodeRef != INVALID_NAVNODEREF)
	{
		//FVector NewLocation(AdjustedDest.X, AdjustedDest.Y, DestNavLocation.Location.Z);
		FVector NewLocation = RotateVectorUsingGravity(FVector(UnrotAdjustedDest.X, UnrotAdjustedDest.Y, UnrotateVectorUsingGravity(DestNavLocation.Location).Z));

		if (bProjectNavMeshWalking)
		{
			//SCOPE_CYCLE_COUNTER(STAT_CharNavProjectLocation);
			const float TotalCapsuleHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
			const float UpOffset = TotalCapsuleHeight * FMath::Max(0.f, NavMeshProjectionHeightScaleUp);
			const float DownOffset = TotalCapsuleHeight * FMath::Max(0.f, NavMeshProjectionHeightScaleDown);
			NewLocation = ProjectLocationFromNavMesh(deltaTime, OldLocation, NewLocation, UpOffset, DownOffset);
		}

		FVector AdjustedDelta = NewLocation - OldLocation;

		if (!AdjustedDelta.IsNearlyZero())
		{
			FHitResult HitResult;
			SafeMoveUpdatedComponent(AdjustedDelta, UpdatedComponent->GetComponentQuat(), bSweepWhileNavWalking, HitResult);
		}

		// Update velocity to reflect actual move
		if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasVelocity())
		{
			Velocity = (GetActorFeetLocation() - OldLocation) / deltaTime;
			MaintainHorizontalGroundVelocity();
		}

		bJustTeleported = false;
	}
	else
	{
		StartFalling(Iterations, deltaTime, deltaTime, DeltaMove, OldLocation);
	}
}

FVector USurfaceNavCharacterMovementComponent::ProjectLocationFromNavMesh(float DeltaSeconds, const FVector& CurrentFeetLocation, const FVector& TargetNavLocation, float UpOffset, float DownOffset)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharNavProjectLocation);

	FVector NewLocation = TargetNavLocation;

	const float ZOffset = -(DownOffset + UpOffset);
	if (ZOffset > -SMALL_NUMBER)
	{
		return NewLocation;
	}

	const FVector TraceStart = FVector(TargetNavLocation.X, TargetNavLocation.Y, TargetNavLocation.Z + UpOffset);
	const FVector TraceEnd = FVector(TargetNavLocation.X, TargetNavLocation.Y, TargetNavLocation.Z - DownOffset);

	// We can skip this trace if we are checking at the same location as the last trace (ie, we haven't moved).
	const bool bCachedLocationStillValid = (CachedProjectedNavMeshHitResult.bBlockingHit &&
		CachedProjectedNavMeshHitResult.TraceStart == TraceStart &&
		CachedProjectedNavMeshHitResult.TraceEnd == TraceEnd);

	NavMeshProjectionTimer -= DeltaSeconds;
	if (NavMeshProjectionTimer <= 0.0f)
	{
		if (!bCachedLocationStillValid || bAlwaysCheckFloor)
		{
			//UE_LOG(LogNavMeshMovement, VeryVerbose, TEXT("ProjectLocationFromNavMesh(): %s interval: %.3f velocity: %s"), *GetNameSafe(CharacterOwner), NavMeshProjectionInterval, *Velocity.ToString());

			FHitResult HitResult;
			FindBestNavMeshLocation(TraceStart, TraceEnd, CurrentFeetLocation, TargetNavLocation, HitResult);

			// discard result if we were already inside something			
			if (HitResult.bStartPenetrating || !HitResult.bBlockingHit)
			{
				CachedProjectedNavMeshHitResult.Reset();
			}
			else
			{
				CachedProjectedNavMeshHitResult = HitResult;
			}
		}
		else
		{
			//UE_LOG(LogNavMeshMovement, VeryVerbose, TEXT("ProjectLocationFromNavMesh(): %s interval: %.3f velocity: %s [SKIP TRACE]"), *GetNameSafe(CharacterOwner), NavMeshProjectionInterval, *Velocity.ToString());
		}

		// Wrap around to maintain same relative offset to tick time changes.
		// Prevents large framerate spikes from aligning multiple characters to the same frame (if they start staggered, they will now remain staggered).
		float ModTime = 0.f;
		if (NavMeshProjectionInterval > SMALL_NUMBER)
		{
			ModTime = FMath::Fmod(-NavMeshProjectionTimer, NavMeshProjectionInterval);
		}

		NavMeshProjectionTimer = NavMeshProjectionInterval - ModTime;
	}

	// Project to last plane we found.
	if (CachedProjectedNavMeshHitResult.bBlockingHit)
	{
		if (bCachedLocationStillValid && FMath::IsNearlyEqual(CurrentFeetLocation.Z, CachedProjectedNavMeshHitResult.ImpactPoint.Z, (FVector::FReal)0.01f))
		{
			// Already at destination.
			NewLocation.Z = CurrentFeetLocation.Z;
		}
		else
		{
			//const FVector ProjectedPoint = FMath::LinePlaneIntersection(TraceStart, TraceEnd, CachedProjectedNavMeshHitResult.ImpactPoint, CachedProjectedNavMeshHitResult.Normal);
			//float ProjectedZ = ProjectedPoint.Z;

			// Optimized assuming we only care about Z coordinate of result.
			const FVector& PlaneOrigin = CachedProjectedNavMeshHitResult.ImpactPoint;
			const FVector& PlaneNormal = CachedProjectedNavMeshHitResult.Normal;
			FVector::FReal ProjectedZ = TraceStart.Z + ZOffset * (((PlaneOrigin - TraceStart) | PlaneNormal) / (ZOffset * PlaneNormal.Z));

			// Limit to not be too far above or below NavMesh location
			ProjectedZ = FMath::Clamp(ProjectedZ, TraceEnd.Z, TraceStart.Z);

			// Interp for smoother updates (less "pop" when trace hits something new). 0 interp speed is instant.
			const FVector::FReal InterpSpeed = FMath::Max<FVector::FReal>(0.f, NavMeshProjectionInterpSpeed);
			ProjectedZ = FMath::FInterpTo(CurrentFeetLocation.Z, ProjectedZ, (FVector::FReal)DeltaSeconds, InterpSpeed);
			ProjectedZ = FMath::Clamp(ProjectedZ, TraceEnd.Z, TraceStart.Z);

			// Final result
			NewLocation.Z = ProjectedZ;
		}
	}

	return NewLocation;
}




void USurfaceNavCharacterMovementComponent::SetPostLandedPhysics(const FHitResult& Hit)
{
	if (CharacterOwner)
	{
		if (CanEverSwim() && IsInWater())
		{
			SetMovementMode(MOVE_Swimming);
		}
		else
		{
			const FVector PreImpactAccel = Acceleration + (IsFalling() ? FVector(0.f, 0.f, GetGravityZ()) : FVector::ZeroVector);
			const FVector PreImpactVelocity = Velocity;

			if (DefaultLandMovementMode == MOVE_Walking ||
				DefaultLandMovementMode == MOVE_NavWalking ||
				DefaultLandMovementMode == MOVE_Falling)
			{
				SetMovementMode(GetGroundMovementMode());
			}
			else
			{
				SetDefaultMovementMode();
			}

			ApplyImpactPhysicsForces(Hit, PreImpactAccel, PreImpactVelocity);
		}
	}
}

void USurfaceNavCharacterMovementComponent::OnTeleported()
{
	if (!HasValidData())
	{
		return;
	}

	Super::OnTeleported();

	bJustTeleported = true;

	// Find floor at current location
	UpdateFloorFromAdjustment();

	// Validate it. We don't want to pop down to walking mode from very high off the ground, but we'd like to keep walking if possible.
	UPrimitiveComponent* OldBase = CharacterOwner->GetMovementBase();
	UPrimitiveComponent* NewBase = NULL;

	if (OldBase && CurrentFloor.IsWalkableFloor() && CurrentFloor.FloorDist <= MAX_FLOOR_DIST && Velocity.Z <= 0.f)
	{
		// Close enough to land or just keep walking.
		NewBase = CurrentFloor.HitResult.Component.Get();
	}
	else
	{
		CurrentFloor.Clear();
	}

	const bool bWasFalling = (MovementMode == MOVE_Falling);
	const bool bWasSwimming = (MovementMode == DefaultWaterMovementMode) || (MovementMode == MOVE_Swimming);

	if (CanEverSwim() && IsInWater())
	{
		if (!bWasSwimming)
		{
			SetMovementMode(DefaultWaterMovementMode);
		}
	}
	else if (!CurrentFloor.IsWalkableFloor() || (OldBase && !NewBase))
	{
		if (!bWasFalling && MovementMode != MOVE_Flying && MovementMode != MOVE_Custom)
		{
			SetMovementMode(MOVE_Falling);
		}
	}
	else if (NewBase)
	{
		if (bWasSwimming)
		{
			SetMovementMode(DefaultLandMovementMode);
		}
		else if (bWasFalling)
		{
			ProcessLanded(CurrentFloor.HitResult, 0.f, 0);
		}
	}

	SaveBaseLocation();
}

void USurfaceNavCharacterMovementComponent::PhysicsVolumeChanged(APhysicsVolume* NewVolume)
{
	if (!HasValidData())
	{
		return;
	}
	if (NewVolume && NewVolume->bWaterVolume)
	{
		// just entered water
		if (!CanEverSwim())
		{
			// AI needs to stop any current moves
			IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
			if (PFAgent)
			{
				//PathFollowingComp->AbortMove(*this, FPathFollowingResultFlags::MovementStop);
				PFAgent->OnUnableToMove(*this);
			}
		}
		else if (!IsSwimming())
		{
			SetMovementMode(MOVE_Swimming);
		}
	}
	else if (IsSwimming())
	{
		// just left the water - check if should jump out
		SetMovementMode(MOVE_Falling);
		FVector JumpDir(0.f);
		FVector WallNormal(0.f);
		if (Acceleration.Z > 0.f && ShouldJumpOutOfWater(JumpDir)
			&& ((JumpDir | Acceleration) > 0.f) && CheckWaterJump(JumpDir, WallNormal))
		{
			JumpOutOfWater(WallNormal);
			Velocity.Z = OutofWaterZ; //set here so physics uses this for remainder of tick
		}
	}
}

FString USurfaceNavCharacterMovementComponent::GetMovementName() const
{
	if (CharacterOwner)
	{
		if (CharacterOwner->GetRootComponent() && CharacterOwner->GetRootComponent()->IsSimulatingPhysics())
		{
			return TEXT("Rigid Body");
		}
	}

	// Using character movement
	switch (MovementMode)
	{
	case MOVE_None:				return TEXT("NULL"); break;
	case MOVE_Walking:

		switch (SurfaceNavMovementMode)
		{
		case MOVE_SN_Walking:
			return TEXT("Walking");
		case MOVE_SN_Climbing:
			return TEXT("Climbing");
		case MOVE_SN_Hanging:
			// TODO: Change max braking
			return TEXT("Hanging");
		default:
			return TEXT("Walking");
		}

		//return TEXT("Walking"); 
		break;
	case MOVE_NavWalking:

		switch (SurfaceNavMovementMode)
		{
		case MOVE_SN_Walking:
			return TEXT("NavWalking");
		case MOVE_SN_Climbing:
			return TEXT("NavClimbing");
		case MOVE_SN_Hanging:
			// TODO: Change max braking
			return TEXT("NavHanging");
		default:
			return TEXT("NavWalking");
		}

		//return TEXT("NavWalking"); 
		break;
	case MOVE_Falling:			return TEXT("Falling"); break;
	case MOVE_Swimming:			return TEXT("Swimming"); break;
	case MOVE_Flying:			return TEXT("Flying"); break;
	case MOVE_Custom:			return TEXT("Custom"); break;
	default:					break;
	}
	return TEXT("Unknown");
}

void USurfaceNavCharacterMovementComponent::FillAsyncInput(const FVector& InputVector, FCharacterMovementComponentAsyncInput& AsyncInput)
{
#if WITH_CHAOS 
	if (!CharacterOwner || !CharacterOwner->Controller)
	{
		return;
	}

	ensure(UpdatedComponent); // assumed to exist in  MockMoveUpdatedComponent
	ensure(CharacterOwner->GetCapsuleComponent()); // check in FindFloor
	ensure(!bRunPhysicsWithNoController);
	ensure(UpdatedComponent->GetOwner()); // assumed in ResolvePenetration
	ensure(GetOwner());

	// here because it can trigger other animation stuff (Mantis)
	UpdateCharacterStateBeforeMovement(AsyncRootMotion.TimeAccumulated);

	AsyncInput.bInitialized = true;

	AsyncInput.InputVector = InputVector;
	AsyncInput.NetworkSmoothingMode = NetworkSmoothingMode;
	AsyncInput.bIsNetModeClient = IsNetMode(NM_Client);
	AsyncInput.bWasSimulatingRootMotion = bWasSimulatingRootMotion;
	AsyncInput.bRunPhysicsWithNoController = bRunPhysicsWithNoController;
	AsyncInput.bForceMaxAccel = bForceMaxAccel;
	AsyncInput.MaxAcceleration = GetMaxAcceleration();
	AsyncInput.MinAnalogWalkSpeed = GetMinAnalogSpeed();
	AsyncInput.bIgnoreBaseRotation = bIgnoreBaseRotation;
	AsyncInput.bOrientRotationToMovement = bOrientRotationToMovement;
	AsyncInput.bUseControllerDesiredRotation = bUseControllerDesiredRotation;
	AsyncInput.bConstrainToPlane = bConstrainToPlane;
	AsyncInput.PlaneConstraintOrigin = PlaneConstraintOrigin;
	AsyncInput.PlaneConstraintNormal = PlaneConstraintNormal;
	AsyncInput.bHasValidData = HasValidData();
	AsyncInput.MaxStepHeight = MaxStepHeight;
	AsyncInput.bAlwaysCheckFloor = bAlwaysCheckFloor;
	AsyncInput.WalkableFloorZ = GetWalkableFloorZ();
	AsyncInput.bUseFlatBaseForFloorChecks = bUseFlatBaseForFloorChecks;
	AsyncInput.GravityZ = GetGravityZ();
	AsyncInput.bCanEverCrouch = CanEverCrouch();
	AsyncInput.MaxSimulationIterations = MaxSimulationIterations;
	AsyncInput.MaxSimulationTimeStep = MaxSimulationTimeStep;
	AsyncInput.bMaintainHorizontalGroundVelocity = bMaintainHorizontalGroundVelocity;
	AsyncInput.bUseSeparateBrakingFriction = bUseSeparateBrakingFriction;
	AsyncInput.GroundFriction = GroundFriction;
	AsyncInput.BrakingFrictionFactor = BrakingFrictionFactor;
	AsyncInput.BrakingFriction = BrakingFriction;
	AsyncInput.BrakingSubStepTime = BrakingSubStepTime;
	AsyncInput.BrakingDecelerationWalking = BrakingDecelerationWalking;
	AsyncInput.BrakingDecelerationFalling = BrakingDecelerationFalling;
	AsyncInput.BrakingDecelerationSwimming = BrakingDecelerationSwimming;
	AsyncInput.BrakingDecelerationFlying = BrakingDecelerationFlying;
	AsyncInput.MaxDepenetrationWithPawn = MaxDepenetrationWithPawn;
	AsyncInput.MaxDepenetrationWithGeometryAsProxy = MaxDepenetrationWithGeometryAsProxy;
	AsyncInput.MaxDepenetrationWithGeometry = MaxDepenetrationWithGeometry;
	AsyncInput.MaxDepenetrationWithPawnAsProxy = MaxDepenetrationWithPawnAsProxy;
	AsyncInput.bCanWalkOffLedgesWhenCrouching = bCanWalkOffLedgesWhenCrouching;
	AsyncInput.bCanWalkOffLedges = bCanWalkOffLedges;
	AsyncInput.LedgeCheckThreshold = LedgeCheckThreshold;
	AsyncInput.PerchRadiusThreshold = PerchRadiusThreshold;
	AsyncInput.AirControl = AirControl;
	AsyncInput.AirControlBoostMultiplier = AirControlBoostMultiplier;
	AsyncInput.AirControlBoostVelocityThreshold = AirControlBoostVelocityThreshold;
	AsyncInput.bApplyGravityWhileJumping = bApplyGravityWhileJumping;
	AsyncInput.PhysicsVolumeTerminalVelocity = GetPhysicsVolume()->TerminalVelocity;
	AsyncInput.MaxJumpApexAttemptsPerSimulation = MaxJumpApexAttemptsPerSimulation;
	AsyncInput.DefaultLandMovementMode = DefaultLandMovementMode;
	AsyncInput.FallingLateralFriction = FallingLateralFriction;
	AsyncInput.JumpZVelocity = JumpZVelocity;
	AsyncInput.bAllowPhysicsRotationDuringAnimRootMotion = bAllowPhysicsRotationDuringAnimRootMotion;
	AsyncInput.bDeferUpdateMoveComponent = bDeferUpdateMoveComponent;
	AsyncInput.bRequestedMoveUseAcceleration = bRequestedMoveUseAcceleration;
	AsyncInput.PerchAdditionalHeight = PerchAdditionalHeight;
	AsyncInput.bNavAgentPropsCanJump = NavAgentProps.bCanJump;
	AsyncInput.bMovementStateCanJump = MovementState.bCanJump;
	AsyncInput.MaxWalkSpeedCrouched = MaxWalkSpeedCrouched;
	AsyncInput.MaxWalkSpeed = MaxWalkSpeed;
	AsyncInput.MaxSwimSpeed = MaxSwimSpeed;
	AsyncInput.MaxFlySpeed = MaxFlySpeed;
	AsyncInput.MaxCustomMovementSpeed = MaxCustomMovementSpeed;
	AsyncInput.RotationRate = RotationRate;

	AsyncInput.RootMotion = AsyncRootMotion;
	AsyncRootMotion.Clear();

	FCachedMovementBaseAsyncData& MovementBaseData = AsyncInput.MovementBaseAsyncData;

	UPrimitiveComponent* MovementBase = GetMovementBase();
	MovementBaseData.CachedMovementBase = MovementBase;
	MovementBaseData.OldBaseQuat = OldBaseQuat;
	MovementBaseData.OldBaseLocation = OldBaseLocation;
	if (IsValid(MovementBase))
	{
		MovementBaseData.bMovementBaseIsValidCached = true;
		MovementBaseData.bMovementBaseOwnerIsValidCached = true;
		MovementBaseData.bMovementBaseUsesRelativeLocationCached = MovementBaseUtility::UseRelativeLocation(MovementBase);
		MovementBaseData.bMovementBaseIsSimulatedCached = MovementBaseUtility::IsSimulatedBase(MovementBase);
		MovementBaseData.bMovementBaseIsDynamicCached = MovementBaseUtility::IsDynamicBase(MovementBase);
		MovementBaseData.bIsBaseTransformValid = MovementBaseUtility::GetMovementBaseTransform(MovementBase, CharacterOwner->GetBasedMovement().BoneName, MovementBaseData.BaseLocation, MovementBaseData.BaseQuat);
	}
	else
	{
		MovementBaseData.bMovementBaseIsValidCached = false;
		MovementBaseData.bMovementBaseOwnerIsValidCached = false;
		MovementBaseData.bMovementBaseUsesRelativeLocationCached = false;
		MovementBaseData.bMovementBaseIsSimulatedCached = false;
		MovementBaseData.bMovementBaseIsDynamicCached = false;
		MovementBaseData.BaseLocation = FVector::ZeroVector;
		MovementBaseData.BaseQuat = FQuat::Identity;
		MovementBaseData.bIsBaseTransformValid = false;
	}

	// Character owner inputs
	TUniquePtr<FCharacterAsyncInput>& CharacterInput = AsyncInput.CharacterInput;
	ensure(CharacterInput.IsValid());
	CharacterOwner->FillAsyncInput(*CharacterInput.Get());

	AsyncInput.World = GetWorld();

	if (ensure(UpdatedComponent))
	{
		TUniquePtr<FUpdatedComponentAsyncInput>& UpdatedComponentInput = AsyncInput.UpdatedComponentInput;
		ensure(UpdatedComponentInput.IsValid());

		UpdatedComponentInput->bIsQueryCollisionEnabled = UpdatedComponent->IsQueryCollisionEnabled();
		UpdatedComponentInput->bIsSimulatingPhysics = UpdatedComponent->IsSimulatingPhysics();
		UpdatedComponentInput->PhysicsHandle = nullptr;

		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(UpdatedComponent))
		{
			static const FName TraceTagName = TEXT("MoveComponent");
			UpdatedComponentInput->bForceGatherOverlaps = !FUpdatedComponentAsyncInput::ShouldCheckOverlapFlagToQueueOverlaps(*PrimitiveComponent);
			UpdatedComponentInput->bGatherOverlaps = PrimitiveComponent->GetGenerateOverlapEvents() || UpdatedComponentInput->bForceGatherOverlaps;
			UpdatedComponentInput->UpdatedComponent = PrimitiveComponent;
			UpdatedComponentInput->PhysicsHandle = PrimitiveComponent->BodyInstance.ActorHandle;
			ensure(UpdatedComponentInput->PhysicsHandle);

			// TODO Double check we initialize all these SQ params correctly and use them in the right spots.

			// Params for MoveComponent queries.
			UpdatedComponentInput->MoveComponentQueryParams = FComponentQueryParams(SCENE_QUERY_STAT(MoveComponent), GetOwner());
			PrimitiveComponent->InitSweepCollisionParams(UpdatedComponentInput->MoveComponentQueryParams, UpdatedComponentInput->MoveComponentCollisionResponseParams);
			UpdatedComponentInput->MoveComponentQueryParams.bIgnoreTouches |= !(UpdatedComponentInput->bGatherOverlaps);
			UpdatedComponentInput->MoveComponentQueryParams.TraceTag = TraceTagName;
			UpdatedComponentInput->CollisionShape = PrimitiveComponent->GetCollisionShape(MovementComponentCVars::PenetrationOverlapCheckInflation);

			// Params for FindFloor, CharMovement queries.
			AsyncInput.QueryParams = FComponentQueryParams(SCENE_QUERY_STAT(ComputeFloorDist), GetOwner());
			PrimitiveComponent->InitSweepCollisionParams(AsyncInput.QueryParams, AsyncInput.CollisionResponseParams);
			AsyncInput.CollisionChannel = PrimitiveComponent->GetCollisionObjectType();


		}
		else
		{
			ensure(false);
		}

		UpdatedComponentInput->Scale = UpdatedComponent->GetComponentScale();
	}
	AsyncInput.RandomStream.Initialize(FApp::bUseFixedSeed ? GetFName() : NAME_None);


	// Only game thread inputs need to be updated here.
	AsyncInput.GTInputs.bWantsToCrouch = bWantsToCrouch;

	if (MovementMode == MOVE_None)
	{
		// TODO fix this, sometimes we don't fall on start and just float in air because movement mode is none. Not sure why.
		// Pawn typically sets movement mode after possess, but something goes wrong.
		AsyncInput.GTInputs.MovementMode = GetGroundMovementMode();
		AsyncInput.GTInputs.bValidMovementMode = true;
	}
	else
	{
		AsyncInput.GTInputs.MovementMode = MovementMode;
		AsyncInput.GTInputs.bValidMovementMode = bMovementModeDirty;
	}
	AsyncInput.GTInputs.bPressedJump = CharacterOwner->bPressedJump;


	if (AsyncSimState->IsValid() == false)
	{
		// Need to fully initialize output as we do not have any stored async state.
		AsyncSimState->bWasSimulatingRootMotion = bWasSimulatingRootMotion;
		AsyncSimState->MovementMode = MovementMode;
		AsyncSimState->GroundMovementMode = GetGroundMovementMode();
		AsyncSimState->CustomMovementMode = CustomMovementMode;
		AsyncSimState->Acceleration = Acceleration;
		AsyncSimState->AnalogInputModifier = AnalogInputModifier;
		AsyncSimState->LastUpdateLocation = LastUpdateLocation;
		AsyncSimState->LastUpdateRotation = LastUpdateRotation;
		AsyncSimState->LastUpdateVelocity = LastUpdateVelocity;
		AsyncSimState->bForceNextFloorCheck = bForceNextFloorCheck;
		AsyncSimState->Velocity = Velocity;
		AsyncSimState->LastPreAdditiveVelocity = FVector::ZeroVector;
		AsyncSimState->bIsAdditiveVelocityApplied = false;
		AsyncSimState->bDeferUpdateBasedMovement = bDeferUpdateBasedMovement;
		AsyncSimState->MoveComponentFlags = MoveComponentFlags;
		AsyncSimState->PendingForceToApply = PendingForceToApply;
		AsyncSimState->PendingImpulseToApply = PendingImpulseToApply;
		AsyncSimState->PendingLaunchVelocity = PendingLaunchVelocity;
		AsyncSimState->bCrouchMaintainsBaseLocation = bCrouchMaintainsBaseLocation;
		AsyncSimState->bJustTeleported = bJustTeleported;
		CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(AsyncSimState->ScaledCapsuleRadius, AsyncSimState->ScaledCapsuleHalfHeight);
		AsyncSimState->bIsCrouched = CharacterOwner->bIsCrouched;
		AsyncSimState->bWantsToCrouch = bWantsToCrouch;
		AsyncSimState->bMovementInProgress = bMovementInProgress;
		AsyncSimState->CurrentFloor = CurrentFloor;
		AsyncSimState->bHasRequestedVelocity = bHasRequestedVelocity;
		AsyncSimState->bRequestedMoveWithMaxSpeed = bRequestedMoveWithMaxSpeed;
		AsyncSimState->RequestedVelocity = RequestedVelocity;
		AsyncSimState->NumJumpApexAttempts = NumJumpApexAttempts;
		AsyncSimState->bShouldApplyDeltaToMeshPhysicsTransforms = false;
		AsyncSimState->DeltaPosition = FVector::ZeroVector;
		AsyncSimState->DeltaQuat = FQuat::Identity;
		AsyncSimState->DeltaTime = 0.0f; // Fill out in async callback.
		AsyncSimState->OldLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
		AsyncSimState->OldVelocity = Velocity;
		AsyncSimState->bShouldDisablePostPhysicsTick = false;
		AsyncSimState->bShouldEnablePostPhysicsTick = false;
		AsyncSimState->bShouldAddMovementBaseTickDependency = false;
		AsyncSimState->bShouldRemoveMovementBaseTickDependency = false;
		AsyncSimState->NewMovementBase = AsyncInput.MovementBaseAsyncData.CachedMovementBase;
		AsyncSimState->NewMovementBaseOwner = AsyncSimState->NewMovementBase ? AsyncSimState->NewMovementBase->GetOwner() : nullptr;

		// Character owner data
		TUniquePtr<FCharacterAsyncOutput>& CharacterOutput = AsyncSimState->CharacterOutput;
		ensure(CharacterOutput.IsValid());
		CharacterOwner->InitializeAsyncOutput(*CharacterOutput.Get());

		AsyncSimState->bIsValid = true;
	}
#endif
}

#pragma endregion



#pragma region "These have been modded or don't need modding"

// *****************************************************
// *****************************************************
// These have been modded or don't need modding.
// There may be additional work needed.
// *****************************************************

bool USurfaceNavCharacterMovementComponent::CanClimb() const
{
	return bCanClimb;
}

bool USurfaceNavCharacterMovementComponent::CanHang() const
{
	return bCanHang;
}

bool USurfaceNavCharacterMovementComponent::IsClimbing() const
{
	return (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking) && (SurfaceNavMovementMode == MOVE_SN_Climbing) && UpdatedComponent;
}

bool USurfaceNavCharacterMovementComponent::IsHanging() const
{
	return (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking) && (SurfaceNavMovementMode == MOVE_SN_Hanging) && UpdatedComponent;
}

void USurfaceNavCharacterMovementComponent::HandleImpact(const FHitResult& Impact, float TimeSlice, const FVector& MoveDelta)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharHandleImpact);

	if (CharacterOwner)
	{
		CharacterOwner->MoveBlockedBy(Impact);
	}

	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		// Also notify path following!
		PFAgent->OnMoveBlockedBy(Impact);
	}

	if (Impact.HitObjectHandle.DoesRepresentClass(APawn::StaticClass()))
	{
		APawn* OtherPawn = Impact.HitObjectHandle.FetchActor<APawn>();
		NotifyBumpedPawn(OtherPawn);
	}

	if (bEnablePhysicsInteraction)
	{
		// Modified to work with custom gravity normal
		//const FVector ForceAccel = Acceleration + (IsFalling() ? FVector(0.f, 0.f, GetGravityZ()) : FVector::ZeroVector);
		const FVector ForceAccel = Acceleration + (IsFalling() ? GetGravity() : FVector::ZeroVector);

		ApplyImpactPhysicsForces(Impact, ForceAccel, Velocity);
	}
}

bool USurfaceNavCharacterMovementComponent::IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, const float CapsuleRadius) const
{
	//if (SurfaceNavigation::IsFlatWorld) // Tests for flat world
	//{
	//	const float DistFromCenterSq = (TestImpactPoint - CapsuleLocation).SizeSquared2D();
	//	const float ReducedRadiusSq = FMath::Square(FMath::Max(SWEEP_EDGE_REJECT_DISTANCE + KINDA_SMALL_NUMBER, CapsuleRadius - SWEEP_EDGE_REJECT_DISTANCE));
	//	return DistFromCenterSq < ReducedRadiusSq;
	//}
	//else // This will actually be a sphere done on a planet so adjust test
	//{
	check(UpdatedComponent); // assumed to exist in  MockMoveUpdatedComponent

	const float DistFromCenterSq = UnrotateVectorUsingGravity(TestImpactPoint - CapsuleLocation).SizeSquared2D();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(SWEEP_EDGE_REJECT_DISTANCE + KINDA_SMALL_NUMBER, CapsuleRadius - SWEEP_EDGE_REJECT_DISTANCE));
	return DistFromCenterSq < ReducedRadiusSq;
	//}
}

void USurfaceNavCharacterMovementComponent::GetCurrentPlanet(AActor*& OutPlanetActor) const
{
	ASurfaceNavAIController* AiController = Cast<ASurfaceNavAIController>(CharacterOwner->GetController());
	if (AiController)
	{
		AiController->GetCurrentPlanet(OutPlanetActor);
	}
}

void USurfaceNavCharacterMovementComponent::ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FFindFloorResult& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult) const
{
	//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("[Role:%d] ComputeFloorDist: %s at location %s"), (int32)CharacterOwner->GetLocalRole(), *GetNameSafe(CharacterOwner), *CapsuleLocation.ToString());
	OutFloorResult.Clear();

	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	bool bIsFlatWorld = SurfaceNavigation::IsFlatWorld;

	bool bSkipSweep = false;
	if (DownwardSweepResult != NULL && DownwardSweepResult->IsValidBlockingHit())
	{

		check(UpdatedComponent); // assumed to exist in  MockMoveUpdatedComponent

		// Get the vector of the sweep from start to end
		const FVector SweepVector = DownwardSweepResult->TraceStart - DownwardSweepResult->TraceEnd;

		FVector GravNormal = GetGravityNormal();



		// if ((DownwardSweepResult->TraceStart.Z > DownwardSweepResult->TraceEnd.Z) &&
		// (DownwardSweepResult->TraceStart - DownwardSweepResult->TraceEnd).SizeSquared2D() <= KINDA_SMALL_NUMBER)

		// Use gravity normal to determine if the downward sweep was actually down.
		// If the dot product is greater than zero then it's down.
		bool IsDown = FVector::DotProduct(GravNormal, SweepVector) > KINDA_SMALL_NUMBER;

		if (IsDown &&
			(UnrotateVectorUsingGravity(SweepVector).SizeSquared2D() <= KINDA_SMALL_NUMBER))
		{
			// Reject hits that are barely on the cusp of the radius of the capsule
			if (IsWithinEdgeTolerance(DownwardSweepResult->Location, DownwardSweepResult->ImpactPoint, PawnRadius))
			{
				// Don't try a redundant sweep, regardless of whether this sweep is usable.
				bSkipSweep = true;

				const bool bIsWalkable = IsWalkable(*DownwardSweepResult);

				//const float FloorDist = (CapsuleLocation.Z - DownwardSweepResult->Location.Z);
				float FloorDist = 0;

				if (bIsFlatWorld)
					FloorDist = (CapsuleLocation.Z - DownwardSweepResult->Location.Z);
				else
				{
					//AActor* CurrentPlanet = nullptr;
					//GetCurrentPlanet(CurrentPlanet);
					//if (!CurrentPlanet)
					//	return;

					//FVector PlanetLoc = CurrentPlanet->GetActorLocation();

					//// The sqrt may drag perf down a good bit depending on how often this function is called.
					//// Can we do something else here?
					//FloorDist = FMath::Sqrt(FVector::DistSquared(CapsuleLocation, PlanetLoc) - FVector::DistSquared(DownwardSweepResult->Location, PlanetLoc));

					FloorDist =
						UnrotateVectorUsingGravity(CapsuleLocation - UpdatedComponent->GetComponentLocation()).Z -
						UnrotateVectorUsingGravity(DownwardSweepResult->Location - UpdatedComponent->GetComponentLocation()).Z;
				}

				OutFloorResult.SetFromSweep(*DownwardSweepResult, FloorDist, bIsWalkable);

				if (bIsWalkable)
				{
					// Use the supplied downward sweep as the floor hit result.			
					return;
				}
			}
		}
		//if (ImpactPointDistFromPlanetSq > (CharDistFromPlanetSq + FMath::Square(PawnHalfHeight - PawnRadius)))

		// We require the sweep distance to be >= the line distance, otherwise the HitResult can't be interpreted as the sweep result.
		if (SweepDistance < LineDistance)
		{
			ensure(SweepDistance >= LineDistance);
			return;
		}

		bool bBlockingHit = false;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComputeFloorDist), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(QueryParams, ResponseParam);
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

		// Sweep test
		if (!bSkipSweep && SweepDistance > 0.f && SweepRadius > 0.f)
		{
			// Use a shorter height to avoid sweeps giving weird results if we start on a surface.
			// This also allows us to adjust out of penetrations.
			const float ShrinkScale = 0.9f;
			const float ShrinkScaleOverlap = 0.1f;
			float ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScale);
			float TraceDist = SweepDistance + ShrinkHeight;

			//FCollisionShape CapsuleShape = NULL;

			//FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(SweepRadius, PawnHalfHeight - ShrinkHeight);

			// Since down may not always be (0,0,-1) then we need to make a sphere shape work in this case
			FCollisionShape CapsuleShape = FCollisionShape::MakeSphere(SweepRadius);

			// We need additional changes here to adjust the distance of the trace to work with sphere instead of capsule
			TraceDist += (PawnHalfHeight - ShrinkHeight - SweepRadius);

			FHitResult Hit(1.f);
			//bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + FVector(0.f, 0.f, -TraceDist), CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
			bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + (GravNormal * TraceDist), CollisionChannel, CapsuleShape, QueryParams, ResponseParam);

			if (bBlockingHit)
			{
				//if (bIsFlatWorld) // Tests for flat world
				//{
				//	// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
				//	// Check 2D distance to impact point, reject if within a tolerance from radius.
				//	if (Hit.bStartPenetrating || !IsWithinEdgeTolerance(CapsuleLocation, Hit.ImpactPoint, CapsuleShape.Capsule.Radius))
				//	{
				//		// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
				//		// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
				//		CapsuleShape.Capsule.Radius = FMath::Max(0.f, CapsuleShape.Capsule.Radius - SWEEP_EDGE_REJECT_DISTANCE - KINDA_SMALL_NUMBER);
				//		if (!CapsuleShape.IsNearlyZero())
				//		{
				//			ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
				//			TraceDist = SweepDistance + ShrinkHeight;
				//			CapsuleShape.Capsule.HalfHeight = FMath::Max(PawnHalfHeight - ShrinkHeight, CapsuleShape.Capsule.Radius);
				//			Hit.Reset(1.f, false);

				//			bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + FVector(0.f, 0.f, -TraceDist), CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
				//		}
				//	}
				//}
				//else // Tests for planet
				//{
					// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
					// Check 2D distance to impact point, reject if within a tolerance from radius.
				if (Hit.bStartPenetrating || !IsWithinEdgeTolerance(CapsuleLocation, Hit.ImpactPoint, CapsuleShape.Capsule.Radius))
				{
					// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
					// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
					CapsuleShape.Sphere.Radius = FMath::Max(0.f, CapsuleShape.Sphere.Radius - SWEEP_EDGE_REJECT_DISTANCE - KINDA_SMALL_NUMBER);
					if (!CapsuleShape.IsNearlyZero())
					{
						ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
						TraceDist = SweepDistance + ShrinkHeight + (PawnHalfHeight - ShrinkHeight - SweepRadius);
						Hit.Reset(1.f, false);

						//bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + FVector(0.f, 0.f, -TraceDist), CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
						bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + (GravNormal * TraceDist), CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
					}
				}
				//}

				// Reduce hit distance by ShrinkHeight because we shrank the capsule for the trace.
				// We allow negative distances here, because this allows us to pull out of penetrations.
				const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
				const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

				OutFloorResult.SetFromSweep(Hit, SweepResult, false);
				if (Hit.IsValidBlockingHit() && IsWalkable(Hit))
				{
					if (SweepResult <= SweepDistance)
					{
						// Hit within test distance.
						OutFloorResult.bWalkableFloor = true;
						return;
					}
				}
			}
		}

		// Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything.
		// We do however want to try a line trace if the sweep was stuck in penetration.
		if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
		{
			OutFloorResult.FloorDist = SweepDistance;
			return;
		}

		// Line trace
		if (LineDistance > 0.f)
		{
			const float ShrinkHeight = PawnHalfHeight;
			const FVector LineTraceStart = CapsuleLocation;
			const float TraceDist = LineDistance + ShrinkHeight;

			// Modified to work with custom gravity normal
			const FVector Down = GravNormal * TraceDist;

			QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

			FHitResult Hit(1.f);
			bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, CollisionChannel, QueryParams, ResponseParam);

			if (bBlockingHit)
			{
				if (Hit.Time > 0.f)
				{
					// Reduce hit distance by ShrinkHeight because we started the trace higher than the base.
					// We allow negative distances here, because this allows us to pull out of penetrations.
					const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
					const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

					OutFloorResult.bBlockingHit = true;
					if (LineResult <= LineDistance && IsWalkable(Hit))
					{
						OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
						return;
					}
				}
			}
		}

		// No hits were acceptable.
		OutFloorResult.bWalkableFloor = false;
	}
}

bool USurfaceNavCharacterMovementComponent::IsWalkable(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit())
	{
		// No hit, or starting in penetration
		return false;
	}

	bool bIsFlatWorld = SurfaceNavigation::IsFlatWorld;
	float DotProdAgainstGround;

	if (bIsFlatWorld)
	{
		// Never walk up vertical surfaces.
		if (Hit.ImpactNormal.Z < KINDA_SMALL_NUMBER)
		{
			return false;
		}
	}
	else
	{
		// Never walk up vertical surfaces.
		// Test the dot product of the impact normal against ground normal.
		// If it's small that means it's vertical relative to it's location on the planet.
		DotProdAgainstGround = FVector::DotProduct(Hit.ImpactNormal, GetGroundNormal());
		if (DotProdAgainstGround < KINDA_SMALL_NUMBER)
		{
			return false;
		}
	}

	//float TestWalkableZ = WalkableFloorZ;
	float TestWalkableZ = GetWalkableFloorZ();

	// See if this component overrides the walkable floor z.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (HitComponent)
	{
		const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
		TestWalkableZ = SlopeOverride.ModifyWalkableFloorZ(TestWalkableZ);
	}

	if (bIsFlatWorld)
	{
		// Can't walk on this surface if it is too steep.
		if (Hit.ImpactNormal.Z < TestWalkableZ)
		{
			return false;
		}
	}
	else
	{
		if (DotProdAgainstGround < TestWalkableZ)
		{
			return false;
		}
	}

	return true;
}

bool USurfaceNavCharacterMovementComponent::StepUp(const FVector& GravDir, const FVector& Delta, const FHitResult& InHit, FStepDownResult* OutStepDownResult)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharStepUp);

	// The step up code is designed to work with gravity = (0,0,-1) as up so we need to adjust to work regardless of up direction.
	// To achieve this we get some of the locations relative to the character and unrotate using characters rotation to get the Z
	// axis working similar to a flat world. It's a bit confusing to follow because sometimes we need to use the original location
	// while other times we need to use the relative location.

	if (!CanStepUp(InHit) || MaxStepHeight <= 0.f)
	{
		return false;
	}

	const bool bIsFlatWorld = SurfaceNavigation::IsFlatWorld;

	FVector OldLocation = UpdatedComponent->GetComponentLocation();

	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	FVector InitialImpactPoint = InHit.ImpactPoint;
	FVector RelativeInitialImpactPoint;

	float InitialImpactZ = InitialImpactPoint.Z;
	float RelativeInitialImpactZ;

	if (bIsFlatWorld) // Flat world
	{
		RelativeInitialImpactPoint = InitialImpactPoint;
		RelativeInitialImpactZ = InitialImpactZ;
	}
	else // Planet
	{
		// Make the impact point relative to the character location and then unrotate it so we're back to dealing with
		// the Z axis.
		RelativeInitialImpactPoint = UnrotateVectorUsingGravity(InitialImpactPoint - OldLocation) + OldLocation;

		// For handline this on a planet, we make everything relative to the character to attempt to decrease
		// the number of code changes required. You'll still want to use the originals when passing to other functions.
		//OldLocation = FVector::ZeroVector; // UnrotateVectorUsingGravity(OldLocation);

		RelativeInitialImpactZ = RelativeInitialImpactPoint.Z;

	}

	// Don't bother stepping up if top of capsule is hitting something.
	if (RelativeInitialImpactZ > OldLocation.Z + (PawnHalfHeight - PawnRadius))
	{
		return false;
	}

	//if (bIsFlatWorld)
	//{
	//	// Flat world
	//	// Don't bother stepping up if top of capsule is hitting something.
	//	float InitialImpactZ = InHit.ImpactPoint.Z;
	//	if (InitialImpactZ > OldLocation.Z + (PawnHalfHeight - PawnRadius))
	//	{
	//		return false;
	//	}
	//}
	//else
	//{
	//	// Planet
	//	// Don't bother stepping up if top of capsule is hitting something.
	//	AActor* CurrentPlanet = nullptr;
	//	GetCurrentPlanet(CurrentPlanet);
	//	if (!CurrentPlanet)
	//		return false;
	//
	//	FVector PlanetLoc = CurrentPlanet->GetActorLocation();
	//
	//	const FVector InitialImpact = InHit.ImpactPoint;
	//	const float ImpactPointDistFromPlanetSq = FVector::DistSquared(InitialImpact, PlanetLoc);
	//	const float CharDistFromPlanetSq = FVector::DistSquared(OldLocation, PlanetLoc);
	//
	//	if (ImpactPointDistFromPlanetSq > (CharDistFromPlanetSq + FMath::Square(PawnHalfHeight - PawnRadius)))
	//	{
	//		return false;
	//	}
	//}

	if (GravDir.IsZero())
	{
		return false;
	}

	// Gravity should be a normalized direction
	ensure(GravDir.IsNormalized());

	float StepTravelUpHeight = MaxStepHeight;
	float StepTravelDownHeight = StepTravelUpHeight;
	const float StepSideZ = -1.f * FVector::DotProduct(InHit.ImpactNormal, GravDir);
	float PawnInitialFloorBaseZ = OldLocation.Z - PawnHalfHeight;
	float PawnFloorPointZ = PawnInitialFloorBaseZ;
	float RelativePawnFloorPointZ = PawnFloorPointZ;

	if (IsMovingOnGround() && CurrentFloor.IsWalkableFloor())
	{
		// Since we float a variable amount off the floor, we need to enforce max step height off the actual point of impact with the floor.
		const float FloorDist = FMath::Max(0.f, CurrentFloor.GetDistanceToFloor());
		PawnInitialFloorBaseZ -= FloorDist;
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (MaxStepHeight + MAX_FLOOR_DIST * 2.f);

		const bool bHitVerticalFace = !IsWithinEdgeTolerance(InHit.Location, InitialImpactPoint, PawnRadius);
		if (!CurrentFloor.bLineTrace && !bHitVerticalFace)
		{
			PawnFloorPointZ = CurrentFloor.HitResult.ImpactPoint.Z;

			if (bIsFlatWorld)
				RelativePawnFloorPointZ = PawnFloorPointZ;
			else
				RelativePawnFloorPointZ = (UnrotateVectorUsingGravity(CurrentFloor.HitResult.ImpactPoint - OldLocation) + OldLocation).Z;
		}
		else
		{
			// Base floor point is the base of the capsule moved down by how far we are hovering over the surface we are hitting.
			PawnFloorPointZ -= CurrentFloor.FloorDist;
			RelativePawnFloorPointZ = PawnFloorPointZ;
		}
	}

	// Don't step up if the impact is below us, accounting for distance from floor.
	//if (InitialImpactZ <= PawnInitialFloorBaseZ)
	if (RelativeInitialImpactZ <= PawnInitialFloorBaseZ)
	{
		return false;
	}

	// Scope our movement updates, and do not apply them until all intermediate moves are completed.
	FScopedMovementUpdate ScopedStepUpMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	// step up - treat as vertical wall
	FHitResult SweepUpHit(1.f);
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	MoveUpdatedComponent(-GravDir * StepTravelUpHeight, PawnRotation, true, &SweepUpHit);

	if (SweepUpHit.bStartPenetrating)
	{
		// Undo movement
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	// step fwd
	FHitResult Hit(1.f);
	MoveUpdatedComponent(Delta, PawnRotation, true, &Hit);

	// Check result of forward movement
	if (Hit.bBlockingHit)
	{
		if (Hit.bStartPenetrating)
		{
			// Undo movement
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If we hit something above us and also something ahead of us, we should notify about the upward hit as well.
		// The forward hit will be handled later (in the bSteppedOver case below).
		// In the case of hitting something above but not forward, we are not blocked from moving so we don't need the notification.
		if (SweepUpHit.bBlockingHit && Hit.bBlockingHit)
		{
			HandleImpact(SweepUpHit);
		}

		// pawn ran into a wall
		HandleImpact(Hit);
		if (IsFalling())
		{
			return true;
		}

		// adjust and try again
		const float ForwardHitTime = Hit.Time;
		const float ForwardSlideAmount = SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);

		if (IsFalling())
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If both the forward hit and the deflection got us nowhere, there is no point in this step up.
		if (ForwardHitTime == 0.f && ForwardSlideAmount == 0.f)
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}
	}

	// Step down
	MoveUpdatedComponent(GravDir * StepTravelDownHeight, UpdatedComponent->GetComponentQuat(), true, &Hit);

	// If step down was initially penetrating abort the step up
	if (Hit.bStartPenetrating)
	{
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	FStepDownResult StepDownResult;
	if (Hit.IsValidBlockingHit())
	{
		// See if this step sequence would have allowed us to travel higher than our max step height allows.
		//const float DeltaZ = Hit.ImpactPoint.Z - PawnFloorPointZ;
		const float DeltaZ = Hit.ImpactPoint.Z - RelativePawnFloorPointZ;
		float RelativeDeltaZ;

		if (bIsFlatWorld)
			RelativeDeltaZ = DeltaZ;
		else
			RelativeDeltaZ = (UnrotateVectorUsingGravity(Hit.ImpactPoint - OldLocation) + OldLocation).Z - RelativePawnFloorPointZ;

		if (RelativeDeltaZ > MaxStepHeight)
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (too high Height %.3f) up from floor base %f to %f"), DeltaZ, PawnInitialFloorBaseZ, NewLocation.Z);
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Reject unwalkable surface normals here.
		if (!IsWalkable(Hit))
		{
			// Reject if normal opposes movement direction
			const bool bNormalTowardsMe = (Delta | Hit.ImpactNormal) < 0.f;
			if (bNormalTowardsMe)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s opposed to movement)"), *Hit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}

			float RelativeHitLocZ = (UnrotateVectorUsingGravity(Hit.Location - OldLocation) + OldLocation).Z;

			// Also reject if we would end up being higher than our starting location by stepping down.
			// It's fine to step down onto an unwalkable normal below us, we will just slide off. Rejecting those moves would prevent us from being able to walk off the edge.
			//if (Hit.Location.Z > OldLocation.Z)
			if (RelativeHitLocZ > OldLocation.Z)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s above old position)"), *Hit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}
		}

		// Reject moves where the downward sweep hit something very close to the edge of the capsule. This maintains consistency with FindFloor as well.
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (outside edge tolerance)"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Don't step up onto invalid surfaces if traveling higher.
		if (RelativeDeltaZ > 0.f && !CanStepUp(Hit))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (up onto surface with !CanStepUp())"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// See if we can validate the floor as a result of this step down. In almost all cases this should succeed, and we can avoid computing the floor outside this method.
		if (OutStepDownResult != NULL)
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), StepDownResult.FloorResult, false, &Hit);

			float RelativeHitLocZ = (UnrotateVectorUsingGravity(Hit.Location - OldLocation) + OldLocation).Z;

			// Reject unwalkable normals if we end up higher than our initial height.
			// It's fine to walk down onto an unwalkable surface, don't reject those moves.
			//if (Hit.Location.Z > OldLocation.Z)
			if (RelativeHitLocZ > OldLocation.Z)
			{
				// We should reject the floor result if we are trying to step up an actual step where we are not able to perch (this is rare).
				// In those cases we should instead abort the step up and try to slide along the stair.
				if (!StepDownResult.FloorResult.bBlockingHit && StepSideZ < CharacterMovementConstants::MAX_STEP_SIDE_Z)
				{
					ScopedStepUpMovement.RevertMove();
					return false;
				}
			}

			StepDownResult.bComputedFloor = true;
		}
	}

	// Copy step down result.
	if (OutStepDownResult != NULL)
	{
		*OutStepDownResult = StepDownResult;
	}

	// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
	bJustTeleported |= !bMaintainHorizontalGroundVelocity;

	return true;
}

float USurfaceNavCharacterMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact)
{
	bool bIsFlatWorld = SurfaceNavigation::IsFlatWorld;

	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	FVector Normal(InNormal);
	float DotProductAgainstGround = FVector::DotProduct(GetGroundNormal(), Normal);

	if (IsMovingOnGround())
	{
		// We don't want to be pushed up an unwalkable surface.
		//if (Normal.Z > 0.f)
		if (DotProductAgainstGround > 0.f)
		{
			if (!IsWalkable(Hit))
			{
				if (bIsFlatWorld)
					Normal = Normal.GetSafeNormal2D();
				else
				{
					// Getting a 2d normal so need to unrotate using character, get normal 2d, then re-rotate
					Normal = RotateVectorUsingGravity(
						UnrotateVectorUsingGravity(Normal).GetSafeNormal2D());
				}
			}
		}
		else if (DotProductAgainstGround < -KINDA_SMALL_NUMBER)
		{
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				const FVector FloorNormal = CurrentFloor.HitResult.Normal;
				//const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - DELTA);
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && ((GetGroundNormal() | FloorNormal) < 1.f - DELTA);
				if (bFloorOpposedToMovement)
				{
					Normal = FloorNormal;
				}

				if (bIsFlatWorld)
					Normal = Normal.GetSafeNormal2D();
				else
				{
					// Getting a 2d normal so need to unrotate using character, get normal 2d, then re-rotate
					Normal = RotateVectorUsingGravity(
						UnrotateVectorUsingGravity(Normal).GetSafeNormal2D());
				}
			}
		}
	}

	return Super::Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}

bool USurfaceNavCharacterMovementComponent::CanStepUp(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit() || !HasValidData() || MovementMode == MOVE_Falling)
	{
		return false;
	}

	// No component for "fake" hits when we are on a known good base.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (!HitComponent)
	{
		return true;
	}

	if (!HitComponent->CanCharacterStepUp(CharacterOwner))
	{
		return false;
	}

	// No actor for "fake" hits when we are on a known good base.

	if (!Hit.HitObjectHandle.IsValid())
	{
		return true;
	}

	const AActor* HitActor = Hit.HitObjectHandle.GetManagingActor();
	if (!HitActor->CanBeBaseForCharacter(CharacterOwner))
	{
		return false;
	}

	return true;
}

FVector USurfaceNavCharacterMovementComponent::ComputeGroundMovementDelta(const FVector& Delta, const FHitResult& RampHit, const bool bHitFromLineTrace) const
{
	const FVector FloorNormal = RampHit.ImpactNormal;
	const FVector ContactNormal = RampHit.Normal;

	float FloorNormalDotGround = GetGroundNormal() | FloorNormal;
	float ContactNormalDotGround = GetGroundNormal() | FloorNormal;

	//if (FloorNormal.Z < (1.f - KINDA_SMALL_NUMBER) && FloorNormal.Z > KINDA_SMALL_NUMBER && ContactNormal.Z > KINDA_SMALL_NUMBER && !bHitFromLineTrace && IsWalkable(RampHit))
	if (FloorNormalDotGround < (1.f - KINDA_SMALL_NUMBER) && FloorNormalDotGround > KINDA_SMALL_NUMBER
		&& ContactNormalDotGround > KINDA_SMALL_NUMBER && !bHitFromLineTrace && IsWalkable(RampHit))
	{
		float FloorDotDelta;
		FVector RampMovement;

		if (SurfaceNavigation::IsFlatWorld) // flat world
		{
			// Compute a vector that moves parallel to the surface, by projecting the horizontal movement direction onto the ramp.
			FloorDotDelta = (FloorNormal | Delta);
			RampMovement = FVector(Delta.X, Delta.Y, -FloorDotDelta / FloorNormal.Z);
		}
		else // planet
		{
			// Compute a vector that moves parallel to the surface, by projecting the horizontal movement direction onto the ramp.
			FVector UnrotDelta = UnrotateVectorUsingGravity(Delta);
			FVector UnrotFloorNormal = UnrotateVectorUsingGravity(FloorNormal);

			FloorDotDelta = (UnrotFloorNormal | UnrotDelta);
			RampMovement = FVector(UnrotDelta.X, UnrotDelta.Y, -FloorDotDelta / UnrotFloorNormal.Z);

			RampMovement = RotateVectorUsingGravity(RampMovement);
		}

		if (bMaintainHorizontalGroundVelocity)
		{
			return RampMovement;
		}
		else
		{
			return RampMovement.GetSafeNormal() * Delta.Size();
		}
	}

	return Delta;
}

void USurfaceNavCharacterMovementComponent::MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	bool bIsFlatWorld = SurfaceNavigation::IsFlatWorld;

	FVector Delta;

	if (bIsFlatWorld)
		// Move along the current floor
		Delta = FVector(InVelocity.X, InVelocity.Y, 0.f) * DeltaSeconds;
	else
	{
		// Move along the current floor
		// Need to unrotate, then get rid of Z and * DeltaSeconds like original, then
		// re-rotate to get back to correct orientation.
		Delta = UnrotateVectorUsingGravity(InVelocity);
		Delta = FVector(Delta.X, Delta.Y, 0.f) * DeltaSeconds;
		Delta = RotateVectorUsingGravity(Delta);
	}

	FHitResult Hit(1.f);
	FVector RampVector = ComputeGroundMovementDelta(Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);
	float LastMoveTimeSlice = DeltaSeconds;

	if (Hit.bStartPenetrating)
	{
		// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch.
		HandleImpact(Hit);
		SlideAlongSurface(Delta, 1.f, Hit.Normal, Hit, true);

		if (Hit.bStartPenetrating)
		{
			OnCharacterStuckInGeometry(&Hit);
		}
	}
	else if (Hit.IsValidBlockingHit())
	{
		// We impacted something (most likely another ramp, but possibly a barrier).
		float PercentTimeApplied = Hit.Time;
		//if ((Hit.Time > 0.f) && (Hit.Normal.Z > KINDA_SMALL_NUMBER) && IsWalkable(Hit))
		if ((Hit.Time > 0.f) && ((GetGroundNormal() | Hit.Normal) > KINDA_SMALL_NUMBER) && IsWalkable(Hit))
		{
			// Another walkable ramp.
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			RampVector = ComputeGroundMovementDelta(Delta * InitialPercentRemaining, Hit, false);
			LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
			SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);

			const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
		}

		if (Hit.IsValidBlockingHit())
		{
			if (CanStepUp(Hit) || (CharacterOwner->GetMovementBase() != nullptr && Hit.HitObjectHandle == CharacterOwner->GetMovementBase()->GetOwner()))
			{
				// hit a barrier, try to step up
				const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
				const FVector GravDir(GetGravityNormal());
				if (!StepUp(GravDir, Delta * (1.f - PercentTimeApplied), Hit, OutStepDownResult))
				{
					//UE_LOG(LogCharacterMovement, Verbose, TEXT("- StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					HandleImpact(Hit, LastMoveTimeSlice, RampVector);
					SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
				}
				else
				{
					//UE_LOG(LogCharacterMovement, Verbose, TEXT("+ StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					if (!bMaintainHorizontalGroundVelocity)
					{
						// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments. Only consider horizontal movement.
						bJustTeleported = true;
						const float StepUpTimeSlice = (1.f - PercentTimeApplied) * DeltaSeconds;
						if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && StepUpTimeSlice >= KINDA_SMALL_NUMBER)
						{
							if (bIsFlatWorld) // flat world
							{
								Velocity = (UpdatedComponent->GetComponentLocation() - PreStepUpLocation) / StepUpTimeSlice;
								Velocity.Z = 0;
							}
							else // planet
							{
								// Unrotate new velocity so we can set Z to zero, then re-rotate
								FVector TempVelocity = UnrotateVectorUsingGravity((UpdatedComponent->GetComponentLocation() - PreStepUpLocation) / StepUpTimeSlice);
								TempVelocity.Z = 0;
								Velocity = RotateVectorUsingGravity(TempVelocity);
							}
						}
					}
				}
			}
			else if (Hit.Component.IsValid() && !Hit.Component.Get()->CanCharacterStepUp(CharacterOwner))
			{
				HandleImpact(Hit, LastMoveTimeSlice, RampVector);
				SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
			}
		}
	}
}

void USurfaceNavCharacterMovementComponent::MaintainHorizontalGroundVelocity()
{
	if (SurfaceNavigation::IsFlatWorld) // flat world
	{
		if (Velocity.Z != 0.f)
		{
			if (bMaintainHorizontalGroundVelocity)
			{
				// Ramp movement already maintained the velocity, so we just want to remove the vertical component.
				Velocity.Z = 0.f;
			}
			else
			{
				// Rescale velocity to be horizontal but maintain magnitude of last update.
				Velocity = Velocity.GetSafeNormal2D() * Velocity.Size();
			}
		}
	}
	else // planet
	{
		// Unrotate velocity, process, then re-rotate
		FVector TempVelocity = UnrotateVectorUsingGravity(Velocity);

		if (TempVelocity.Z != 0.f)
		{
			if (bMaintainHorizontalGroundVelocity)
			{
				// Ramp movement already maintained the velocity, so we just want to remove the vertical component.
				TempVelocity.Z = 0.f;
			}
			else
			{
				// Rescale velocity to be horizontal but maintain magnitude of last update.
				TempVelocity = TempVelocity.GetSafeNormal2D() * TempVelocity.Size();
			}

			Velocity = RotateVectorUsingGravity(TempVelocity);
		}
	}
}

FVector USurfaceNavCharacterMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
	FVector Result = RootMotionVelocity;

	// Do not override Velocity.Z if in falling physics, we want to keep the effect of gravity.
	if (IsFalling())
	{
		if (SurfaceNavigation::IsFlatWorld)
			Result.Z = CurrentVelocity.Z;
		else
		{
			FVector TempResult = UnrotateVectorUsingGravity(Result);
			FVector TempCurrentVelocity = UnrotateVectorUsingGravity(CurrentVelocity);

			TempResult.Z = TempCurrentVelocity.Z;

			Result = RotateVectorUsingGravity(TempResult);
		}
	}

	return Result;
}

void USurfaceNavCharacterMovementComponent::ApplyRootMotionToVelocity(float deltaTime)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceApply);

	bool bIsFlatWorld = SurfaceNavigation::IsFlatWorld;

	// Animation root motion is distinct from root motion sources right now and takes precedence
	if (HasAnimRootMotion() && deltaTime > 0.f)
	{
		Velocity = ConstrainAnimRootMotionVelocity(AnimRootMotionVelocity, Velocity);
		if (IsFalling())
		{
			if (bIsFlatWorld)
			{
				Velocity += FVector(DecayingFormerBaseVelocity.X, DecayingFormerBaseVelocity.Y, 0.f);
			}
			else
			{
				// Unrotate DecayingFormerBaseVelocity, set Z to zero, then re-rotate
				FVector TempDecayingFormerBaseVelocity = UnrotateVectorUsingGravity(DecayingFormerBaseVelocity);
				TempDecayingFormerBaseVelocity.Z = 0;
				Velocity += RotateVectorUsingGravity(TempDecayingFormerBaseVelocity);
			}
		}

		return;
	}

	const FVector OldVelocity = Velocity;

	bool bAppliedRootMotion = false;

	// Apply override velocity
	if (CurrentRootMotion.HasOverrideVelocity())
	{
		// TODO: AccumulateOverrideRootMotionVelocity may cause a problem with overriding Z if we can't work around
		CurrentRootMotion.AccumulateOverrideRootMotionVelocity(deltaTime, *CharacterOwner, *this, Velocity);

		if (IsFalling())
		{
			if (bIsFlatWorld) // flat world
			{
				Velocity += CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(DecayingFormerBaseVelocity.X, DecayingFormerBaseVelocity.Y, 0.f) : DecayingFormerBaseVelocity;
			}
			else // planet
			{
				if (CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate())
				{
					// If we don't need to ignore Z, then just add DecayingFormerBaseVelocity
					Velocity += DecayingFormerBaseVelocity;
				}
				else
				{
					// Unrotate DecayingFormerBaseVelocity, set Z to zero, then re-rotate
					FVector TempDecayingFormerBaseVelocity = UnrotateVectorUsingGravity(DecayingFormerBaseVelocity);
					TempDecayingFormerBaseVelocity.Z = 0;
					Velocity += RotateVectorUsingGravity(TempDecayingFormerBaseVelocity);
				}
			}
		}
		bAppliedRootMotion = true;

		//#if ROOT_MOTION_DEBUG
		//		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		//		{
		//			FString AdjustedDebugString = FString::Printf(TEXT("ApplyRootMotionToVelocity HasOverrideVelocity Velocity(%s)"),
		//				*Velocity.ToCompactString());
		//			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		//		}
		//#endif
	}

	// Next apply additive root motion
	if (CurrentRootMotion.HasAdditiveVelocity())
	{
		CurrentRootMotion.LastPreAdditiveVelocity = Velocity; // Save off pre-additive Velocity for restoration next tick

		// TODO: AccumulateOverrideRootMotionVelocity may cause a problem with overriding Z if we can't work around
		CurrentRootMotion.AccumulateAdditiveRootMotionVelocity(deltaTime, *CharacterOwner, *this, Velocity);

		CurrentRootMotion.bIsAdditiveVelocityApplied = true; // Remember that we have it applied
		bAppliedRootMotion = true;

		//#if ROOT_MOTION_DEBUG
		//		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		//		{
		//			FString AdjustedDebugString = FString::Printf(TEXT("ApplyRootMotionToVelocity HasAdditiveVelocity Velocity(%s) LastPreAdditiveVelocity(%s)"),
		//				*Velocity.ToCompactString(), *CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString());
		//			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		//		}
		//#endif
	}

	// Switch to Falling if we have vertical velocity from root motion so we can lift off the ground
	FVector AppliedVelocityDelta = Velocity - OldVelocity;

	if (!bIsFlatWorld) // planet
		// We're on a planet, so unrotate velocity delta so we can test the Z portion
		AppliedVelocityDelta = UnrotateVectorUsingGravity(AppliedVelocityDelta);

	if (bAppliedRootMotion && AppliedVelocityDelta.Z != 0.f && IsMovingOnGround())
	{
		float LiftoffBound;
		if (CurrentRootMotion.LastAccumulatedSettings.HasFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck))
		{
			// Sensitive bounds - "any positive force"
			LiftoffBound = SMALL_NUMBER;
		}
		else
		{
			// Default bounds - the amount of force gravity is applying this tick
			LiftoffBound = FMath::Max(GetGravityZ() * deltaTime, SMALL_NUMBER);
		}

		if (AppliedVelocityDelta.Z > LiftoffBound)
		{
			SetMovementMode(MOVE_Falling);
		}
	}
}

bool USurfaceNavCharacterMovementComponent::ShouldComputePerchResult(const FHitResult& InHit, bool bCheckRadius) const
{
	if (!InHit.IsValidBlockingHit())
	{
		return false;
	}

	// Don't try to perch if the edge radius is very small.
	if (GetPerchRadiusThreshold() <= SWEEP_EDGE_REJECT_DISTANCE)
	{
		return false;
	}

	if (bCheckRadius)
	{
		//const float DistFromCenterSq = (InHit.ImpactPoint - InHit.Location).SizeSquared2D();

		// Need to unrotate if on a planet so we can get the proper distance to compare to the radius.
		const float DistFromCenterSq = SurfaceNavigation::IsFlatWorld ? (InHit.ImpactPoint - InHit.Location).SizeSquared2D() :
			UnrotateVectorUsingGravity(InHit.ImpactPoint - InHit.Location).SizeSquared2D();

		const float StandOnEdgeRadius = GetValidPerchRadius();
		if (DistFromCenterSq <= FMath::Square(StandOnEdgeRadius))
		{
			// Already within perch radius.
			return false;
		}
	}

	return true;
}

bool USurfaceNavCharacterMovementComponent::ComputePerchResult(const float TestRadius, const FHitResult& InHit, const float InMaxFloorDist, FFindFloorResult& OutPerchFloorResult) const
{
	if (InMaxFloorDist <= 0.f)
	{
		return false;
	}

	// Sweep further than actual requested distance, because a reduced capsule radius means we could miss some hits that the normal radius would contact.
	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
	const FVector CapsuleLocation = (bUseFlatBaseForFloorChecks ? InHit.TraceStart : InHit.Location);

	float InHitAboveBase;

	if (SurfaceNavigation::IsFlatWorld)
		InHitAboveBase = FMath::Max<float>(0.f, InHit.ImpactPoint.Z - (CapsuleLocation.Z - PawnHalfHeight));
	else
	{
		InHitAboveBase = FMath::Max<float>(0.f,
			UnrotateVectorUsingGravity(InHit.ImpactPoint - UpdatedComponent->GetComponentLocation()).Z -
			(UnrotateVectorUsingGravity(CapsuleLocation - UpdatedComponent->GetComponentLocation()).Z - PawnHalfHeight));
	}

	const float PerchLineDist = FMath::Max(0.f, InMaxFloorDist - InHitAboveBase);
	const float PerchSweepDist = FMath::Max(0.f, InMaxFloorDist);

	const float ActualSweepDist = PerchSweepDist + PawnRadius;
	ComputeFloorDist(CapsuleLocation, PerchLineDist, ActualSweepDist, OutPerchFloorResult, TestRadius);

	if (!OutPerchFloorResult.IsWalkableFloor())
	{
		return false;
	}
	else if (InHitAboveBase + OutPerchFloorResult.FloorDist > InMaxFloorDist)
	{
		// Hit something past max distance
		OutPerchFloorResult.bWalkableFloor = false;
		return false;
	}

	return true;
}

FVector USurfaceNavCharacterMovementComponent::GetLedgeMove(const FVector& OldLocation, const FVector& Delta, const FVector& GravDir) const
{
	if (!HasValidData() || Delta.IsZero())
	{
		return FVector::ZeroVector;
	}

	//FVector SideDir(Delta.Y, -1.f * Delta.X, 0.f);
	FVector SideDir = RotateVectorUsingGravity(FVector(Delta.Y, -1.f * Delta.X, 0.f));

	// try left
	if (CheckLedgeDirection(OldLocation, SideDir, GravDir))
	{
		return SideDir;
	}

	// try right
	//SideDir *= -1.f;
	SideDir = RotateVectorUsingGravity(FVector(-1.f * Delta.Y, Delta.X, 0.f));

	if (CheckLedgeDirection(OldLocation, SideDir, GravDir))
	{
		return SideDir;
	}

	return FVector::ZeroVector;
}

void USurfaceNavCharacterMovementComponent::StartFalling(int32 Iterations, float remainingTime, float timeTick, const FVector& Delta, const FVector& subLoc)
{
	// start falling 
	const float DesiredDist = Delta.Size();

	//const float ActualDist = (UpdatedComponent->GetComponentLocation() - subLoc).Size2D();
	const float ActualDist = UnrotateVectorUsingGravity((UpdatedComponent->GetComponentLocation() - subLoc)).Size2D();

	remainingTime = (DesiredDist < KINDA_SMALL_NUMBER)
		? 0.f
		: remainingTime + timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));

	if (IsMovingOnGround())
	{
		// This is to catch cases where the first frame of PIE is executed, and the
		// level is not yet visible. In those cases, the player will fall out of the
		// world... So, don't set MOVE_Falling straight away.
		if (!GIsEditor || (GetWorld()->HasBegunPlay() && (GetWorld()->GetTimeSeconds() >= 1.f)))
		{
			SetMovementMode(MOVE_Falling); //default behavior if script didn't change physics
		}
		else
		{
			// Make sure that the floor check code continues processing during this delay.
			bForceNextFloorCheck = true;
		}
	}
	StartNewPhysics(remainingTime, Iterations);
}

void USurfaceNavCharacterMovementComponent::AdjustFloorHeight()
{
	//SCOPE_CYCLE_COUNTER(STAT_CharAdjustFloorHeight);

	// If we have a floor check that hasn't hit anything, don't adjust height.
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	float OldFloorDist = CurrentFloor.FloorDist;
	if (CurrentFloor.bLineTrace)
	{
		if (OldFloorDist < MIN_FLOOR_DIST && CurrentFloor.LineDist >= MIN_FLOOR_DIST)
		{
			// This would cause us to scale unwalkable walls
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("Adjust floor height aborting due to line trace with small floor distance (line: %.2f, sweep: %.2f)"), CurrentFloor.LineDist, CurrentFloor.FloorDist);
			return;
		}
		else
		{
			// Falling back to a line trace means the sweep was unwalkable (or in penetration). Use the line distance for the vertical adjustment.
			OldFloorDist = CurrentFloor.LineDist;
		}
	}

	// Move up or down to maintain floor height.
	if (OldFloorDist < MIN_FLOOR_DIST || OldFloorDist > MAX_FLOOR_DIST)
	{
		bool bIsFlatWorld = SurfaceNavigation::IsFlatWorld;

		FHitResult AdjustHit(1.f);

		const FVector InitialLoc = UpdatedComponent->GetComponentLocation();
		const float InitialZ = UpdatedComponent->GetComponentLocation().Z;

		const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;
		const float MoveDist = AvgFloorDist - OldFloorDist;
		SafeMoveUpdatedComponent(FVector(0.f, 0.f, MoveDist), UpdatedComponent->GetComponentQuat(), true, AdjustHit);
		//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("Adjust floor height %.3f (Hit = %d)"), MoveDist, AdjustHit.bBlockingHit);

		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist;
		}
		else if (MoveDist > 0.f)
		{
			const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;

			if (bIsFlatWorld)
				CurrentFloor.FloorDist += CurrentZ - InitialZ;
			else
				CurrentFloor.FloorDist += UnrotateVectorUsingGravity(UpdatedComponent->GetComponentLocation() - InitialLoc).Z;
		}
		else
		{
			checkSlow(MoveDist < 0.f);

			const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;

			if (bIsFlatWorld)
				CurrentFloor.FloorDist = CurrentZ - AdjustHit.Location.Z;
			else
				CurrentFloor.FloorDist = UnrotateVectorUsingGravity(UpdatedComponent->GetComponentLocation() - AdjustHit.Location).Z;

			if (IsWalkable(AdjustHit))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
		}

		// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
		// Also avoid it if we moved out of penetration
		bJustTeleported |= !bMaintainHorizontalGroundVelocity || (OldFloorDist < 0.f);

		// If something caused us to adjust our height (especially a depentration) we should ensure another check next frame or we will keep a stale result.
		if (CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
		{
			bForceNextFloorCheck = true;
		}
	}
}

void USurfaceNavCharacterMovementComponent::StartSwimming(FVector OldLocation, FVector OldVelocity, float timeTick, float remainingTime, int32 Iterations)
{
	if (remainingTime < MIN_TICK_TIME || timeTick < MIN_TICK_TIME)
	{
		return;
	}

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && !bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick; //actual average velocity
		Velocity = 2.f * Velocity - OldVelocity; //end velocity has 2* accel of avg
		Velocity = Velocity.GetClampedToMaxSize(GetPhysicsVolume()->TerminalVelocity);
	}
	const FVector End = FindWaterLine(UpdatedComponent->GetComponentLocation(), OldLocation);
	float waterTime = 0.f;
	if (End != UpdatedComponent->GetComponentLocation())
	{
		const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size();
		if (ActualDist > KINDA_SMALL_NUMBER)
		{
			waterTime = timeTick * (End - UpdatedComponent->GetComponentLocation()).Size() / ActualDist;
			remainingTime += waterTime;
		}
		MoveUpdatedComponent(End - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true);
	}

	if (SurfaceNavigation::IsFlatWorld) // flat world
	{
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (Velocity.Z > 2.f * CharacterMovementConstants::SWIMBOBSPEED) && (Velocity.Z < 0.f)) //allow for falling out of water
		{
			Velocity.Z = CharacterMovementConstants::SWIMBOBSPEED - Velocity.Size2D() * 0.7f; //smooth bobbing
		}
	}
	else // planet
	{
		// Unrotate the velocity so we can work on just Z
		FVector TempVelocity = UnrotateVectorUsingGravity(Velocity);

		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (TempVelocity.Z > 2.f * CharacterMovementConstants::SWIMBOBSPEED) && (TempVelocity.Z < 0.f)) //allow for falling out of water
		{
			TempVelocity.Z = CharacterMovementConstants::SWIMBOBSPEED - TempVelocity.Size2D() * 0.7f; //smooth bobbing

			// Re-rotate the modified TempVelocity and assign back to Velocity
			Velocity = RotateVectorUsingGravity(TempVelocity);
		}
	}

	if ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		PhysSwimming(remainingTime, Iterations);
	}
}

void USurfaceNavCharacterMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysWalking);
	//CSV_SCOPED_TIMING_STAT_EXCLUSIVE(CharPhysWalking);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		SetMovementMode(MOVE_Walking);
		return;
	}

	//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN before Iteration (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

	bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float remainingTime = deltaTime;

	bool bIsFlatWorld = SurfaceNavigation::IsFlatWorld;

	// Perform the move
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity() || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)))
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Save current values
		UPrimitiveComponent* const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FFindFloorResult OldFloor = CurrentFloor;

		RestorePreAdditiveRootMotionVelocity();

		// Ensure velocity is horizontal.
		MaintainHorizontalGroundVelocity();
		FVector OldVelocity = Velocity;

		if (bIsFlatWorld)
		{
			Acceleration.Z = 0.f;
		}
		else
		{
			// Unrotate acceleration, set Z to zero, then re-rotate
			FVector TempAcceleration = UnrotateVectorUsingGravity(Acceleration);
			TempAcceleration.Z = 0;
			Acceleration = RotateVectorUsingGravity(TempAcceleration);
		}

		// Apply acceleration
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			CalcVelocity(timeTick, GroundFriction, false, GetMaxBrakingDeceleration());
			//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
		}

		ApplyRootMotionToVelocity(timeTick);
		//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after Root Motion application (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

		if (IsFalling())
		{
			// Root motion could have put us into Falling.
			// No movement has taken place this movement tick so we pass on full time/past iteration count
			StartNewPhysics(remainingTime + timeTick, Iterations - 1);
			return;
		}

		// Compute move parameters
		const FVector MoveVelocity = Velocity;
		const FVector Delta = timeTick * MoveVelocity;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;

		if (bZeroDelta)
		{
			remainingTime = 0.f;
		}
		else
		{
			// try to move forward
			MoveAlongFloor(MoveVelocity, timeTick, &StepDownResult);

			if (IsFalling())
			{
				// pawn decided to jump up
				const float DesiredDist = Delta.Size();
				if (DesiredDist > KINDA_SMALL_NUMBER)
				{
					if (bIsFlatWorld)
					{
						const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size2D();
						remainingTime += timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));
					}
					else
					{
						const float ActualDist = UnrotateVectorUsingGravity(UpdatedComponent->GetComponentLocation() - OldLocation).Size2D();
						remainingTime += timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));
					}
				}
				StartNewPhysics(remainingTime, Iterations);
				return;
			}
			else if (IsSwimming()) //just entered water
			{
				StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		// Update floor.
		// StepUp might have already done it for us.
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, NULL);
		}

		// check for ledges here
		const bool bCheckLedges = !CanWalkOffLedges();
		if (bCheckLedges && !CurrentFloor.IsWalkableFloor())
		{
			// calculate possible alternate movement
			const FVector GravDir = GetGravityNormal(); // FVector(0.f, 0.f, -1.f);
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, GravDir);
			if (!NewDelta.IsZero())
			{
				// first revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

				// avoid repeated ledge moves if the first one fails
				bTriedLedgeMove = true;

				// Try new movement direction
				Velocity = NewDelta / timeTick;
				remainingTime += timeTick;
				continue;
			}
			else
			{
				// see if it is OK to jump
				// @todo collision : only thing that can be problem is that oldbase has world collision on
				bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;

				// revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				remainingTime = 0.f;
				break;
			}
		}
		else
		{
			// Validate the floor check
			if (CurrentFloor.IsWalkableFloor())
			{
				if (ShouldCatchAir(OldFloor, CurrentFloor))
				{
					HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
					if (IsMovingOnGround())
					{
						// If still walking, then fall. If not, assume the user set a different mode they want to keep.
						StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation);
					}
					return;
				}

				AdjustFloorHeight();
				SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
			}
			else if (CurrentFloor.HitResult.bStartPenetrating && remainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration
				// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
				FHitResult Hit(CurrentFloor.HitResult);

				//Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, MAX_FLOOR_DIST);
				Hit.TraceEnd = Hit.TraceStart + (GetGroundNormal() * MAX_FLOOR_DIST);

				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				bForceNextFloorCheck = true;
			}

			// check if just entered water
			if (IsSwimming())
			{
				StartSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}

			// See if we need to start falling.
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				const bool bMustJump = bJustTeleported || bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;
			}
		}


		// Allow overlap events and such to change physics state and velocity
		if (IsMovingOnGround())
		{
			// Make velocity reflect actual move
			if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && timeTick >= MIN_TICK_TIME)
			{
				// TODO-RootMotionSource: Allow this to happen during partial override Velocity, but only set allowed axes?
				Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick;
				MaintainHorizontalGroundVelocity();
			}
		}

		// If we didn't move at all this iteration then abort (since future iterations will also be stuck).
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
	}

	if (IsMovingOnGround())
	{
		MaintainHorizontalGroundVelocity();
	}
}

FVector USurfaceNavCharacterMovementComponent::GetFallingLateralAcceleration(float DeltaTime)
{
	// No acceleration in Z
	//FVector FallAcceleration = FVector(Acceleration.X, Acceleration.Y, 0.f);

	// Need to unrotate first acceleration first
	FVector FallAcceleration = UnrotateVectorUsingGravity(Acceleration);
	// Now we can zero out Z
	FallAcceleration.Z = 0.f;

	// bound acceleration, falling object has minimal ability to impact acceleration
	if (!HasAnimRootMotion() && FallAcceleration.SizeSquared2D() > 0.f)
	{
		// Don't need to re-rotate FallAcceleration since GetAirControl assumes
		// it's already unrotated.
		FallAcceleration = GetAirControl(DeltaTime, AirControl, FallAcceleration);
		FallAcceleration = FallAcceleration.GetClampedToMaxSize(GetMaxAcceleration());
	}

	//return FallAcceleration;
	return RotateVectorUsingGravity(FallAcceleration);
}

float USurfaceNavCharacterMovementComponent::BoostAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	// Allow a burst of initial acceleration
	if (AirControlBoostMultiplier > 0.f && UnrotateVectorUsingGravity(Velocity).SizeSquared2D() < FMath::Square(AirControlBoostVelocityThreshold))
	{
		TickAirControl = FMath::Min(1.f, AirControlBoostMultiplier * TickAirControl);
	}

	return TickAirControl;
}

bool USurfaceNavCharacterMovementComponent::ShouldLimitAirControl(float DeltaTime, const FVector& FallAcceleration) const
{
	// ***************************************************************
	// SurfaceNav Note: 
	// ~~~~~~~~~~~~~~~~
	// Do not attempts to unrotate this vector even though we're calling SizeSquared2D.
	// This is because only PhysFalling calls this function. PhysFalling already has an 
	// unrotated vector so we don't want to unrotate it a 2nd time.
	// ***************************************************************


	return (FallAcceleration.SizeSquared2D() > 0.f);
}

FVector USurfaceNavCharacterMovementComponent::LimitAirControl(float DeltaTime, const FVector& FallAcceleration, const FHitResult& HitResult, bool bCheckForValidLandingSpot)
{
	FVector Result(FallAcceleration);

	if (SurfaceNavigation::IsFlatWorld) // flat world
	{
		if (HitResult.IsValidBlockingHit() && HitResult.Normal.Z > CharacterMovementConstants::VERTICAL_SLOPE_NORMAL_Z)
		{
			if (!bCheckForValidLandingSpot || !IsValidLandingSpot(HitResult.Location, HitResult))
			{
				// If acceleration is into the wall, limit contribution.
				if (FVector::DotProduct(FallAcceleration, HitResult.Normal) < 0.f)
				{
					// Allow movement parallel to the wall, but not into it because that may push us up.
					const FVector Normal2D = HitResult.Normal.GetSafeNormal2D();
					Result = FVector::VectorPlaneProject(FallAcceleration, Normal2D);
				}
			}
		}
		else if (HitResult.bStartPenetrating)
		{
			// Allow movement out of penetration.
			return (FVector::DotProduct(Result, HitResult.Normal) > 0.f ? Result : FVector::ZeroVector);
		}
	}
	else // planet
	{
		if (HitResult.IsValidBlockingHit() && (GetGroundNormal() | HitResult.Normal) > CharacterMovementConstants::VERTICAL_SLOPE_NORMAL_Z)
		{
			if (!bCheckForValidLandingSpot || !IsValidLandingSpot(HitResult.Location, HitResult))
			{
				// If acceleration is into the wall, limit contribution.
				if (FVector::DotProduct(FallAcceleration, HitResult.Normal) < 0.f)
				{
					// Allow movement parallel to the wall, but not into it because that may push us up.
					const FVector Normal2D = RotateVectorUsingGravity(UnrotateVectorUsingGravity(HitResult.Normal).GetSafeNormal2D());
					Result = FVector::VectorPlaneProject(FallAcceleration, Normal2D);
				}
			}
		}
		else if (HitResult.bStartPenetrating)
		{
			// Allow movement out of penetration.
			return (FVector::DotProduct(Result, HitResult.Normal) > 0.f ? Result : FVector::ZeroVector);
		}
	}

	return Result;
}

FVector USurfaceNavCharacterMovementComponent::HandleSlopeBoosting(const FVector& SlideResult, const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const
{
	FVector Result = SlideResult;
	FVector UnrotResult = UnrotateVectorUsingGravity(Result);
	FVector UnrotDelta = UnrotateVectorUsingGravity(Delta);
	FVector UnrotNormal = UnrotateVectorUsingGravity(Normal);

	if (UnrotResult.Z > 0.f)
	{
		// Don't move any higher than we originally intended.
		const float ZLimit = UnrotDelta.Z * Time;
		if (UnrotResult.Z - ZLimit > KINDA_SMALL_NUMBER)
		{
			if (ZLimit > 0.f)
			{
				// Rescale the entire vector (not just the Z component) otherwise we change the direction and likely head right back into the impact.
				const float UpPercent = ZLimit / UnrotResult.Z;
				UnrotResult *= UpPercent;
			}
			else
			{
				// We were heading down but were going to deflect upwards. Just make the deflection horizontal.
				UnrotResult = FVector::ZeroVector;
			}


			// Make remaining portion of original result horizontal and parallel to impact normal.
			const FVector RemainderXY = RotateVectorUsingGravity((UnrotateVectorUsingGravity(SlideResult) - UnrotResult) * FVector(1.f, 1.f, 0.f));
			const FVector NormalXY = RotateVectorUsingGravity(UnrotNormal.GetSafeNormal2D());

			// Need to bypass character movement function so call Super::Super::
			const FVector Adjust = Super::Super::ComputeSlideVector(RemainderXY, 1.f, NormalXY, Hit);

			Result = RotateVectorUsingGravity(UnrotResult);
			Result += Adjust;
		}
	}

	return Result;
}

bool USurfaceNavCharacterMovementComponent::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}

	// Skip some checks if penetrating. Penetration will be handled by the FindFloor call (using a smaller capsule)
	if (!Hit.bStartPenetrating)
	{
		// Reject unwalkable floor normals.
		if (!IsWalkable(Hit))
		{
			return false;
		}

		float PawnRadius, PawnHalfHeight;
		CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

		// Reject hits that are above our lower hemisphere (can happen when sliding down a vertical surface).
		//const float LowerHemisphereZ = Hit.Location.Z - PawnHalfHeight + PawnRadius;
		const float LowerHemisphereZ = UnrotateVectorUsingGravity(Hit.Location).Z - PawnHalfHeight + PawnRadius;

		//if (Hit.ImpactPoint.Z >= LowerHemisphereZ)
		if (UnrotateVectorUsingGravity(Hit.ImpactPoint).Z >= LowerHemisphereZ)
		{
			return false;
		}

		// Reject hits that are barely on the cusp of the radius of the capsule
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			return false;
		}
	}
	else
	{
		// Penetrating
		//if (Hit.Normal.Z < KINDA_SMALL_NUMBER)
		if ((GetGroundNormal() | Hit.Normal) < KINDA_SMALL_NUMBER)
		{
			// Normal is nearly horizontal or downward, that's a penetration adjustment next to a vertical or overhanging wall. Don't pop to the floor.
			return false;
		}
	}

	FFindFloorResult FloorResult;
	FindFloor(CapsuleLocation, FloorResult, false, &Hit);

	if (!FloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}

void USurfaceNavCharacterMovementComponent::TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const
{
	const FVector InDelta = UnrotateVectorUsingGravity(Delta);
	Super::Super::TwoWallAdjust(Delta, Hit, OldHitNormal);

	FVector UnrotDelta = UnrotateVectorUsingGravity(Delta);

	if (IsMovingOnGround())
	{
		// Allow slides up walkable surfaces, but not unwalkable ones (treat those as vertical barriers).
		if (UnrotDelta.Z > 0.f)
		{
			FVector UnrotHitNormal = UnrotateVectorUsingGravity(Hit.Normal);

			//if ((Hit.Normal.Z >= WalkableFloorZ || IsWalkable(Hit)) && Hit.Normal.Z > KINDA_SMALL_NUMBER)
			if ((UnrotHitNormal.Z >= GetWalkableFloorZ() || IsWalkable(Hit)) && UnrotHitNormal.Z > KINDA_SMALL_NUMBER)
			{
				// Maintain horizontal velocity
				const float Time = (1.f - Hit.Time);
				const FVector ScaledDelta = UnrotDelta.GetSafeNormal() * InDelta.Size();
				UnrotDelta = FVector(InDelta.X, InDelta.Y, ScaledDelta.Z / UnrotHitNormal.Z) * Time;

				// Should never exceed MaxStepHeight in vertical component, so rescale if necessary.
				// This should be rare (Hit.Normal.Z above would have been very small) but we'd rather lose horizontal velocity than go too high.
				if (UnrotDelta.Z > MaxStepHeight)
				{
					const float Rescale = MaxStepHeight / UnrotDelta.Z;
					UnrotDelta *= Rescale;
				}
			}
			else
			{
				UnrotDelta.Z = 0.f;
			}

			Delta = RotateVectorUsingGravity(UnrotDelta);
		}
		else if (UnrotDelta.Z < 0.f)
		{
			// Don't push down into the floor.
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				UnrotDelta.Z = 0.f;
			}

			Delta = RotateVectorUsingGravity(UnrotDelta);
		}
	}
}

void USurfaceNavCharacterMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysFalling);
	//CSV_SCOPED_TIMING_STAT_EXCLUSIVE(CharPhysFalling);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	// SN Note: There's so much usage that we just want to create 2 and sync up
	// from time to time.
	FVector FallAcceleration = GetFallingLateralAcceleration(deltaTime);
	FVector UnrotFallAcceleration = UnrotateVectorUsingGravity(FallAcceleration);

	// Zero out Z for unrotated version
	UnrotFallAcceleration.Z = 0.f;

	// Currently PhysFalling is the only function that ccalls ShouldLimitAirControl so save some
	// processing by not re-rotating here, only to need to unrotate and re-rotate inside ShouldLimitAirControl.
	// ShouldLimitAirControl assumes that FallAcceleration is already unrotated.
	const bool bHasLimitedAirControl = ShouldLimitAirControl(deltaTime, UnrotFallAcceleration);

	// Re-sync
	FallAcceleration = RotateVectorUsingGravity(UnrotFallAcceleration);

	float remainingTime = deltaTime;
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		Iterations++;
		float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
		bJustTeleported = false;

		// SN Note: There's so much usage that we just want to create 2 and sync up
		// from time to time.
		const FVector OldVelocityWithRootMotion = Velocity;
		const FVector UnrotOldVelocityWithRootMotion = UnrotateVectorUsingGravity(OldVelocityWithRootMotion);

		RestorePreAdditiveRootMotionVelocity();

		// SN Note: There's so much usage that we just want to create 2 and sync up
		// from time to time.
		const FVector OldVelocity = Velocity;
		const FVector UnrotOldVelocity = UnrotateVectorUsingGravity(OldVelocity);

		// Apply input
		const float MaxDecel = GetMaxBrakingDeceleration();
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			// Compute Velocity
			{
				// Acceleration = FallAcceleration for CalcVelocity(), but we restore it after using it.
				TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);

				//Velocity.Z = 0.f;
				//CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
				//Velocity.Z = OldVelocity.Z;


				// Unrotate velocity, zero out Z, then re-rotate for processing in CalcVelocity
				FVector TempUnrotVelocity = UnrotateVectorUsingGravity(Velocity);
				TempUnrotVelocity.Z = 0.f;
				Velocity = RotateVectorUsingGravity(TempUnrotVelocity);

				CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);

				// Now unrotate Velocity to get the Z from unrotated OldVelocity
				// and set it to the Z for velocity, then re-rotate velocity
				TempUnrotVelocity = UnrotateVectorUsingGravity(Velocity);
				TempUnrotVelocity.Z = UnrotOldVelocity.Z;
				Velocity = RotateVectorUsingGravity(TempUnrotVelocity);
			}
		}

		// Compute current gravity
		//const FVector Gravity(0.f, 0.f, GetGravityZ());
		const FVector Gravity = GetGravity();

		float GravityTime = timeTick;

		// If jump is providing force, gravity may be affected.
		bool bEndingJumpForce = false;
		if (CharacterOwner->JumpForceTimeRemaining > 0.0f)
		{
			// Consume some of the force time. Only the remaining time (if any) is affected by gravity when bApplyGravityWhileJumping=false.
			const float JumpForceTime = FMath::Min(CharacterOwner->JumpForceTimeRemaining, timeTick);
			GravityTime = bApplyGravityWhileJumping ? timeTick : FMath::Max(0.0f, timeTick - JumpForceTime);

			// Update Character state
			CharacterOwner->JumpForceTimeRemaining -= JumpForceTime;
			if (CharacterOwner->JumpForceTimeRemaining <= 0.0f)
			{
				CharacterOwner->ResetJumpState();
				bEndingJumpForce = true;
			}
		}

		// Apply gravity
		Velocity = NewFallVelocity(Velocity, Gravity, GravityTime);

		//UE_LOG(LogCharacterMovement, Log, TEXT("dt=(%.6f) OldLocation=(%s) OldVelocity=(%s) OldVelocityWithRootMotion=(%s) NewVelocity=(%s)"), timeTick, *(UpdatedComponent->GetComponentLocation()).ToString(), *OldVelocity.ToString(), *OldVelocityWithRootMotion.ToString(), *Velocity.ToString());
		ApplyRootMotionToVelocity(timeTick);
		DecayFormerBaseVelocity(timeTick);

		// See if we need to sub-step to exactly reach the apex. This is important for avoiding "cutting off the top" of the trajectory as framerate varies.
		//if (CharacterMovementCVars::ForceJumpPeakSubstep && OldVelocityWithRootMotion.Z > 0.f && Velocity.Z <= 0.f && NumJumpApexAttempts < MaxJumpApexAttemptsPerSimulation)
		{
			FVector TempUnrotVelocity = UnrotateVectorUsingGravity(Velocity);
			if (CharacterMovementCVars::ForceJumpPeakSubstep && UnrotOldVelocityWithRootMotion.Z > 0.f && TempUnrotVelocity.Z <= 0.f && NumJumpApexAttempts < MaxJumpApexAttemptsPerSimulation)
			{
				//const FVector DerivedAccel = (Velocity - OldVelocityWithRootMotion) / timeTick;
				const FVector UnrotDerivedAccel = (TempUnrotVelocity - UnrotOldVelocityWithRootMotion) / timeTick;

				if (!FMath::IsNearlyZero(UnrotDerivedAccel.Z))
				{
					const float TimeToApex = -UnrotOldVelocityWithRootMotion.Z / UnrotDerivedAccel.Z;

					// The time-to-apex calculation should be precise, and we want to avoid adding a substep when we are basically already at the apex from the previous iteration's work.
					const float ApexTimeMinimum = 0.0001f;
					if (TimeToApex >= ApexTimeMinimum && TimeToApex < timeTick)
					{
						//const FVector ApexVelocity = OldVelocityWithRootMotion + (DerivedAccel * TimeToApex);
						//Velocity = ApexVelocity;
						//Velocity.Z = 0.f; // Should be nearly zero anyway, but this makes apex notifications consistent.

						const FVector UnrotApexVelocity = UnrotOldVelocityWithRootMotion + (UnrotDerivedAccel * TimeToApex);
						TempUnrotVelocity = UnrotApexVelocity;
						TempUnrotVelocity.Z = 0.f; // Should be nearly zero anyway, but this makes apex notifications consistent.
						Velocity = RotateVectorUsingGravity(TempUnrotVelocity);

						// We only want to move the amount of time it takes to reach the apex, and refund the unused time for next iteration.
						const float TimeToRefund = (timeTick - TimeToApex);

						remainingTime += TimeToRefund;
						timeTick = TimeToApex;
						Iterations--;
						NumJumpApexAttempts++;

						// Refund time to any active Root Motion Sources as well
						for (TSharedPtr<FRootMotionSource> RootMotionSource : CurrentRootMotion.RootMotionSources)
						{
							const float RewoundRMSTime = FMath::Max(0.0f, RootMotionSource->GetTime() - TimeToRefund);
							RootMotionSource->SetTime(RewoundRMSTime);
						}
					}
				}
			}

			//if (bNotifyApex && (Velocity.Z < 0.f))
			if (bNotifyApex && (TempUnrotVelocity.Z < 0.f))
			{
				// Just passed jump apex since now going down
				bNotifyApex = false;
				NotifyJumpApex();
			}
		}

		// Compute change in position (using midpoint integration method).
		FVector Adjusted = 0.5f * (OldVelocityWithRootMotion + Velocity) * timeTick;

		// Special handling if ending the jump force where we didn't apply gravity during the jump.
		if (bEndingJumpForce && !bApplyGravityWhileJumping)
		{
			// We had a portion of the time at constant speed then a portion with acceleration due to gravity.
			// Account for that here with a more correct change in position.
			const float NonGravityTime = FMath::Max(0.f, timeTick - GravityTime);
			Adjusted = (OldVelocityWithRootMotion * NonGravityTime) + (0.5f * (OldVelocityWithRootMotion + Velocity) * GravityTime);
		}

		// Move
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Adjusted, PawnRotation, true, Hit);

		if (!HasValidData())
		{
			return;
		}

		float LastMoveTimeSlice = timeTick;
		float subTimeTickRemaining = timeTick * (1.f - Hit.Time);

		if (IsSwimming()) //just entered water
		{
			remainingTime += subTimeTickRemaining;
			StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
			return;
		}
		else if (Hit.bBlockingHit)
		{
			if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
			{
				remainingTime += subTimeTickRemaining;
				ProcessLanded(Hit, remainingTime, Iterations);
				return;
			}
			else
			{
				// Compute impact deflection based on final velocity, not integration step.
				// This allows us to compute a new velocity from the deflected vector, and ensures the full gravity effect is included in the slide result.
				Adjusted = Velocity * timeTick;

				// See if we can convert a normally invalid landing spot (based on the hit result) to a usable one.
				if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(timeTick, Adjusted, Hit))
				{
					const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
					FFindFloorResult FloorResult;
					FindFloor(PawnLocation, FloorResult, false);
					if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
					{
						remainingTime += subTimeTickRemaining;
						ProcessLanded(FloorResult.HitResult, remainingTime, Iterations);
						return;
					}
				}

				HandleImpact(Hit, LastMoveTimeSlice, Adjusted);

				// If we've changed physics mode, abort.
				if (!HasValidData() || !IsFalling())
				{
					return;
				}

				// Limit air control based on what we hit.
				// We moved to the impact point using air control, but may want to deflect from there based on a limited air control acceleration.
				FVector VelocityNoAirControl = OldVelocity;
				FVector UnrotVelocityNoAirControl = UnrotOldVelocity;

				FVector AirControlAccel = Acceleration;
				//FVector UnrotAirControlAccel = UnrotateVectorUsingGravity(AirControlAccel);

				if (bHasLimitedAirControl)
				{
					// Compute VelocityNoAirControl
					{
						// Find velocity *without* acceleration.
						/*TGuardValue<FVector> RestoreAcceleration(Acceleration, FVector::ZeroVector);
						TGuardValue<FVector> RestoreVelocity(Velocity, OldVelocity);
						Velocity.Z = 0.f;
						CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
						VelocityNoAirControl = FVector(Velocity.X, Velocity.Y, OldVelocity.Z);
						VelocityNoAirControl = NewFallVelocity(VelocityNoAirControl, Gravity, GravityTime);*/


						TGuardValue<FVector> RestoreAcceleration(Acceleration, FVector::ZeroVector);
						TGuardValue<FVector> RestoreVelocity(Velocity, OldVelocity);

						FVector TempUnrotVelocity = UnrotateVectorUsingGravity(Velocity);
						TempUnrotVelocity.Z = 0.f;
						Velocity = RotateVectorUsingGravity(TempUnrotVelocity);

						CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
						TempUnrotVelocity = UnrotateVectorUsingGravity(Velocity);

						UnrotVelocityNoAirControl = FVector(TempUnrotVelocity.X, TempUnrotVelocity.Y, UnrotOldVelocity.Z);
						VelocityNoAirControl = NewFallVelocity(RotateVectorUsingGravity(UnrotVelocityNoAirControl), Gravity, GravityTime);
						UnrotVelocityNoAirControl = UnrotateVectorUsingGravity(VelocityNoAirControl);
					}

					const bool bCheckLandingSpot = false; // we already checked above.
					AirControlAccel = (Velocity - VelocityNoAirControl) / timeTick;
					const FVector AirControlDeltaV = LimitAirControl(LastMoveTimeSlice, AirControlAccel, Hit, bCheckLandingSpot) * LastMoveTimeSlice;
					Adjusted = (VelocityNoAirControl + AirControlDeltaV) * LastMoveTimeSlice;
				}

				const FVector OldHitNormal = Hit.Normal;
				const FVector UnrotOldHitNormal = UnrotateVectorUsingGravity(OldHitNormal);

				const FVector OldHitImpactNormal = Hit.ImpactNormal;
				const FVector UnrotOldHitImpactNormal = UnrotateVectorUsingGravity(OldHitImpactNormal);

				FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, OldHitNormal, Hit);

				// Compute velocity after deflection (only gravity component for RootMotion)
				const UPrimitiveComponent* HitComponent = Hit.GetComponent();
				if (CharacterMovementCVars::UseTargetVelocityOnImpact && !Velocity.IsNearlyZero() && MovementBaseUtility::IsSimulatedBase(HitComponent))
				{
					const FVector ContactVelocity = MovementBaseUtility::GetMovementBaseVelocity(HitComponent, NAME_None) + MovementBaseUtility::GetMovementBaseTangentialVelocity(HitComponent, NAME_None, Hit.ImpactPoint);
					const FVector NewVelocity = Velocity - Hit.ImpactNormal * FVector::DotProduct(Velocity - ContactVelocity, Hit.ImpactNormal);

					/*Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate()
						? FVector(Velocity.X, Velocity.Y, NewVelocity.Z)
						: NewVelocity;*/
					if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate())
					{
						FVector UnrotVelocity = UnrotateVectorUsingGravity(Velocity);
						FVector UnrotNewVelocity = UnrotateVectorUsingGravity(NewVelocity);

						Velocity = RotateVectorUsingGravity(FVector(UnrotVelocity.X, UnrotVelocity.Y, UnrotNewVelocity.Z));
					}
					else
						Velocity = NewVelocity;
				}
				else if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
				{
					const FVector NewVelocity = (Delta / subTimeTickRemaining);

					/*Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate()
						? FVector(Velocity.X, Velocity.Y, NewVelocity.Z)
						: NewVelocity;*/
					if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate())
					{
						FVector UnrotVelocity = UnrotateVectorUsingGravity(Velocity);
						FVector UnrotNewVelocity = UnrotateVectorUsingGravity(NewVelocity);

						Velocity = RotateVectorUsingGravity(FVector(UnrotVelocity.X, UnrotVelocity.Y, UnrotNewVelocity.Z));
					}
					else
						Velocity = NewVelocity;
				}

				if (subTimeTickRemaining > KINDA_SMALL_NUMBER && (Delta | Adjusted) > 0.f)
				{
					// Move in deflected direction.
					SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);

					if (Hit.bBlockingHit)
					{
						// hit second wall
						LastMoveTimeSlice = subTimeTickRemaining;
						subTimeTickRemaining = subTimeTickRemaining * (1.f - Hit.Time);

						if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
						{
							remainingTime += subTimeTickRemaining;
							ProcessLanded(Hit, remainingTime, Iterations);
							return;
						}

						HandleImpact(Hit, LastMoveTimeSlice, Delta);

						// If we've changed physics mode, abort.
						if (!HasValidData() || !IsFalling())
						{
							return;
						}

						// Act as if there was no air control on the last move when computing new deflection.
						//if (bHasLimitedAirControl && Hit.Normal.Z > CharacterMovementConstants::VERTICAL_SLOPE_NORMAL_Z)
						if (bHasLimitedAirControl && (GetGroundNormal() | Hit.Normal) > CharacterMovementConstants::VERTICAL_SLOPE_NORMAL_Z)
						{
							const FVector LastMoveNoAirControl = VelocityNoAirControl * LastMoveTimeSlice;
							Delta = ComputeSlideVector(LastMoveNoAirControl, 1.f, OldHitNormal, Hit);
						}

						FVector PreTwoWallDelta = Delta;
						TwoWallAdjust(Delta, Hit, OldHitNormal);

						// Limit air control, but allow a slide along the second wall.
						if (bHasLimitedAirControl)
						{
							const bool bCheckLandingSpot = false; // we already checked above.
							const FVector AirControlDeltaV = LimitAirControl(subTimeTickRemaining, AirControlAccel, Hit, bCheckLandingSpot) * subTimeTickRemaining;

							// Only allow if not back in to first wall
							if (FVector::DotProduct(AirControlDeltaV, OldHitNormal) > 0.f)
							{
								Delta += (AirControlDeltaV * subTimeTickRemaining);
							}
						}

						// Compute velocity after deflection (only gravity component for RootMotion)
						if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
						{
							const FVector NewVelocity = (Delta / subTimeTickRemaining);

							//Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
							if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate())
							{
								FVector UnrotVelocity = UnrotateVectorUsingGravity(Velocity);
								FVector UnrotNewVelocity = UnrotateVectorUsingGravity(NewVelocity);

								Velocity = UnrotateVectorUsingGravity(FVector(UnrotVelocity.X, UnrotVelocity.Y, UnrotNewVelocity.Z));
							}
							else
								Velocity = NewVelocity;
						}

						// bDitch=true means that pawn is straddling two slopes, neither of which he can stand on
						//bool bDitch = ((OldHitImpactNormal.Z > 0.f) && (Hit.ImpactNormal.Z > 0.f) && (FMath::Abs(Delta.Z) <= KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));
						bool bDitch = ((UnrotOldHitImpactNormal.Z > 0.f) && (UnrotateVectorUsingGravity(Hit.ImpactNormal).Z > 0.f) && (FMath::Abs(UnrotateVectorUsingGravity(Delta).Z) <= KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));

						SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
						if (Hit.Time == 0.f)
						{
							// if we are stuck then try to side step
							/*FVector SideDelta = (OldHitNormal + Hit.ImpactNormal).GetSafeNormal2D();
							if (SideDelta.IsNearlyZero())
							{
								SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0).GetSafeNormal();
							}
							SafeMoveUpdatedComponent(SideDelta, PawnRotation, true, Hit);*/


							FVector SideDelta = UnrotateVectorUsingGravity(OldHitNormal + Hit.ImpactNormal).GetSafeNormal2D();
							if (SideDelta.IsNearlyZero())
							{
								SideDelta = FVector(UnrotOldHitNormal.Y, -UnrotOldHitNormal.X, 0).GetSafeNormal();
							}
							SafeMoveUpdatedComponent(RotateVectorUsingGravity(SideDelta), PawnRotation, true, Hit);
						}

						if (bDitch || IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit) || Hit.Time == 0.f)
						{
							remainingTime = 0.f;
							ProcessLanded(Hit, remainingTime, Iterations);
							return;
						}
						else if (GetPerchRadiusThreshold() > 0.f && Hit.Time == 1.f && UnrotOldHitImpactNormal.Z >= GetWalkableFloorZ())
						{
							// We might be in a virtual 'ditch' within our perch radius. This is rare.
							const FVector PawnLocation = UpdatedComponent->GetComponentLocation();

							/*const float ZMovedDist = FMath::Abs(PawnLocation.Z - OldLocation.Z);
							const float MovedDist2DSq = (PawnLocation - OldLocation).SizeSquared2D();*/

							const FVector UnrotPawnLocDiff = UnrotateVectorUsingGravity(PawnLocation - OldLocation);
							const float ZMovedDist = FMath::Abs(UnrotPawnLocDiff.Z);
							const float MovedDist2DSq = UnrotPawnLocDiff.SizeSquared2D();

							if (ZMovedDist <= 0.2f * timeTick && MovedDist2DSq <= 4.f * timeTick)
							{
								/*Velocity.X += 0.25f * GetMaxSpeed() * (RandomStream.FRand() - 0.5f);
								Velocity.Y += 0.25f * GetMaxSpeed() * (RandomStream.FRand() - 0.5f);
								Velocity.Z = FMath::Max<float>(JumpZVelocity * 0.25f, 1.f);*/

								FVector TempUnrotVelocity = UnrotateVectorUsingGravity(Velocity);
								TempUnrotVelocity.X += 0.25f * GetMaxSpeed() * (RandomStream.FRand() - 0.5f);
								TempUnrotVelocity.Y += 0.25f * GetMaxSpeed() * (RandomStream.FRand() - 0.5f);
								TempUnrotVelocity.Z = FMath::Max<float>(JumpZVelocity * 0.25f, 1.f);
								Velocity = RotateVectorUsingGravity(TempUnrotVelocity);

								Delta = Velocity * timeTick;
								SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
							}
						}
					}
				}
			}
		}

		/*if (Velocity.SizeSquared2D() <= KINDA_SMALL_NUMBER * 10.f)
		{
			Velocity.X = 0.f;
			Velocity.Y = 0.f;
		}*/

		FVector TempUnrotVelocity = UnrotateVectorUsingGravity(Velocity);
		if (TempUnrotVelocity.SizeSquared2D() <= KINDA_SMALL_NUMBER * 10.f)
		{
			TempUnrotVelocity.X = 0.f;
			TempUnrotVelocity.Y = 0.f;
			Velocity = RotateVectorUsingGravity(TempUnrotVelocity);
		}
	}
}

void USurfaceNavCharacterMovementComponent::JumpOff(AActor* MovementBaseActor)
{
	if (!bPerformingJumpOff)
	{
		bPerformingJumpOff = true;
		if (CharacterOwner)
		{
			const float MaxSpeed = GetMaxSpeed() * 0.85f;
			Velocity += MaxSpeed * GetBestDirectionOffActor(MovementBaseActor);

			/*if (Velocity.Size2D() > MaxSpeed)
			{
				Velocity = MaxSpeed * Velocity.GetSafeNormal();
			}
			Velocity.Z = JumpOffJumpZFactor * JumpZVelocity;*/

			FVector TempUrotVelocity = UnrotateVectorUsingGravity(Velocity);
			if (TempUrotVelocity.Size2D() > MaxSpeed)
			{
				Velocity = MaxSpeed * Velocity.GetSafeNormal();
				TempUrotVelocity = UnrotateVectorUsingGravity(Velocity);
			}
			TempUrotVelocity.Z = JumpOffJumpZFactor * JumpZVelocity;
			Velocity = RotateVectorUsingGravity(TempUrotVelocity);


			SetMovementMode(MOVE_Falling);
		}
		bPerformingJumpOff = false;
	}
}

void USurfaceNavCharacterMovementComponent::PerformAirControlForPathFollowing(FVector Direction, float ZDiff)
{
	FVector TempUnrotVelocity = UnrotateVectorUsingGravity(Velocity);
	FVector UnrotDirection = UnrotateVectorUsingGravity(Direction);

	// use air control if low grav or above destination and falling towards it
	if (CharacterOwner && TempUnrotVelocity.Z < 0.f && (ZDiff < 0.f || GetGravityZ() > 0.9f * GetWorld()->GetDefaultGravityZ()))
	{
		if (ZDiff < 0.f)
		{
			if ((TempUnrotVelocity.X == 0.f) && (TempUnrotVelocity.Y == 0.f))
			{
				Acceleration = FVector::ZeroVector;
			}
			else
			{
				float Dist2D = UnrotDirection.Size2D();
				//Direction.Z = 0.f;
				Acceleration = Direction.GetSafeNormal() * GetMaxAcceleration();

				if ((Dist2D < 0.5f * FMath::Abs(UnrotDirection.Z)) && ((Velocity | Direction) > 0.5f * FMath::Square(Dist2D)))
				{
					Acceleration *= -1.f;
				}

				if (Dist2D < 1.5f * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius())
				{
					TempUnrotVelocity.X = 0.f;
					TempUnrotVelocity.Y = 0.f;
					Velocity = RotateVectorUsingGravity(TempUnrotVelocity);
					Acceleration = FVector::ZeroVector;
				}
				else if ((Velocity | Direction) < 0.f)
				{
					float M = FMath::Max(0.f, 0.2f - GetWorld()->DeltaTimeSeconds);
					TempUnrotVelocity.X *= M;
					TempUnrotVelocity.Y *= M;
					Velocity = RotateVectorUsingGravity(TempUnrotVelocity);
				}
			}
		}
	}
}

void USurfaceNavCharacterMovementComponent::RequestPathMove(const FVector& MoveInput)
{
	FVector AdjustedMoveInput(MoveInput);

	FVector UnrotMoveInput = UnrotateVectorUsingGravity(MoveInput);

	// preserve magnitude when moving on ground/falling and requested input has Z component
	// see ConstrainInputAcceleration for details
	if (UnrotMoveInput.Z != 0.f && (IsMovingOnGround() || IsFalling()))
	{
		const float Mag = MoveInput.Size();
		AdjustedMoveInput = RotateVectorUsingGravity(UnrotMoveInput.GetSafeNormal2D()) * Mag;
	}

	Super::RequestPathMove(AdjustedMoveInput);
}

void USurfaceNavCharacterMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	if (MoveVelocity.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (ShouldPerformAirControlForPathFollowing())
	{
		const FVector FallVelocity = MoveVelocity.GetClampedToMaxSize(GetMaxSpeed());
		PerformAirControlForPathFollowing(FallVelocity, UnrotateVectorUsingGravity(FallVelocity).Z);
		return;
	}

	RequestedVelocity = MoveVelocity;
	bHasRequestedVelocity = true;
	bRequestedMoveWithMaxSpeed = bForceMaxSpeed;

	if (IsMovingOnGround())
	{
		FVector TempUnrotRequestedVelocity = UnrotateVectorUsingGravity(RequestedVelocity);
		TempUnrotRequestedVelocity.Z = 0.0f;
		RequestedVelocity = RotateVectorUsingGravity(TempUnrotRequestedVelocity);
	}
}

void USurfaceNavCharacterMovementComponent::CapsuleTouched(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bEnablePhysicsInteraction)
	{
		return;
	}

	if (OtherComp != NULL && OtherComp->IsAnySimulatingPhysics())
	{
		const FVector OtherLoc = OtherComp->GetComponentLocation();
		const FVector Loc = UpdatedComponent->GetComponentLocation();

		const FVector UnrotLocDelta = UnrotateVectorUsingGravity(OtherLoc - Loc);

		//FVector ImpulseDir = FVector(OtherLoc.X - Loc.X, OtherLoc.Y - Loc.Y, 0.25f).GetSafeNormal();
		FVector ImpulseDir = RotateVectorUsingGravity(FVector(UnrotLocDelta.X, UnrotLocDelta.Y, 0.25f)).GetSafeNormal();

		const FVector TempUnrotVelocity = UnrotateVectorUsingGravity(Velocity);

		//ImpulseDir = (ImpulseDir + Velocity.GetSafeNormal2D()) * 0.5f;
		ImpulseDir = (ImpulseDir + RotateVectorUsingGravity(TempUnrotVelocity.GetSafeNormal2D())) * 0.5f;

		ImpulseDir.Normalize();

		FName BoneName = NAME_None;
		if (OtherBodyIndex != INDEX_NONE)
		{
			BoneName = ((USkinnedMeshComponent*)OtherComp)->GetBoneName(OtherBodyIndex);
		}

		float TouchForceFactorModified = TouchForceFactor;

		if (bTouchForceScaledToMass)
		{
			FBodyInstance* BI = OtherComp->GetBodyInstance(BoneName);
			TouchForceFactorModified *= BI ? BI->GetBodyMass() : 1.0f;
		}

		/*float ImpulseStrength = FMath::Clamp<FVector::FReal>(Velocity.Size2D() * TouchForceFactorModified,
			MinTouchForce > 0.0f ? MinTouchForce : -FLT_MAX,
			MaxTouchForce > 0.0f ? MaxTouchForce : FLT_MAX);*/
		float ImpulseStrength = FMath::Clamp<FVector::FReal>(TempUnrotVelocity.Size2D() * TouchForceFactorModified,
			MinTouchForce > 0.0f ? MinTouchForce : -FLT_MAX,
			MaxTouchForce > 0.0f ? MaxTouchForce : FLT_MAX);

		FVector Impulse = ImpulseDir * ImpulseStrength;

		OtherComp->AddImpulse(Impulse, BoneName);
	}
}

#pragma endregion

#endif