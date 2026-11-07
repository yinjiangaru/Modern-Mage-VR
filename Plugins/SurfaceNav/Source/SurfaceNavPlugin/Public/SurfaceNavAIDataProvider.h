// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "DataProviders/AIDataProvider.h"
#include "SurfaceNavAIDataProvider.generated.h"

USTRUCT()
struct SURFACENAVPLUGIN_API FAIDataProviderFIntVectorValue : public FAIDataProviderTypedValue
{
	GENERATED_USTRUCT_BODY()

	FAIDataProviderFIntVectorValue();

	UPROPERTY(EditAnywhere, Category = Value)
		FIntVector DefaultValue;

	FIntVector GetValue() const;
	virtual FString ValueToString() const override;
};

USTRUCT()
struct SURFACENAVPLUGIN_API FAIDataProviderFVectorValue : public FAIDataProviderTypedValue
{
	GENERATED_USTRUCT_BODY()

	FAIDataProviderFVectorValue();

	UPROPERTY(EditAnywhere, Category = Value)
		FVector DefaultValue;

	FVector GetValue() const;
	virtual FString ValueToString() const override;
};