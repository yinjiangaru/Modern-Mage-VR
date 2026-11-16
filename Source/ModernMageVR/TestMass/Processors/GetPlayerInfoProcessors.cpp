// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "GetPlayerInfoProcessors.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"
#include "ModernMageVR/TestMass/TestMassType.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UGetPlayerInfoProcessors::UGetPlayerInfoProcessors()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
}

void UGetPlayerInfoProcessors::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FPlayerInfoFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UGetPlayerInfoProcessors::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UGetPlayerInfoProcessors::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (ACharacter* Player = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0))
	{
		FVector PlayerPos = Player->GetActorLocation();

		TArray<FMassEntityHandle> EntitiesToSignalPathDone;

		EntityQuery.ForEachEntityChunk(EntityManager, Context, 
		[&, this](FMassExecutionContext& Context)
		{
			TArrayView<FPlayerInfoFragment> PlayerInfoList =
					Context.GetMutableFragmentView<FPlayerInfoFragment>();

			TConstArrayView<FTransformFragment> TransformList =
				Context.GetFragmentView<FTransformFragment>();

			for (int32 i = 0; i < Context.GetNumEntities(); ++i)
			{
				FPlayerInfoFragment& PlayerInfo = PlayerInfoList[i];

				const FTransformFragment& Transform = TransformList[i];

				const float DistanceToPlayer = FVector::Dist(
					PlayerPos, Transform.GetTransform().GetLocation());

				PlayerInfo.TargetPos = PlayerPos;
				PlayerInfo.DistanceToPlayer = DistanceToPlayer;

				EntitiesToSignalPathDone.Add(Context.GetEntity(i));
			}
		});

		if (EntitiesToSignalPathDone.Num())
		{
			SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, EntitiesToSignalPathDone);
		}
	}
}