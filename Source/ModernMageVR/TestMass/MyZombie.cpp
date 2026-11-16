// Copyright (C) RenZhai.2026.All Rights Reserved.

#include "MyZombie.h"
#include "TestMassType.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassSignalSubsystem.h"
#include "MassCommandBuffer.h"

AMyZombie::AMyZombie()
{

}

void AMyZombie::MarkZombieDead()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMassActorSubsystem* MassActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>(World);
	if (!MassActorSubsystem) return;

	FMassEntityHandle Entity = MassActorSubsystem->GetEntityHandleFromActor(this);
	if (!Entity.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("No Mass Entity for %s"), *GetName());
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem) return;

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	FMassDeathFragment* DeathFragment = EntityManager.GetFragmentDataPtr<FMassDeathFragment>(Entity);
	if (!DeathFragment) return;

	if (DeathFragment->bIsDead) return;

	DeathFragment->bIsDead = true;

	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AMyZombie::OnDeathAnimationFinished, 2.5f, false);
}

void AMyZombie::OnDeathAnimationFinished()
{
	UWorld* World = GetWorld();

	UMassActorSubsystem* MassActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>(World);
	if (!MassActorSubsystem) return;

	FMassEntityHandle Entity = MassActorSubsystem->GetEntityHandleFromActor(this);

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem) return;

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	EntityManager.Defer().DestroyEntity(Entity);
}