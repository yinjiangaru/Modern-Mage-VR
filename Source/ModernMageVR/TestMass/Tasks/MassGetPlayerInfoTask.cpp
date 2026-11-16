// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "MassGetPlayerInfoTask.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

bool FMassGetPlayerInfoTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);

	return Super::Link(Linker);
}

EStateTreeRunStatus FMassGetPlayerInfoTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FMassGetPlayerInfoInstanceData>(*this);

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (ACharacter* PC = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(World, 0)))
	{
		const FVector PlayerLocation = PC->GetActorLocation();
		InstanceData.PlayerLocation = PlayerLocation;

		const FTransformFragment& TransformFragment = Context.GetExternalData(TransformHandle);
		FVector MyLocation = TransformFragment.GetTransform().GetLocation();

		InstanceData.DistanceToTarget = FVector::Distance(MyLocation, InstanceData.PlayerLocation);
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMassGetPlayerInfoTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FMassGetPlayerInfoInstanceData>(*this);

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	ACharacter* PC = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	InstanceData.PlayerCharacter = PC;

	if (!PC)
	{
		return EStateTreeRunStatus::Running;
	}

	const FVector PlayerLocation = PC->GetActorLocation();
	InstanceData.PlayerLocation = PlayerLocation;

	const FTransformFragment& TransformFragment = Context.GetExternalData(TransformHandle);
	FVector MyLocation = TransformFragment.GetTransform().GetLocation();

	InstanceData.DistanceToTarget = FVector::Distance(MyLocation, InstanceData.PlayerLocation);

	return EStateTreeRunStatus::Running;
}
