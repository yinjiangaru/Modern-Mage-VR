// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "SurfaceNavPlanetComponent.generated.h"


/** Allows the DSN plugin to detect planets and understand the ground normal for any point relative to a planet. */
UCLASS(ClassGroup = (Navigation), meta = (BlueprintSpawnableComponent))
class SURFACENAVPLUGIN_API USurfaceNavPlanetComponent : public UActorComponent
{
	GENERATED_BODY()
//
//protected:
//	
//
public:
	USurfaceNavPlanetComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
//
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
//
};
