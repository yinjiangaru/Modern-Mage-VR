// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavPlanetComponent.h"
#include "NavigationSystem.h"
#include "SurfaceNavigationSystem.h"


USurfaceNavPlanetComponent::USurfaceNavPlanetComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = false;
}

void USurfaceNavPlanetComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	auto NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	if (NavSys && GetWorld()->GetNetMode() < ENetMode::NM_Client)
	{
		NavSys->RegisterSurfaceNavPlanetComponent(this);
	}
}

void USurfaceNavPlanetComponent::Deactivate()
{
	auto NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	if (NavSys)
	{
		NavSys->UnregisterSurfaceNavPlanetComponent(this);
	}

	Super::Deactivate();
}

