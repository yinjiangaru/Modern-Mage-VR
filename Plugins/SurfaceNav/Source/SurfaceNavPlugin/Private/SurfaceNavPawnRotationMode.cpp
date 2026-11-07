// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavPawnRotationMode.h"
#include "SurfaceNavMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"

FRotator USurfaceNavDefaultRotationMode::DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation,
	FVector& TargetUpVector, FVector& TargetForwardVector)
{
	return FRotationMatrix::MakeFromZX(TargetUpVector, TargetForwardVector).Rotator();
}

//FRotator USurfaceNav1x1TraceRotationMode::DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation,
//	FVector& TargetUpVector, FVector& TargetForwardVector)
//{
//	if (!SurfaceMovementComp)
//		return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
//
//	if (SurfaceMovementComp->HasCachedRotation() && SurfaceMovementComp->IncrementRotationModeTickDelta() > SMALL_NUMBER)
//	{
//		TargetUpVector = SurfaceMovementComp->GetCachedRotationUpVector();
//		return SurfaceMovementComp->GetCachedRotation();
//	}
//
//	SurfaceMovementComp->ResetRotationCache();
//
//	const APawn* PawnOwner = SurfaceMovementComp->GetPawnOwner();
//	if (!PawnOwner)
//		return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
//
//	FVector TraceStart = SurfaceMovementComp->GetActorLocation(); // ActorOwner->GetActorLocation();
//
//	//const FVector& PawnRightVector = PawnOwner->GetActorRightVector();
//	//FVector PawnForwardVector = PawnOwner->GetActorForwardVector();
//
//	FVector TraceDist(SurfaceMovementComp->GetRotationMode1stTraceDistance());
//	if (SurfaceMovementComp->RotationModeTraceShouldScale())
//		TraceDist *= PawnOwner->GetActorScale();
//
//	FVector TraceDir = PawnOwner->GetActorForwardVector().RotateAngleAxis(SurfaceMovementComp->GetRotationMode1stTraceAngle(), PawnOwner->GetActorRightVector()).GetSafeNormal();
//	FVector TraceEnd = TraceStart + (TraceDir * TraceDist);
//
//	if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
//	{
//		DrawDebugLine(
//			PawnOwner->GetWorld(),
//			TraceStart,
//			TraceEnd,
//			//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
//			FColor::Green,
//			false,
//			2.f,
//			0,
//			1.f
//		);
//	}
//
//	FCollisionQueryParams GridPointTraceQueryParams = FCollisionQueryParams();
//	GridPointTraceQueryParams.bTraceComplex = false;
//	GridPointTraceQueryParams.bReturnPhysicalMaterial = false;
//	FHitResult Hit;
//	if (PawnOwner->GetWorld()->LineTraceSingleByProfile
//	(
//		Hit,
//		TraceStart,    //start
//		TraceEnd,
//		FName("IgnoreOnlyPawn"),
//		//FName("BlockAllDynamic"),
//		GridPointTraceQueryParams
//	))
//	{
//		// If we got a hit, we may be trying to go around a corner so let's adjust our forward a bit
//		auto HitActor = Hit.GetActor();
//		auto HitComponent = Hit.GetComponent();
//
//		// Don't want to continue if the hit actor/comp is empty or the actor is a pawn.
//		if (HitActor == nullptr || HitComponent == nullptr || HitActor->IsA<APawn>()) // || !HitComponent->IsNavigationRelevant())
//			return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
//
//		TargetUpVector = Hit.ImpactNormal;
//
//		FRotator NewRotation = Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
//		SurfaceMovementComp->SetLastRotationFound(NewRotation, TargetUpVector);
//		return NewRotation;
//
//		// Get a cross product vector to use as a rotation axis.
//		//FVector RotationAxis = FVector::CrossProduct(Hit.ImpactNormal, TargetForwardVector);
//
//		// Now, rotate the impact point normal by 90 to get our new foward direction
//		//TargetForwardVector = (TargetForwardVector + (Hit.ImpactNormal.RotateAngleAxis(90.f, RotationAxis) * SurfaceMovementComp->GetCornerTraversalBoost())).GetSafeNormal();
//	}
//
//	return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
//};


