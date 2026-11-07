// Copyright BMK Games LLC. All Rights Reserved.


#include "EQS/Generators/EnvQueryGenerator_DSN_Sphere.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "SurfaceNavigationSystem.h"


#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_DSN_Sphere::UEnvQueryGenerator_DSN_Sphere(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Center = UEnvQueryContext_Querier::StaticClass();
	//GridHalfSize.DefaultValue = 500.0f;
	InnerRadius.DefaultValue = 500.0f;
	OuterRadius.DefaultValue = 1000.0f;
	DistanceMultiplier.DefaultValue = 2;

	ArcDirection.DirMode = EEnvDirection::Rotation;
	ArcDirection.LineFrom = UEnvQueryContext_Querier::StaticClass();
	ArcDirection.Rotation = UEnvQueryContext_Querier::StaticClass();
	ArcAngle.DefaultValue = 360.f;

	WorldAxisOffset.DefaultValue = FVector(0.f);
	AxisOffsetClosestToNormal.DefaultValue = 0.f;
}

void UEnvQueryGenerator_DSN_Sphere::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	TArray<AActor*> CenterActors;
	QueryInstance.PrepareContext(Center, CenterActors);

	if (CenterActors.Num() <= 0)
	{
		return;
	}

	UObject* BindOwner = QueryInstance.Owner.Get();
	InnerRadius.BindData(BindOwner, QueryInstance.QueryID);
	OuterRadius.BindData(BindOwner, QueryInstance.QueryID);
	DistanceMultiplier.BindData(BindOwner, QueryInstance.QueryID);
	WorldAxisOffset.BindData(BindOwner, QueryInstance.QueryID);
	AxisOffsetClosestToNormal.BindData(BindOwner, QueryInstance.QueryID);
	ArcAngle.BindData(BindOwner, QueryInstance.QueryID);

	float ArcAngleValue = ArcAngle.GetValue();
	float InnerRadiusValue = InnerRadius.GetValue();
	float OuterRadiusValue = OuterRadius.GetValue();
	int DistanceMultiplierValue = DistanceMultiplier.GetValue();
	FVector MyAbsoluteAxisOffset = WorldAxisOffset.GetValue();
	int MyAxisOffsetClosestToNormal = AxisOffsetClosestToNormal.GetValue();

	float InnerRadiusSq = FMath::Square(InnerRadius.GetValue());
	float OuterRadiusSq = FMath::Square(OuterRadius.GetValue());

	//DistanceMultiplierValue = FMath::Min(FMath::Max(DistanceMultiplierValue, 1), 5);

	float SphereDegreesAsDot = (ArcAngleValue - 180.f) / 180.f * -1.f;

	FBox GridBox(MyAbsoluteAxisOffset, MyAbsoluteAxisOffset);

	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(Center, ContextLocations);

	float BlockSize = SurfaceNavProxySettings::BlockSize;

	int SingleAxisVoxelCount = FMath::TruncToInt(BlockSize / SurfaceNavProxySettings::GridPointDistance);
	float GridPointDist = BlockSize / SingleAxisVoxelCount;

	float GridPointsPerBlock = FMath::Pow(BlockSize / GridPointDist, 3);
	float BlocksWithinExtents = FMath::Pow(FMath::Max(OuterRadiusValue / BlockSize, 1.f), 3);
	float GridPointsWithinRadius = GridPointsPerBlock * BlocksWithinExtents;

	TArray<FNavLocation> GridPoints;

	if (InnerRadiusValue >= OuterRadiusValue)
	{
		StoreNavPoints(GridPoints, QueryInstance);
		return;
	}

	GridPoints.Reserve(GridPointsWithinRadius * ContextLocations.Num());

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());
	if (NavSys)
	{
		for (int32 CenterIndex = 0; CenterIndex < CenterActors.Num(); CenterIndex++)
		{
			AActor* CurActor = CenterActors[CenterIndex];
			//const FVector TestDirVector = InvertConeValue ? ContextLocations[ContextIndex]->GetActorForwardVector() * -1.f : ContextLocations[ContextIndex]->GetActorForwardVector();
			const FVector TestDirVector = GetDirVector(QueryInstance, CurActor);
			const FVector ActorLocation = CurActor->GetActorLocation();

			TArray<FSurfaceNavGridPoint*> SurfaceNavGridPoints = NavSys->GetGridPointsInRadius(ActorLocation, OuterRadiusValue);

			for (int index = 0; index < SurfaceNavGridPoints.Num(); index++)
			{
				FSurfaceNavGridPoint*& GridPoint = SurfaceNavGridPoints[index];
				FIntVector GridPointVectorIndex = GridPoint->VectorIndex;

				float DistToPointSq = FVector::DistSquared(ActorLocation, GridPoint->WorldLocation);
				if (DistToPointSq < InnerRadiusSq)
					continue;

				FIntVector AdjustedGridPointIndex = *GridPoint->OwningBlock->BlockKey * SingleAxisVoxelCount + GridPointVectorIndex;

				int ModX = FMath::Abs(AdjustedGridPointIndex.X % DistanceMultiplierValue);
				int ModY = FMath::Abs(AdjustedGridPointIndex.Y % DistanceMultiplierValue);
				int ModZ = FMath::Abs(AdjustedGridPointIndex.Z % DistanceMultiplierValue);

				if (ModX == ModY && ModX == ModZ && ModY == ModZ)
				{
					if (bDefineArc && FVector::DotProduct(TestDirVector, (GridPoint->WorldLocation - ActorLocation).GetSafeNormal()) < SphereDegreesAsDot)
						continue;

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

	StoreNavPoints(GridPoints, QueryInstance);
}

FVector UEnvQueryGenerator_DSN_Sphere::GetDirVector(FEnvQueryInstance& QueryInstance, AActor* CurActor) const
{
	FVector DirVector(0.f);

	if (bDefineArc)
	{
		if (ArcDirection.DirMode == EEnvDirection::TwoPoints)
		{
			TArray<FVector> Start;
			TArray<FVector> End;
			QueryInstance.PrepareContext(ArcDirection.LineFrom, Start);
			QueryInstance.PrepareContext(ArcDirection.LineTo, End);

			if (Start.Num() > 0 && End.Num() > 0)
				DirVector = (End[0] - Start[0]).GetSafeNormal();
		}
		else
		{
			TArray<FRotator> Rot;
			QueryInstance.PrepareContext(ArcDirection.Rotation, Rot);

			if (Rot.Num() > 0)
			{
				DirVector = FRotationMatrix(Rot[0]).GetScaledAxis(EAxis::X);
			}
		}
	}

	return DirVector;
}

#if WITH_EDITOR

void UEnvQueryGenerator_DSN_Sphere::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != NULL)
	{
		const FName PropName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_DSN_Sphere, ArcAngle))
		{
			ArcAngle.DefaultValue = FMath::Clamp(ArcAngle.DefaultValue, 0.0f, 360.f);
			bDefineArc = (ArcAngle.DefaultValue < 360.f) && (ArcAngle.DefaultValue > 0.f);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_DSN_Sphere, InnerRadius))
		{
			InnerRadius.DefaultValue = FMath::Clamp(InnerRadius.DefaultValue, 0.f, OuterRadius.DefaultValue - 1.f);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_DSN_Sphere, OuterRadius))
		{
			OuterRadius.DefaultValue = FMath::Clamp(OuterRadius.DefaultValue, FMath::Max(1.f, InnerRadius.DefaultValue), 20000.f);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_DSN_Sphere, DistanceMultiplier))
		{
			DistanceMultiplier.DefaultValue = FMath::Clamp(DistanceMultiplier.DefaultValue, 1, 5);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

FText UEnvQueryGenerator_DSN_Sphere::GetDescriptionTitle() const
{
	return FText::Format(LOCTEXT("DSN Sphere", "{0}: generate around {1}"),
		Super::GetDescriptionTitle(), UEnvQueryTypes::DescribeContext(Center));
};

FText UEnvQueryGenerator_DSN_Sphere::GetDescriptionDetails() const
{
	FText Desc = FText::Format(LOCTEXT("DSN SphereDescription", "Inner Radius: {0}, Outer Radius: {1}"),
		FText::FromString(InnerRadius.ToString()), FText::FromString(OuterRadius.ToString()));

	FText ProjDesc = ProjectionData.ToText();
	if (!ProjDesc.IsEmpty())
	{
		FFormatNamedArguments ProjArgs;
		ProjArgs.Add(TEXT("Description"), Desc);
		ProjArgs.Add(TEXT("ProjectionDescription"), ProjDesc);
		Desc = FText::Format(LOCTEXT("DSN SphereDescriptionWithProjection", "{Description}, {ProjectionDescription}"), ProjArgs);
	}

	return Desc;
}

#undef LOCTEXT_NAMESPACE
