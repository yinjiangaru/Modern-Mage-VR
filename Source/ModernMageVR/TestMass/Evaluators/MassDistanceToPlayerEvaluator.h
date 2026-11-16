// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "MassDistanceToPlayerEvaluator.generated.h"

struct FTransformFragment;

USTRUCT()
struct FMassDistanceToPlayerEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	float Distance = 0.f;
};

USTRUCT(meta = (DisplayName = "Distance To Player"))
struct FMassDistanceToPlayerEvaluator : public FMassStateTreeEvaluatorBase
{
	GENERATED_BODY()

	FMassDistanceToPlayerEvaluator();

	using FInstanceDataType = FMassDistanceToPlayerEvaluatorInstanceData;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FMassDistanceToPlayerEvaluatorInstanceData::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};