FRotator USurfaceNav2x1TraceRotationMode::DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation,
	FVector& TargetUpVector, FVector& TargetForwardVector)
{
	if (!SurfaceMovementComp)
		return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);

	if (SurfaceMovementComp->HasCachedRotation() && SurfaceMovementComp->IncrementRotationModeTickDelta() > SMALL_NUMBER)
	{
		TargetUpVector = SurfaceMovementComp->GetCachedRotationUpVector();
		return SurfaceMovementComp->GetCachedRotation();
	}

	SurfaceMovementComp->ResetRotationCache();

	const APawn* PawnOwner = SurfaceMovementComp->GetPawnOwner();
	if (!PawnOwner)
		return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);

	FVector TraceStart = SurfaceMovementComp->GetActorLocation(); // ActorOwner->GetActorLocation();

	//const FVector& PawnRightVector = PawnOwner->GetActorRightVector();
	//FVector PawnForwardVector = PawnOwner->GetActorForwardVector();

	bool ShouldScale = SurfaceMovementComp->RotationModeTraceShouldScale();
	FVector PawnScale = PawnOwner->GetActorScale();

	FVector TraceDist(SurfaceMovementComp->GetRotationMode1stTraceDistance());
	if (ShouldScale)
		TraceDist *= PawnScale;

	FVector PawnForward = PawnOwner->GetActorForwardVector();
	FVector PawnRight = PawnOwner->GetActorRightVector();

	FVector TraceDir = PawnForward.RotateAngleAxis(SurfaceMovementComp->GetRotationMode1stTraceAngle(), PawnRight).GetSafeNormal();
	FVector TraceEnd = TraceStart + (TraceDir * TraceDist);

	if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
	{
		DrawDebugLine(
			PawnOwner->GetWorld(),
			TraceStart,
			TraceEnd,
			//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
			FColor::Green,
			false,
			2.f,
			0,
			1.f
		);
	}

	FCollisionQueryParams GridPointTraceQueryParams = FCollisionQueryParams();
	GridPointTraceQueryParams.bTraceComplex = false;
	GridPointTraceQueryParams.bReturnPhysicalMaterial = false;
	FHitResult Hit;
	if (PawnOwner->GetWorld()->LineTraceSingleByProfile
	(
		Hit,
		TraceStart,    //start
		TraceEnd,
		FName("IgnoreOnlyPawn"),
		//FName("BlockAllDynamic"),
		GridPointTraceQueryParams
	))
	{
		// If we got a hit, we may be trying to go around a corner so let's adjust our forward a bit
		auto HitActor = Hit.GetActor();
		auto HitComponent = Hit.GetComponent();

		// Don't want to continue if the hit actor/comp is empty or the actor is a pawn.
		if (HitActor == nullptr || HitComponent == nullptr || HitActor->IsA<APawn>()) // || !HitComponent->IsNavigationRelevant())
			return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);

		TargetUpVector = Hit.ImpactNormal;

		FRotator NewRotation = Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
		SurfaceMovementComp->SetLastRotationFound(NewRotation, TargetUpVector);
		return NewRotation;

		// Get a cross product vector to use as a rotation axis.
		//FVector RotationAxis = FVector::CrossProduct(Hit.ImpactNormal, TargetForwardVector);

		// Now, rotate the impact point normal by 90 to get our new foward direction
		//TargetForwardVector = (TargetForwardVector + (Hit.ImpactNormal.RotateAngleAxis(90.f, RotationAxis) * SurfaceMovementComp->GetCornerTraversalBoost())).GetSafeNormal();
	}
	else
	{
		FVector StartOffset(SurfaceMovementComp->RotationMode2ndTraceStartOffset);
		if (ShouldScale)
			StartOffset *= PawnScale;

		TraceStart += TraceDir * SurfaceMovementComp->RotationMode2ndTraceStartOffset;

		FVector Trace2Dir = PawnForward.RotateAngleAxis(SurfaceMovementComp->RotationMode2ndTraceAngle, PawnRight).GetSafeNormal();

		TraceDist = FVector(SurfaceMovementComp->RotationMode2ndTraceDistance);
		if (ShouldScale)
			TraceDist *= PawnScale;

		TraceEnd = TraceStart + (Trace2Dir * TraceDist);

		if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
		{
			DrawDebugLine(
				PawnOwner->GetWorld(),
				TraceStart,
				TraceEnd,
				//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
				FColor::Blue,
				false,
				2.f,
				0,
				1.f
			);
		}

		if (PawnOwner->GetWorld()->LineTraceSingleByProfile
		(
			Hit,
			TraceStart,    //start
			TraceEnd,
			FName("IgnoreOnlyPawn"),
			//FName("BlockAllDynamic"),
			GridPointTraceQueryParams
		))
		{
			// If we got a hit, we may be trying to go around a corner so let's adjust our forward a bit
			auto HitActor = Hit.GetActor();
			auto HitComponent = Hit.GetComponent();

			// Don't want to continue if the hit actor/comp is empty or the actor is a pawn.
			if (HitActor == nullptr || HitComponent == nullptr || HitActor->IsA<APawn>()) // || !HitComponent->IsNavigationRelevant())
				return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);

			TargetUpVector = Hit.ImpactNormal;

			FRotator NewRotation = Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
			SurfaceMovementComp->SetLastRotationFound(NewRotation, TargetUpVector);
			return NewRotation;
		}
	}

	return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
}

