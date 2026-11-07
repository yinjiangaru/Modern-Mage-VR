// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "SurfaceNavInvokerComponent.h"

#include "SurfaceNavInvoker.generated.h"

/** An actor you can place in the level which registers for DSN plugin’s navigation generation around itself. */
UCLASS(ClassGroup = (Navigation))
class SURFACENAVPLUGIN_API ASurfaceNavInvoker : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurfaceNav")
		USurfaceNavInvokerComponent* InvokerComponent;
	//TObjectPtr<USurfaceNavInvokerComponent> InvokerComponent;

};
