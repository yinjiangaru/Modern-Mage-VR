// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "MassTestTrait.h"
#include "MassEntityTemplateRegistry.h"

UMassTestTrait::UMassTestTrait()
{
	HellValue = 0.f;
}

void UMassTestTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	//BuildContext.AddFragment<FTestFragment>();

	FTestFragment &InFra = BuildContext.AddFragment_GetRef<FTestFragment>();
	InFra.Value = HellValue;
	//BuildContext.RequireFragment<FTestFragment>();

	//BuildContext.AddChunkFragment<FTestFragment>();
}
