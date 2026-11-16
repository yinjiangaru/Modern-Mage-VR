#pragma once

#include "AnimToTextureDataAsset.h"
#include "MassEntityTypes.h"
#include "TestMassType.generated.h"

USTRUCT()
struct FTestFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	float Value = 0.f;
};

USTRUCT()
struct FPlayerInfoFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FVector TargetPos = FVector::ZeroVector;

	UPROPERTY()
	float DistanceToPlayer = 0.f;
};

USTRUCT()
struct FVertexAnimInfoFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UAnimToTextureDataAsset> AnimToTextureData;

	float GlobalStartTime = 0.0f;
	float PlayRate = 1.0f;
	int32 AnimationStateIndex = 0;
	bool bCustomAnimation = false;
	int AnimPosition = 0;
};

USTRUCT()
struct FVertexAnimLocomotionFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 IdleAnimIndex = 0;

	UPROPERTY(EditAnywhere)
	int32 RunAnimIndex = 0;

	UPROPERTY(EditAnywhere)
	float SpeedThreshhold = 50.f;
};

USTRUCT()
struct FVertexAnimDeathFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 DeathAnimIndex = 0;

	UPROPERTY(EditAnywhere)
	float DeathAnimLength = 1.5f;

	float ElapsedTime = 0.f;
};

USTRUCT()
struct FMassMontageFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UAnimMontage> Montage;

	float Position = 0.f;

	FMassMontageFragment() = default;

	FMassMontageFragment(const TSoftObjectPtr<UAnimMontage>& InMontage) : Montage(InMontage) {};
};

USTRUCT()
struct FMassDeathFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsDead = false;
};