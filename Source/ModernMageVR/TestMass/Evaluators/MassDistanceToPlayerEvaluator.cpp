// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "MassDistanceToPlayerEvaluator.h"
#include "MassCommonFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "MassStateTreeDependency.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

FMassDistanceToPlayerEvaluator::FMassDistanceToPlayerEvaluator()
{
}

bool FMassDistanceToPlayerEvaluator::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);

	return true;
}

void FMassDistanceToPlayerEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FTransformFragment& TransformFragment = Context.GetExternalData(TransformHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FVector EntityPos = TransformFragment.GetTransform().GetLocation();
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(Context.GetWorld(), 0);

	if (Player)
	{
		const FVector PlayerPos = Player->GetActorLocation();
		InstanceData.Distance = FVector::Dist(EntityPos, PlayerPos);
	}
	else
	{
		InstanceData.Distance = -1.f;
	}
}

void FMassDistanceToPlayerEvaluator::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FTransformFragment>();
}
