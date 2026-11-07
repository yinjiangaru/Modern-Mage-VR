// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavMovementComponent.h"

#include "SurfaceNavPawnRotationMode.h"

#include "Runtime/Launch/Resources/Version.h"
#include "AIController.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/WorldSettings.h"
#include "DrawDebugHelpers.h"


USurfaceNavMovementComponent::USurfaceNavMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
	bUseAccelerationForPaths = true;
#else
	NavMovementProperties.bUseAccelerationForPaths = true;
#endif

	// Initialize these so the related processes reset the deltas to zero
	// and do the initial processing...otherwise it will take a few frames
	RotationModeTickDelta = RotationModeTickInterval;
	//CornerBoostTickDelta = CornerBoostTickInterval;

	//RotationMode = USurfaceNavPawnDefaultRotationMode::StaticClass();

//#if WITH_EDITOR
//	if (!IsRunningGame())
//		// Create sphere, wait OnRegister to attach
//		OffsetLocationIndicator = CreateDefaultSubobject<USphereComponent>(TEXT("OffsetLocationIndicator"));
//#endif

}

void USurfaceNavMovementComponent::BeginPlay()
{
	if (UpdatedComponent)
		NextRotator = UpdatedComponent->GetComponentRotation();

	Super::BeginPlay();
}

void USurfaceNavMovementComponent::OnRegister()
{
	Super::OnRegister();

	//#if WITH_EDITOR
	//	if (!IsRunningGame() && OffsetLocationIndicator && GetOwner())
	//	{
	//		OffsetLocationIndicator->SetupAttachment(GetOwner()->GetRootComponent());
	//		OffsetLocationIndicator->SetWorldLocation(GetActorSurfaceLocation());
	//	}
	//#endif
}

void USurfaceNavMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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

	const AAIController* Controller = Cast<AAIController>(PawnOwner->GetController());

	FRotator CurRot = UpdatedComponent->GetComponentRotation();
	const float AngleTolerance = 1e-3f;

	if (!bSkipNextRotationLerp && !bDisableDefaultRotation && Controller && Controller->IsLocalController())
	{
		// ***************************************************
		// Keeping this here as I'm playing with options for allowing controller rotation,
		// but still needs a lot of work.
		// ***************************************************
		/*bool UseFocalPoint = false;
		FRotator NewControlRotation = Controller->GetControlRotation();
		FVector FocalPoint = Controller->GetFocalPoint();
		if (FAISystem::IsValidLocation(FocalPoint))
		{
			FVector UnrotFocalPoint = CurRot.UnrotateVector(FocalPoint);
			UnrotFocalPoint.Z = PawnOwner->GetPawnViewLocation().Z;
			FocalPoint = CurRot.RotateVector(UnrotFocalPoint);

			FVector MyUpVector = FRotationMatrix(NextRotator).GetScaledAxis(EAxis::Z);
			NextRotator = FRotationMatrix::MakeFromZX(MyUpVector, FocalPoint - PawnOwner->GetPawnViewLocation()).Rotator();
			NewControlRotation = (FocalPoint - PawnOwner->GetPawnViewLocation()).Rotation();
			UseFocalPoint = true;
		}
		else if (Controller->bSetControlRotationFromPawnOrientation)
		{
			NewControlRotation = PawnOwner->GetActorRotation();
		}*/
		// ***************************************************



		if (!NextRotator.Equals(CurRot, AngleTolerance))
		{
			MoveUpdatedComponent(FVector::ZeroVector, UKismetMathLibrary::RLerp(CurRot, NextRotator, FMath::Min(1.f, DeltaTime * RotationSpeed), true),
				/*bSweep*/ false);
		}
	}

	bSkipNextRotationLerp = false;

}

//void USurfaceNavMovementComponent::OnDetermineRotation_Implementation()
//{
//
//}

