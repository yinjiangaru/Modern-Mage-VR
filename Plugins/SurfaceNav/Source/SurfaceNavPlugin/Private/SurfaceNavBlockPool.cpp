// Copyright BMK Games LLC. All Rights Reserved.


#include "SurfaceNavBlockPool.h"
#include "SurfaceNavigationSystem.h"
#include "Runtime/Launch/Resources/Version.h"


// Sets default values
FSurfaceNavBlockPoolComponent::FSurfaceNavBlockPoolComponent(
	const int& InMaxPoolSize,
	const int& InInitialPoolSize,
	UWorld* InWorld,
	const FReleaseBlockDelegate& InReleaseBlockDel,
	const FBlockOverlapsFoundDelegate& InBlockOverlapsFoundDel,
	const FBlockNavUpdatedDelegate& InBlockNavUpdatedDel,
	const FHandleBlockMarkedDirtyDelegate& InHandleBlockMarkedDirtyDel,
	const FActorsInBlockChangedDelegate& InActorsInBlockChangedDel,
	FSurfaceNavGridPointPool& InGridPointPool) :
	World(InWorld),
	ReleaseBlockDel(InReleaseBlockDel),
	BlockOverlapsFoundDel(InBlockOverlapsFoundDel),
	BlockNavUpdatedDel(InBlockNavUpdatedDel),
	HandleBlockMarkedDirtyDel(InHandleBlockMarkedDirtyDel),
	ActorsInBlockChangedDel(InActorsInBlockChangedDel),
	MaxPoolSize(InMaxPoolSize),
	InitialPoolSize(InInitialPoolSize),
	GridPointPool(InGridPointPool)
{
	
}

FSurfaceNavBlockPoolComponent::~FSurfaceNavBlockPoolComponent()
{
	AvailableBlocks.Empty();
}

inline FSurfaceNavBlock* FSurfaceNavBlockPoolComponent::CreateInstance(UWorld* InWorld)
{
	check(InWorld);

	FSurfaceNavBlock* navBlock = new FSurfaceNavBlock(InWorld);

	navBlock->ReleaseBlockDel = &ReleaseBlockDel;
	navBlock->BlockOverlapsFoundDel = &BlockOverlapsFoundDel;
	navBlock->BlockNavUpdatedDel = &BlockNavUpdatedDel;
	navBlock->HandleBlockMarkedDirtyDel = &HandleBlockMarkedDirtyDel;
	navBlock->ActorsInBlockChangedDel = &ActorsInBlockChangedDel;
	navBlock->GridPointPool = &GridPointPool;
	navBlock->Init();

	return navBlock;
}

void FSurfaceNavBlockPoolComponent::Release(UWorld* InWorld, FSurfaceNavBlock*& block)
{
	if (!block || !block->IsValid())
		return;

	block->ResetAll();
	block->Destroy();

	USurfaceNavigationSystem::QueueBlockToDelete(block);
}

/** Pull a block off the pool */
FSurfaceNavBlock* FSurfaceNavBlockPoolComponent::Pop(UWorld* InWorld, const FVector& SpawnLoc, const FIntVector& IntLoc) //UWorld* World)
{
	check(InWorld);

	if (this->AvailableBlocks.Num() > 0)
	{

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
		FSurfaceNavBlock* navBlock = this->AvailableBlocks.Pop(false);
#else
		FSurfaceNavBlock* navBlock = this->AvailableBlocks.Pop(EAllowShrinking::No);
#endif

		navBlock->SetLocation(SpawnLoc);
		navBlock->BlockKey = new FIntVector(IntLoc);

		if (this->AvailableBlocks.Num() == 0)
			UE_LOG(LogTemp, Warning, TEXT("*** SURFACE NAVIGATION: BLOCK POOL HAS BEEN EXHAUSTED ***"));

		return navBlock; 
	}
	else
	{
		FSurfaceNavBlock* navBlock = CreateInstance(InWorld);
		navBlock->SetLocation(SpawnLoc);
		navBlock->BlockKey = new FIntVector(IntLoc);

		return navBlock;
	}
}
