// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MyZombie.generated.h"

UCLASS()
class AMyZombie : public ACharacter
{
	GENERATED_BODY()

public:
	AMyZombie();

protected:
	UFUNCTION(BlueprintCallable)
	void MarkZombieDead();

	void OnDeathAnimationFinished();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	UAnimMontage* DeathMontage;

	bool bIsDead = false;
};