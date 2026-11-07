// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavInvokerComponent.h"
#include "SurfaceNavigationSystem.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Runtime/Engine/Classes/Engine/World.h"

USurfaceNavInvokerComponent::USurfaceNavInvokerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanEverAffectNavigation = true;

	bAutoActivate = true;

	PrimaryComponentTick.bCanEverTick = false;
}

void USurfaceNavInvokerComponent::DestroyComponent(bool bPromoteChildren)
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		USurfaceNavigationSystem::UnregisterNavigationInvoker(Owner);
	}

	Super::DestroyComponent(bPromoteChildren);
}

void USurfaceNavInvokerComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		USurfaceNavigationSystem::UnregisterNavigationInvoker(Owner);
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void USurfaceNavInvokerComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	AActor* Owner = GetOwner();
	if (Owner && GetWorld()->GetNetMode() < ENetMode::NM_Client)
	{
		USurfaceNavigationSystem::RegisterSurfaceNavInvoker(Owner);
	}
}

void USurfaceNavInvokerComponent::Deactivate()
{
	// Don't rely on this as it doesn't seem to get called when needed during level
	// streaming out.
	AActor* Owner = GetOwner();
	if (Owner)
	{
		USurfaceNavigationSystem::UnregisterNavigationInvoker(Owner);
	}

	Super::Deactivate();
}
