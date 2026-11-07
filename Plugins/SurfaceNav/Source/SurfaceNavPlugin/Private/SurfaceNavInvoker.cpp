// Copyright BMK Games LLC. All Rights Reserved.

//#include "SurfaceNavChunk.h"
//#include "SurfaceNavigationModule.h"
//
#include "SurfaceNavInvoker.h"
#include "UObject/Object.h"

class USurfaceNavigationSystem;

ASurfaceNavInvoker::ASurfaceNavInvoker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bNetLoadOnClient = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));

	InvokerComponent = CreateDefaultSubobject<USurfaceNavInvokerComponent>(TEXT("SNInvokerComp"));
}
