// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "VertexAnimLocomotionProcessors.generated.h"

UCLASS()
class UVertexAnimLocomotionProcessors : public UMassProcessor
{
	GENERATED_BODY()

public:
	UVertexAnimLocomotionProcessors();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};