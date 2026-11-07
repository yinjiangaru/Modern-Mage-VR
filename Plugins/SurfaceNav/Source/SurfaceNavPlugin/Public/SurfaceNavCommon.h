// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CollisionShape.h"
#include "Components/ActorComponent.h"
#include "Math/IntVector.h"


struct FSurfaceNavGridPoint;
struct FSurfaceNavPoint;
struct FSurfaceNavPointBase;
struct FCollisionQueryParams;
struct FHitResult;

#define ONE_DIV_90 0.0111111111111111f // 1 / 90
#define ONE_DIV_180 0.0055555555555556f // 1 / 180

//#define STATGROUP_BlockPool 1
//#define STATGROUP_GridPointPool 2
//
//DECLARE_STATS_GROUP(TEXT("Surface Nav Block Pooling"), STATGROUP_BlockPool, STATCAT_Advanced);
//DECLARE_STATS_GROUP(TEXT("Surface Nav Grid Point Pooling"), STATGROUP_GridPointPool, STATCAT_Advanced);

DECLARE_DELEGATE_OneParam(FReleaseBlockDelegate, const FIntVector&);
DECLARE_DELEGATE_OneParam(FBlockOverlapsFoundDelegate, const FIntVector&);
DECLARE_DELEGATE_OneParam(FHandleBlockMarkedDirtyDelegate, const FIntVector&);
DECLARE_DELEGATE_OneParam(FBlockNavUpdatedDelegate, const FIntVector&);
//DECLARE_DELEGATE_OneParam(FNavigableComponentUpdatedDelegate, const UActorComponent&);
//DECLARE_DELEGATE_TwoParams(FActorsInBlockChangedDelegate, const UActorComponent& Comp, const FIntVector&);

DECLARE_DELEGATE_FiveParams(FActorsInBlockChangedDelegate, const FIntVector&, const FVector&, const FVector&, const FBox&, const bool&);

DECLARE_DELEGATE_SixParams(FTraceForNavDelegate, bool&, FHitResult&, const FVector&, const FVector&, const FCollisionShape&, const FCollisionQueryParams&);
DECLARE_DELEGATE_FourParams(FCheckPointForSurfaceDelegate, bool&, FIntVector&, FVector&, FCollisionQueryParams&);

//DECLARE_DELEGATE_OneParam(FGetInvokerActorsDelegate, TArray<AActor*> Actors);
DECLARE_DELEGATE_OneParam(FQueueBlocksNearInvokersDelegate, uint8 ThreadId);
DECLARE_DELEGATE_TwoParams(FGridPointFinishedDelegate, FSurfaceNavGridPoint*, int);
DECLARE_DELEGATE_OneParam(FGridPointCanBeReleasedDelegate, const FIntVector&);
DECLARE_DELEGATE(FGridPointProcessedDelegate);

//DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnNeedsPathMoveRotationSignature, USurfaceNavMovementComponent, OnNeedsPathMoveRotation, FVector, TargetLocation, FVector, TargetUpVector, FVector, TargetForwardVector);

UENUM()
namespace ESurfaceNavOverlapShape
{
	enum Type
	{
		/** Use a sphere shape for this overlap test. */
		Sphere,
		/** Use a box shape for this overlap test. */
		Box
	};
}

UENUM(BlueprintType)
enum ESurfaceNavTraceMethod
{
	/** Traces to generate navigation will use the configured profile. */
	Profile,
	/** Traces to generate navigation will use the configured channel. */
	Channel
};



namespace SurfaceNavProxySettings
{
	extern bool AllowNavigationGenerationWithoutPlayer;
	extern bool IsFlatWorld;
	extern float BlockSize;
	extern float BlockExtent;
	extern float BlockExtentPadding;
	extern float PaddedBlockExtent;
	extern float FarBlockGenerationRadius;
	extern float NearBlockGenerationRadius;
	extern float MaxDistanceFromPlayerForGeneration;
	extern float CleanBlockInterval;
	extern int MaxBlockPoolSize;
	extern int InitialBlockPoolSize;
	extern float GridPointDistance;
	extern float GridPointDistanceSq;
	extern float GridPointDistancePadding;
	extern float PaddedGridPointDistance;
	extern int GridPointsPerBlock;
	extern int SingleAxisVoxelCount;
	extern int MaxPathingIterations;
	extern int MaxPathRequestsPerTick;
	extern float ObservedPathsTickInterval;
	extern float MaxDistanceFromNavigation;
	//extern float MagicLineTraceOffset;
	extern bool ShowDebugNavPointsDuringBuild;
	extern bool ShowDebug_AI_Paths;
	extern bool PerformExtraDebuggingTests;	
	extern float MaxExpenseModifier;
	extern float ExpenseModifierUpStep;
	extern float ExpenseModifierDownStep;
	extern float DelayBeforePathFindingAllowed;
	extern FCollisionShape BlockTestShape;
	extern FCollisionShape GridPointTestShape;
	extern float RefreshLocationDataInterval;
	extern float ProcessComponentUpdatesInterval;
	extern float ProcessPendingDirtyNavigationInterval;
	extern bool ShowNavPoints;
	extern bool ShowNavGridPoints;
	extern bool ShowNavBlocks;
	extern bool ShowNavNeighborDirs;
	extern bool ShowWhereNavigationEnds;
	extern bool ShowNavLinks;
	extern bool ShowNavPointNormals;
	extern bool ShowDebugStats;
	extern float DisplayRadiusFromCamera;
	extern float TraceSphereRadius;
	extern float TraceSphereStartOffset;
	extern float TraceSphereEndOffset;
	extern float MaxComponentSphereRadiusToProcess;
	extern int MaxRandomRepathOffset;
	extern float MaxExpenseModifierSq;
	extern float ExpenseModifierUpStepSq;
	extern float ExpenseModifierDownStepSq;

	extern TEnumAsByte<enum ESurfaceNavTraceMethod> TraceMethod;
	extern TEnumAsByte<enum ECollisionChannel> TraceChannel;

	extern TEnumAsByte<ESurfaceNavOverlapShape::Type> BlockOverlapTestShape;
	extern TEnumAsByte<ESurfaceNavOverlapShape::Type> GridPointOverlapTestShape;

	extern bool EnhancedNavModBrushTest;

	extern FString TraceProfile;

	extern uint8 BackgroundWorkerThreadCount;

	extern FVector GridOffset;

	extern bool FlagNarrowCorridors;
	extern float NarrowCorridorPercentToFlagSq;

	extern FSurfaceNavPoint* EmptyNavPoint;
	extern FSurfaceNavPointBase* EmptyNavPointBase;

	extern FTraceForNavDelegate TraceForNavDel;
	extern FCheckPointForSurfaceDelegate CheckPointForSurfaceDel;
}
