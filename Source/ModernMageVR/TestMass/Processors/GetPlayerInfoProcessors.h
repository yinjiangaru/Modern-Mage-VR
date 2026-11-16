// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "GetPlayerInfoProcessors.generated.h"

class UMassSignalSubsystem;

UCLASS()
class UGetPlayerInfoProcessors : public UMassProcessor
{
	GENERATED_BODY()

public:
	UGetPlayerInfoProcessors();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem = nullptr;
};