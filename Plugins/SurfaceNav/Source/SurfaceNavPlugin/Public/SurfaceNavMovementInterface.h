// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "Templates/SubclassOf.h"

#include "SurfaceNavMovementInterface.generated.h"


//DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnPawnNeedsRotationUpdatedSignature, USurfaceNavMovementComponent, OnPawnNeedsRotationUpdated, FVector, TargetLocation, FVector, TargetUpVector, FVector, TargetForwardVector);
//DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnNeedsPathMoveRotationSignature, USurfaceNavMovementComponent, OnNeedsPathMoveRotation, FVector, TargetLocation, FVector, TargetUpVector, FVector, TargetForwardVector);

//UDELEGATE(BlueprintAuthorityOnly)
//DECLARE_DELEGATE(FSurfaceNavMoveCompOnDetermineRotationSignature); //(USurfaceNavMovementComponent, OnDetermineRotation, FVector)


/** Surface Nav Movement modes for Characters. */
UENUM(BlueprintType)
enum ESurfaceNavMovementMode
{
	/***********************************************/
	/***********************************************/
	/* Begin Custom Movement Modes */
	/*---------------------------------------------*/

	/** Character forward vector will face the surface. */
	MOVE_SN_Walking 	UMETA(DisplayName = "Moving Along Ground"),

	/** Character down vector will face the surface. */
	MOVE_SN_Climbing 	UMETA(DisplayName = "Climbing"),

	/** Character up vector will face the surface. */
	MOVE_SN_Hanging 	UMETA(DisplayName = "Hanging"),

	/*---------------------------------------------*/
	/* End Custom Movement Modes */
	/***********************************************/
	/***********************************************/
};

///** Surface Nav Movement orientations for Characters. */
//UENUM(BlueprintType)
//enum ESurfaceNavOrientation
//{
//	/***********************************************/
//	/***********************************************/
//	/* Begin Custom Movement Modes */
//	/*---------------------------------------------*/
//
//    /** Character forward vector will face the surface. */
//	ORIENTATION_SN_ForwardVectorFacingSurface 	UMETA(DisplayName = "Forward Vector Facing Surface"),
//
//	/** Character down vector will face the surface. */
//	ORIENTATION_SN_DownVectorFacingSurface 	UMETA(DisplayName = "Down Vector Facing Surface"),
//
//	/** Character up vector will face the surface. */
//	ORIENTATION_SN_UpVectorFacingSurface 	UMETA(DisplayName = "Up Vector Facing Surface"),
//
//	/*---------------------------------------------*/
//	/* End Custom Movement Modes */
//	/***********************************************/
//	/***********************************************/
//};


UINTERFACE(MinimalAPI, NotBlueprintable)
class USurfaceNavMovementInterface : public UInterface
{
	GENERATED_BODY()

};

class SURFACENAVPLUGIN_API ISurfaceNavMovementInterface
{
    GENERATED_BODY()

public:

	//virtual float GetPawnMovementOffset() const { return 0.f; }
	virtual float GetMinAngleFromGround() const { return 0.f; }
	virtual float GetMaxAngleFromGround() const { return 0.f; }

	virtual FVector GetActorSurfaceLocation() const { return FVector::ZeroVector; }

	virtual float GetPathPointAcceptanceRadius() const { return 0.f; }
	virtual float GetNavLinkPointAcceptanceRadius() const { return 0.f; }
	virtual void HandleRotation(FVector TargetLocation, FVector TargetUpVector, FVector TargetFowardVector) { }
	//virtual bool GetEnableTraceForRotation() const { return false; }

	
	virtual int GetMaxPathingIterations() const { return 0; }

	//virtual float GetCornerBoostTickInterval() const { return 0.f; }
	//virtual float GetCornerBoostTickDelta() const { return 0.f; }
	//virtual FVector GetLastCornerBoost() const { return FVector::ZeroVector; }
	//virtual void SetLastCornerBoost(FVector LastCornerAdj) { }
	//virtual float IncrementCornerBoostTickDelta() { return 0.f; }
	//virtual void ResetCornerBoostCache() { };
	//virtual bool HasCachedCornerBoost() const { return false; };

