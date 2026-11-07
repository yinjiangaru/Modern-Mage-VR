// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "AI/NavigationSystemBase.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "SurfaceNavPathingTypes.h"
#include "Misc/ScopeRWLock.h"
#include "SurfaceNavPath.generated.h"

typedef TSharedRef<struct FSurfaceNavPath, ESPMode::ThreadSafe> FSurfaceNavPathSharedRef;
typedef TSharedPtr<struct FSurfaceNavPath, ESPMode::ThreadSafe> FSurfaceNavPathSharedPtr;
typedef TWeakPtr<struct FSurfaceNavPath, ESPMode::ThreadSafe> FSurfaceNavPathWeakPtr;

// Custom surface nav query data so we can get some data we need.
// May be able to simplify this with a completely new object instead
// of inheriting from FPathFindingQueryData.
struct SURFACENAVPLUGIN_API FSurfaceNavPathFindingQueryData //: public FPathFindingQueryData
{
	TWeakObjectPtr<const UObject> Owner;
	FVector StartLocation;
	FVector EndLocation;

	FVector NonOffsetStartLocation;
	FVector NonOffsetEndLocation;

	//FSharedConstNavQueryFilter QueryFilter;

	float MinAngleFromGround = 0.f;
	float MaxAngleFromGround = 180.f;
	float AcceptanceRadius = 10.f;

	TSubclassOf<UNavigationQueryFilter> FilterClass = NULL;
	FSurfaceNavPathSharedPtr PathInstanceToFill;

	/** cost limit of nodes allowed to be added to the open list */
	float CostLimit;

	/** additional flags passed to navigation data handling request */
	int32 NavDataFlags;

	/** Max pathing iterations allowed each frame */
	uint32 MaxPathingIterations : 24,
		/** if set, allow partial paths as a result */
		bAllowPartialPaths : 1,
		/** If set allows bypassing some of the iteration limits for things like EQS tests. */
		bIsForTesting : 1;
	/** If set, will move to the provided end location. Does nothing when target is an actor. */
		//bMoveToEndLocation : 1;

	TArray<FPawnAngleExpenseMultiplier> PawnAngleExpenseMultipliers;

	FSurfaceNavPathFindingQueryData() : StartLocation(FNavigationSystem::InvalidLocation), EndLocation(FNavigationSystem::InvalidLocation), CostLimit(FLT_MAX), NavDataFlags(0), bAllowPartialPaths(0), bIsForTesting(0) {}

	FSurfaceNavPathFindingQueryData(const UObject* InOwner, const FVector& InStartLocation, const FVector& InEndLocation, 
		TSubclassOf<UNavigationQueryFilter> InFilterClass = NULL, FSurfaceNavPathSharedPtr InPathInstanceToFill = NULL, 
		int32 InNavDataFlags = 0, bool bInAllowPartialPaths = true, const float InCostLimit = FLT_MAX, float InMinAngleFromGround = 0.f,
		float InMaxAngleFromGround = 180.f, float InAcceptanceRadius = 10.f, int InMaxPathingIterations = 0, bool InIsForTesting = false, 
		TArray<FPawnAngleExpenseMultiplier> InPawnAngleExpenseMultipliers = {}) :
		Owner(InOwner), StartLocation(InStartLocation), EndLocation(InEndLocation), MinAngleFromGround(InMinAngleFromGround), 
		MaxAngleFromGround(InMaxAngleFromGround), AcceptanceRadius(InAcceptanceRadius), FilterClass(InFilterClass),
		PathInstanceToFill(InPathInstanceToFill), CostLimit(InCostLimit), NavDataFlags(InNavDataFlags), MaxPathingIterations(InMaxPathingIterations),
		bAllowPartialPaths(bInAllowPartialPaths), bIsForTesting(InIsForTesting), PawnAngleExpenseMultipliers(InPawnAngleExpenseMultipliers)
	{
		NonOffsetStartLocation = InStartLocation;
		NonOffsetEndLocation = InEndLocation;
	}

