// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

//#include "GameFramework/Actor.h"
//#include "DrawDebugHelpers.h"
//#include "WorldCollision.h"
//#include "SurfaceNavGridPoint.h"
#include "SurfaceNavCommon.h"
#include "SurfaceNavBlock.h"
#include "SurfaceNavGridPointPool.h"
#include "WorldCollision.h"
#include "Engine/World.h"
//#include "Kismet/KismetSystemLibrary.h"
//#include "Runtime/Engine/Classes/Components/BoxComponent.h"
//#include "SurfaceNavBlockPool.generated.h"



/** Actor which traces for world objects with collisions */
//USTRUCT()
struct SURFACENAVPLUGIN_API FSurfaceNavBlockPoolComponent //: public UObject
{
	//GENERATED_USTRUCT_BODY();
	
public:	
	// Sets default values for this actor's properties
	FSurfaceNavBlockPoolComponent(
		const int& InMaxPoolSizePoolSize,
		const int& InInitialPoolSize,
		//const int& InPoolSizeIncreaseStep,
		//USurfaceNavigationSystem& InNavSys,
		UWorld* InWorld,
		//const FCollisionShape& initGridPointTestSphere,
		//const FCollisionQueryParams& initGridPointOverlapSphereQueryParams,
		//const TDelegate<void(const FIntVector&)>& InReleaseBlockDel,
		const FReleaseBlockDelegate& InReleaseBlockDel,
		const FBlockOverlapsFoundDelegate& InBlockOverlapsFoundDel,
		const FBlockNavUpdatedDelegate& InBlockNavUpdatedDel,
		const FHandleBlockMarkedDirtyDelegate& InHandleBlockMarkedDirtyDel,
		//const TDelegate<void(const UActorComponent& Comp, const FIntVector&)>& InActorsInBlockChangedDel,
		const FActorsInBlockChangedDelegate& InActorsInBlockChangedDel,
		FSurfaceNavGridPointPool& InGridPointPool);
		//const FGridPointFinishedDelegate& initGridPointFinishedDel);

	~FSurfaceNavBlockPoolComponent();

protected:
	// Called when the game starts or when spawned
	//virtual void BeginPlay() override;

	FSurfaceNavBlock* CreateInstance(UWorld* InWorld);


	int InstancedBlocks;
	//TArray<FSurfaceNavBlock*> AvailableBlocks = {};
	TArray<FSurfaceNavBlock*> AvailableBlocks = {};

	//FCollisionShape GridPointTestSphere;
	
	//TArray<FSurfaceNavBlock*> UsedBlocks;
	
	//UWorld* World;
	FVector DefualtSpawnLoc = FVector(0);

	//USurfaceNavigationSystem& NavSys;
	UWorld* World;

	//const FCollisionShape& GridPointTestSphere;
	//const FCollisionQueryParams& GridPointOverlapSphereQueryParams;
	
	//const TDelegate<void(const FIntVector&)>& ReleaseBlockDel;
	const FReleaseBlockDelegate& ReleaseBlockDel;

	const FBlockOverlapsFoundDelegate& BlockOverlapsFoundDel;
	const FBlockNavUpdatedDelegate& BlockNavUpdatedDel;
	const FHandleBlockMarkedDirtyDelegate& HandleBlockMarkedDirtyDel;

	//const TDelegate<void(const UActorComponent& Comp, const FIntVector&)>& ActorsInBlockChangedDel;
	const FActorsInBlockChangedDelegate& ActorsInBlockChangedDel;
	
	//const FGridPointFinishedDelegate& GridPointFinishedDel;

	const int& MaxPoolSize;
	const int& InitialPoolSize;
	//const int& PoolSizeIncreaseStep;
	//int CurrentPoolSize;

	FSurfaceNavGridPointPool& GridPointPool;

	//FCollisionShape BlockTestSphere;
	//FCollisionShape BlockTestBox;

public:	
	/** Push a block back into the pool */
	void Release(UWorld* InWorld, FSurfaceNavBlock*& block);

	/** Pull a block off the pool */
	FSurfaceNavBlock* Pop(UWorld* InWorld, const FVector& SpawnLoc, const FIntVector& IntLoc); // UWorld* InWorld);

	//void SetWorld(UWorld* InWorld);
	//virtual void Tick(float DeltaTime) override;
	
};
