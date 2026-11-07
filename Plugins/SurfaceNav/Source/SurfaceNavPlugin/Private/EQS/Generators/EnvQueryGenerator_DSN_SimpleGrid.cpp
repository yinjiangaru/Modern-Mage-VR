// Copyright BMK Games LLC. All Rights Reserved.


#include "EQS/Generators/EnvQueryGenerator_DSN_SimpleGrid.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "SurfaceNavigationSystem.h"


#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_DSN_SimpleGrid::UEnvQueryGenerator_DSN_SimpleGrid(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	GenerateAround = UEnvQueryContext_Querier::StaticClass();
	GridHalfSize.DefaultValue = 500.0f;
	DistanceMultiplier.DefaultValue = 2;
	WorldAxisOffset.DefaultValue = FVector(0);
	AxisOffsetClosestToNormal.DefaultValue = 0.f;
}

void UEnvQueryGenerator_DSN_SimpleGrid::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	UObject* BindOwner = QueryInstance.Owner.Get();
	GridHalfSize.BindData(BindOwner, QueryInstance.QueryID);
	DistanceMultiplier.BindData(BindOwner, QueryInstance.QueryID);
	WorldAxisOffset.BindData(BindOwner, QueryInstance.QueryID);
	AxisOffsetClosestToNormal.BindData(BindOwner, QueryInstance.QueryID);

	float GridHalfSizeValue = GridHalfSize.GetValue();
	int DistanceMultiplierValue = DistanceMultiplier.GetValue();
	FVector MyAbsoluteAxisOffset = WorldAxisOffset.GetValue();
	int MyAxisOffsetClosestToNormal = AxisOffsetClosestToNormal.GetValue();

	DistanceMultiplierValue = FMath::Min(FMath::Max(DistanceMultiplierValue, 1), 5);

	FBox GridBox(MyAbsoluteAxisOffset, MyAbsoluteAxisOffset);

	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(GenerateAround, ContextLocations);

	float BlockSize = SurfaceNavProxySettings::BlockSize;

	int SingleAxisVoxelCount = FMath::TruncToInt(BlockSize / SurfaceNavProxySettings::GridPointDistance);
	float GridPointDist = BlockSize / SingleAxisVoxelCount;

	float GridPointsPerBlock = FMath::Pow(BlockSize / GridPointDist, 3);
	float BlocksWithinExtents = FMath::Pow(FMath::Max(GridHalfSizeValue / BlockSize, 1.f), 3);
	float GridPointsWithinRadius = GridPointsPerBlock * BlocksWithinExtents;
	
	TArray<FNavLocation> GridPoints;
	GridPoints.Reserve(GridPointsWithinRadius * ContextLocations.Num());

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());
	if (NavSys)
	{
		for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
		{
			TArray<FSurfaceNavGridPoint*> SurfaceNavGridPoints = NavSys->GetGridPointsInBox(ContextLocations[ContextIndex], GridHalfSizeValue);

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
					FVector AxisAlignedNormalOffset(0.f);
					if (MyAxisOffsetClosestToNormal != 0.f)
					{
						FVector AvgGridPointNormal = GridPoint->GetAvgNormal();

						float ForwardDelta = FVector::DotProduct(FVector::ForwardVector, AvgGridPointNormal);
						float UpDelta = FVector::DotProduct(FVector::UpVector, AvgGridPointNormal);
						float RightDelta = FVector::DotProduct(FVector::RightVector, AvgGridPointNormal);

						float AbsForwardDelta = FMath::Abs(ForwardDelta);
						float AbsUpDelta = FMath::Abs(UpDelta);
						float AbsRightDelta = FMath::Abs(RightDelta);
						
						FVector ClosestAxisNormal = ForwardDelta > 0.f ? FVector::ForwardVector : FVector::BackwardVector;

						if (AbsUpDelta > AbsForwardDelta)
							if (AbsRightDelta > AbsUpDelta)
								ClosestAxisNormal = RightDelta > 0.f ? FVector::RightVector : FVector::LeftVector;
							else
								ClosestAxisNormal = UpDelta > 0.f ? FVector::UpVector : FVector::DownVector;
						else if (AbsRightDelta > AbsForwardDelta)
							ClosestAxisNormal = RightDelta > 0.f ? FVector::RightVector : FVector::LeftVector;

						AxisAlignedNormalOffset = ClosestAxisNormal * MyAxisOffsetClosestToNormal;
					}

					FNavLocation NavLoc = FNavLocation(DetermineProjectedLocation(GridPoint) + MyAbsoluteAxisOffset + AxisAlignedNormalOffset);

					GridPoints.Add(NavLoc);
				}
			}
		}
	}

	//ProjectAndFilterNavPoints(GridPoints, QueryInstance);
	StoreNavPoints(GridPoints, QueryInstance);
}

#if WITH_EDITOR

void UEnvQueryGenerator_DSN_SimpleGrid::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != NULL)
	{
		const FName PropName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_DSN_SimpleGrid, GridHalfSize))
		{
			GridHalfSize.DefaultValue = FMath::Clamp(GridHalfSize.DefaultValue, 0.f, 20000.f);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_DSN_SimpleGrid, DistanceMultiplier))
		{
			DistanceMultiplier.DefaultValue = FMath::Clamp(DistanceMultiplier.DefaultValue, 1, 5);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

FText UEnvQueryGenerator_DSN_SimpleGrid::GetDescriptionTitle() const
{
	return FText::Format(LOCTEXT("DSN SimpleGrid", "{0}: generate around {1}"),
		Super::GetDescriptionTitle(), UEnvQueryTypes::DescribeContext(GenerateAround));
};

FText UEnvQueryGenerator_DSN_SimpleGrid::GetDescriptionDetails() const
{
	FText Desc = FText::Format(LOCTEXT("DSN SimpleGridDescription", "Grid Half Size: {0}"),
		FText::FromString(GridHalfSize.ToString()));

	FText ProjDesc = ProjectionData.ToText();
	if (!ProjDesc.IsEmpty())
	{
		FFormatNamedArguments ProjArgs;
		ProjArgs.Add(TEXT("Description"), Desc);
		ProjArgs.Add(TEXT("ProjectionDescription"), ProjDesc);
		Desc = FText::Format(LOCTEXT("DSN SimpleGridDescriptionWithProjection", "{Description}, {ProjectionDescription}"), ProjArgs);
	}

	return Desc;
}

#undef LOCTEXT_NAMESPACE
