// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavPawn.h"
#include "Components/ArrowComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "SurfaceNavMovementComponent.h"

FName ASurfaceNavPawn::CapsuleComponentName(TEXT("CollisionCylinder"));
FName ASurfaceNavPawn::SurfaceNavMovementComponentName(TEXT("SurfaceNavMoveComp"));
FName ASurfaceNavPawn::MeshComponentName(TEXT("MeshComp"));

ASurfaceNavPawn::ASurfaceNavPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName ID_Characters;
		FText NAME_Characters;
		FConstructorStatics()
			: ID_Characters(TEXT("Characters"))
			, NAME_Characters(NSLOCTEXT("SpriteCategory", "Characters", "Characters"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	// Surface Nav pawns can potentially climb walls and ceilings so movement component needs 
	// control over all rotations
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(ASurfaceNavPawn::CapsuleComponentName);
	CapsuleComponent->InitCapsuleSize(34.0f, 88.0f);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CapsuleComponent->CanCharacterStepUpOn = ECB_No;
	CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->bDynamicObstacle = true;

	RootComponent = CapsuleComponent;

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (ArrowComponent)
	{
		ArrowComponent->ArrowColor = FColor(150, 200, 255);
		ArrowComponent->bTreatAsASprite = true;
		ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_Characters;
		ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Characters;
		ArrowComponent->SetupAttachment(CapsuleComponent);
		ArrowComponent->bIsScreenSizeScaled = true;
	}
#endif // WITH_EDITORONLY_DATA

	SurfaceNavMovement = CreateDefaultSubobject<USurfaceNavMovementComponent>(ASurfaceNavPawn::SurfaceNavMovementComponentName);
	if (SurfaceNavMovement)
	{
		SurfaceNavMovement->SetUpdatedComponent(CapsuleComponent);
	}

	MeshComp = CreateOptionalDefaultSubobject<USkeletalMeshComponent>(ASurfaceNavPawn::MeshComponentName);
	if (MeshComp)
	{
		MeshComp->AlwaysLoadOnClient = true;
		MeshComp->AlwaysLoadOnServer = true;
		MeshComp->bOwnerNoSee = false;
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
		MeshComp->bCastDynamicShadow = true;
		MeshComp->bAffectDynamicIndirectLighting = true;
		MeshComp->PrimaryComponentTick.TickGroup = TG_PrePhysics;
		MeshComp->SetupAttachment(CapsuleComponent);
		static FName SurfaceNavMeshCollisionProfileName(TEXT("CharacterMesh"));
		MeshComp->SetCollisionProfileName(SurfaceNavMeshCollisionProfileName);
		MeshComp->SetGenerateOverlapEvents(false);
		MeshComp->SetCanEverAffectNavigation(false);
	}
}