	FSurfaceNavPathFindingQueryData(const UObject* InOwner, const FVector& InStartLocation, const FVector& InEndLocation, 
		const FVector& InNonOffsetStartLocation, const FVector& InNonOffsetEndLocation,
		TSubclassOf<UNavigationQueryFilter> InFilterClass = NULL, FSurfaceNavPathSharedPtr InPathInstanceToFill = NULL,
		int32 InNavDataFlags = 0, bool bInAllowPartialPaths = true, const float InCostLimit = FLT_MAX, float InMinAngleFromGround = 0.f,
		float InMaxAngleFromGround = 180.f, float InAcceptanceRadius = 10.f, int InMaxPathingIterations = 0, bool InIsForTesting = false,
		TArray<FPawnAngleExpenseMultiplier> InPawnAngleExpenseMultipliers = {}) :
		Owner(InOwner), StartLocation(InStartLocation), EndLocation(InEndLocation), NonOffsetStartLocation(InNonOffsetStartLocation), 
		NonOffsetEndLocation(InNonOffsetEndLocation), MinAngleFromGround(InMinAngleFromGround),
		MaxAngleFromGround(InMaxAngleFromGround), AcceptanceRadius(InAcceptanceRadius), FilterClass(InFilterClass),
		PathInstanceToFill(InPathInstanceToFill), CostLimit(InCostLimit), NavDataFlags(InNavDataFlags), MaxPathingIterations(InMaxPathingIterations),
		bAllowPartialPaths(bInAllowPartialPaths), bIsForTesting(InIsForTesting), PawnAngleExpenseMultipliers(InPawnAngleExpenseMultipliers) {}

	virtual ~FSurfaceNavPathFindingQueryData() {}

	// ***********************************************
	// ***********************************************

	/*FSurfaceNavPathFindingQueryData() : FPathFindingQueryData()
	{
	}*/

	FSurfaceNavPathFindingQueryData(const FSurfaceNavPathFindingQueryData& Source);

	/*{
		Super();
	};*/

	//FSurfaceNavPathFindingQueryData(const UObject* InOwner, const FVector& InStartLocation, const FVector& InEndLocation, FSharedConstNavQueryFilter InQueryFilter = nullptr, int32 InNavDataFlags = 0, bool bInAllowPartialPaths = true, const float InCostLimit = FLT_MAX)
	//	: FPathFindingQueryData(InOwner, InStartLocation, InEndLocation, InQueryFilter, InNavDataFlags, bInAllowPartialPaths, InCostLimit)
	//{
	//	//Owner = InOwner;
	//}

	explicit FSurfaceNavPathFindingQueryData(FSurfaceNavPathSharedRef PathToRecalculate);


	//{
	//	//Super(InOwner, InStartLocation, InEndLocation, InQueryFilter, InNavDataFlags, bInAllowPartialPaths, InCostLimit);
	//};

	/*float HeightOffset = 0.f;

	virtual void SetHeightOffset(float InHeightOffset) { HeightOffset = InHeightOffset; }
	float GetHeightOffset() { return HeightOffset; }*/

	FSurfaceNavPathFindingQueryData& SetPathInstanceToUpdate(FSurfaceNavPathSharedPtr InPathInstanceToFill) { PathInstanceToFill = InPathInstanceToFill; return *this; }
	FSurfaceNavPathFindingQueryData& SetAllowPartialPaths(bool bAllow) { bAllowPartialPaths = bAllow; return *this; }
	//FSurfaceNavPathFindingQueryData& SetIsForTesting(bool InIsForTesting) { bIsForTesting = InIsForTesting; return *this; }


	void SetNavigationFilter(TSubclassOf<UNavigationQueryFilter> Filter) { FilterClass = Filter; }
	TSubclassOf<UNavigationQueryFilter> GetNavigationFilter() const { return FilterClass; }
};

USTRUCT(BlueprintType)
struct SURFACENAVPLUGIN_API FSurfaceNavPathPointData
{
	GENERATED_BODY()

	FSurfaceNavPathPointData() 
	{}

