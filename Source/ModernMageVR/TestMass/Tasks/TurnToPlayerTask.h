// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalSubsystem.h"
#include "MassCommonFragments.h"
#include "MassStateTreeTypes.h"
#include "ModernMageVR/TestMass/TestMassType.h"
#include "TurnToPlayerTask.generated.h"

USTRUCT()
struct FTurnToPlayerTaskInstanceData
{
	GENERATED_BODY()

	bool bFinished = false;
};

USTRUCT(meta = (DisplayName = "Turn to Player Task"))
struct FTurnToPlayerTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTurnToPlayerTaskInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FTurnToPlayerTaskInstanceData::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

protected:
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<FPlayerInfoFragment> PlayerInfoHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
};