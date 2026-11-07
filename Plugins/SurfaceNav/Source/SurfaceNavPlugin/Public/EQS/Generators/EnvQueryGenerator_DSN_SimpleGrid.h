// Copyright BMK Games LLC. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryGenerator_DSN_ProjectedPoints.h"
//#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"

#include "SurfaceNavAIDataProvider.h"

#include "EnvQueryGenerator_DSN_SimpleGrid.generated.h"


/**
 *  DSN Simple grid, can generate points near navigation in a cube shape around context.
 */



UCLASS(meta = (DisplayName = "DSN: Simple Grid"))
class SURFACENAVPLUGIN_API UEnvQueryGenerator_DSN_SimpleGrid : public UEnvQueryGenerator_DSN_ProjectedPoints
{
	GENERATED_UCLASS_BODY()

	/** Half of square's extent, like a radius */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderFloatValue GridHalfSize;

	/** The navigation's grid distance may be too dense for EQS and cause performance problems.
		The larger this number the further apart the points will be. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderIntValue DistanceMultiplier;

	/** The offset to apply to the EQS point along a given world aligned axis. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderFVectorValue WorldAxisOffset;

	/** Finds the axis that's closest to the navigation's normal and offset on that axis
		by the provided amount. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderFloatValue AxisOffsetClosestToNormal;

	/** Which context should the query points be generated around. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		TSubclassOf<UEnvQueryContext> GenerateAround;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};