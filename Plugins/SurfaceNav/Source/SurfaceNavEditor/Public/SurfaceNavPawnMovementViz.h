// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"

class SURFACENAVEDITOR_API FSurfaceNavPawnMovementViz : public FComponentVisualizer
{
private:
	// This is the only method we need to override from FComponentVisualizer
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};