	FSurfaceNavPathPointData(FVector InLocation, FVector InNormal, float InExpense)
		: Location(InLocation), Normal(InNormal), Expense(InExpense) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Point Data")
	FVector Location { FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Point Data")
	FVector Normal { FVector::ZeroVector };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Point Data")
	float Expense { 0.f };
};

struct SURFACENAVPLUGIN_API FSurfaceNavPath : public TSharedFromThis<FSurfaceNavPath, ESPMode::ThreadSafe>
{
	//DECLARE_DELEGATE_OneParam(FPathObserverDelegate, FNavigationPath*);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FSurfaceNavPathObserverDelegate, FSurfaceNavPath*, ENavPathEvent::Type);

	FSurfaceNavPath();
	FSurfaceNavPath(const TArray<FVector>& Points, const TArray<float>& Expenses, const TArray<FVector>& Normals, TArray<int64>& CustomLinkIDs, AActor* Base = NULL);
	virtual ~FSurfaceNavPath()
	{ }

	FORCEINLINE bool IsValid() const
	{
		return bIsReady && PathPoints.Num() > 1 && bUpToDate;
	}
	FORCEINLINE bool IsUpToDate() const
	{
		return bUpToDate;
	}
	FORCEINLINE bool IsReady() const
	{
		return bIsReady;
	}
	FORCEINLINE bool IsPartial() const
	{
		return bIsPartial;
	}
	FORCEINLINE bool DidSearchReachedLimit() const
	{
		return bReachedSearchLimit;
	}
	FORCEINLINE bool IsWaitingForRepath() const
	{
		return bWaitingForRepath;
	}
	FORCEINLINE void SetManualRepathWaiting(const bool bInWaitingForRepath)
	{
		bWaitingForRepath = bInWaitingForRepath;
	}
	FORCEINLINE bool ShouldUpdateStartPointOnRepath() const
	{
		return bUpdateStartPointOnRepath;
	}
	FORCEINLINE bool ShouldUpdateEndPointOnRepath() const
	{
		return bUpdateEndPointOnRepath;
	}
	FORCEINLINE FVector GetDestinationLocation() const
	{
		return IsValid() ? PathPoints.Last().Location : INVALID_NAVEXTENT;
	}
	FORCEINLINE FSurfaceNavPathObserverDelegate& GetObserver()
	{
		return ObserverDelegate;
	}
	FORCEINLINE FDelegateHandle AddObserver(FSurfaceNavPathObserverDelegate::FDelegate NewObserver)
	{
		return ObserverDelegate.Add(NewObserver);
	}
	FORCEINLINE void RemoveObserver(FDelegateHandle HandleOfObserverToRemove)
	{
		ObserverDelegate.Remove(HandleOfObserverToRemove);
	}

	FORCEINLINE void MarkReady()
	{
		bIsReady = true;
	}

	//FORCEINLINE void SetNavigationDataUsed(const ANavigationData* const NewData);

	/*FORCEINLINE ANavigationData* GetNavigationDataUsed() const
	{
		return NavigationDataUsed.Get();
	}*/
	FORCEINLINE void SetQuerier(const UObject* InQuerier)
	{
		PathFindingQueryData.Owner = InQuerier;
	}
	FORCEINLINE const UObject* GetQuerier() const
	{
		return PathFindingQueryData.Owner.Get();
	}
	FORCEINLINE void SetQueryData(const FSurfaceNavPathFindingQueryData& QueryData)
	{
		PathFindingQueryData = QueryData;
	}
	FORCEINLINE FSurfaceNavPathFindingQueryData GetQueryData() const
	{
		// return copy of query data
		return PathFindingQueryData;
	}

	//FORCEINLINE void SetObserver(const FPathObserverDelegate& Observer) { ObserverDelegate = Observer; }
	FORCEINLINE void SetIsPartial(const bool bPartial)
	{
		bIsPartial = bPartial;
	}
	FORCEINLINE void SetSearchReachedLimit(const bool bLimited)
	{
		bReachedSearchLimit = bLimited;
	}

	//FORCEINLINE void SetFilter(FSharedConstNavQueryFilter InFilter)
	//{
	//	PathFindingQueryData.QueryFilter = InFilter;
	//	//Filter = InFilter;
	//}
	//FORCEINLINE FSharedConstNavQueryFilter GetFilter() const
	//{
	//	return PathFindingQueryData.QueryFilter;
	//}
	FORCEINLINE AActor* GetBaseActor() const
	{
		return Base.Get();
	}

