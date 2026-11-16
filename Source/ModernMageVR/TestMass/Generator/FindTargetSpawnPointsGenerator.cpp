// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "FindTargetSpawnPointsGenerator.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "MassSpawnLocationProcessor.h"

void UFindTargetSpawnPointsGenerator::Generate(
	UObject& QueryOwner,
	TConstArrayView<FMassSpawnedEntityType> EntityTypes,
	int32 Count,
	FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
	if (Count <= 0)
	{
		//Ö±½Ó·µ»Ø
		FinishedGeneratingSpawnPointsDelegate.Execute(TArray<FMassEntitySpawnDataGeneratorResult>());
		return;
	}

	TArray<FMassEntitySpawnDataGeneratorResult> Results;
	BuildResultsFromEntityTypes(Count, EntityTypes, Results);

	if (AActor* InActor = Cast<AActor>(&QueryOwner))
	{
		TArray<ATargetPoint*> AllowSpawnPoints;
		for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
		{
			ATargetPoint* InSpawnPoint = *It;
			if (InSpawnPoint)
			{
				AllowSpawnPoints.Add(InSpawnPoint);
			}
		}

		int32 LocationIndex = 0;
		int32 InNumber = AllowSpawnPoints.Num();
		if (InNumber > 0)
		{
			for (FMassEntitySpawnDataGeneratorResult& Result : Results)
			{
				Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
				Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();
				FMassTransformsSpawnData& Transforms = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();

				Transforms.Transforms.Reserve(Result.NumEntities);
				for (int i = 0; i < Result.NumEntities; i++)
				{
					FTransform& Transform = Transforms.Transforms.AddDefaulted_GetRef();
					if (ATargetPoint *InPoint = AllowSpawnPoints[LocationIndex % InNumber])
					{
						FTransform InOwenrTrasnform = InPoint->GetActorTransform();
						Transform.SetLocation(InOwenrTrasnform.GetLocation());
						Transform.SetRotation(InOwenrTrasnform.GetRotation());
					}

					LocationIndex++;
				}
			}
		}
	}

	FinishedGeneratingSpawnPointsDelegate.Execute(Results);
}
