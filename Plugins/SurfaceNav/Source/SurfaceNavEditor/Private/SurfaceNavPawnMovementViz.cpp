// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavPawnMovementViz.h"
#include "SurfaceNavMovementComponent.h"

// Contains most of the stuff you need for visualization
#include "SceneManagement.h"

void FSurfaceNavPawnMovementViz::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
    // Cast the incoming component to our USurfaceNavMovementComponent class
    const USurfaceNavMovementComponent* MoveComp = Cast<const USurfaceNavMovementComponent>(Component);

    if (!MoveComp || MoveComp->IsBeingDestroyed())
        return;

    FRotator rot = FRotator::ZeroRotator;
    FVector radii(10.f);

    // Draw a small wire sphere at the location of the movement offset
    DrawWireSphere(
        PDI,
        MoveComp->GetActorSurfaceLocation(),
        FLinearColor::Blue,
        10.f,
        10,
        0,
        2.f);
}