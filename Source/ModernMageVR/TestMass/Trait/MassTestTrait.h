// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "ModernMageVR/TestMass/TestMassType.h"
#include "MassTestTrait.generated.h"

UCLASS(meta = (DisplayName = "Hello Trait"))
class UMassTestTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UMassTestTrait();

protected:
	UPROPERTY(EditAnywhere, config, Category = Trait)
	float HellValue;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};