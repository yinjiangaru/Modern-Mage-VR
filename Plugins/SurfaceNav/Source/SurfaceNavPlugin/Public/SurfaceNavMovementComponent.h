// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/World.h"

#include "SurfaceNavCommon.h"
#include "SurfaceNavMovementInterface.h"
#include "SurfaceNavPawnRotationMode.h"
#include "Components/SphereComponent.h"
#include "SurfaceNavPathingTypes.h"
#include "SurfaceNavMovementComponent.generated.h"

UENUM(BlueprintType)
enum ESurfaceNavMovementAxisOffsetMode
{
	/** Surface Nav movement used when character is navigating on a wall defined by MinClimbingAngle */
	OFFSET_FROM_BOUNDS		UMETA(DisplayName = "Offset from Bounds"),
	OFFSET_FROM_CENTER		UMETA(DisplayName = "Offset from Center")
};


DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnNeedsPathMoveRotationSignature, USurfaceNavMovementComponent, OnNeedsPathMoveRotation, FVector, TargetLocation, FVector, TargetUpVector, FVector, TargetForwardVector);

/** This component is a wrapper around UE’s native Floating Pawn Movement Component and adds needed logic to work with the DSN plugin. */
UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class SURFACENAVPLUGIN_API USurfaceNavMovementComponent : public UFloatingPawnMovement, public ISurfaceNavMovementInterface
{
	GENERATED_UCLASS_BODY()


	//Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Begins Play for the component.
	 * Called when the owning Actor begins play or when the component is created if the Actor has already begun play.
	 * Actor BeginPlay normally happens right after PostInitializeComponents but can be delayed for networked or child actors.
	 * Requires component to be registered and initialized.
	 */
	virtual void BeginPlay() override;

public:

	// Needed to attach other component
	virtual void OnRegister() override;

	/** Returns braking distance for acceleration driven path following */
	virtual float GetPathFollowingBrakingDistance(float CurrentMaxSpeed) const;

	/** Different pawns will require unique rotation mechanics so this event allows you to define what the target rotation
	should be based on your needs. You will want to call the SetNextRotator function here if you’re relying on the DSN
	plugin to apply the rotation to the pawn. */
	UPROPERTY(BlueprintAssignable, Category = "Surface Nav|Pawn Movement")
	FOnNeedsPathMoveRotationSignature OnNeedsPathMoveRotation;

	/** Sets the next rotator to be used by the DSN plugin’s default rotation lerp. Should be called during the
	OnNeedsPathMoveRotation event. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|Pawn Movement")
	void SetNextRotator(FRotator Rot) { NextRotator = Rot; };

	/** Skips applying a rotation lerp for a single tick. Useful if there's something special you
	need your character to do for a few frames. If you plan on handling rotations yourself on every tick,
	then use DisableDefaultRotationLerp instead. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|Pawn Movement")
	void SkipNextRotationLerp() { bSkipNextRotationLerp = true; };

	/** Disables the default rotation lerp handling. This is useful if you know you're
	going to always handle rotations and don't want to call SkipNextRotationLerp on
	every tick. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|Pawn Movement")
	void DisableDefaultRotationLerp() { bDisableDefaultRotation = true; };

	/** Enables the default rotation lerp handling. This is enabled by default, so calling
	this is only required if it's been disabled and you'd like to re-enable it. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|Pawn Movement")
	void EnableDefaultRotationLerp() { bDisableDefaultRotation = false; };

	/** Skips processing the next movement input vector. This may seem similar to consume movement
	input, but they are a bit different. Use this function to prevent the current movement input vector as
	well as any additional movement input from being processed up to and including move requests such
	as RequestPathMove or RequestDirectMove which are both used by the path following component. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|Pawn Movement")
	void SkipNextMovementInput() { bSkipNextMovementInput = true; };


	/** Affects how fast your pawn will rotate to the target rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Pawn Movement", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float RotationSpeed = 3.f;
	

	/** Min angle relative to ground normal this pawn is allowed to navigate on with 0 degrees
	being the ground below the pawn and 180 degrees being the ceiling. Must be less than MaxAngleFromGround. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Pawn Movement",
		meta = (ClampMin = "0.0", ClampMax = "175.0", UIMin = "0.0", UIMax = "175.0"))
	float MinAngleFromGround = 0.f;

	/** Max angle from ground this pawn is allowed to navigate on with 0 degrees
	being the ground below the pawn and 180 degrees being the ceiling. Must be greater
	than MinAngleFromGround. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Pawn Movement",
		meta = (ClampMin = "5.0", ClampMax = "180.0", UIMin = "5.0", UIMax = "180.0"))
	float MaxAngleFromGround = 180.f;


	/** path following: request new velocity */
	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;

	/** path following: request new move input (normal vector = full strength) */
	virtual void RequestPathMove(const FVector& MoveInput) override;

	/** Gets the location of the actor after applying PawnMovementOffset. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Surface Nav|Pawn Movement")
	FVector GetActorLocationWithOffset() const
	{
		return GetActorSurfaceLocation();
	}

	/********************************************************************/
	/********************************************************************/
	/* Begin ISurfaceNavMovementInterface */
	/*------------------------------------------------------------------*/

	inline virtual float GetPathPointAcceptanceRadius() const override { return GetOwner()->GetActorScale().X * PathPointAcceptanceRadius; }
	inline virtual float GetNavLinkPointAcceptanceRadius() const override { return GetOwner()->GetActorScale().X * NavLinkPointAcceptanceRadius; }

	inline virtual float GetMinAngleFromGround() const override { return MinAngleFromGround; }
	inline virtual float GetMaxAngleFromGround() const override { return MaxAngleFromGround; }

	virtual FVector GetActorSurfaceLocation() const override;

	virtual void HandleRotation(FVector TargetLocation, FVector TargetUpVector, FVector TargetFowardVector) override; // , bool ForceDefault = false);

	virtual bool GetEnableCornerBoost() const override { return EnableCornerBoost; }
	virtual float GetCornerBoostMultiplier() const override { return CornerBoostMultiplier; }
	virtual int GetMaxPathingIterations() const override { return MaxPathingIterations; }


	virtual TSubclassOf<class USurfaceNavPawnRotationMode> GetRotationMode() const override { return RotationMode; }
	virtual float GetRotationModeTickInterval() const override { return RotationModeTickInterval; }
	virtual float GetRotationModeTickDelta() const override { return RotationModeTickDelta; }
	virtual FRotator GetCachedRotation() const override { return CachedRotation; }
	virtual FVector GetCachedRotationUpVector() const override { return CachedRotationUpVector; }
	virtual void ResetRotationCache() override { bHasCachedRotation = false; };
	virtual bool HasCachedRotation() const override { return bHasCachedRotation; };
	virtual bool RotationModeTraceShouldScale() const override { return bRotationModeTraceShouldScale; };
	virtual void SetLastRotationFound(FRotator LastRotation, FVector LastUpVector) override
	{
		bHasCachedRotation = true;
		CachedRotation = LastRotation;
		CachedRotationUpVector = LastUpVector;
	}
	virtual float IncrementRotationModeTickDelta() override
	{
		UWorld* MyWorld = GetWorld();
		if (MyWorld)
		{
			RotationModeTickDelta += MyWorld->GetDeltaSeconds();
			if (RotationModeTickDelta >= RotationModeTickInterval)
				RotationModeTickDelta = 0.f;
		}
		return RotationModeTickDelta;
	}
	virtual float GetRotationMode1stTraceAngle() const override { return RotationMode1stTraceAngle; }
	virtual float GetRotationMode1stTraceDistance() const override { return RotationMode1stTraceDistance; }


	virtual bool GetEnableCornerDeceleration() const override { return EnableCornerDeceleration; }
	virtual float GetCornerDecelerationThreshold() const override { return CornerDecelerationThreshold; }
	virtual float GetCornerDecelerationMinSpeedPercent() const override { return CornerDecelerationMinSpeedPercent; }
	virtual float GetCornerDecelerationMinSpeedSq() const override { return FMath::Square(CornerDecelerationMinSpeedPercent * GetMaxSpeed()); }
	virtual float GetCornerDecelerationTestDistance() const override { return CornerDecelerationTestDistance; }
	virtual int GetCornerDecelerationTestNavPointCount() const override { return FMath::Max(FMath::CeilToInt(CornerDecelerationTestDistance / SurfaceNavProxySettings::GridPointDistance), 1); }
	virtual float GetCornerDecelerationBrakingMultiplier() const { return CornerDecelerationBrakingMultiplier; }

	virtual bool GetEnableOutsideCornerAssist() const override { return EnableOutsideCornerAssist; }
	virtual float GetOutsideCornerAssistThreshold() const override { return OutsideCornerAssistThreshold; }
	virtual float GetOutsideCornerAcceptanceDelta() const override { return 1.f - OutsideCornerAcceptanceAngle * ONE_DIV_90; }
	virtual float GetOutsideCornerAcceptanceDistance() const override { return FMath::Square(OutsideCornerAcceptanceDistance); }
	virtual bool GetShowOutsideCornerAssistDebugData() const override { return bShowOutsideCornerAssistDebugData; }
	virtual float GetShowOutsideCornerAssistDebugTime() const override { return ShowOutsideCornerAssistDebugTime; }


	virtual bool GetEnableInsideCornerAssist() const override { return EnableInsideCornerAssist; }
	virtual float GetInsideCornerAssistThreshold() const override { return InsideCornerAssistThreshold; }
	virtual float GetInsideCornerAcceptanceDelta() const override { return 1.f - InsideCornerAcceptanceAngle * ONE_DIV_90; }
	virtual float GetInsideCornerAcceptanceDistance() const override { return FMath::Square(InsideCornerAcceptanceDistance); }
	virtual bool GetShowInsideCornerAssistDebugData() const override { return bShowInsideCornerAssistDebugData; }
	virtual float GetShowInsideCornerAssistDebugTime() const override { return ShowInsideCornerAssistDebugTime; }


	/*------------------------------------------------------------------*/
	/* End ISurfaceNavMovementInterface */
	/********************************************************************/
	/********************************************************************/



#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/** Use this to apply an offset from the surface the pawn is currently navigating
	over. If the offset isn't high enough then the collision may drag when going over
	edges so you may need to adjust position of the collision or increase the offset.
	Use the movement offset modes for each axis to control how these offset values are
	applied. You typically want at least 1 axis offset from the pawn's bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Pawn Movement")
	FVector PawnMovementOffset = FVector(0.f, 0.f, -30.f);

	/** Should the movement offset for this axis be applied from the bounds or from 
	the center position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Pawn Movement")
	TEnumAsByte<enum ESurfaceNavMovementAxisOffsetMode> MovementOffsetModeForX = ESurfaceNavMovementAxisOffsetMode::OFFSET_FROM_BOUNDS;

	/** Should the movement offset for this axis be applied from the bounds or from
	the center position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Pawn Movement")
	TEnumAsByte<enum ESurfaceNavMovementAxisOffsetMode> MovementOffsetModeForY = ESurfaceNavMovementAxisOffsetMode::OFFSET_FROM_BOUNDS;

	/** Should the movement offset for this axis be applied from the bounds or from
	the center position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Pawn Movement")
	TEnumAsByte<enum ESurfaceNavMovementAxisOffsetMode> MovementOffsetModeForZ = ESurfaceNavMovementAxisOffsetMode::OFFSET_FROM_BOUNDS;

	/** Acceptance radius used for points while path following. If this is too small then
	the pawn could end up rapidly jerking as it moves from point to point. If this is too
	large, then you risk the pawn accepting a path point on the other side of a wall
	in specific scenarios. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following", meta = (ClampMin = "5", UIMin = "5"))
	float PathPointAcceptanceRadius = 100;

	/** Acceptance radius used for nav link points while path following. Make sure this is
	not too large so the pawn is close enough to perform the action as expected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following", meta = (ClampMin = "5", UIMin = "5"))
	float NavLinkPointAcceptanceRadius = 100;

	/** Allows providing expense multipliers to apply to the pathfinding algorithm based on angle ranges from 0 to 180. The angles
	provided should be relative to the ground normal so an angle of 0 would be ground. An angle of 90 relative to ground would be
	like a wall. And an angle of 180 relative to ground would be like the ceiling. It may make sense for climbing a wall or hanging 
	on a ceiling to be more costly than walking on the ground for your pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following")
	TArray<FPawnAngleExpenseMultiplier> PawnAngleExpenseMultipliers;

	/** This setting limits the max number of iterations that path finding can perform before stopping for this
	pawn only. The higher this setting the more CPU usage you will see. If the setting is too low it could
	result in an incorrect partial path to the target. Leave at zero to use max iterations from the SurfaceNavProxy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following", meta = (ClampMin = "0", UIMin = "0", ClampMax = "50000", UIMax = "50000"))
	int MaxPathingIterations = 0;	

	/** Enable outside corner assist when criteria is met. Corner assist can improve how your pawn goes over/around
	a corner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Outside Corners")
	bool EnableOutsideCornerAssist = false;

	/** This parameter relates to the dot product of the normals for the previous nav point and the target nav point. If the dot
	product of those 2 normals is less than this parameter (further from the same direction) then corner assist will apply. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Outside Corners", meta = (ClampMin = "0.5", UIMin = "0.5", ClampMax = "0.95", UIMax = "0.95", EditCondition = "EnableOutsideCornerAssist"))
	float OutsideCornerAssistThreshold = 0.8f;

	/** This is the angle between the nav point's normal and the vector from the nav point to the pawn. Once this angle is reached
	then we consider the pawn to be around/over the corner and can move on to the next nav point. Typically around 90 degrees
	or a bit lower is good, but you can make it smaller if your pawn needs more room to be over the corner. Making this too high
	could cause path following to move to the next point too soon which could cause problems as the pawn tries to traverse the corner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Outside Corners", meta = (ClampMin = "45", UIMin = "45", ClampMax = "135", UIMax = "135", EditCondition = "EnableOutsideCornerAssist"))
	float OutsideCornerAcceptanceAngle = 90.f;

	/** This value represents another way we accept the corner nav point as accepted to move on to the next. If the pawn's offset
	location (using PawnMovementOffset)	is within this distance to the corner nav point then consider the pawn around/over the
	corner and move on to the next nav point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Outside Corners", meta = (ClampMin = "2", UIMin = "2", ClampMax = "50", UIMax = "50", EditCondition = "EnableOutsideCornerAssist"))
	float OutsideCornerAcceptanceDistance = 5.f;

	/** Enables corner boost which gives the pawn an extra boost when corner assist criteria is met. Without this, the pawn may
	end up slowly dragging itself over some corners. This is probably not needed at faster speeds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Outside Corners|Corner Boost", meta = (EditCondition = "EnableOutsideCornerAssist"))
	bool EnableCornerBoost = false;

	/** This is the boost multiplier to use when corner boost is applied. You'll need to do some testing to find the right value that
	works for a particular pawn at its velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Outside Corners|Corner Boost", meta = (ClampMin = "0.1", UIMin = "0.1", ClampMax = "10", UIMax = "10", EditCondition = "EnableOutsideCornerAssist && EnableCornerBoost"))
	float CornerBoostMultiplier = 3.f;

	/** For the path following segement which triggered the corner assist it draws a green debug point at the start point for the
	segment, a red debug point at the end point for the segment and a blue line from the pawn's offset location to the end point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Outside Corners|Debug", meta = (EditCondition = "EnableOutsideCornerAssist"))
	bool bShowOutsideCornerAssistDebugData = false;

	/** How long in seconds should the debug data be shown for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Outside Corners|Debug", meta = (EditCondition = "EnableOutsideCornerAssist && bShowOutsideCornerAssistDebugData"))
	float ShowOutsideCornerAssistDebugTime = 5.f;

	/** Enable inside corner assist when criteria is met. This can help your pawn stay closer to the surface as it navigates
	inside corners. You'll want to test various corners with your below parameters so you don't
	cause your pawn to get stuck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Inside Corners")
	bool EnableInsideCornerAssist = false;

	/** This parameter relates to the dot product of the normals for the previous nav point and the targer nav point. If the dot
	product of those 2 normals is less than this parameter (further from the same direction) then corner assist will apply. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Inside Corners", meta = (ClampMin = "0.2", UIMin = "0.2", ClampMax = "0.95", UIMax = "0.95", EditCondition = "EnableInsideCornerAssist"))
	float InsideCornerAssistThreshold = 0.8f;

	/** This is the angle between the nav point's normal and the vector from the nav point to the pawn. Once this angle is reached
	then we accept the corner nav point and can move on to the next nav point. The larger this value, the closer the pawn
	needs to be to the inside corner for the nav point to be accepted. Making this too large may result in the pawn getting hung up
	as it may not be able to get close enough to accept the nav point. Making it too low could result in the pawn moving on to the
	next nav point too soon which could make the pawn appear to float over the inside corner a bit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Inside Corners", meta = (ClampMin = "30", UIMin = "30", ClampMax = "90", UIMax = "90", EditCondition = "EnableInsideCornerAssist"))
	float InsideCornerAcceptanceAngle = 60.f;

	/** This value represents another way we accept the corner nav point as accepted to move on to the next. If the pawn's offset
	location (using PawnMovementOffset)	is within this distance to the corner nav point then accept this nav point and move on
	to the next nav point. Larger pawns will require larger values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Inside Corners", meta = (ClampMin = "5", UIMin = "5", ClampMax = "5000", UIMax = "5000", EditCondition = "EnableInsideCornerAssist"))
	float InsideCornerAcceptanceDistance = 60.f;

	/** For the path following segment which triggered the corner assist it draws a green debug point at the start point for the
	segment, a red debug point at the end point for the segment and a blue line from the pawn's offset location to the end point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Inside Corners|Debug", meta = (EditCondition = "EnableInsideCornerAssist"))
	bool bShowInsideCornerAssistDebugData = false;

	/** How long in seconds should the debug data be shown for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Inside Corners|Debug", meta = (EditCondition = "EnableInsideCornerAssist && bShowInsideCornerAssistDebugData"))
	float ShowInsideCornerAssistDebugTime = 5.f;

	/** Enable corner deceleration when certain criteria are met. This will cause the pawn to slow down when approaching
	a corner. The final amount of deceleration is dynamically calculated based on the provided values. This will be more
	useful for faster moving pawns and may not give much benefit to slower pawns. You may also have better results when
	used with Outside Corner Assist and Inside Corner Assist. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Corner Deceleration")
	bool EnableCornerDeceleration = false;

	/** The minimum speed, as a percentage of the set max speed, that the pawn will be decelerated to when approaching a corner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Corner Deceleration", meta = (ClampMin = "0.1", UIMin = "0.1", ClampMax = "0.95", UIMax = "0.95", EditCondition = "EnableCornerDeceleration"))
	float CornerDecelerationMinSpeedPercent = 0.5f;

	/** This is the dot product value that is used as the threshold for whether dynamic deceleration is applied. The vectors
	used for the dot product are the pawn's current forward vector and the vector of the path segment being tested. If a test
	results in a value less than this value, then corner deceleration will be applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Corner Deceleration", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "0.8", UIMax = "0.8", EditCondition = "EnableCornerDeceleration"))
	float CornerDecelerationThreshold = 0.6f;

	/** This is used to approximate the number of nav points for a path to look ahead and test. You may want this larger for fast
	pawns, but making this too large could start hurting performance as it'll need to do tests further out as the pawn
	moves along the path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Corner Deceleration", meta = (ClampMin = "50.0", UIMin = "50.0", ClampMax = "5000.0", UIMax = "5000.0", EditCondition = "EnableCornerDeceleration"))
	float CornerDecelerationTestDistance = 300.f;

	/** Use this multiplier to adjust how much the pawn will decelerate when approaching a corner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Corners|Corner Deceleration", meta = (ClampMin = "0.5", UIMin = "0.5", ClampMax = "5.0", UIMax = "5.0", EditCondition = "EnableCornerDeceleration"))
	float CornerDecelerationBrakingMultiplier = 1.f;

	/** Determines which rotation logic to use. The default is the cheapest, but also will be the least accurate. You will
	almost always get better results by using the OnNeedsPathMoveRotation event and setting a custom rotation, but one of
	the modes here may be sufficient for your needs. If you do use the OnNeedsPathMoveRotation to set a custom rotation,
	then leave this set to it's default as some of the modes use traces so will add unnecessary overhead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Experimental|Rotation Handling")
	TSubclassOf<class USurfaceNavPawnRotationMode> RotationMode = USurfaceNavDefaultRotationMode::StaticClass();

	UPROPERTY()
	int8 RotationModeValue = 0;

	/** How often it runs the selected rotation mode logic (0 = every frame). It will cache the rotation to reuse until the
	next tick interval is reached. This can improve performance by not doing traces every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Experimental|Rotation Handling", meta = (EditCondition = "RotationModeValue > 0"))
	float RotationModeTickInterval = 0.1f;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Experimental|Rotation Handling", meta = (ClampMin = "10", UIMin = "10", ClampMax = "2000", UIMax = "2000"))
	bool bRotationModeTraceShouldScale = false;

	/** This is for the first trace in a given direction. The angle to rotate the pawn's forward vector down by using the
	pawn's right vector as the rotation axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Experimental|Rotation Handling", meta = (ClampMin = "0", UIMin = "0", ClampMax = "90", UIMax = "90", EditCondition = "RotationModeValue > 0"))
	float RotationMode1stTraceAngle = 45.f;

	/** This is for the first trace in a given direction. The length of the trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Experimental|Rotation Handling", meta = (ClampMin = "10", UIMin = "10", ClampMax = "2000", UIMax = "2000", EditCondition = "RotationModeValue > 0"))
	float RotationMode1stTraceDistance = 200.f;

	/** How far out along the first trace to offset the start of the second trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Experimental|Rotation Handling", meta = (ClampMin = "10", UIMin = "10", ClampMax = "2000", UIMax = "2000", EditCondition = "RotationModeValue > 0"))
	float RotationMode2ndTraceStartOffset = 100.f;

	/** This is for the second trace in a given direction. The angle to rotate the pawn's forward vector down by using the
	pawn's right vector as the rotation axis. It will ultimately point down and back towards the pawn. Show the debug lines
	to make sure these second set of traces don't cross as that could cause strange rotation behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Experimental|Rotation Handling", meta = (ClampMin = "90", UIMin = "90", ClampMax = "180", UIMax = "180", EditCondition = "RotationModeValue > 0"))
	float RotationMode2ndTraceAngle = 140.f;

	/** This is for the second trace in a given direction. The length of the trace. It will ultimately point down and back towards the pawn. Show the debug lines
	to make sure these second set of traces don't cross as that could cause strange rotation behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Experimental|Rotation Handling", meta = (ClampMin = "10", UIMin = "10", ClampMax = "2000", UIMax = "2000", EditCondition = "RotationModeValue > 0"))
	float RotationMode2ndTraceDistance = 120.f;

	/** This is the angle that will be used to rotate the traces to the left/right when doing traces in 2 directions.
	It does the rotation around the pawn's up vector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Experimental|Rotation Handling", meta = (ClampMin = "10", UIMin = "10", ClampMax = "70", UIMax = "70", EditCondition = "RotationModeValue == 2"))
	float RotationModeLeftRightAngle = 45.f;

	/** Shows trace lines for the chosen rotation mode to help get an idea of placement. Also, when using a rotation mode
	which does traces in two direction it will also show a horizontal line representing the right vector between the hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Nav|Path Following|Experimental|Rotation Handling|Debug", meta = (EditCondition = "RotationModeValue > 0"))
	bool bRotationModeShouldDrawTraceLines = false;

protected:

	FRotator NextRotator;
	bool bSkipNextMovementInput = false;
	bool bSkipNextRotationLerp = false;
	bool bDisableDefaultRotation = false;

	float RotationModeTickDelta = 0.f;
	FRotator CachedRotation;
	FVector CachedRotationUpVector;
	bool bHasCachedRotation = false;


	FVector CachedCornerBoost;

};