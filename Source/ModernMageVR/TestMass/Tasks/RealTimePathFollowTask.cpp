// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "RealTimePathFollowTask.h"
#include "MassMovementFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "NavigationSystem.h"
#include "NavCorridor.h"
#include "MassNavigationFragments.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

bool FRealtimePathFollowTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(AgentRadiusHandle);
	Linker.LinkExternalData(MovementParamsHandle);
	Linker.LinkExternalData(VelocityHandle);
	Linker.LinkExternalData(PlayerInfoHandle);

	return Super::Link(Linker);
}

EStateTreeRunStatus FRealtimePathFollowTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData<FRealtimePathFollowTaskInstanceData>(*this);

	const FMassMovementParameters& MovementParams = Context.GetExternalData(MovementParamsHandle);
	const FAgentRadiusFragment& AgentRadius = Context.GetExternalData(AgentRadiusHandle);

	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	FPlayerInfoFragment& PlayerInfo = Context.GetExternalData(PlayerInfoHandle);
	FMassVelocityFragment& Velocity = Context.GetExternalData(VelocityHandle);

	UNavigationSystemV1* NavMeshSubsystem = Cast<UNavigationSystemV1>(Context.GetWorld()->GetNavigationSystem());
	if (!NavMeshSubsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FNavAgentProperties NavAgentProperties(AgentRadius.Radius);
	const FVector AgentLocation = Context.GetExternalData(TransformHandle).GetTransform().GetLocation();

	const ANavigationData* NavData = NavMeshSubsystem->GetNavDataForProps(NavAgentProperties, AgentLocation);
	if (!NavData)
	{
		return EStateTreeRunStatus::Failed;
	}

	FPathFindingQuery Query(NavMeshSubsystem, *NavData, AgentLocation, InstanceData.TargetLocation);
	if (!Query.NavData.IsValid())
	{
		Query.NavData = NavMeshSubsystem->GetNavDataForProps(NavAgentProperties, Query.StartLocation);
	}

	FPathFindingResult Result(ENavigationQueryResult::Error);
	if (Query.NavData.IsValid())
	{
		Result = Query.NavData->FindPath(NavAgentProperties, Query);
	}

	if (!Result.IsSuccessful() || Result.Path->GetPathPoints().Num() <= 1)
	{
		return EStateTreeRunStatus::Failed;
	}

	const TArray<FNavPathPoint>& PathPoints = Result.Path->GetPathPoints();
	const FVector PathEndLocation = Result.Path->GetEndLocation();

	const float BaseSpeed = MovementParams.GenerateDesiredSpeed(InstanceData.MovementStyle, MassContext.GetEntity().Index);
	const float DesiredSpeed = FMath::Min(BaseSpeed * InstanceData.SpeedScale, MovementParams.MaxSpeed);
	const float DistanceToGoal = FVector::Distance(PathEndLocation, AgentLocation);

	//�޸�Ѱ·�����ӱ���
	if (PathPoints.Num() > 1)
	{
		FVector NextWayPoint = PathPoints[1].Location;
		FVector MoveDirection = (NextWayPoint - AgentLocation).GetSafeNormal2D();

		Velocity.Value = MoveDirection * DesiredSpeed;

		MoveTarget.DesiredSpeed.Set(DesiredSpeed);
		MoveTarget.DistanceToGoal = DistanceToGoal;
		MoveTarget.Center = NextWayPoint;
		MoveTarget.Forward = (NextWayPoint - AgentLocation).GetSafeNormal2D();
		MoveTarget.IntentAtGoal = EMassMovementAction::Move;
	}

	if (DistanceToGoal <= AgentRadius.Radius)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}