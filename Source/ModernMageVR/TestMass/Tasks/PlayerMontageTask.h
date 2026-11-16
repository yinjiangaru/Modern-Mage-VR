// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"
#include "PlayerMontageTask.generated.h"

USTRUCT()
struct FPlayMontageTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TSoftObjectPtr<UAnimMontage> Montage;

	float MontageLength = 0.f;

	float TimeWaited = 0.f;
};

USTRUCT(meta = (DisplayName = "Play Montage Task"))
struct FPlayMontageTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPlayMontageTaskInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FPlayMontageTaskInstanceData::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void StateCompleted(FStateTreeExecutionContext& Context, const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates& CompletedActiveStates) const override;

protected:
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
};