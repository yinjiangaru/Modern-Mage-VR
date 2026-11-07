// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavigationSystemTypes.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Pathfinding.h"

#include "EnvQueryTest_DSN_Pathfinding.generated.h"

#define DECLARE_DELEGATE_RetVal_ElevenParams( ReturnValueType, DelegateName, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type, Param9Type, Param10Type, Param11Type ) FUNC_DECLARE_DELEGATE( DelegateName, ReturnValueType, Param1Type, Param2Type, Param3Type, Param4Type, Param5Type, Param6Type, Param7Type, Param8Type, Param9Type, Param10Type, Param11Type )

class USurfaceNavigationSystem;

/**
 *  DSN pathfinding test, uses DSN navigation to pathfind to each query point for scoring or filtering.
 */



UCLASS()
class SURFACENAVPLUGIN_API UEnvQueryTest_DSN_Pathfinding : public UEnvQueryTest
{
	GENERATED_UCLASS_BODY()

	/** Testing mode */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding)
		TEnumAsByte<EEnvTestPathfinding::Type> TestMode;

	/** Other end of pathfinding test */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding)
		TSubclassOf<UEnvQueryContext> Context;

	/** Pathfinding direction */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding)
		FAIDataProviderBoolValue PathFromContext;

	/** If set, items with failed path will be invalidated (PathCost, PathLength) */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding, AdvancedDisplay)
		FAIDataProviderBoolValue SkipUnreachable;

	/** Min angle relative to ground normal this pawn is allowed to navigate on with 0 degrees
	being the ground below the pawn and 180 degrees being the ceiling. Must be less than MaxAngleFromGround. */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding, AdvancedDisplay)
		FAIDataProviderFloatValue MinAngleLimit;

	/** Max angle from ground this pawn is allowed to navigate on with 0 degrees
	being the ground below the pawn and 180 degrees being the ceiling. Must be greater
	than MinAngleFromGround. */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding, AdvancedDisplay)
		FAIDataProviderFloatValue MaxAngleLimit;

	/** How close the last path point needs to be to the target's world location
	to be considered a complete path. If targeting an actor then this value
	will need to increase with size of the actor. */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding, AdvancedDisplay)
		FAIDataProviderFloatValue AcceptanceRadius;

	/** How many iterations can the pathfinding algorithm go through to try
	to reach the target. */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding, AdvancedDisplay)
		FAIDataProviderIntValue MaxPathingIterations;

	/** Navigation filter to use in pathfinding */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding)
		TSubclassOf<UNavigationQueryFilter> FilterClass;

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

#if WITH_EDITOR
	/** update test properties after changing mode */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;

protected:

	DECLARE_DELEGATE_RetVal_ElevenParams(bool, FTestPathSignature, const FVector&, const FVector&, EPathFindingMode::Type, USurfaceNavigationSystem&, TSubclassOf<UNavigationQueryFilter>, const UObject*, float, float, float, int, bool);
	DECLARE_DELEGATE_RetVal_ElevenParams(float, FFindPathSignature, const FVector&, const FVector&, EPathFindingMode::Type, USurfaceNavigationSystem&, TSubclassOf<UNavigationQueryFilter>, const UObject*, float, float, float, int, bool);

	bool TestPathFrom(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, USurfaceNavigationSystem& NavSys, TSubclassOf<UNavigationQueryFilter> NavFilter, const UObject* PathOwner, float InMinAngleLimit, float InMaxAngleLimit, float InAcceptanceRadius, int InMaxPathingIterations, bool InAllowPartial) const;
	bool TestPathTo(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, USurfaceNavigationSystem& NavSys, TSubclassOf<UNavigationQueryFilter> NavFilter, const UObject* PathOwner, float InMinAngleLimit, float InMaxAngleLimit, float InAcceptanceRadius, int InMaxPathingIterations, bool InAllowPartial) const;
	float FindPathCostFrom(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, USurfaceNavigationSystem& NavSys, TSubclassOf<UNavigationQueryFilter> NavFilter, const UObject* PathOwner, float InMinAngleLimit, float InMaxAngleLimit, float InAcceptanceRadius, int InMaxPathingIterations, bool InAllowPartial) const;
	float FindPathCostTo(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, USurfaceNavigationSystem& NavSys, TSubclassOf<UNavigationQueryFilter> NavFilter, const UObject* PathOwner, float InMinAngleLimit, float InMaxAngleLimit, float InAcceptanceRadius, int InMaxPathingIterations, bool InAllowPartial) const;
	float FindPathLengthFrom(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, USurfaceNavigationSystem& NavSys, TSubclassOf<UNavigationQueryFilter> NavFilter, const UObject* PathOwner, float InMinAngleLimit, float InMaxAngleLimit, float InAcceptanceRadius, int InMaxPathingIterations, bool InAllowPartial) const;
	float FindPathLengthTo(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, USurfaceNavigationSystem& NavSys, TSubclassOf<UNavigationQueryFilter> NavFilter, const UObject* PathOwner, float InMinAngleLimit, float InMaxAngleLimit, float InAcceptanceRadius, int InMaxPathingIterations, bool InAllowPartial) const;

	//ANavigationData* FindNavigationData(USurfaceNavigationSystem& NavSys, UObject* Owner) const;
};