	FVector GetStartLocation() const
	{
		return PathPoints.Num() > 0 ? PathPoints[0].Location : FNavigationSystem::InvalidLocation;
	}
	FVector GetEndLocation() const
	{
		return PathPoints.Num() > 0 ? PathPoints.Last().Location : FNavigationSystem::InvalidLocation;
	}

	FORCEINLINE void DoneUpdating(ENavPathUpdateType::Type UpdateType)
	{
		static const ENavPathEvent::Type PathUpdateTypeToPathEvent[] = {
			ENavPathEvent::UpdatedDueToGoalMoved // GoalMoved,
			, ENavPathEvent::UpdatedDueToNavigationChanged // NavigationChanged,
			, ENavPathEvent::MetaPathUpdate // MetaPathUpdate,
			, ENavPathEvent::Custom // Custom,
		};

		bUpToDate = true;
		bWaitingForRepath = false;

		if (bUseOnPathUpdatedNotify)
		{
			// notify path before observers
			OnPathUpdated(UpdateType);
		}

		ObserverDelegate.Broadcast(this, PathUpdateTypeToPathEvent[uint8(UpdateType)]);
	}

	FORCEINLINE int64 GetTimeStamp() const { return LastUpdateTimeStamp; }
	FORCEINLINE void SetTimeStamp(int64 TimeStamp) { LastUpdateTimeStamp = TimeStamp; }

	void Invalidate();
	void RePathFailed();

	/** Resets all variables describing generated path before attempting new pathfinding call.
	  * This function will NOT reset setup variables like goal actor, filter, observer, etc */
	virtual void ResetForRepath();

	virtual void DebugDraw(const ANavigationData* NavData, const FColor PathColor, class UCanvas* Canvas, const bool bPersistent, const float LifeTime, const uint32 NextPathPointIndex = 0) const;

#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
	virtual FString GetDescription() const;
#endif // ENABLE_VISUAL_LOG

	/** check if path contains specific custom nav link */
	virtual bool ContainsCustomLink(uint32 UniqueLinkId) const;

	/** check if path contains any custom nav link */
	virtual bool ContainsAnyCustomLink() const;

	virtual bool PathPointIsNavLink(int index) const { return PathPointIsNavLink(PathPoints[index]); }

	virtual bool PathPointIsNavLink(FNavPathPoint PathPoint) const 
	{ 
		// We test not equal to zero because we use negative hashes for the IDs of the simple links
		// so that they won't get interrupted during their moves
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
		return PathPoint.CustomLinkId != 0;
#else
		return PathPoint.CustomNavLinkId.GetId() != 0;
#endif
	}

	///** check if path contains given node */
	virtual bool ContainsNode(FNavPathPoint Node) const { return PathPoints.Contains(Node); }

	/** get cost of path, starting from given point */
	virtual float GetCostFromIndex(int32 PathPointIndex) const;
	//{
	//	// return approximation of full path * default cost, there's not enough data to give accurate value
	//	/*const UNavArea* DefaultAreaOb = static_cast<const UNavArea*>(FNavigationSystem::GetDefaultWalkableArea().GetDefaultObject());
	//	const float DefaultAreaCost = DefaultAreaOb ? DefaultAreaOb->DefaultCost : 1.0f;
	//	return FMath::Max(PathPoints.Num() - PathPointIndex, 0) * DefaultAreaCost;*/

	//	if (!IsValid())
	//		return MAX_FLT;


	//	return IsValid() ? PointExpenses[PathPointIndex] : MAX_FLT;

	//	//return FMath::Max(GetPointExpense(PathPointIndex), 0);
	//}

	///** get cost of path, starting from given node */
	//virtual float GetCostFromNode(NavNodeRef PathNode) const
	//{
	//	return 0.0f;
	//}

	FORCEINLINE float GetCost() const
	{
		return GetCostFromIndex(0);
	}

