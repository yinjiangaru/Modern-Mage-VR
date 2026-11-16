// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "MassGroundProjectionProcessor.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "NavigationSystem.h"
#include "MassMovementFragments.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

//ProcessingPhase����׶Σ�
//ProcessorGroup���߼��飩
//Processor�����崦������

//UMassActorToMassSyncProcessor
//UMassTransformToMassProcessor
//
// UMassStateTreeProcessor
// UMassPerceptionProcessor
// UMassTargetFinderProcessor
// 
// UMassMoveToTaskProcessor
// UMassLookAtTaskProcessor
// 
// UMassNavigationObstacleProcessor
//UMassCrowdAvoidanceProcessor
// 
// FinalVelocity =
//NavVelocity
//+ AvoidanceVelocity
//+ ExternalForce
// 
// UMassVelocityIntegratorProcessor
//UMassMovementProcessor
// 
// UMassTransformToActorProcessor
//UMassRepresentationProcessor
//PrePhysics
//	SyncWorldToMass
//	Behavior
//	Tasks
//	Avoidance
//	ApplyForces
//	Movement
//PostPhysics
//	UpdateWorldFromMass

UMassGroundProjectionProcessor::UMassGroundProjectionProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;

	ExecutionFlags = (int32)(EProcessorExecutionFlags::All);

	ProcessingPhase = EMassProcessingPhase::PostPhysics;

	bRequiresGameThreadExecution = true;

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
}

void UMassGroundProjectionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UMassGroundProjectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(EntityManager.GetWorld());
	if (!NavSys)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, NavSys](FMassExecutionContext& Context)
	{
		const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
		const int32 NumEntities = Context.GetNumEntities();

		if (FMassActorFragment* ActorFragment = Context.GetMutableFragmentView<FMassActorFragment>().GetData())
		{
			for (int32 i = 0; i < NumEntities; ++i)
			{
				FTransform& Transform = TransformList[i].GetMutableTransform();
				FVector Location = Transform.GetLocation();

				FNavLocation ProjectedLoc;
				const FVector QueryExtent(50.f, 50.f, 1000.f);

				if (NavSys->ProjectPointToNavigation(Location, ProjectedLoc, QueryExtent))
				{
					if (ACharacter* InTargetActor = Cast<ACharacter>(ActorFragment[i].GetMutable()))
					{
						if (UCapsuleComponent* InComponent = InTargetActor->GetCapsuleComponent())
						{
							float CapsuleHalfHeight = InComponent->GetScaledCapsuleHalfHeight();
							const float NewZ = ProjectedLoc.Location.Z + CapsuleHalfHeight;

							Location.Z = NewZ;
							Transform.SetLocation(Location);

							auto InNewLocation = FMath::VInterpConstantTo(
								InTargetActor->GetActorLocation(),
								Location,
								InTargetActor->GetWorld()->GetDeltaSeconds(), 500.f);

							InTargetActor->SetActorLocation(
								InNewLocation,
								false,
								nullptr,
								ETeleportType::TeleportPhysics);
						}
					}
				}
			}
		}
	});
}