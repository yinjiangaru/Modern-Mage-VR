// Copyright BMK Games LLC. All Rights Reserved.


#include "SurfaceNavProxy.h"
#include "SurfaceNavigationSystem.h"
#include "Engine/Canvas.h"

// Sets default values
ASurfaceNavProxy::ASurfaceNavProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bNetLoadOnClient = false;
	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITOR
	//DebugStringsComp = CreateDefaultSubobject<UDebugStrings>(TEXT("SNBlockCollision"));
#endif
}

void ASurfaceNavProxy::PostLoad()
{
	Super::PostLoad();

	UWorld* MyWorld = GetWorld();
	if (!MyWorld || MyWorld->WorldType == EWorldType::Inactive || MyWorld->WorldType == EWorldType::None)
		return;

	RefreshSettings();
}

#if WITH_EDITOR

void ASurfaceNavProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == "NearBlockGenerationRadius")
	{
		if (NearBlockGenerationRadius < FarBlockGenerationRadius)
		{
			NearBlockGenerationRadius = FarBlockGenerationRadius;
		}
	}
	else if (PropertyChangedEvent.GetPropertyName() == "FarBlockGenerationRadius")
	{
		if (FarBlockGenerationRadius < CalcSingleAxisBlockSize() * 1.5f)
		{
			FarBlockGenerationRadius = CalcSingleAxisBlockSize() * 1.5f;
		}
		else if (FarBlockGenerationRadius > NearBlockGenerationRadius)
		{
			FarBlockGenerationRadius = NearBlockGenerationRadius;
		}
	}
	else if (PropertyChangedEvent.GetPropertyName() == "BlockSize")
	{
		if (FarBlockGenerationRadius < CalcSingleAxisBlockSize() * 1.5f)
		{
			FarBlockGenerationRadius = CalcSingleAxisBlockSize() * 1.5f;
		}

		if (NearBlockGenerationRadius < FarBlockGenerationRadius)
		{
			NearBlockGenerationRadius = FarBlockGenerationRadius;
		}
	}

	TArray<FName> PropsNotNeedingRefresh = 
	{ 
		"ShowNavBlocks", 
		"ShowNavGridPoints", 
		"ShowNavPoints", 
		"ShowNavNeighborDirections",
		"ShowWhereNavigationEnds",
		"ShowNavLinks",
		"ShowNavPointNormals",
		"ShowNavPointsDuringBuild",
		"DisplayRadiusFromCamera",
		"ShowDebugStats",
		"Show_AI_Paths",
		"PerformExtraDebuggingTests",
		"MaxPathRequestsPerTick",
		"ObservedPathsTickInterval",
		"MaxDistanceFromNavigation",
		"MaxPathingIterations",
		"NearBlockGenerationRadius",
		"FarBlockGenerationRadius",
		"MaxDistanceFromPlayerForGeneration",
		"DelayBeforePathFindingAllowed"
	};

	if (PropsNotNeedingRefresh.Contains(PropertyChangedEvent.GetPropertyName()))
	{
		GetWorld()->GetTimerManager().SetTimer(
			TimerRebuildAllHandle,
			this,
			//TimerInitDel,
			&ASurfaceNavProxy::RefreshSettings,
			1.f,
			false,
			-1.f
		);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(
			TimerRebuildAllHandle,
			this,
			//TimerInitDel,
			&ASurfaceNavProxy::SurfaceNavRebuildAll,
			1.f,
			false,
			-1.f
		);
	}
}

#endif