//FRotator USurfaceNav1x2TraceRotationMode::DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation,
//	FVector& TargetUpVector, FVector& TargetForwardVector)
//{
//	if (!SurfaceMovementComp)
//		return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
//
//	if (SurfaceMovementComp->HasCachedRotation() && SurfaceMovementComp->IncrementRotationModeTickDelta() > SMALL_NUMBER)
//	{
//		TargetUpVector = SurfaceMovementComp->GetCachedRotationUpVector();
//		return SurfaceMovementComp->GetCachedRotation();
//	}
//
//	SurfaceMovementComp->ResetRotationCache();
//
//	const APawn* PawnOwner = SurfaceMovementComp->GetPawnOwner();
//	if (!PawnOwner)
//		return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
//
//
//	//const FVector& PawnRightVector = PawnOwner->GetActorRightVector();
//	//FVector PawnForwardVector = PawnOwner->GetActorForwardVector();
//
//	// ***********************************
//	// Get some values that will be helpful with the traces
//	// ***********************************
//
//	bool ShouldScale = SurfaceMovementComp->RotationModeTraceShouldScale();
//	FVector PawnScale = PawnOwner->GetActorScale();
//
//	FVector TraceDist(SurfaceMovementComp->GetRotationMode1stTraceDistance());
//	if (ShouldScale)
//		TraceDist *= PawnScale;
//
//	FVector PawnForward = PawnOwner->GetActorForwardVector();
//	FVector PawnRight = PawnOwner->GetActorRightVector();
//	FVector PawnUp = PawnOwner->GetActorUpVector();
//
//	FVector TraceDir1 = PawnForward.RotateAngleAxis(SurfaceMovementComp->GetRotationMode1stTraceAngle(), PawnRight);
//
//	// ***********************************
//	// Handle the left side hit
//	// ***********************************
//
//	FVector TraceStart = SurfaceMovementComp->GetActorLocation(); // ActorOwner->GetActorLocation();
//
//	FVector LTraceDir = TraceDir1.RotateAngleAxis(-SurfaceMovementComp->RotationModeLeftRightAngle, PawnUp).GetSafeNormal();
//	FVector TraceEnd = TraceStart + (LTraceDir * TraceDist);
//
//	if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
//	{
//		DrawDebugLine(
//			PawnOwner->GetWorld(),
//			TraceStart,
//			TraceEnd,
//			//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
//			FColor::Green,
//			false,
//			2.f,
//			0,
//			1.f
//		);
//	}
//
//	FCollisionQueryParams GridPointTraceQueryParams = FCollisionQueryParams();
//	GridPointTraceQueryParams.bTraceComplex = false;
//	GridPointTraceQueryParams.bReturnPhysicalMaterial = false;
//	FHitResult LHit;
//	bool ValidLHit = false;
//	if (PawnOwner->GetWorld()->LineTraceSingleByProfile
//	(
//		LHit,
//		TraceStart,    //start
//		TraceEnd,
//		FName("IgnoreOnlyPawn"),
//		//FName("BlockAllDynamic"),
//		GridPointTraceQueryParams
//	))
//	{
//		AActor* HitActor = LHit.GetActor();
//		UPrimitiveComponent* HitComponent = LHit.GetComponent();
//
//		// Don't want to continue if the hit actor/comp is empty or the actor is a pawn.
//		if (HitActor && HitComponent && !HitActor->IsA<APawn>()) // || !HitComponent->IsNavigationRelevant())
//			ValidLHit = true;
//	}
//
//	// ***********************************
//	// Now let's handle the right side hit
//	// ***********************************
//
//	TraceStart = SurfaceMovementComp->GetActorLocation();
//
//	FVector RTraceDir = TraceDir1.RotateAngleAxis(SurfaceMovementComp->RotationModeLeftRightAngle, PawnUp).GetSafeNormal();
//	TraceEnd = TraceStart + (RTraceDir * TraceDist);
//
//	if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
//	{
//		DrawDebugLine(
//			PawnOwner->GetWorld(),
//			TraceStart,
//			TraceEnd,
//			//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
//			FColor::Red,
//			false,
//			2.f,
//			0,
//			1.f
//		);
//	}
//
//	FHitResult RHit;
//	bool ValidRHit = false;
//	if (PawnOwner->GetWorld()->LineTraceSingleByProfile
//	(
//		RHit,
//		TraceStart,    //start
//		TraceEnd,
//		FName("IgnoreOnlyPawn"),
//		//FName("BlockAllDynamic"),
//		GridPointTraceQueryParams
//	))
//	{
//		AActor* HitActor = RHit.GetActor();
//		UPrimitiveComponent* HitComponent = RHit.GetComponent();
//
//		// Don't want to continue if the hit actor/comp is empty or the actor is a pawn.
//		if (HitActor && HitComponent && !HitActor->IsA<APawn>()) // || !HitComponent->IsNavigationRelevant())
//			ValidRHit = true;
//	}
//
//	if (ValidLHit || ValidRHit)
//	{
//		FVector LImpactPoint(ValidLHit ? LHit.ImpactPoint : FVector::ZeroVector);
//		FVector RImpactPoint(ValidRHit ? RHit.ImpactPoint : FVector::ZeroVector);
//
//		if (ValidLHit && !ValidRHit)
//		{
//			// Let's calculate a best guess for right impact point
//			RImpactPoint = LImpactPoint + (PawnRight * 100.f).RotateAngleAxis(-45.f, PawnForward);
//		}
//		else if (ValidRHit && !ValidLHit)
//		{
//			// Let's calculate a best guess for left impact point
//			LImpactPoint = RImpactPoint + (PawnRight * -100.f).RotateAngleAxis(45.f, PawnForward);
//		}
//
//		if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
//		{
//			DrawDebugLine(
//				PawnOwner->GetWorld(),
//				LImpactPoint,
//				RImpactPoint,
//				//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
//				FColor::Turquoise,
//				false,
//				2.f,
//				0,
//				1.f
//			);
//		}
//
//		FVector SimplifiedRightVector = PawnOwner->GetActorRotation().UnrotateVector((RImpactPoint - LImpactPoint).GetSafeNormal());
//		SimplifiedRightVector = PawnOwner->GetActorRotation().RotateVector(FVector(0.f, 1.f, SimplifiedRightVector.Z).GetSafeNormal());
//
//
//		float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(TargetForwardVector.Dot(PawnForward)));
//
//		// Check if target forward is to the left of our pawn, if so then
//		// negate AngleToTarget
//		if (TargetForwardVector.Dot(PawnRight) < 0)
//			AngleToTarget *= -1.f;
//
//		SimplifiedRightVector = SimplifiedRightVector.RotateAngleAxis(AngleToTarget, PawnUp);
//
//		//return FRotationMatrix::MakeFromYX(SimplifiedRightVector, TargetForwardVector).Rotator();
//
//		FRotator NewRotation = FRotationMatrix::MakeFromYX(SimplifiedRightVector, TargetForwardVector).Rotator();
//		SurfaceMovementComp->SetLastRotationFound(NewRotation, NewRotation.Vector().RotateAngleAxis(90, SimplifiedRightVector));
//		return NewRotation;
//	}
//
//	return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
//}

