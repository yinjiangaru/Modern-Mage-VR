// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "MassCommonFragments.h"
#include "MassNavigationTypes.h"
#include "MassGetPlayerInfoTask.generated.h"

USTRUCT()
struct FMassGetPlayerInfoInstanceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<ACharacter> PlayerCharacter;

	UPROPERTY(EditAnywhere)
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere)
	FMassTargetLocation TargetLocation;

	UPROPERTY(VisibleAnywhere)
	float DistanceToTarget = 0.0f;
};

USTRUCT(meta = (DisplayName = "Get Player Info Task"))
struct FMassGetPlayerInfoTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassGetPlayerInfoInstanceData;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;

	virtual const UStruct* GetInstanceDataType() const override { return FMassGetPlayerInfoInstanceData::StaticStruct(); };
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};