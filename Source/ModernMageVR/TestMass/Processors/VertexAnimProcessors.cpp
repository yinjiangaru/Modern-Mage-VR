// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "VertexAnimProcessors.h"
#include "MassRepresentationFragments.h"
#include "AnimToTextureInstancePlaybackHelpers.h"
#include "MassRepresentationSubsystem.h"
#include "ModernMageVR/TestMass/VertexAnimInstance.h"

UVertexAnimProcessors::UVertexAnimProcessors()
	: EntityQuery(*this),
	UpdateAnimInstanceQuery(*this),
	UpdateMontageQuery(*this),
	UpdateMontagePositionQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;

	/*
	PrePhysics
	DuringPhysics
	PostPhysics
	SyncWorldToMass
	Representation
	Tasks
	Avoidance
	Movement
	*/
}

void UVertexAnimProcessors::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FVertexAnimInfoFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);

	//LOD
	//Fragment
	//ChunkFragment
	// struct FMyChunkData : public FMassChunkFragment
	//{
	//	int32 TeamID;
	//};
	//SharedFragment 

	EntityQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.SetChunkFilter(FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);

	UpdateAnimInstanceQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	UpdateAnimInstanceQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	UpdateAnimInstanceQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadOnly);
	UpdateAnimInstanceQuery.SetChunkFilter(FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);

	UpdateMontageQuery.AddRequirement<FMassMontageFragment>(EMassFragmentAccess::ReadOnly);
	UpdateMontageQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	UpdateMontageQuery.AddSubsystemRequirement<UMassRepresentationSubsystem>(EMassFragmentAccess::ReadWrite);

	UpdateMontagePositionQuery.AddRequirement<FMassMontageFragment>(EMassFragmentAccess::ReadWrite);
}