	/** calculates total length of segments from NextPathPoint to the end of path, plus distance from CurrentPosition to NextPathPoint */
	virtual float GetLengthFromPosition(FVector SegmentStart, uint32 NextPathPointIndex) const;

	FORCEINLINE float GetLength() const
	{
		return PathPoints.Num() ? GetLengthFromPosition(PathPoints[0].Location, 1) : 0.0f;
	}

	static bool GetPathPoint(const FSurfaceNavPath* Path, uint32 PathVertIdx, FNavPathPoint& PathPoint)
	{
		if (Path && Path->GetPathPoints().IsValidIndex((int32)PathVertIdx))
		{
			PathPoint = Path->PathPoints[PathVertIdx];
			return true;
		}

		return false;
	}

	FORCEINLINE FVector GetPointNormal(int32 index) const
	{
		return IsValid() ? PointNormals[index] : FVector::UpVector;
	}

	FORCEINLINE float GetPointExpense(int32 index) const
	{
		return IsValid() ? PointExpenses[index] : MAX_FLT;
	}

	FORCEINLINE FVector GetPointLocation(int32 index) const
	{
		return IsValid() && PathPoints.Num() > index ? PathPoints[index].Location : FNavigationSystem::InvalidLocation;
	}

	FORCEINLINE const TArray<FNavPathPoint>& GetPathPoints() const
	{
		return PathPoints;
	}
	FORCEINLINE TArray<FNavPathPoint>& GetPathPoints()
	{
		return PathPoints;
	}

	FORCEINLINE void UpdatePathData(const TArray<FVector>& Points, const TArray<float>& Expenses, const TArray<FVector>& Normals, TArray<int64>& CustomLinkIDs)
	{
		PathPoints.SetNumZeroed(Points.Num());
		PointExpenses.SetNumZeroed(Expenses.Num());
		PointNormals.SetNumZeroed(Normals.Num());

		check(PathPoints.Num() == PointExpenses.Num() && PathPoints.Num() == PointNormals.Num());

		for (int32 i = 0; i < Points.Num(); i++)
		{
			FBasedPosition BasedPoint(Base.IsValid() ? Base.Get() : nullptr, Points[i]);
			PathPoints[i] = FNavPathPoint(*BasedPoint);

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
			PathPoints[i].CustomLinkId = CustomLinkIDs[i];
#else
			PathPoints[i].CustomNavLinkId = FNavLinkId(CustomLinkIDs[i]);
#endif

			PointNormals[i] = Normals[i];
			PointExpenses[i] = Expenses[i];
		}

		PathPointsBox = FBox(Points);

		SetTimeStamp(FDateTime::UtcNow().ToUnixTimestamp());
		MarkReady();
	}

	/** get based position of path point */
	FBasedPosition GetPathPointLocation(uint32 Index) const;

	virtual bool DoesIntersectBoxFast(const FBox& Box) const
	{
		return IsValid() && Box.Intersect(PathPointsBox);
	}

	/** checks if given path, starting from StartingIndex, intersects with given AABB box */
	virtual bool DoesIntersectBox(const FBox& Box, uint32 StartingIndex = 0, int32* IntersectingSegmentIndex = NULL, FVector* AgentExtent = NULL) const;
	/** checks if given path, starting from StartingIndex, intersects with given AABB box. This version uses AgentLocation as beginning of the path
	 *	with segment between AgentLocation and path's StartingIndex-th node treated as first path segment to check */
	virtual bool DoesIntersectBox(const FBox& Box, const FVector& AgentLocation, uint32 StartingIndex = 0, int32* IntersectingSegmentIndex = NULL, FVector* AgentExtent = NULL) const;
	/** retrieves normalized direction vector to given path segment
	 *	for '0'-th segment returns same as for 1st segment
	 */
	virtual FVector GetSegmentDirection(uint32 SegmentEndIndex) const;

	void GetRawPathData(bool& Success, TArray<FSurfaceNavPathPointData>& PointData) const;