FRotator USurfaceNav2x2TraceRotationMode::DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation,
	FVector& TargetUpVector, FVector& TargetForwardVector)
{
	if (!SurfaceMovementComp)
		return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);

	if (SurfaceMovementComp->HasCachedRotation() && SurfaceMovementComp->IncrementRotationModeTickDelta() > SMALL_NUMBER)
	{
		TargetUpVector = SurfaceMovementComp->GetCachedRotationUpVector();
		return SurfaceMovementComp->GetCachedRotation();
	}

	SurfaceMovementComp->ResetRotationCache();

	const APawn* PawnOwner = SurfaceMovementComp->GetPawnOwner();
	if (!PawnOwner)
		return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);


	//const FVector& PawnRightVector = PawnOwner->GetActorRightVector();
	//FVector PawnForwardVector = PawnOwner->GetActorForwardVector();

	// ***********************************
	// Get some values that will be helpful with the traces
	// ***********************************

	bool ShouldScale = SurfaceMovementComp->RotationModeTraceShouldScale();
	FVector PawnScale = PawnOwner->GetActorScale();

	FVector TraceDist(SurfaceMovementComp->GetRotationMode1stTraceDistance());
	if (ShouldScale)
		TraceDist *= PawnScale;

	FVector PawnForward = PawnOwner->GetActorForwardVector();
	FVector PawnRight = PawnOwner->GetActorRightVector();
	FVector PawnUp = PawnOwner->GetActorUpVector();

	FVector TraceDir1 = PawnForward.RotateAngleAxis(SurfaceMovementComp->GetRotationMode1stTraceAngle(), PawnRight);
	FVector TraceDir2 = PawnForward.RotateAngleAxis(SurfaceMovementComp->RotationMode2ndTraceAngle, PawnRight);

	// ***********************************
	// Handle the left side hit
	// ***********************************
	
	FVector TraceStart = SurfaceMovementComp->GetActorLocation(); // ActorOwner->GetActorLocation();

	FVector LTraceDir = TraceDir1.RotateAngleAxis(-SurfaceMovementComp->RotationModeLeftRightAngle, PawnUp).GetSafeNormal();
	FVector TraceEnd = TraceStart + (LTraceDir * TraceDist);

	if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
	{
		DrawDebugLine(
			PawnOwner->GetWorld(),
			TraceStart,
			TraceEnd,
			//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
			FColor::Green,
			false,
			2.f,
			0,
			1.f
		);
	}

	FCollisionQueryParams GridPointTraceQueryParams = FCollisionQueryParams();
	GridPointTraceQueryParams.bTraceComplex = false;
	GridPointTraceQueryParams.bReturnPhysicalMaterial = false;
	FHitResult LHit;
	bool ValidLHit = false;
	if (PawnOwner->GetWorld()->LineTraceSingleByProfile
	(
		LHit,
		TraceStart,    //start
		TraceEnd,
		FName("IgnoreOnlyPawn"),
		//FName("BlockAllDynamic"),
		GridPointTraceQueryParams
	))
	{
		AActor* HitActor = LHit.GetActor();
		UPrimitiveComponent* HitComponent = LHit.GetComponent();

		// Don't want to continue if the hit actor/comp is empty or the actor is a pawn.
		if (HitActor && HitComponent && !HitActor->IsA<APawn>()) // || !HitComponent->IsNavigationRelevant())
			ValidLHit = true;
	}

	if (!ValidLHit)
	{
		FVector StartOffset(SurfaceMovementComp->RotationMode2ndTraceStartOffset);
		if (ShouldScale)
			StartOffset *= PawnScale;

		TraceStart += LTraceDir * StartOffset;

		LTraceDir = TraceDir2.RotateAngleAxis(-SurfaceMovementComp->RotationModeLeftRightAngle, PawnUp).GetSafeNormal();

		TraceDist = FVector(SurfaceMovementComp->RotationMode2ndTraceDistance);
		if (ShouldScale)
			TraceDist *= PawnScale;

		TraceEnd = TraceStart + (LTraceDir * TraceDist);

		if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
		{
			DrawDebugLine(
				PawnOwner->GetWorld(),
				TraceStart,
				TraceEnd,
				//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
				FColor::Blue,
				false,
				2.f,
				0,
				1.f
			);
		}

		// TODO: adjust start and end
		if (PawnOwner->GetWorld()->LineTraceSingleByProfile
			(
				LHit,
				TraceStart,    //start
				TraceEnd,
				FName("IgnoreOnlyPawn"),
				//FName("BlockAllDynamic"),
				GridPointTraceQueryParams
			))
		{
			AActor* HitActor = LHit.GetActor();
			UPrimitiveComponent* HitComponent = LHit.GetComponent();

			// Don't want to continue if the hit actor/comp is empty or the actor is a pawn.
			if (HitActor && HitComponent && !HitActor->IsA<APawn>()) // || !HitComponent->IsNavigationRelevant())
				ValidLHit = true;
		}
	}

	// ***********************************
	// Now let's handle the right side hit
	// ***********************************

	TraceStart = SurfaceMovementComp->GetActorLocation();

	FVector RTraceDir = TraceDir1.RotateAngleAxis(SurfaceMovementComp->RotationModeLeftRightAngle, PawnUp).GetSafeNormal();
	TraceEnd = TraceStart + (RTraceDir * TraceDist);

	if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
	{
		DrawDebugLine(
			PawnOwner->GetWorld(),
			TraceStart,
			TraceEnd,
			//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
			FColor::Red,
			false,
			2.f,
			0,
			1.f
		);
	}

	FHitResult RHit;
	bool ValidRHit = false;
	if (PawnOwner->GetWorld()->LineTraceSingleByProfile
	(
		RHit,
		TraceStart,    //start
		TraceEnd,
		FName("IgnoreOnlyPawn"),
		//FName("BlockAllDynamic"),
		GridPointTraceQueryParams
	))
	{
		AActor* HitActor = RHit.GetActor();
		UPrimitiveComponent* HitComponent = RHit.GetComponent();

		// Don't want to continue if the hit actor/comp is empty or the actor is a pawn.
		if (HitActor && HitComponent && !HitActor->IsA<APawn>()) // || !HitComponent->IsNavigationRelevant())
			ValidRHit = true;
	}

	if (!ValidRHit)
	{
		FVector StartOffset(SurfaceMovementComp->RotationMode2ndTraceStartOffset);
		if (ShouldScale)
			StartOffset *= PawnScale;

		TraceStart += RTraceDir * StartOffset;

		RTraceDir = TraceDir2.RotateAngleAxis(SurfaceMovementComp->RotationModeLeftRightAngle, PawnUp).GetSafeNormal();

		TraceDist = FVector(SurfaceMovementComp->RotationMode2ndTraceDistance);
		if (ShouldScale)
			TraceDist *= PawnScale;

		TraceEnd = TraceStart + (RTraceDir * TraceDist);

		if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
		{
			DrawDebugLine(
				PawnOwner->GetWorld(),
				TraceStart,
				TraceEnd,
				//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
				FColor::Yellow,
				false,
				2.f,
				0,
				1.f
			);
		}

		// TODO: adjust start and end
		if (PawnOwner->GetWorld()->LineTraceSingleByProfile
		(
			RHit,
			TraceStart,    //start
			TraceEnd,
			FName("IgnoreOnlyPawn"),
			//FName("BlockAllDynamic"),
			GridPointTraceQueryParams
		))
		{
			AActor* HitActor = RHit.GetActor();
			UPrimitiveComponent* HitComponent = RHit.GetComponent();

			// Don't want to continue if the hit actor/comp is empty or the actor is a pawn.
			if (HitActor && HitComponent && !HitActor->IsA<APawn>()) // || !HitComponent->IsNavigationRelevant())
				ValidRHit = true;
		}
	}

	if (ValidLHit || ValidRHit)
	{
		FVector LImpactPoint(ValidLHit ? LHit.ImpactPoint : FVector::ZeroVector);
		FVector RImpactPoint(ValidRHit ? RHit.ImpactPoint : FVector::ZeroVector);

		if (ValidLHit && !ValidRHit)
		{
			// Let's calculate a best guess for right impact point
			RImpactPoint = LImpactPoint + (PawnRight * 100.f).RotateAngleAxis(-45.f, PawnForward);
		}
		else if (ValidRHit && !ValidLHit)
		{
			// Let's calculate a best guess for left impact point
			LImpactPoint = RImpactPoint + (PawnRight * -100.f).RotateAngleAxis(45.f, PawnForward);
		}

		if (SurfaceMovementComp->bRotationModeShouldDrawTraceLines)
		{
			DrawDebugLine(
				PawnOwner->GetWorld(),
				LImpactPoint,
				RImpactPoint,
				//StartLocation + (StartLocation - EndLocation).GetSafeNormal() * 10.f,
				FColor::Turquoise,
				false,
				2.f,
				0,
				1.f
			);
		}

		FVector SimplifiedRightVector = PawnOwner->GetActorRotation().UnrotateVector((RImpactPoint - LImpactPoint).GetSafeNormal());
		SimplifiedRightVector = PawnOwner->GetActorRotation().RotateVector(FVector(0.f, 1.f, SimplifiedRightVector.Z).GetSafeNormal());

		
		float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(TargetForwardVector, PawnForward)));

		// Check if target forward is to the left of our pawn, if so then
		// negate AngleToTarget
		if (FVector::DotProduct(TargetForwardVector, PawnRight) < 0.f)
			AngleToTarget *= -1.f;

		SimplifiedRightVector = SimplifiedRightVector.RotateAngleAxis(AngleToTarget, PawnUp);

		//return FRotationMatrix::MakeFromYX(SimplifiedRightVector, TargetForwardVector).Rotator();

		FRotator NewRotation = FRotationMatrix::MakeFromYX(SimplifiedRightVector, TargetForwardVector).Rotator();
		SurfaceMovementComp->SetLastRotationFound(NewRotation, NewRotation.Vector().RotateAngleAxis(90, SimplifiedRightVector));
		return NewRotation;
	}

	return Super::DeterminePawnRotation(SurfaceMovementComp, TargetLocation, TargetUpVector, TargetForwardVector);
}