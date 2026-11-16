// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "VertexAnimLocomotionProcessors.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"

UVertexAnimLocomotionProcessors::UVertexAnimLocomotionProcessors()
	: EntityQuery(*this)
{
}

void UVertexAnimLocomotionProcessors::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FVertexAnimInfoFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FVertexAnimLocomotionFragment>(EMassFragmentAccess::ReadOnly);
}

void UVertexAnimLocomotionProcessors::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
		const auto MassVelocityFragments = Context.GetFragmentView<FMassVelocityFragment>();
		const auto VertexAnimFragments = Context.GetMutableFragmentView<FVertexAnimInfoFragment>();
		const auto VertexAnimLocomotionFragments = Context.GetFragmentView<FVertexAnimLocomotionFragment>();

		const int32 NumEntities = Context.GetNumEntities();
		for (int EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
		{
			const auto& VelocityFragment = MassVelocityFragments[EntityIdx];
			auto& VertexAnimFragment = VertexAnimFragments[EntityIdx];
			const auto& VertexAnimLocomotionFragment = VertexAnimLocomotionFragments[EntityIdx];

			float Speed = VelocityFragment.Value.Length();
			VertexAnimFragment.AnimationStateIndex = 
				Speed >= VertexAnimLocomotionFragment.SpeedThreshhold ? 
				VertexAnimLocomotionFragment.RunAnimIndex : VertexAnimLocomotionFragment.IdleAnimIndex;
		}
	});
}
