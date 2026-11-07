// Copyright BMK Games LLC. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryGenerator_DSN_ProjectedPoints.h"
#include "SurfaceNavAIDataProvider.h"

#include "EnvQueryGenerator_DSN_Sphere.generated.h"


/**
 *  DSN Sphere, can generate points near navigation in a filled sphere pattern or hollow sphere, like a shell.
 */



UCLASS(meta = (DisplayName = "DSN: Sphere"))
class SURFACENAVPLUGIN_API UEnvQueryGenerator_DSN_Sphere : public UEnvQueryGenerator_DSN_ProjectedPoints
{
	GENERATED_UCLASS_BODY()

	/** Min distance between point and context. A value greater than zero results in a hollow sphere of points,
	similar to a donut. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderFloatValue InnerRadius;

	/** Max distance between point and context */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderFloatValue OuterRadius;

	/** The navigation's grid distance may be too dense for EQS and cause performance problems.
		The larger this number the further apart the points will be. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderIntValue DistanceMultiplier;

	/** Enables options to control the direction of the arc of the sphere. Only used if ArcAngle is less than 360. */
	UPROPERTY(EditDefaultsOnly, Category = Generator, meta = (EditCondition = "bDefineArc"))
		FEnvDirection ArcDirection;

	/** Specifies the angle of the arc cut of the sphere. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderFloatValue ArcAngle;

	/** The offset to apply to the EQS point along a given world aligned axis. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderFVectorValue WorldAxisOffset;

	/** Finds the axis that's closest to the navigation's normal and offset on that axis
		by the provided amount. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		FAIDataProviderFloatValue AxisOffsetClosestToNormal;

	/** Which context should the query points be generated around. */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
		TSubclassOf<UEnvQueryContext> Center;

	UPROPERTY(EditAnywhere, Category = Generator, meta = (InlineEditConditionToggle))
		uint32 bDefineArc : 1;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	FVector GetDirVector(FEnvQueryInstance& QueryInstance, AActor* CurActor) const;
};