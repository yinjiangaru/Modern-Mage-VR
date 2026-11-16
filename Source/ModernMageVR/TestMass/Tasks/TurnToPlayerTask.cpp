// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "TurnToPlayerTask.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FTurnToPlayerTask::Link(FStateTreeLinker& Linker)
{
    Linker.LinkExternalData(TransformHandle);
    Linker.LinkExternalData(PlayerInfoHandle);

    return Super::Link(Linker);
}

EStateTreeRunStatus FTurnToPlayerTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FTurnToPlayerTaskInstanceData& InstanceData = Context.GetInstanceData<FTurnToPlayerTaskInstanceData>(*this);

    InstanceData.bFinished = false;
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTurnToPlayerTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
    if (InstanceData.bFinished)
    {
        return EStateTreeRunStatus::Succeeded;
    }

    FTransformFragment& TransformFragment = Context.GetExternalData(TransformHandle);
    const FPlayerInfoFragment& PlayerInfo = Context.GetExternalData(PlayerInfoHandle);

    const FVector SelfLocation = TransformFragment.GetTransform().GetLocation();

    FVector ToPlayer = PlayerInfo.TargetPos - SelfLocation;
    ToPlayer.Z = 0.f;

    if (ToPlayer.IsNearlyZero())
    {
        return EStateTreeRunStatus::Running;
    }

    FRotator CurrentRot = TransformFragment.GetTransform().GetRotation().Rotator();
    FRotator TargetRotation = ToPlayer.Rotation();

    FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRotation, DeltaTime, 6.f);

    TransformFragment.GetMutableTransform().SetRotation(NewRot.Quaternion());

    return EStateTreeRunStatus::Running;
}
