// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "SurfaceNavPath.h"
#include "Navigation/MetaNavMeshPath.h"
#include "GameFramework/Actor.h"


struct SURFACENAVPLUGIN_API FSurfaceNavMetaPath : public FSurfaceNavPath
{
	typedef FSurfaceNavPath Super;
	static const FNavPathType Type;

	FSurfaceNavMetaPath();
	FSurfaceNavMetaPath(const TArray<FMetaPathWayPoint>& InWaypoints);
	FSurfaceNavMetaPath(const TArray<FMetaPathWayPoint>& InWaypoints, const AController& Owner);
	FSurfaceNavMetaPath(const TArray<FVector>& InWaypoints);
	FSurfaceNavMetaPath(const TArray<FVector>& InWaypoints, const AController& Owner);

	/** initialize path for path following */
	virtual void Initialize(const FVector& AgentLocation);

	/** try switching to next waypoint, depends on WaypointSwitchRadius */
	virtual bool ConditionalMoveToNextSection(const FVector& AgentLocation, EMetaPathUpdateReason Reason);

	/** force switching to next waypoint */
	bool ForceMoveToNextSection(const FVector& AgentLocation);

	/** updates underlying navmesh path for current target waypoint */
	virtual bool UpdatePath(const FVector& AgentLocation);

	/** copy properties of other meta path */
	virtual void CopyFrom(const FSurfaceNavMetaPath& Other);

	/** returns true if path at last waypoint */
	bool IsLastSection() const { return (TargetWaypointIdx == (Waypoints.Num() - 1)) && (Waypoints.Num() > 0); }

	/** returns index of current target waypoint */
	int32 GetTargetWaypointIndex() const { return TargetWaypointIdx; }

	/** returns number of waypoints */
	int32 GetNumWaypoints() const { return Waypoints.Num(); }

	/** returns waypoint array */
	const TArray<FMetaPathWayPoint>& GetWaypointArray() const { return Waypoints; }

	/** returns cached path goal */
	AActor* GetMetaPathGoal() const { return PathGoal.Get(); }

	/** tries to set waypoints, fails when path is ready being followed */
	bool SetWaypoints(const TArray<FMetaPathWayPoint>& InWaypoints);

	/** tries to set waypoints, fails when path is ready being followed */
	bool SetWaypoints(const TArray<FVector>& InWaypoints);

	/** returns radius for switching to next waypoint during path following */
	float GetWaypointSwitchRadius() const { return WaypointSwitchRadius; }

	/** sets radius for switching to next waypoint during path following */
	void SetWaypointSwitchRadius(float InSwitchRadius) { WaypointSwitchRadius = InSwitchRadius; }

	/** returns approximate length of path, ignores parameters */
	virtual float GetLengthFromPosition(FVector SegmentStart, uint32 NextPathPointIndex) const override;

	/** returns approximate length of path, ignores parameter */
	virtual float GetCostFromIndex(int32 PathPointIndex) const override;

//#if ENABLE_VISUAL_LOG
//	virtual void DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const override;
//#endif
//	virtual void DebugDraw(const ANavigationData* NavData, const FColor PathColor, UCanvas* Canvas, const bool bPersistent, const float LifeTime, const uint32 NextPathPointIndex = 0) const override;

protected:

	/** list of waypoints, including start and end of path */
	TArray<FMetaPathWayPoint> Waypoints;

	/** sum of 3D distance along waypoints, used for approximating length of path */
	float ApproximateLength;

	/** update navmesh path when this close to target waypoint */
	float WaypointSwitchRadius;

	/** current target of path following */
	int32 TargetWaypointIdx;

	/** stored goal actor */
	TWeakObjectPtr<AActor> PathGoal;

	/** stored goal actor's tether distance */
	float PathGoalTetherDistance;

	/** switch to next waypoint  */
	bool MoveToNextSection(const FVector& AgentLocation);
};