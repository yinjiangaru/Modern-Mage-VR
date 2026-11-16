// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "MassDeathEvaluator.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FMassDeathEvaluator::FMassDeathEvaluator()
{
}

bool FMassDeathEvaluator::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(DeathFragmentHandle);

	return true;
}

void FMassDeathEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const FMassDeathFragment& DeathFragment = Context.GetExternalData(DeathFragmentHandle);

	InstanceData.bIsDead = DeathFragment.bIsDead;
}
