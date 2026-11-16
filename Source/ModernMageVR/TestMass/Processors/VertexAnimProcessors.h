// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimToTextureDataAsset.h"
#include "MassProcessor.h"
#include "MassRepresentationTypes.h"
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "ModernMageVR/TestMass/TestMassType.h"
#include "VertexAnimProcessors.generated.h"

struct FMassActorFragment;
struct FMassVelocityFragment;

UCLASS()
class UVertexAnimProcessors : public UMassProcessor
{
	GENERATED_BODY()

public:
	UVertexAnimProcessors();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UFUNCTION()
	void UpdateISMVertexAnimation(FMassInstancedStaticMeshInfo& ISMInfo, FVertexAnimInfoFragment& AnimationData,
		float LODSignificance, float PrevLODSignificance, int32 NumFloatsToPad);

	UFUNCTION()
	void UpdateAnimInstance(const FMassVelocityFragment& VelocityFragment, const FMassActorFragment& ActorFragment);

	FMassEntityQuery EntityQuery;
	FMassEntityQuery UpdateAnimInstanceQuery;
	FMassEntityQuery UpdateMontageQuery;
	FMassEntityQuery UpdateMontagePositionQuery;
};