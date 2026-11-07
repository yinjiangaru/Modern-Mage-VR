// Copyright BMK Games LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "SurfaceNavPawn.generated.h"

class USurfaceNavMovementComponent;
class UCapsuleComponent;
class UArrowComponent;

UCLASS(config = Game, BlueprintType, Blueprintable, Abstract, meta = (ShortTooltip = "A base pawn with required components to work with the DSN plugin which you can inherit from."))
class SURFACENAVPLUGIN_API ASurfaceNavPawn : public APawn
{
	GENERATED_BODY()
public:
	/** Default UObject constructor. */
	ASurfaceNavPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	/** The main skeletal mesh associated with this pawn. */
	UPROPERTY(Category = "Surface Nav Pawn", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* MeshComp;
	//TObjectPtr<USkeletalMeshComponent> MeshComp;

	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category = "Surface Nav Pawn", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USurfaceNavMovementComponent* SurfaceNavMovement;
	//TObjectPtr<USurfaceNavMovementComponent> SurfaceNavMovement;

	/** The CapsuleComponent being used for movement collision. Always treated as being vertically aligned in simple collision check functions. */
	UPROPERTY(Category = "Surface Nav Pawn", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* CapsuleComponent;
	//TObjectPtr<UCapsuleComponent> CapsuleComponent;

#if WITH_EDITORONLY_DATA
	/** Component shown in the editor only to indicate pawn facing */
	UPROPERTY()
	UArrowComponent* ArrowComponent;
	//TObjectPtr<UArrowComponent> ArrowComponent;
#endif

public:
	/** Name of the CapsuleComponent. */
	static FName CapsuleComponentName;

	/** Name of the CharacterMovement component. Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass). */
	static FName SurfaceNavMovementComponentName;

	/** Name of the MeshComponent. Use this name if you want to prevent creation of the component (with ObjectInitializer.DoNotCreateDefaultSubobject). */
	static FName MeshComponentName;
};