	/*virtual bool GetEnableCornerTraversalAdjustments() const { return false; }
	virtual float GetCornerTraversalBoost() const { return 0.f; }	
	virtual float GetCornerTraversalTickInterval() const { return 0.f; }
	virtual float GetCornerTraversalTickDelta() const { return 0.f; }
	virtual FVector GetLastCornerTraversalAdjustment() const { return FVector::ZeroVector; }
	virtual void SetLastCornerTraversalAdjustment(FVector LastCornerAdj) { }
	virtual float IncrementCornerTraversalTickDelta() { return 0.f; }
	virtual void ResetCornerAdjustmentCache() { };
	virtual bool HasCachedCornerAdjustment() const { return false; };*/



	virtual TSubclassOf<class USurfaceNavPawnRotationMode> GetRotationMode() const { return NULL; }
	virtual float GetRotationModeTickInterval() const { return 0.f; }
	virtual float GetRotationModeTickDelta() const { return 0.f; }
	virtual FRotator GetCachedRotation() const { return FRotator::ZeroRotator; }
	virtual FVector GetCachedRotationUpVector() const { return FVector::ZeroVector; }
	virtual void SetLastRotationFound(FRotator LastRotation, FVector LastUpVector) { }
	virtual float IncrementRotationModeTickDelta() { return 0.f; }
	virtual float GetRotationMode1stTraceAngle() const { return 0.f; }
	virtual float GetRotationMode1stTraceDistance() const { return 0.f; }
	virtual void ResetRotationCache() { };
	virtual bool HasCachedRotation() const { return false; };
	virtual bool RotationModeTraceShouldScale() const { return false; };
	
	
	virtual bool GetEnableCornerDeceleration() const { return false; }
	virtual float GetCornerDecelerationThreshold() const { return 0.f; }
	virtual float GetCornerDecelerationMinSpeedPercent() const { return 0.f; }
	virtual float GetCornerDecelerationMinSpeedSq() const { return 0.f; }
	virtual float GetCornerDecelerationTestDistance() const { return 0.f; }
	virtual float GetCornerDecelerationBrakingMultiplier() const { return 0.f; }
	virtual int GetCornerDecelerationTestNavPointCount() const { return 0.f; }

	virtual bool GetEnableOutsideCornerAssist() const { return false; }
	virtual float GetOutsideCornerAssistThreshold() const { return 0.f; }
	virtual float GetOutsideCornerAcceptanceDelta() const { return 0.f; }
	virtual float GetOutsideCornerAcceptanceDistance() const { return 0.f; }
	virtual bool GetShowOutsideCornerAssistDebugData() const { return false; }
	virtual float GetShowOutsideCornerAssistDebugTime() const { return 0.f; }
	virtual bool GetEnableCornerBoost() const { return false; }
	virtual float GetCornerBoostMultiplier() const { return 0.f; }

	virtual bool GetEnableInsideCornerAssist() const { return false; }
	virtual float GetInsideCornerAssistThreshold() const { return 0.f; }
	virtual float GetInsideCornerAcceptanceDelta() const { return 0.f; }
	virtual float GetInsideCornerAcceptanceDistance() const { return 0.f; }
	virtual bool GetShowInsideCornerAssistDebugData() const { return false; }
	virtual float GetShowInsideCornerAssistDebugTime() const { return 0.f; }


#if false
	virtual bool IsClimbing() const { return false;	}
	virtual bool IsHanging() const { return false; }

	virtual bool CanClimb() const { return false; }
	virtual bool CanHang() const { return false; }

	virtual float GetStartClimbingAngle() const { return 45.0f; }
	virtual float GetStartHangingAngle() const { return 135.0f; }

	virtual ESurfaceNavOrientation GetClimbingOrientation() const { return ESurfaceNavOrientation::ORIENTATION_SN_ForwardVectorFacingSurface; }
	virtual ESurfaceNavOrientation GetHangingOrientation() const { return ESurfaceNavOrientation::ORIENTATION_SN_ForwardVectorFacingSurface; }
#endif
};