void UVertexAnimProcessors::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk( Context, [this](FMassExecutionContext& ExecutionContext)
	{
		UMassRepresentationSubsystem* RepresentationSubsystem = ExecutionContext.GetMutableSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
	
		check(RepresentationSubsystem);

		const TConstArrayView<FMassRepresentationFragment> RepresentationFragmentList = ExecutionContext.GetFragmentView<FMassRepresentationFragment>();
		const TConstArrayView<FMassRepresentationLODFragment> RepresentationLODFragmentList = ExecutionContext.GetFragmentView<FMassRepresentationLODFragment>();
		const TArrayView<FVertexAnimInfoFragment> VertexAnimationInfoFragmentList = ExecutionContext.GetMutableFragmentView<FVertexAnimInfoFragment>();
	
		const int32 NumEntities = ExecutionContext.GetNumEntities();

		for (int EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
		{
			const FMassRepresentationFragment& RepresentationFragment = RepresentationFragmentList[EntityIdx];
			const FMassRepresentationLODFragment& RepresentationLODFragment = RepresentationLODFragmentList[EntityIdx];
			FVertexAnimInfoFragment& VertexAnimInfoFragment = VertexAnimationInfoFragmentList[EntityIdx];

			if (RepresentationLODFragment.LOD == EMassLOD::High ||
				RepresentationLODFragment.PrevLOD == EMassLOD::High &&
				RepresentationFragment.CurrentRepresentation == EMassRepresentationType::StaticMeshInstance)
			{
				auto SMDesc = RepresentationFragment.StaticMeshDescHandle.ToIndex();
				auto& ISMInfo = RepresentationSubsystem->GetMutableInstancedStaticMeshInfos()[SMDesc];

				const float PrevPlayRate = VertexAnimInfoFragment.PlayRate;
				float GlobalTime = GetWorld()->GetTimeSeconds();

				//ԭ���Ž��� = (GlobalTime - OldStartTime) * OldRate
				VertexAnimInfoFragment.GlobalStartTime = GlobalTime - PrevPlayRate * 
					(GlobalTime - VertexAnimInfoFragment.GlobalStartTime) / VertexAnimInfoFragment.PlayRate;

				UpdateISMVertexAnimation(
					ISMInfo,
					VertexAnimInfoFragment,
					RepresentationLODFragment.LOD, 
					RepresentationLODFragment.PrevLOD,
					0);
			}
		}
	});

	UpdateAnimInstanceQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& ExecutionContext)
	{
		const auto& VelocityFragments = ExecutionContext.GetFragmentView<FMassVelocityFragment>();
		const auto& ActorFragments = ExecutionContext.GetFragmentView<FMassActorFragment>();

		const int32 NumEntities = ExecutionContext.GetNumEntities();
		for (int EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
		{
			const auto& VelocityFragment = VelocityFragments[EntityIdx];
			const auto& ActorFragment = ActorFragments[EntityIdx];

			UpdateAnimInstance(VelocityFragment, ActorFragment);
		}
	});

	UpdateMontageQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& ExecutionContext)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateAnimMontage)

		const auto& MontageFragments = ExecutionContext.GetFragmentView<FMassMontageFragment>();
		const auto& MassActorFragments = ExecutionContext.GetFragmentView<FMassActorFragment>();

		const int32 NumEntities = ExecutionContext.GetNumEntities();
		for (int EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
		{
			const auto& MontageFragment = MontageFragments[EntityIdx];
			const auto& MassActorFragment = MassActorFragments[EntityIdx];

			UAnimMontage* Montage = MontageFragment.Montage.IsNull() ? 
				nullptr : MontageFragment.Montage.LoadSynchronous();

			if (Montage == nullptr) { continue; }

			const AActor* Actor = MassActorFragment.Get();
			if (!Actor) { continue; }

			auto SKM = Actor->FindComponentByClass<USkeletalMeshComponent>();
			if (!SKM) { continue; }

			if (UAnimInstance* AnimInstance = SKM->GetAnimInstance())
			{
				bool bMontageAlreadyPlaying = false;
				for (int32 InstanceIndex = 0; InstanceIndex < AnimInstance->MontageInstances.Num(); InstanceIndex++)
				{
					FAnimMontageInstance* MontageInstance = AnimInstance->MontageInstances[InstanceIndex];
					if (MontageInstance && MontageInstance->Montage == Montage && MontageInstance->IsPlaying())
					{
						bMontageAlreadyPlaying = true;
						break;
					}
				}

				if (!bMontageAlreadyPlaying)
				{
					FAlphaBlendArgs BlendIn;
					BlendIn = Montage->GetBlendInArgs();

					AnimInstance->Montage_PlayWithBlendIn(
						Montage, 
						BlendIn, 
						1.0f, EMontagePlayReturnType::MontageLength, 
						MontageFragment.Position);
				}
			}
		}
	});

	UpdateMontagePositionQuery.ForEachEntityChunk( Context, [this](FMassExecutionContext& ExecutionContext)
	{
		const auto& MontageFragments = ExecutionContext.GetMutableFragmentView<FMassMontageFragment>();

		const int32 NumEntities = ExecutionContext.GetNumEntities();
		for (int EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
		{
			auto& MontageFragment = MontageFragments[EntityIdx];
			MontageFragment.Position += ExecutionContext.GetDeltaTimeSeconds();

			if (MontageFragment.Position > MontageFragment.Montage.LoadSynchronous()->GetPlayLength())
			{
				ExecutionContext.Defer().RemoveFragment<FMassMontageFragment>(ExecutionContext.GetEntity(EntityIdx));
			}
		}
	});
}

void UVertexAnimProcessors::UpdateISMVertexAnimation(
	FMassInstancedStaticMeshInfo& ISMInfo,
	FVertexAnimInfoFragment& AnimationData,
	float LODSignificance, 
	float PrevLODSignificance, 
	int32 NumFloatsToPad)
{
	FAnimToTextureAutoPlayData PlayData;
	if (UAnimToTextureInstancePlaybackLibrary::GetAutoPlayDataFromDataAsset(
		AnimationData.AnimToTextureData.LoadSynchronous(),
		AnimationData.AnimationStateIndex,
		PlayData))
	{
		if (PrevLODSignificance >= 4.f) { return; }

		ISMInfo.AddBatchedCustomData<FAnimToTextureAutoPlayData>(
			PlayData, 
			LODSignificance, PrevLODSignificance, NumFloatsToPad);
	}
}

void UVertexAnimProcessors::UpdateAnimInstance(
	const FMassVelocityFragment& VelocityFragment,
	const FMassActorFragment& ActorFragment)
{
	auto Actor = ActorFragment.Get();
	if (!Actor) { return; }

	auto SKComp = Actor->GetComponentByClass<USkeletalMeshComponent>();
	if (!SKComp) { return; }

	if (auto ActorAnimInstance = Cast<UVertexAnimInstance>(SKComp->GetAnimInstance()))
	{
		ActorAnimInstance->SetVelocity(VelocityFragment.Value);
	}
}