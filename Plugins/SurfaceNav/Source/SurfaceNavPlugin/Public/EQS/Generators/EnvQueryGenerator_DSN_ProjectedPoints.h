// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AI/Navigation/NavigationTypes.h"
//#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EQS/SurfaceNavEnvQueryTypes.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvQueryGenerator_DSN_ProjectedPoints.generated.h"

UCLASS(Abstract)
class SURFACENAVPLUGIN_API UEnvQueryGenerator_DSN_ProjectedPoints : public UEnvQueryGenerator
{
	GENERATED_UCLASS_BODY()

	/** trace params */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FSurfaceNavEnvTraceData ProjectionData;

	virtual FVector DetermineProjectedLocation(struct FSurfaceNavGridPoint*& GridPoint) const;

	/** store points as generator's result */
	virtual void StoreNavPoints(const TArray<FNavLocation>& Points, FEnvQueryInstance& QueryInstance) const;

	virtual void PostLoad() override;
};
