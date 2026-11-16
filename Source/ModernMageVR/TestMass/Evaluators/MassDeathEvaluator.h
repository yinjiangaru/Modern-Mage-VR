// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ModernMageVR/TestMass/TestMassType.h"
#include "MassDeathEvaluator.generated.h"

USTRUCT()
struct FMassDeathEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	bool bIsDead = false;
};

USTRUCT(meta = (DisplayName = "Mass Death Evaluator"))
struct FMassDeathEvaluator : public FMassStateTreeEvaluatorBase
{
	GENERATED_BODY()

	FMassDeathEvaluator();

	using FInstanceDataType = FMassDeathEvaluatorInstanceData;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FMassDeathEvaluatorInstanceData::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FMassDeathFragment> DeathFragmentHandle;
};
