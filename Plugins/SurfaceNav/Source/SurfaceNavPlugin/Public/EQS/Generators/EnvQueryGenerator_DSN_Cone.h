// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryGenerator_DSN_ProjectedPoints.h"
//#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"

#include "SurfaceNavAIDataProvider.h"

#include "EnvQueryGenerator_DSN_Cone.generated.h"


/**
 *  DSN Cone, can generate points near navigation in a cone shape from context.
 */



UCLASS(meta = (DisplayName = "DSN: Cone"))
class SURFACENAVPLUGIN_API UEnvQueryGenerator_DSN_Cone : public UEnvQueryGenerator_DSN_ProjectedPoints
{
	GENERATED_UCLASS_BODY()

	/** Maximum degrees of the generated cone */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderFloatValue ConeDegrees;

	/** Generation distance */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderFloatValue Range;

	/** The navigation's grid distance may be too dense for EQS and cause performance problems.
		The larger this number the further apart the points will be. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderIntValue DistanceMultiplier;

	/** If enabled, generates cone in backwards direction instead of forward direction. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderBoolValue InvertCone;

	/** Which context should the query points be generated around. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		TSubclassOf<UEnvQueryContext> CenterActor;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};