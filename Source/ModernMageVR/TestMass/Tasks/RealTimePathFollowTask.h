// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "MassMovementTypes.h"
#include "MassNavigationTypes.h"
#include "ModernMageVR/TestMass/TestMassType.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "RealtimePathFollowTask.generated.h"

struct FTransformFragment;
struct FMassMoveTargetFragment;
struct FAgentRadiusFragment;
struct FMassMovementParameters;
struct FMassDesiredMovementFragment;

USTRUCT()
struct FRealtimePathFollowTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassMovementStyleRef MovementStyle;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float SpeedScale = 1.f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float CorridorWidth = 300.f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float OffsetFromBoundaries = 10.f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float EndDistanceThreshold = 20.f;
};

USTRUCT(meta = (DisplayName = "Realtime Path Follow Task"))
struct FRealtimePathFollowTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRealtimePathFollowTaskInstanceData;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FRealtimePathFollowTaskInstanceData::StaticStruct(); };
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;
	TStateTreeExternalDataHandle<FAgentRadiusFragment> AgentRadiusHandle;
	TStateTreeExternalDataHandle<FMassMovementParameters> MovementParamsHandle;
	TStateTreeExternalDataHandle<FMassVelocityFragment> VelocityHandle;
	TStateTreeExternalDataHandle<FPlayerInfoFragment> PlayerInfoHandle;
};