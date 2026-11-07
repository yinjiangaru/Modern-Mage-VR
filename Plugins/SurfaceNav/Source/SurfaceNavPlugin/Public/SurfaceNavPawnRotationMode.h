// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "SurfaceNavPawnRotationMode.generated.h"

class USurfaceNavMovementComponent;

UCLASS(HideDropdown, NotPlaceable, Transient, ClassGroup = "Surface Nav Movement|Rotation")
class SURFACENAVPLUGIN_API USurfaceNavPawnRotationMode : public UObject
{
	GENERATED_BODY()

public:
	virtual FRotator DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation, FVector& TargetUpVector, FVector& TargetForwardVector) 
	{ return FRotationMatrix::MakeFromZX(TargetUpVector, TargetForwardVector).Rotator(); };

private:

	//UProperty()

};

/** This is just the default rotation which uses the normals of the nav points to determine rotation. This should
really only be used for prototyping. */
UCLASS(NotPlaceable, Transient, ClassGroup = "Surface Nav Movement|Rotation")
class SURFACENAVPLUGIN_API USurfaceNavDefaultRotationMode : public USurfaceNavPawnRotationMode
{
	GENERATED_BODY()

public:
	virtual FRotator DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation, FVector& TargetUpVector, FVector& TargetForwardVector) override;

private:

};

//UCLASS(ClassGroup = "Surface Nav Movement|Rotation")
//class SURFACENAVPLUGIN_API USurfaceNavTraceToPointRotationMode : public USurfaceNavPawnRotationMode
//{
//	GENERATED_BODY()
//
//public:
//	virtual FRotator DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation, FVector& TargetUpVector, FVector& TargetForwardVector) override;
//
//private:
//
//};

///** [1 Trace x 1 Direction (front) = Totals 1 Trace]
//Uses the RotationMode1stTraceAngle and RotationMode1stTraceDistance properties of the SurfaceNavMovementComponent
//to do a trace towards the pawn's forward-down direction. This mode is light weight with only 1 trace, but dosn't 
//offer the best rotation when going over ledges/corners. 
//or corners. Also, this may be fine for smaller pawns, but may not look as good with medium to larger pawns. */
//UCLASS(ClassGroup = "Surface Nav Movement|Rotation")
//class SURFACENAVPLUGIN_API USurfaceNav1x1TraceRotationMode : public USurfaceNavPawnRotationMode
//{
//	GENERATED_BODY()
//
//public:
//	virtual FRotator DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation, FVector& TargetUpVector, FVector& TargetForwardVector) override;
//
//private:
//
//};

/** [2 Traces x 1 Direction (forward) = Totals 2 Traces]
Uses the RotationMode1stTraceAngle and RotationMode1stTraceDistance properties of the SurfaceNavMovementComponent
to do a trace towards the pawn's forward-down direction. If that first trace has no hit, then it uses the 
RotationMode2ndTraceAngle and RotationMode2ndTraceDistance properties to do another trace starting out ahead of the pawn
going in the backward-down direction. This may be ok for small pawns, but may not look right for medium to large pawns. */
UCLASS(NotPlaceable, Transient, ClassGroup = "Surface Nav Movement|Rotation")
class SURFACENAVPLUGIN_API USurfaceNav2x1TraceRotationMode : public USurfaceNavPawnRotationMode
{
	GENERATED_BODY()

public:
	virtual FRotator DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation, FVector& TargetUpVector, FVector& TargetForwardVector) override;

private:

};

///** [1 Trace x 2 Directions = Totals 2 Traces]
//This is similar to SurfaceNav1x1TraceRotationMode, but instead of just doing a single trace forward and down, if that first trace
//misses then another trace will be done ahead of the pawn going down and back to attempt to find a surface. Along with both
//RotationModeTraceDownFromForwardAngle and RotationModeTraceDownFromForwardDistance properties, this also uses RotationMode2ndTraceStartOffset,
//RotationMode2ndTraceAngle and RotationMode2ndTraceDistance. This may be fine for small pawns, but may not look as good with
//medium to larger pawns. */
//UCLASS(ClassGroup = "Surface Nav Movement|Rotation")
//class SURFACENAVPLUGIN_API USurfaceNav1x2TraceRotationMode : public USurfaceNavPawnRotationMode
//{
//	GENERATED_BODY()
//
//public:
//	virtual FRotator DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation, FVector& TargetUpVector, FVector& TargetForwardVector) override;
//
//private:
//
//};

/** [2 Traces x 2 Directions (forward-left/forward-right) = Totals 4 Traces]
Uses the RotationMode1stTraceAngle and RotationMode1stTraceDistance properties of the SurfaceNavMovementComponent
to do a trace towards the pawn's left-forward-down and right-forward-down directions. If that first trace has no hit, then it uses the
RotationMode2ndTraceAngle and RotationMode2ndTraceDistance properties to do another trace starting out ahead of the pawn (left & right)
going in the backward-down direction. The RotationModeLeftRightAngle property of the SurfaceNavMovementComponent is used to determine
the left and right angle from froward. */
UCLASS(NotPlaceable, Transient, ClassGroup = "Surface Nav Movement|Rotation")
class SURFACENAVPLUGIN_API USurfaceNav2x2TraceRotationMode : public USurfaceNavPawnRotationMode
{
	GENERATED_BODY()

public:
	virtual FRotator DeterminePawnRotation(USurfaceNavMovementComponent* SurfaceMovementComp, FVector TargetLocation, FVector& TargetUpVector, FVector& TargetForwardVector) override;

private:

};