void ASurfaceNavProxy::RefreshSettings()
{
	SurfaceNavProxySettings::BlockSize = CalcSingleAxisBlockSize();
	SurfaceNavProxySettings::BlockExtent = SurfaceNavProxySettings::BlockSize / 2.0f;

	SurfaceNavProxySettings::GridPointDistance = GridPointDistance;
	SurfaceNavProxySettings::GridPointDistanceSq = FMath::Square(GridPointDistance);
	SurfaceNavProxySettings::SingleAxisVoxelCount = GridPointsPerBlockAxis; // truncf(SurfaceNavProxySettings::BlockSize / SurfaceNavProxySettings::GridPointDistance);
	SurfaceNavProxySettings::GridPointsPerBlock = SurfaceNavProxySettings::SingleAxisVoxelCount * SurfaceNavProxySettings::SingleAxisVoxelCount * SurfaceNavProxySettings::SingleAxisVoxelCount;
	//SurfaceNavProxySettings::GridPointDistance = SurfaceNavProxySettings::BlockSize / SurfaceNavProxySettings::SingleAxisVoxelCount;
	SurfaceNavProxySettings::PaddedGridPointDistance = SurfaceNavProxySettings::GridPointDistance + SurfaceNavProxySettings::GridPointDistancePadding;

	SurfaceNavProxySettings::GridOffset = GridOffsetPercent * GridPointDistance;

	SurfaceNavProxySettings::PaddedBlockExtent = SurfaceNavProxySettings::BlockExtent + SurfaceNavProxySettings::GridPointDistance / 2.f + SurfaceNavProxySettings::BlockExtentPadding;

	MaxExpenseModifierSq = FMath::Square(MaxExpenseModifier);
	ExpenseModifierUpStepSq = FMath::Square(ExpenseModifierUpStep);
	ExpenseModifierDownStepSq = FMath::Square(ExpenseModifierDownStep);

	SurfaceNavProxySettings::MaxExpenseModifierSq = MaxExpenseModifierSq;
	SurfaceNavProxySettings::ExpenseModifierUpStepSq = ExpenseModifierUpStepSq;
	SurfaceNavProxySettings::ExpenseModifierDownStepSq = ExpenseModifierDownStepSq;

	SurfaceNavProxySettings::FlagNarrowCorridors = FlagNarrowCorridors;
	SurfaceNavProxySettings::NarrowCorridorPercentToFlagSq = FMath::Square(NarrowCorridorPercentToFlag * GridPointDistance);

	SurfaceNavProxySettings::AllowNavigationGenerationWithoutPlayer = AllowNavigationGenerationWithoutPlayer;
	SurfaceNavProxySettings::IsFlatWorld = IsFlatWorld;
	SurfaceNavProxySettings::FarBlockGenerationRadius = FarBlockGenerationRadius;
	SurfaceNavProxySettings::NearBlockGenerationRadius = NearBlockGenerationRadius;
	SurfaceNavProxySettings::MaxDistanceFromPlayerForGeneration = MaxDistanceFromPlayerForGeneration;
	SurfaceNavProxySettings::CleanBlockInterval = CleanBlockInterval;
	SurfaceNavProxySettings::MaxPathingIterations = MaxPathingIterations;
	SurfaceNavProxySettings::MaxPathRequestsPerTick = MaxPathRequestsPerTick;
	SurfaceNavProxySettings::ObservedPathsTickInterval = ObservedPathsTickInterval;
	SurfaceNavProxySettings::MaxDistanceFromNavigation = MaxDistanceFromNavigation;	
	SurfaceNavProxySettings::ShowDebugNavPointsDuringBuild = ShowNavPointsDuringBuild;
	SurfaceNavProxySettings::ShowDebug_AI_Paths = Show_AI_Paths;
	SurfaceNavProxySettings::PerformExtraDebuggingTests = PerformExtraDebuggingTests;	
	SurfaceNavProxySettings::MaxExpenseModifier = MaxExpenseModifier;
	SurfaceNavProxySettings::ExpenseModifierUpStep = ExpenseModifierUpStep;
	SurfaceNavProxySettings::ExpenseModifierDownStep = ExpenseModifierDownStep;
	SurfaceNavProxySettings::DelayBeforePathFindingAllowed = DelayBeforePathFindingAllowed;
	SurfaceNavProxySettings::RefreshLocationDataInterval = RefreshLocationDataInterval;
	SurfaceNavProxySettings::ProcessComponentUpdatesInterval = ProcessComponentUpdatesInterval;
	SurfaceNavProxySettings::ProcessPendingDirtyNavigationInterval = ProcessPendingDirtyNavigationInterval;
	SurfaceNavProxySettings::ShowNavPoints = ShowNavPoints;
	SurfaceNavProxySettings::ShowNavGridPoints = ShowNavGridPoints;
	SurfaceNavProxySettings::ShowNavBlocks = ShowNavBlocks;
	SurfaceNavProxySettings::ShowNavNeighborDirs = ShowNavNeighborDirections;
	SurfaceNavProxySettings::ShowWhereNavigationEnds = ShowWhereNavigationEnds;
	SurfaceNavProxySettings::ShowNavLinks = ShowNavLinks;
	SurfaceNavProxySettings::ShowNavPointNormals = ShowNavPointNormals;
	SurfaceNavProxySettings::DisplayRadiusFromCamera = DisplayRadiusFromCamera;
	SurfaceNavProxySettings::ShowDebugStats = ShowDebugStats;
	SurfaceNavProxySettings::MaxComponentSphereRadiusToProcess = MaxComponentSphereRadiusToProcess;

	SurfaceNavProxySettings::TraceSphereRadius = TraceSphereRadius;
	SurfaceNavProxySettings::TraceSphereStartOffset = TraceSphereStartOffset;
	SurfaceNavProxySettings::TraceSphereEndOffset = TraceSphereEndOffset;

	SurfaceNavProxySettings::TraceMethod = TraceMethod;
	SurfaceNavProxySettings::TraceChannel = TraceChannel;
	SurfaceNavProxySettings::TraceProfile = TraceProfile;


	SurfaceNavProxySettings::BlockOverlapTestShape = BlockOverlapTestShape;

	// Default to sphere since sphere tests are slightly faster.
	// Need to make sure the sphere goes out to the corners of the block
	float c1 = sqrt(pow(SurfaceNavProxySettings::PaddedBlockExtent, 2) + pow(SurfaceNavProxySettings::PaddedBlockExtent, 2));
	float c2 = sqrt(pow(c1, 2) + pow(SurfaceNavProxySettings::PaddedBlockExtent, 2));
	SurfaceNavProxySettings::BlockTestShape = FCollisionShape::MakeSphere(c2);

	if (SurfaceNavProxySettings::BlockOverlapTestShape == ESurfaceNavOverlapShape::Box)
	{
		SurfaceNavProxySettings::BlockTestShape = FCollisionShape::MakeBox(FVector(SurfaceNavProxySettings::PaddedBlockExtent));
	}

	// Default to sphere since sphere tests are slightly faster.
	// The sphere only needs to go to the faces of the grid, not the corners.
	SurfaceNavProxySettings::GridPointOverlapTestShape = GridPointOverlapTestShape;
	SurfaceNavProxySettings::GridPointTestShape = FCollisionShape::MakeSphere(SurfaceNavProxySettings::PaddedGridPointDistance);

	if (SurfaceNavProxySettings::GridPointOverlapTestShape == ESurfaceNavOverlapShape::Box)
	{
		SurfaceNavProxySettings::GridPointTestShape = FCollisionShape::MakeBox(FVector(SurfaceNavProxySettings::PaddedGridPointDistance));
	}
	
	SurfaceNavProxySettings::EnhancedNavModBrushTest = EnhancedNavModBrushTest;
}

