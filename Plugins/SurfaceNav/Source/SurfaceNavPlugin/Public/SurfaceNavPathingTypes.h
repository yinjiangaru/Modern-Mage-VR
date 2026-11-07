// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "SurfaceNavPathingTypes.generated.h"


USTRUCT(BlueprintType)
struct SURFACENAVPLUGIN_API FPawnAngleExpenseMultiplier // : public UObject
{
	GENERATED_BODY()

	/** The min angle to use for this multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Expense Multiplier", meta = (ClampMin = "0", UIMin = "0", ClampMax = "179", UIMax = "179"))
	float MinAngle { 0.f };

	/** The max angle to use for this multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Expense Multiplier", meta = (ClampMin = "1", UIMin = "1", ClampMax = "180", UIMax = "180"))
	float MaxAngle { 180.f };

	/** The expense multiplier to use for the angle range provided. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Expense Multiplier", meta = (ClampMin = "0.01", UIMin = "0.01", ClampMax = "100", UIMax = "100"))
	float ExpenseMultiplier { 1.f };
};