void USurfaceNavMovementComponent::HandleRotation(FVector TargetLocation, FVector TargetUpVector, FVector TargetForwardVector) //, bool ForceDefault)
{
	bool ShouldSkipNextRotation = false;

	if (TargetUpVector == FVector::ZeroVector)
		ShouldSkipNextRotation = true;
	else
	{
		if (RotationMode)
			SetNextRotator(RotationMode.GetDefaultObject()->DeterminePawnRotation(this, TargetLocation, TargetUpVector, TargetForwardVector));
		else
			SetNextRotator(FRotationMatrix::MakeFromZX(TargetUpVector, TargetForwardVector).Rotator());
	}

	if (OnNeedsPathMoveRotation.IsBound())
		OnNeedsPathMoveRotation.Broadcast(TargetLocation, TargetUpVector, TargetForwardVector);
	else if (ShouldSkipNextRotation)
		bSkipNextRotationLerp = true;
}

void USurfaceNavMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	if (bSkipNextMovementInput)
	{
		bSkipNextMovementInput = false;
		ConsumeInputVector();
		return;
	}

	Super::RequestDirectMove(MoveVelocity, bForceMaxSpeed);
}

void USurfaceNavMovementComponent::RequestPathMove(const FVector& MoveInput)
{
	if (bSkipNextMovementInput)
	{
		bSkipNextMovementInput = false;
		ConsumeInputVector();
		return;
	}

	/*if (OverridePathMovesWithForwardVector)
		Super::RequestPathMove(PawnOwner->GetActorForwardVector());
	else*/

	Super::RequestPathMove(MoveInput);
}

FVector USurfaceNavMovementComponent::GetActorSurfaceLocation() const
{
	FVector BaseLocation;
	FBox CompBounds;
	FVector CompScale;

	if (UpdatedComponent)
	{
		BaseLocation = UpdatedComponent->GetComponentLocation();
		CompBounds = UpdatedComponent->CalcLocalBounds().GetBox();
		CompScale = UpdatedComponent->GetComponentScale();
	}
	else if (GetOwner() && GetOwner()->GetRootComponent())
	{
		USceneComponent* RootComp = GetOwner()->GetRootComponent();
		BaseLocation = RootComp->GetComponentLocation();
		CompBounds = RootComp->CalcLocalBounds().GetBox();
		CompScale = RootComp->GetComponentScale();
	}

	FVector BoxMin = CompBounds.Min;
	FVector BoxMax = CompBounds.Max;

	float OffsetX = 0.f;
	float OffsetY = 0.f;
	float OffsetZ = 0.f;

	if (MovementOffsetModeForX == ESurfaceNavMovementAxisOffsetMode::OFFSET_FROM_BOUNDS)
	{
		if (PawnMovementOffset.X < 0.f)
			OffsetX = BoxMin.X;
		else if (PawnMovementOffset.X > 0.f)
			OffsetX = BoxMax.X;
	}
	OffsetX += PawnMovementOffset.X;

	if (MovementOffsetModeForY == ESurfaceNavMovementAxisOffsetMode::OFFSET_FROM_BOUNDS)
	{
		if (PawnMovementOffset.Y < 0.f)
			OffsetY = BoxMin.Y;
		else if (PawnMovementOffset.Y > 0.f)
			OffsetY = BoxMax.Y;
	}
	OffsetY += PawnMovementOffset.Y;

	if (MovementOffsetModeForZ == ESurfaceNavMovementAxisOffsetMode::OFFSET_FROM_BOUNDS)
	{
		if (PawnMovementOffset.Z < 0.f)
			OffsetZ = BoxMin.Z;
		else if (PawnMovementOffset.Z > 0.f)
			OffsetZ = BoxMax.Z;
	}
	OffsetZ += PawnMovementOffset.Z;

	FVector LocOffset(OffsetX, OffsetY, OffsetZ);
	LocOffset = GetOwner()->GetActorRotation().RotateVector(LocOffset * CompScale);
	return BaseLocation + LocOffset;
}