void ASurfaceNavProxy::SurfaceNavRebuildAll()
{
	// Only allow 1 request to enter beyond this point. Subsequent back to back
	// requests will return immediately.
	if (BlockGridPointQueueCount.Increment() > 1)
		return;

	// we don't want multiple of these ever running at the same time
	FWriteScopeLock wRebuildAllLock(RebuildAllLock);

	RefreshSettings();

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());
	if (NavSys)
		NavSys->SurfaceNavRebuildAll();

	// Reset the count so we can accept the next request
	BlockGridPointQueueCount.Set(0);
}

void ASurfaceNavProxy::DrawDebugNavData(const float InDrawRadius,
	const bool InShowBlocks, const bool InShowGridPoints, const bool InShowNavPoints, 
	const bool InShowNavPointNormals, const bool InShowNeighborDirLines, const bool InShowWhereNavigationEnds,
	const bool InShowNavLinkPoints, const float InDuration) const
{
#if WITH_EDITOR
	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());
	if (NavSys)
		NavSys->DrawDebugNavData(InDrawRadius, InShowBlocks, InShowGridPoints, InShowNavPoints, InShowNavPointNormals, InShowNeighborDirLines, InShowWhereNavigationEnds, InShowNavLinkPoints, InDuration);
#endif
}



// *************************************************************************
// Leave here as this can give useful information when enabled. The problem is
// it's doesn't compile in all modes...may just need the right conditional compilation
// *************************************************************************
// 
//#if WITH_EDITOR
//
//void ASurfaceNavProxy::RefreshDebugSceneProxy(TArray<FDebugSceneProxyData::FDebugText>& DebugTextData)
//{
//	if (!this || !DebugStringsComp)
//		return;
//
//	DebugStringsComp->RefreshDebugSceneProxy(DebugTextData);
//}
//
//#endif
//
//#pragma region Draw Degug String
//
//FDebugSceneProxy::FDebugSceneProxy(const UPrimitiveComponent* InComponent)
//	: FDebugRenderSceneProxy(InComponent)
//{
//	DrawType = EDrawType::SolidAndWireMeshes;
//	ViewFlagName = TEXT("DSN_BlockLabels");
//}
//
//void FDebugTextDelegateHelper::DrawDebugLabels(UCanvas* Canvas, APlayerController* PlayerController)
//{
//	if (!Canvas) return;
//
//	const FColor OldDrawColor = Canvas->DrawColor;
//	Canvas->SetDrawColor(FColor::White);
//	const FSceneView* View = Canvas->SceneView;
//	const UFont* Font = GEngine->GetSmallFont();
//	const FDebugSceneProxyData::FDebugText* DebugText = DebugLabels.GetData();
//
//	for (int32 i = 0; i < DebugLabels.Num(); ++i, ++DebugText)
//	{
//		if (View->ViewFrustum.IntersectSphere(DebugText->Location, 1.0f))
//		{
//			const FVector ScreenLoc = Canvas->Project(DebugText->Location);
//			Canvas->DrawText(Font, DebugText->Text, ScreenLoc.X, ScreenLoc.Y, 1.5f, 1.5f);
//		}
//	}
//
//	Canvas->SetDrawColor(OldDrawColor);
//}
//
//void FDebugTextDelegateHelper::SetupFromProxy(const FDebugSceneProxyData& InProxyData)
//{
//	DebugLabels.Reset();
//	DebugLabels.Append(InProxyData.DebugLabels);
//}
//
//UDebugStrings::UDebugStrings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
//{
//	FEngineShowFlags::RegisterCustomShowFlag(TEXT("DSN_BlockLabels"), false, SFG_Normal,
//		FText::FromString("DSN Block Labels"));
//}
//
//FDebugRenderSceneProxy* UDebugStrings::CreateDebugSceneProxy()
//{
//	return new FDebugSceneProxy(this);
//}
//
//#if UE_ENABLE_DEBUG_DRAWING
//
//void UDebugStrings::RefreshDebugSceneProxy(TArray<FDebugSceneProxyData::FDebugText>& DebugTextData)
//{
//	FDebugSceneProxyData ProxyData;
//	ProxyData.DebugLabels.Append(DebugTextData);
//	DebugDrawDelegateManager.SetupFromProxy(ProxyData);
//}
//
//#endif
//
//#pragma endregion