	void GetRawPathPointData(int Index, bool& Success, FSurfaceNavPathPointData& PointData) const;

private:
	bool DoesPathIntersectBoxImplementation(const FBox& Box, const FVector& StartLocation, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const;

	/** reset variables describing built path, leaves setup variables required for rebuilding */
	void InternalResetNavigationPath();

public:

	/** type safe casts */
	template<typename T>
	FORCEINLINE const T* CastPath() const
	{
		return PathType.IsA(T::Type) ? static_cast<const T*>(this) : NULL;
		//return this->IsA<T>() ? static_cast<const T*>(this) : NULL;
	}

	template<typename T>
	FORCEINLINE T* CastPath()
	{
		return PathType.IsA(T::Type) ? static_cast<T*>(this) : NULL;
		//return this->IsA<T>() ? static_cast<T*>(this) : NULL;
	}

	/** enables path observing specified AActor's location and update itself if actor changes location */
	void SetGoalActorObservation(const AActor& ActorToObserve, float TetherDistance);
	/** Modifies distance to the GoalActor at which we'll update the path */
	void SetGoalActorTetherDistance(const float NewTetherDistace) { GoalActorLocationTetherDistanceSq = FMath::Square(NewTetherDistace); }
	/** turns goal actor location's observation */
	void DisableGoalActorObservation();
	/** set's up the path to use SourceActor's location in case of recalculation */
	void SetSourceActor(const AActor& InSourceActor);

	const AActor* GetSourceActor() const { return SourceActor.Get(); }
	const INavAgentInterface* GetSourceActorAsNavAgent() const { return SourceActorAsNavAgent; }

	FVector GetLastRepathGoalLocation() const { return GoalActorLastLocation; }
	void UpdateLastRepathGoalLocation();

	int64 GetLastUpdateTime() const { return LastUpdateTimeStamp; }
	float GetGoalActorTetherDistance() const { return FMath::Sqrt(GoalActorLocationTetherDistanceSq); }

	/** if enabled path will request recalculation if it gets invalidated due to a change to underlying navigation */
	void EnableRecalculationOnInvalidation(bool bShouldAutoUpdate)
	{
		bDoAutoUpdateOnInvalidation = bShouldAutoUpdate;
	}
	bool WillRecalculateOnInvalidation() const
	{
		return bDoAutoUpdateOnInvalidation;
	}

	/** if ignoring, path will stay bUpToDate after being invalidated due to a change to underlying navigation (observer and auto repath will NOT be triggered!) */
	void SetIgnoreInvalidation(bool bShouldIgnore) { bIgnoreInvalidation = bShouldIgnore; }
	bool GetIgnoreInvalidation() const { return bIgnoreInvalidation; }

	EPathObservationResult::Type TickPathObservation();

	/** If GoalActor is set it retrieved its navigation location, if not retrieved last path point location */
	FVector GetGoalLocation() const;
	void GetGoalLocations(FVector& Loc, FVector& OffsetLoc) const;

	/** retrieved location to start path finding from (in case of path recalculation) */
	void GetPathFindingStartLocations(FVector& Loc, FVector& OffsetLoc) const;

	const AActor* GetGoalActor() const
	{
		return bObservingGoalActor ? GoalActor.Get() : nullptr;
	}
	const INavAgentInterface* GetGoalActorAsNavAgent() const
	{
		return GoalActor.IsValid() ? GoalActorAsNavAgent : NULL;
	}

	// @todo this is navigation-type specific and should not be implemented here.
	/** additional node refs used during path following shortcuts */
	/*TArray<NavNodeRef> ShortcutNodeRefs;*/

protected:

	

	/** optional notify called when path finishes update, before broadcasting to observes - requires bUseOnPathUpdatedNotify flag set */
	virtual void OnPathUpdated(ENavPathUpdateType::Type UpdateType) {};

	/**
	* IMPORTANT: path is assumed to be valid if it contains _MORE_ than _ONE_ point
	*	point 0 is path's starting point - if it's the only point on the path then there's no path per se
	*/
	TArray<FNavPathPoint> PathPoints;
	TArray<float> PointExpenses;
	TArray<FVector> PointNormals;

	/** base actor, if exist path points locations will be relative to it */
	TWeakObjectPtr<AActor> Base;

private:
	/** if set path will observe GoalActor's location and update itself if goal moves more then
	*	@note only actual navigation paths can use this feature, meaning the ones associated with
	*	a NavigationData instance (meaning NavigationDataUsed != NULL) */
	TWeakObjectPtr<const AActor> GoalActor;

	/** cached result of GoalActor casting to INavAgentInterface */
	const INavAgentInterface* GoalActorAsNavAgent;

	/** if set will be queried for location in case of path's recalculation */
	TWeakObjectPtr<const AActor> SourceActor;

	/** cached result of PathSource casting to INavAgentInterface */
	const INavAgentInterface* SourceActorAsNavAgent;

protected:
	// DEPRECATED: filter used to build this path
	//FSharedConstNavQueryFilter Filter;

	/** type of path */
	static const FNavPathType Type;

	FNavPathType PathType;

	FBox PathPointsBox;

	///** type of path */
	//static const FNavPathType SurfaceNavType;

	//FNavPathType SurfaceNavPathType;

	/** A delegate that will be called when path becomes invalid */
	FSurfaceNavPathObserverDelegate ObserverDelegate;

	/** "true" until navigation data used to generate this path has been changed/invalidated */
	uint32 bUpToDate : 1;

	/** when false it means path instance has been created, but not filled with data yet */
	uint32 bIsReady : 1;

	/** "true" when path is only partially generated, when goal is unreachable and path represent best guess */
	uint32 bIsPartial : 1;

	/** set to true when path finding algorithm reached a technical limit (like limit of A* nodes).
	*	This generally means path cannot be trusted to lead to requested destination
	*	although it might lead closer to destination. */
	uint32 bReachedSearchLimit : 1;

	/** if true path will request re-pathing if it gets invalidated due to underlying navigation changed */
	uint32 bDoAutoUpdateOnInvalidation : 1;

	/** if true path will keep bUpToDate value after getting invalidated due to underlying navigation changed
	 *  (observer and auto repath will NOT be triggered!)
	 *  it's NOT safe to use if path relies on navigation data references (e.g. poly corridor)
	 */
	uint32 bIgnoreInvalidation : 1;

	/** if true path will use GetPathFindingStartLocation() for updating QueryData before repath */
	uint32 bUpdateStartPointOnRepath : 1;

	/** if true path will use GetGoalLocation() for updating QueryData before repath */
	uint32 bUpdateEndPointOnRepath : 1;

	/** set when path is waiting for recalc from navigation data */
	uint32 bWaitingForRepath : 1;

	/** if true path will call OnPathUpdated notify */
	uint32 bUseOnPathUpdatedNotify : 1;

	/** indicates whether at any point GoalActor was a valid Actor. Used as
	 *	an optimization in FNavigationPath::TickPathObservation */
	uint32 bObservingGoalActor : 1;

	/** navigation data used to generate this path */
	//TWeakObjectPtr<ANavigationData> NavigationDataUsed;

	/** essential part of query used to generate this path */
	FSurfaceNavPathFindingQueryData PathFindingQueryData;
	//FPathFindingQueryData PathFindingQueryData;

	/** gets set during path creation and on subsequent path's updates */
	int64 LastUpdateTimeStamp;

private:
	/* if GoalActor is set this is the distance we'll try to keep GoalActor from end of path. If GoalActor
	* moves more then this from the end of the path we'll recalculate the path */
	float GoalActorLocationTetherDistanceSq;

	/** last location of goal actor that was used for repaths to prevent spamming when path is partial */
	FVector GoalActorLastLocation;
};





struct SURFACENAVPLUGIN_API FSurfaceNavPathRecalculationRequest
{
	FSurfaceNavPathWeakPtr Path;
	ENavPathUpdateType::Type Reason;

	FSurfaceNavPathRecalculationRequest() {}

	FSurfaceNavPathRecalculationRequest(const FSurfaceNavPathSharedPtr& InPath, ENavPathUpdateType::Type InReason)
		: Path(InPath.ToSharedRef()), Reason(InReason)
	{}

	bool operator==(const FSurfaceNavPathRecalculationRequest& Other) const { return Path == Other.Path; }
};



