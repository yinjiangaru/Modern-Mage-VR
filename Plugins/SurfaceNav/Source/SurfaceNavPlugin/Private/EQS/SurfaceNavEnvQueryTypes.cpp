// Copyright BMK Games LLC. All Rights Reserved.

#include "EQS/SurfaceNavEnvQueryTypes.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

FText FSurfaceNavEnvTraceData::ToText() const
{
	FText Desc;

	if (TraceMode == ESurfaceNavEnvQueryTrace::ClosestSurface)
	{
		Desc = LOCTEXT("SurfaceNavProjection", "DSN Closest Surface Projection");
	}
	else if (TraceMode == ESurfaceNavEnvQueryTrace::Navigation)
	{
		Desc = LOCTEXT("SurfaceNavProjection", "DSN Navigation Projection");
	}
	else if (TraceMode == ESurfaceNavEnvQueryTrace::CheapestSurface)
	{
		Desc = LOCTEXT("SurfaceNavProjection", "DSN Cheapest Surface Projection");
	}

	return Desc;
}

void FSurfaceNavEnvTraceData::SetCheapestSurface()
{
	TraceMode = ESurfaceNavEnvQueryTrace::CheapestSurface;
	/*bCanTraceOnGeometry = true;
	bCanTraceOnNavMesh = false;
	bCanDisableTrace = false;*/
}

void FSurfaceNavEnvTraceData::SetClosestSurface()
{
	TraceMode = ESurfaceNavEnvQueryTrace::ClosestSurface;
	/*bCanTraceOnGeometry = true;
	bCanTraceOnNavMesh = false;
	bCanDisableTrace = false;*/
}

void FSurfaceNavEnvTraceData::SetNavigation()
{
	TraceMode = ESurfaceNavEnvQueryTrace::Navigation;
	/*bCanTraceOnGeometry = false;
	bCanTraceOnNavMesh = true;
	bCanDisableTrace = false;*/
}

void FSurfaceNavEnvTraceData::OnPostLoad()
{

}

#undef LOCTEXT_NAMESPACE
