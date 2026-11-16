// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "FindTargetSpawnPointsGenerator.generated.h"

UCLASS(BlueprintType, meta = (DisplayName = "Find Target Spawn Points"))
class UFindTargetSpawnPointsGenerator : public UMassEntitySpawnDataGeneratorBase
{
	GENERATED_BODY()

public:
	virtual void Generate(
		UObject& QueryOwner,
		TConstArrayView<FMassSpawnedEntityType> EntityTypes, 
		int32 Count,
		FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const override;

};