float USurfaceNavMovementComponent::GetPathFollowingBrakingDistance(float CurrentMaxSpeed) const
{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
	if (bUseFixedBrakingDistanceForPaths)
	{
		return FixedPathBrakingDistance;
	}
#else
	if (NavMovementProperties.bUseFixedBrakingDistanceForPaths)
	{
		return NavMovementProperties.FixedPathBrakingDistance;
	}
#endif
	

	const float BrakingDeceleration = FMath::Abs(CurrentMaxSpeed);

	// character won't be able to stop with negative or nearly zero deceleration, use MaxSpeed for path length calculations
	const float BrakingDistance = (BrakingDeceleration < KINDA_SMALL_NUMBER) ? CurrentMaxSpeed : BrakingDeceleration * 0.3f; // (FMath::Square(CurrentMaxSpeed) / (2.f * BrakingDeceleration));
	return BrakingDistance;
}

#if WITH_EDITOR

void USurfaceNavMovementComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != NULL)
	{
		const FName PropName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropName == GET_MEMBER_NAME_CHECKED(USurfaceNavMovementComponent, RotationMode))
		{
			if (!RotationMode)
				RotationModeValue = 0;
			else if (RotationMode->GetDefaultObject()->IsA(USurfaceNav2x1TraceRotationMode::StaticClass())) //->GetDefaultObject()->StaticClass() == USurfaceNav2x1TraceRotationMode::StaticClass()
				RotationModeValue = 1;
			else if (RotationMode->GetDefaultObject()->IsA(USurfaceNav2x2TraceRotationMode::StaticClass())) //->GetDefaultObject()->StaticClass() == USurfaceNav2x2TraceRotationMode::StaticClass())
				RotationModeValue = 2;
			else
				RotationModeValue = 0;
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(USurfaceNavMovementComponent, MinAngleFromGround))
		{
			if (MinAngleFromGround > MaxAngleFromGround)
				MinAngleFromGround = MaxAngleFromGround;
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(USurfaceNavMovementComponent, MaxAngleFromGround))
		{
			if (MaxAngleFromGround < MinAngleFromGround)
				MaxAngleFromGround = MinAngleFromGround;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

//FRotator USurfaceNavMovementComponent::GetDeltaRotation(float DeltaTime) const
//{
//	return FRotator(GetAxisDeltaRotation(RotationRate.Pitch, DeltaTime), GetAxisDeltaRotation(RotationRate.Yaw, DeltaTime), GetAxisDeltaRotation(RotationRate.Roll, DeltaTime));
//}
//
//float USurfaceNavMovementComponent::GetAxisDeltaRotation(float InAxisRotationRate, float DeltaTime) const
//{
//	// Values over 360 don't do anything, see FMath::FixedTurn. However we are trying to avoid giant floats from overflowing other calculations.
//	return (InAxisRotationRate >= 0.f) ? FMath::Min(InAxisRotationRate * DeltaTime, 360.f) : 360.f;
//}

//FRotator UCharacterMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const
//{
//	if (Acceleration.SizeSquared() < KINDA_SMALL_NUMBER)
//	{
//		// AI path following request can orient us in that direction (it's effectively an acceleration)
//		if (bHasRequestedVelocity && RequestedVelocity.SizeSquared() > KINDA_SMALL_NUMBER)
//		{
//			return NextRotator; // RequestedVelocity.GetSafeNormal().Rotation();
//		}
//
//		// Don't change rotation if there is no acceleration.
//		return CurrentRotation;
//	}
//
//	// Rotate toward direction of acceleration.
//	return Acceleration.GetSafeNormal().Rotation();
//}

//void USurfaceNavMovementComponent::StopActiveMovement()
//{
//	if (!bStopMovementAbortPaths)
//	{
//		return;
//	}
//
//	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
//	if (PFAgent)
//	{
//		PFAgent->OnUnableToMove(*this);
//	}
//}
//
//void USurfaceNavMovementComponent::StopMovementImmediately()
//{
//
//}
