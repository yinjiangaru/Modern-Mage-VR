// Copyright (C) RenZhai.2026.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "VertexAnimInstance.generated.h"

UCLASS()
class UVertexAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Velocity;

	void SetVelocity(const FVector& NewVelocity);
};