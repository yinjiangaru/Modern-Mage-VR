// Copyright BMK Games LLC. All Rights Reserved.


#include "EQS/Generators/EnvQueryGenerator_DSN_Cone.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "GameFramework/Actor.h"
#include "SurfaceNavigationSystem.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_DSN_Cone::UEnvQueryGenerator_DSN_Cone(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	CenterActor = UEnvQueryContext_Querier::StaticClass();

	ConeDegrees.DefaultValue = 90.f;
	Range.DefaultValue = 1000.f;
	InvertCone.DefaultValue = false;
	DistanceMultiplier.DefaultValue = 2;
}

void UEnvQueryGenerator_DSN_Cone::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	TArray<AActor*> CenterActors;
	QueryInstance.PrepareContext(CenterActor, CenterActors);

	if (CenterActors.Num() <= 0)
	{
		return;
	}

	UObject* BindOwner = QueryInstance.Owner.Get();
	ConeDegrees.BindData(BindOwner, QueryInstance.QueryID);
	InvertCone.BindData(BindOwner, QueryInstance.QueryID);
	
	Range.BindData(BindOwner, QueryInstance.QueryID);

	DistanceMultiplier.BindData(BindOwner, QueryInstance.QueryID);

	const float ConeDegreesValue = FMath::Clamp(FMath::Abs(ConeDegrees.GetValue()), 0.f, 360.f);
	if (ConeDegreesValue == 0.f)
		return;

	const float InvertConeValue = InvertCone.GetValue();

	float ConeDegreesAsDot = (ConeDegreesValue - 180.f) / 180.f * -1.f;

	const float GenerationRange = FMath::Clamp(Range.GetValue(), 0.f, 20000.f);

	if (GenerationRange == 0.f)
		return;

	int DistanceMultiplierValue = DistanceMultiplier.GetValue();
	FVector MyAbsoluteAxisOffset(0.f);

	DistanceMultiplierValue = FMath::Min(FMath::Max(DistanceMultiplierValue, 1), 5);

	float FurthestRadius = GenerationRange * FMath::Tan(12.5f);

	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(CenterActor, ContextLocations);

	float BlockSize = SurfaceNavProxySettings::BlockSize;

	int SingleAxisVoxelCount = FMath::TruncToInt(BlockSize / SurfaceNavProxySettings::GridPointDistance);
	float GridPointDist = BlockSize / SingleAxisVoxelCount;

	float GridPointsPerBlock = FMath::Pow(BlockSize / GridPointDist, 3);
	float BlocksWithinRadius = FMath::Pow(FMath::Max(GenerationRange / BlockSize, 1.f), 3.f);
	float GridPointsWithinRadius = GridPointsPerBlock * BlocksWithinRadius;


	TArray<FNavLocation> GridPoints;
	GridPoints.Reserve(GridPointsWithinRadius * ContextLocations.Num());

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());
	if (NavSys)
	{
		for (int32 CenterIndex = 0; CenterIndex < CenterActors.Num(); CenterIndex++)
		{
			AActor* CurActor = CenterActors[CenterIndex];
			const FVector TestDirVector = InvertConeValue ? CurActor->GetActorForwardVector() * -1.f : CurActor->GetActorForwardVector();
			const FVector ActorLocation = CurActor->GetActorLocation();

			TArray<FSurfaceNavGridPoint*> SurfaceNavGridPoints = NavSys->GetGridPointsInRadius(ActorLocation, GenerationRange + FurthestRadius);

			for (int index = 0; index < SurfaceNavGridPoints.Num(); index++)
			{
				FSurfaceNavGridPoint*& GridPoint = SurfaceNavGridPoints[index];
				FIntVector GridPointVectorIndex = GridPoint->VectorIndex;

				FIntVector AdjustedGridPointIndex = *GridPoint->OwningBlock->BlockKey * SingleAxisVoxelCount + GridPointVectorIndex;

				int ModX = FMath::Abs(AdjustedGridPointIndex.X % DistanceMultiplierValue);
				int ModY = FMath::Abs(AdjustedGridPointIndex.Y % DistanceMultiplierValue);
				int ModZ = FMath::Abs(AdjustedGridPointIndex.Z % DistanceMultiplierValue);

				if (ModX == ModY && ModX == ModZ && ModY == ModZ)					
				{
					// Make sure the point is within the cone, if not then skip
					if (FVector::DotProduct(TestDirVector, (GridPoint->WorldLocation - ActorLocation).GetSafeNormal()) < ConeDegreesAsDot)
						continue;

					FVector AxisAlignedNormalOffset(0.f);

					FNavLocation NavLoc = FNavLocation(DetermineProjectedLocation(GridPoint) + MyAbsoluteAxisOffset + AxisAlignedNormalOffset);

					GridPoints.Add(NavLoc);
				}
			}
		}
	}

	StoreNavPoints(GridPoints, QueryInstance);
}

#if WITH_EDITOR

void UEnvQueryGenerator_DSN_Cone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != NULL)
	{
		const FName PropName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_DSN_Cone, ConeDegrees))
		{
			ConeDegrees.DefaultValue = FMath::Clamp(ConeDegrees.DefaultValue, 0.f, 360.f);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_DSN_Cone, Range))
		{
			Range.DefaultValue = FMath::Clamp(Range.DefaultValue, 0.f, 20000.f);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_DSN_Cone, DistanceMultiplier))
		{
			DistanceMultiplier.DefaultValue = FMath::Clamp(DistanceMultiplier.DefaultValue, 1, 5);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

FText UEnvQueryGenerator_DSN_Cone::GetDescriptionTitle() const
{
	return FText::Format(LOCTEXT("DSN Cone", "{0}: generate around {1}"),
		Super::GetDescriptionTitle(), UEnvQueryTypes::DescribeContext(CenterActor));
};

FText UEnvQueryGenerator_DSN_Cone::GetDescriptionDetails() const
{
	FText Desc = FText::Format(LOCTEXT("DSN ConeDescription", "GridHalfSize: {0}"),
		FText::FromString(ConeDegrees.ToString()));

	FText ProjDesc = ProjectionData.ToText();
	if (!ProjDesc.IsEmpty())
	{
		FFormatNamedArguments ProjArgs;
		ProjArgs.Add(TEXT("Description"), Desc);
		ProjArgs.Add(TEXT("ProjectionDescription"), ProjDesc);
		Desc = FText::Format(LOCTEXT("DSN ConeDescriptionWithProjection", "{Description}, {ProjectionDescription}"), ProjArgs);
	}

	return Desc;
}

#undef LOCTEXT_NAMESPACE
