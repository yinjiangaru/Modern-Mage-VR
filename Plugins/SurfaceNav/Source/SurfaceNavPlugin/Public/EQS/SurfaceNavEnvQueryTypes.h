// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "SurfaceNavEnvQueryTypes.generated.h"


UENUM()
namespace ESurfaceNavEnvQueryTrace
{
	enum Type
	{		
		None,
		/** Snaps the query point to the average of the nearby navigation. */
		Navigation,
		/** Snaps the query point to the closest surface with navigation. */
		ClosestSurface,
		/** Snaps the query point to the cheapest nearby navigation. */
		CheapestSurface
	};
}


USTRUCT()
struct SURFACENAVPLUGIN_API FSurfaceNavEnvTraceData
{
	GENERATED_USTRUCT_BODY()

		/*enum EDescriptionMode
	{
		Brief,
		Detailed,
	};*/

	FSurfaceNavEnvTraceData() :
		OffsetAlongNavNormal(0.0f), TraceMode(ESurfaceNavEnvQueryTrace::None)
	{
	}

	///** version number for updates */
	//UPROPERTY()
	//	int32 VersionNum;

	/** Navigation filter to use for this query. */
	UPROPERTY(EditDefaultsOnly, Category = Trace)
		TSubclassOf<UNavigationQueryFilter> NavigationFilter;

	///** search height: below point */
	//UPROPERTY(EditDefaultsOnly, Category = Trace, meta = (UIMin = 0, ClampMin = 0))
	//	float ProjectDown;

	///** search height: above point */
	//UPROPERTY(EditDefaultsOnly, Category = Trace, meta = (UIMin = 0, ClampMin = 0))
	//	float ProjectUp;

	///** shape parameter for trace */
	//UPROPERTY(EditDefaultsOnly, Category = Trace, meta = (UIMin = 0, ClampMin = 0))
	//	float ExtentX;

	///** shape parameter for trace */
	//UPROPERTY(EditDefaultsOnly, Category = Trace, meta = (UIMin = 0, ClampMin = 0))
	//	float ExtentY;

	///** shape parameter for trace */
	//UPROPERTY(EditDefaultsOnly, Category = Trace, meta = (UIMin = 0, ClampMin = 0))
	//	float ExtentZ;

	/** Offsets the query point by the provided amount along the normal for this navigation point. */
	UPROPERTY(EditDefaultsOnly, Category = Trace)
		float OffsetAlongNavNormal;

	///** geometry trace channel */
	//UPROPERTY(EditDefaultsOnly, Category = Trace)
	//	TEnumAsByte<enum ETraceTypeQuery> TraceChannel;

	///** geometry trace channel for serialization purposes */
	//UPROPERTY(EditDefaultsOnly, Category = Trace)
	//	TEnumAsByte<enum ECollisionChannel> SerializedChannel;

	///** geometry trace profile */
	//UPROPERTY(EditDefaultsOnly, Category = Trace)
	//	FName TraceProfileName;

	///** shape used for geometry tracing */
	//UPROPERTY(EditDefaultsOnly, Category = Trace)
	//	TEnumAsByte<EEnvTraceShape::Type> TraceShape;

	/** Where should the query point be snapped to. */
	UPROPERTY(EditDefaultsOnly, Category = Trace)
		TEnumAsByte<ESurfaceNavEnvQueryTrace::Type> TraceMode;

	///** if set, trace will run on complex collisions */
	//UPROPERTY(EditDefaultsOnly, Category = Trace, AdvancedDisplay)
	//	uint32 bTraceComplex : 1;

	///** if set, trace will look only for blocking hits */
	//UPROPERTY(EditDefaultsOnly, Category = Trace, AdvancedDisplay)
	//	uint32 bOnlyBlockingHits : 1;

	///** if set, editor will allow picking navmesh trace */
	//UPROPERTY(EditDefaultsOnly, Category = Trace)
	//	uint32 bCanTraceOnNavMesh : 1;

	///** if set, editor will allow picking geometry trace */
	//UPROPERTY(EditDefaultsOnly, Category = Trace)
	//	uint32 bCanTraceOnGeometry : 1;

	///** if set, editor will allow  */
	//UPROPERTY(EditDefaultsOnly, Category = Trace)
	//	uint32 bCanDisableTrace : 1;

	///** if set, editor show height up/down properties for projection */
	//UPROPERTY(EditDefaultsOnly, Category = Trace)
	//	uint32 bCanProjectDown : 1;

	FText ToText() const;

	void SetCheapestSurface();
	void SetClosestSurface();
	void SetNavigation();

	void OnPostLoad();
};