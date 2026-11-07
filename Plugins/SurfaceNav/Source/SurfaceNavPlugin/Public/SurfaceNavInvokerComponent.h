// Copyright BMK Games LLC. All Rights Reserved.

#pragma once


#include "Components/ActorComponent.h"

#include "SurfaceNavInvokerComponent.generated.h"

/** Use this component within one of your own actors to allow it to
register for DSN plugin's navigation generation around itself. This
will work for both static/stationary actors as well as moveable
actors or pawns. */
UCLASS(ClassGroup = (Navigation), meta = (BlueprintSpawnableComponent))
class SURFACENAVPLUGIN_API USurfaceNavInvokerComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

protected:



public:

	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;

	/** Unregister the component, remove it from its outer Actor's Components array and mark for pending kill. */
	virtual void DestroyComponent(bool bPromoteChildren = false) override;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	//#if WITH_EDITOR
	//	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//#endif

		/** How far should blocks be generated from the invoker when near to a player. This could also be
		considered the MAX generation radius. This must be >= FarBlockGenerationRadius and will be adjusted
		automatically if this value is < FarBlockGenerationRadius. The lower you're able to get this value, the
		better your performance will be. Leaving this zero will use defaults from SurfaceNavProxy. */
	UPROPERTY(EditAnywhere, Category = "SurfaceNav|General", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "100000.0", UIMax = "100000.0"))
	float NearBlockGenerationRadius = 0.f;

	/** How far should blocks be generated from the invoker when far from a player. This could also be
	considered the MIN generation radius. This will default to 1.5x the block size if left ZERO or the provided
	value is less than 1.5x block size. The lower you're able to get this value, the better your performance will be.
	Leaving this zero will use defaults from SurfaceNavProxy.*/
	UPROPERTY(EditAnywhere, Category = "SurfaceNav|General", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "100000.0", UIMax = "100000.0"))
	float FarBlockGenerationRadius = 0.f;

	/** The max distance from a player that blocks will be generated for navigation. The lower you're able to get
	this value, the better your performance will be. Leaving this zero will use defaults from SurfaceNavProxy.*/
	UPROPERTY(EditAnywhere, Category = "SurfaceNav|General", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "100000.0", UIMax = "100000.0"))
	float MaxDistanceFromPlayerForGeneration = 0.f;

};
