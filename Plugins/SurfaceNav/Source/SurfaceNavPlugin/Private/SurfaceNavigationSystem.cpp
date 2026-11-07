// Copyright BMK Games LLC. All Rights Reserved.


#include "SurfaceNavigationSystem.h"
#include "SurfaceNavPlanetComponent.h"
#include "SurfaceNavDefinitions.h"
#include "SurfaceNavPath.h"
#include "SurfaceNavLinkPoint.h"

#include "SurfaceNavProxy.h"

#include "Async/Async.h"
#include "Misc/ScopeRWLock.h"
#include "Navigation/NavLinkProxy.h"
#include "NavLinkCustomComponent.h"
#include "GameFramework/Pawn.h"

#include "EngineGlobals.h"
#include "Engine/Engine.h"

#if WITH_EDITOR
#include "Editor.h"
#include "IPluginWardenModule.h"
#include "Account/IPortalUser.h"
#include "Editor/EditorEngine.h"
#include "Engine/GameViewportClient.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Slate/SceneViewport.h"
#include "Engine/DebugCameraController.h"
#include "GameFramework/CheatManager.h"

#if ENGINE_MAJOR_VERSION < 5
#include "LevelEditorViewport.h"
#else
#include "Subsystems/UnrealEditorSubsystem.h"
#endif

#endif

bool SurfaceNavProxySettings::AllowNavigationGenerationWithoutPlayer = false;
bool SurfaceNavProxySettings::IsFlatWorld = true;
float SurfaceNavProxySettings::BlockSize = 1000.f;
float SurfaceNavProxySettings::BlockExtent = 500.f;
float SurfaceNavProxySettings::BlockExtentPadding = 0.1f;
float SurfaceNavProxySettings::PaddedBlockExtent = 0.f;

float SurfaceNavProxySettings::GridPointDistancePadding = 0.1f;
float SurfaceNavProxySettings::PaddedGridPointDistance = 0.f;

float SurfaceNavProxySettings::FarBlockGenerationRadius = 0.f;
float SurfaceNavProxySettings::NearBlockGenerationRadius = 6000.f;
float SurfaceNavProxySettings::MaxDistanceFromPlayerForGeneration = 10000.f;
float SurfaceNavProxySettings::CleanBlockInterval = 5.f;
int SurfaceNavProxySettings::MaxBlockPoolSize = 0;
int SurfaceNavProxySettings::InitialBlockPoolSize = 0;
float SurfaceNavProxySettings::GridPointDistance = 80.f;
float SurfaceNavProxySettings::GridPointDistanceSq = SurfaceNavProxySettings::GridPointDistance * SurfaceNavProxySettings::GridPointDistance;
int SurfaceNavProxySettings::GridPointsPerBlock = 0;
int SurfaceNavProxySettings::SingleAxisVoxelCount = 0;
int SurfaceNavProxySettings::MaxPathingIterations = 10000;
int SurfaceNavProxySettings::MaxPathRequestsPerTick = 5;
float SurfaceNavProxySettings::ObservedPathsTickInterval = 0.2f;
float SurfaceNavProxySettings::MaxDistanceFromNavigation = 300.f;

//float SurfaceNavProxySettings::MagicLineTraceOffset = 0.f;


float SurfaceNavProxySettings::MaxExpenseModifier = 450.f;
float SurfaceNavProxySettings::ExpenseModifierUpStep = 200.f;
float SurfaceNavProxySettings::ExpenseModifierDownStep = 70.f;
float SurfaceNavProxySettings::MaxExpenseModifierSq = 40000.f;
float SurfaceNavProxySettings::ExpenseModifierUpStepSq = 10000.f;
float SurfaceNavProxySettings::ExpenseModifierDownStepSq = 2500.f;

float SurfaceNavProxySettings::DelayBeforePathFindingAllowed = 1.f;
FCollisionShape SurfaceNavProxySettings::BlockTestShape = FCollisionShape(); // ::DefaultCollisionShape;
FCollisionShape SurfaceNavProxySettings::GridPointTestShape = FCollisionShape(); // ::DefaultCollisionShape;
float SurfaceNavProxySettings::RefreshLocationDataInterval = 0.5f;
float SurfaceNavProxySettings::ProcessComponentUpdatesInterval = 0.5f;
float SurfaceNavProxySettings::ProcessPendingDirtyNavigationInterval = 0.5f;

float SurfaceNavProxySettings::DisplayRadiusFromCamera = 2000.f;
bool SurfaceNavProxySettings::ShowNavPoints = false;
bool SurfaceNavProxySettings::ShowNavGridPoints = false;
bool SurfaceNavProxySettings::ShowNavBlocks = false;
bool SurfaceNavProxySettings::ShowNavPointNormals = false;
bool SurfaceNavProxySettings::ShowNavNeighborDirs = false;
bool SurfaceNavProxySettings::ShowWhereNavigationEnds = false;
bool SurfaceNavProxySettings::ShowNavLinks = false;
bool SurfaceNavProxySettings::ShowDebugNavPointsDuringBuild = false;
bool SurfaceNavProxySettings::ShowDebug_AI_Paths = false;
bool SurfaceNavProxySettings::PerformExtraDebuggingTests = false;
bool SurfaceNavProxySettings::ShowDebugStats = false;

float SurfaceNavProxySettings::TraceSphereRadius = 0.0001f;
float SurfaceNavProxySettings::TraceSphereStartOffset = 0.0f;
float SurfaceNavProxySettings::TraceSphereEndOffset = 0.00004f;
float SurfaceNavProxySettings::MaxComponentSphereRadiusToProcess = 10000.f;
int SurfaceNavProxySettings::MaxRandomRepathOffset = 3;

TEnumAsByte<ESurfaceNavOverlapShape::Type> SurfaceNavProxySettings::BlockOverlapTestShape = ESurfaceNavOverlapShape::Sphere;
TEnumAsByte<ESurfaceNavOverlapShape::Type> SurfaceNavProxySettings::GridPointOverlapTestShape = ESurfaceNavOverlapShape::Sphere;

bool SurfaceNavProxySettings::EnhancedNavModBrushTest = false;

TEnumAsByte<enum ESurfaceNavTraceMethod> SurfaceNavProxySettings::TraceMethod = ESurfaceNavTraceMethod::Profile;
TEnumAsByte<enum ECollisionChannel> SurfaceNavProxySettings::TraceChannel = ECollisionChannel::ECC_Camera;
FString SurfaceNavProxySettings::TraceProfile = "IgnoreOnlyPawn";
uint8 SurfaceNavProxySettings::BackgroundWorkerThreadCount = 4;
FVector SurfaceNavProxySettings::GridOffset = FVector::ZeroVector;

bool SurfaceNavProxySettings::FlagNarrowCorridors = true;
float SurfaceNavProxySettings::NarrowCorridorPercentToFlagSq = 1.0f;

FSurfaceNavPoint* SurfaceNavProxySettings::EmptyNavPoint = nullptr;
FSurfaceNavPointBase* SurfaceNavProxySettings::EmptyNavPointBase = nullptr;

FTraceForNavDelegate SurfaceNavProxySettings::TraceForNavDel;
FCheckPointForSurfaceDelegate SurfaceNavProxySettings::CheckPointForSurfaceDel;


namespace SurfaceNavigationSystemConsts
{
#if WITH_EDITOR
	bool SN_Flag = false;
#endif

	/*void TestTraceForNavDel(bool& Result, FHitResult& HitResult, const FVector& StartLoc, const FVector& EndLoc, const FCollisionShape& Shape, const FCollisionQueryParams& QueryParams)
	{
		Result = true;
		HitResult = FHitResult(StartLoc, EndLoc);
	}

	void CheckPointForSurfaceDel(bool& Result, FIntVector& VectorIndex, FVector& GridPointLoc, FCollisionQueryParams& collisionQueryParams)
	{
		Result = true;
	}*/
}

TWeakObjectPtr<USurfaceNavigationSystem> USurfaceNavigationSystem::CurrentSurfaceNavSystem;

USurfaceNavigationSystem::USurfaceNavigationSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set CheckPointForSurfaceDel to override the logic for the grid point overlap tests 
	//SurfaceNavProxySettings::CheckPointForSurfaceDel.BindStatic(&YOURCUSTOMFUNCTION);

	// Set TraceForNavDel to override the logic for the line traces
	//SurfaceNavProxySettings::TraceForNavDel.BindStatic(&YOURCUSTOMFUNCTION);
}

//----------------------------------------------------------------------//
// helpers
//----------------------------------------------------------------------//
//namespace
//{
//#if ENABLE_VISUAL_LOG
//	void SurfaceNavigationDataDump(const UObject* Object, const FName& CategoryName, const ELogVerbosity::Type Verbosity, const FBox& Box, const UWorld& World, FVisualLogEntry& CurrentEntry)
//	{
//		/*const ANavigationData* MainNavData = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World)->GetDefaultNavDataInstance();
//		const FNavDataGenerator* Generator = MainNavData ? MainNavData->GetGenerator() : nullptr;
//		if (Generator)
//		{
//			Generator->GrabDebugSnapshot(&CurrentEntry, (!Box.IsValid || FMath::IsNearlyZero(Box.GetVolume())) ? MainNavData->GetBounds().ExpandBy(FVector(20, 20, 20)) : Box, CategoryName, Verbosity);
//		}*/
//	}
//#endif // ENABLE_VISUAL_LOG
//}

void USurfaceNavigationSystem::OnSurfaceNavPIEEnd()
{
	//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::OnSurfaceNavPIEEnd: Begin"));

	UWorld* const& MyWorld = GetWorld();
	if (MyWorld)
	{
		if (MyWorld->GetNetMode() >= ENetMode::NM_Client)
			return;

		USurfaceNavigationSystem::CurrentSurfaceNavSystem = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(MyWorld);

		// There may have been some values changes in the proxy which would still be saved at this point so
		// let's tell the proxy that nav needs a rebuild.
		AActor* FoundActor = UGameplayStatics::GetActorOfClass(MyWorld, ASurfaceNavProxy::StaticClass());
		ASurfaceNavProxy* SurfaceNavProxy = Cast<ASurfaceNavProxy>(FoundActor);
		if (SurfaceNavProxy)
			SurfaceNavProxy->SurfaceNavRebuildAll();
	}
	else
		USurfaceNavigationSystem::CurrentSurfaceNavSystem = nullptr;
}

void USurfaceNavigationSystem::DoInitialSetup()
{
//#if WITH_EDITOR
//	UWorld* MyWorld = GetWorld();
//	if (MyWorld->WorldType == EWorldType::Editor)
//	{
//		bSurfaceNavGenerationEnabled = false;
//	}
//#endif

	// This section was in the constructor, but didn't have the world available yet, which we
	// needed to test IsServer in so this section was moved here.
	if (GetWorld() && GetWorld()->GetNetMode() < ENetMode::NM_Client)
	{
		USurfaceNavigationSystem::CurrentSurfaceNavSystem = this;

		struct FSurfaceNavDelegatesInitializer
		{
			FSurfaceNavDelegatesInitializer()
			{
				UNavigationSystemBase::OnActorRegisteredDelegate().BindLambda([](AActor& Actor) { USurfaceNavigationSystem::OnActorRegistered(&Actor); UNavigationSystemV1::OnActorRegistered(&Actor); });
				UNavigationSystemBase::OnActorUnregisteredDelegate().BindLambda([](AActor& Actor) { USurfaceNavigationSystem::OnActorUnregistered(&Actor); UNavigationSystemV1::OnActorUnregistered(&Actor); });
				UNavigationSystemBase::OnComponentRegisteredDelegate().BindLambda([](UActorComponent& Comp) { USurfaceNavigationSystem::OnComponentRegistered(&Comp); UNavigationSystemV1::OnComponentRegistered(&Comp); });
				UNavigationSystemBase::OnComponentUnregisteredDelegate().BindLambda([](UActorComponent& Comp) { USurfaceNavigationSystem::OnComponentUnregistered(&Comp); UNavigationSystemV1::OnComponentUnregistered(&Comp); });
				UNavigationSystemBase::UpdateComponentDataDelegate().BindStatic(&USurfaceNavigationSystem::UpdateComponentData);
				UNavigationSystemBase::UpdateActorDataDelegate().BindStatic(&USurfaceNavigationSystem::UpdateActorData);


#if WITH_EDITOR
				UNavigationSystemBase::OnPIEEndDelegate().BindLambda([](UWorld& World) {
					USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(&World);
					if (NavSys)
					{
						NavSys->OnSurfaceNavPIEEnd();
						NavSys->OnPIEEnd();
					}
					});

#endif

				//#if ENABLE_VISUAL_LOG
				//			FVisualLogger::NavigationDataDumpDelegate.AddStatic(&SurfaceNavigationDataDump);
				//#endif // ENABLE_VISUAL_LOG

			}
		};

		static FSurfaceNavDelegatesInitializer SurfaceNavDelegatesInitializer;
	}

	Super::DoInitialSetup();

	SurfaceNavDoInitialSetup();
}

void USurfaceNavigationSystem::SurfaceNavDoInitialSetup()
{
	//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::SurfaceNavDoInitialSetup: Begin"));
	if (bIsCleaningUp || bCleanUpDone)
		return;

	UWorld* MyWorld = GetWorld();

	// Don't need to continue if not in a valid world
	if (MyWorld == nullptr || MyWorld->bIsTearingDown || MyWorld->GetNetMode() >= ENetMode::NM_Client)
		return;

	//#if WITH_EDITOR
	//	if (!SurfaceNavigationSystemConsts::SN_Flag && GetWorld()->WorldType != EWorldType::Game)
	//	{
	//		char msg[] = SN_PROMPT;
	//		SN_PLUGIN_1(
	//			FText::FromString(TEXT(SN_DATA_0)),
	//			SN_DATA_1,
	//			SN_DATA_2,
	//			SN_CACHE_LEVEL,
	//			FText::FromString(FString(ANSI_TO_TCHAR(msg))),
	//			//FText::GetEmpty(),
	//			SN_ERR_RESPONSE,
	//			[&]() {});
	//
	//		SurfaceNavigationSystemConsts::SN_Flag = true;
	//		//SurfaceNavigationSystemConsts::ErrHandling = SN_ERR_RESPONSE_ALT;
	//	}
	//#endif

	//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::BeginPlay(): Read Lock - DestroyLock"));
	FReadScopeLock rDestoryLock(USurfaceNavigationSystem::DestroyLock);

	check(NavBlocks.Num() == 0);
	/*if (NavBlocks.Num() > 0)
		UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::SurfaceNavDoInitialSetup: There are blocks loaded at init, this could cause problems"));*/


	int PointsPerBlock = FMath::RoundToInt(SurfaceNavProxySettings::BlockSize / SurfaceNavProxySettings::GridPointDistance);
	//int PointsPerBlock = RoundToInt(BlockSize / GridPointDistance);
	PointsPerBlock = PointsPerBlock * PointsPerBlock * PointsPerBlock; // FGenericPlatformMath::Pow(PointsPerBlock, 3);

	// Dividing by 60 is just a guess of how to adjust the pool
	// so it's not too large
	// TODO: Do we even need a pool for grid points
	GridPointPoolSize = 0;

	// Dividing by 10 is just a guess of how to adjust the pool
	// so it's not too large
	// TODO: Do we even need a pool for nav points
	NavPointPoolSize = 0;

	NavPointPool = new FSurfaceNavPointPool();

	GridPointPool = new FSurfaceNavGridPointPool(
		GridPointPoolSize,
		*GetWorld(),
		GridPointFinishedDel,
		*NavPointPool
	);

	SurfaceNavProxySettings::InitialBlockPoolSize = FMath::Min(SurfaceNavProxySettings::MaxBlockPoolSize, SurfaceNavProxySettings::InitialBlockPoolSize);

	BlockPool = new FSurfaceNavBlockPoolComponent(
		SurfaceNavProxySettings::MaxBlockPoolSize,
		SurfaceNavProxySettings::InitialBlockPoolSize,
		//*this,
		GetWorld(),
		ReleaseBlockDel,
		BlockOverlapsFoundDel,
		BlockNavUpdatedDel,
		HandleBlockMarkedDirtyDel,
		ActorsInBlockChangedDel,
		*GridPointPool);

	//SetInitSurfaceNavTimer();

	DelayTryInitSurfaceNav();


	//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::BeginPlay(): End"));

}

void USurfaceNavigationSystem::DelayTryInitSurfaceNav()
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]()
		{
			FPlatformProcess::Sleep(0.001f);
			AsyncTask(ENamedThreads::GameThread, [this]()
				{
					InitSurfaceNav();
				});
		});
}

void USurfaceNavigationSystem::InitSurfaceNav()
{
	if (bIsCleaningUp || bCleanUpDone)
		return;

	check(IsInGameThread());

	UWorld* MyWorld = GetWorld();

	// If world isn't valid then end without retrying init
	if (!MyWorld || MyWorld->bIsTearingDown || MyWorld->GetNetMode() >= ENetMode::NM_Client)
		return;

	//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::Init(): Begin"));

//#if WITH_EDITOR
//
//	bool flag = false;
//
//	if (MyWorld->WorldType != EWorldType::Game)
//	{
//		char msg[] = SN_PROMPT;
//		SN_PLUGIN_1(
//			FText::FromString(TEXT(SN_DATA_0)),
//			SN_DATA_1,
//			SN_DATA_2,
//			SN_CACHE_LEVEL,
//			FText::FromString(FString(ANSI_TO_TCHAR(msg))),
//			SN_ERR_RESPONSE_ALT,
//			[&]() {
//				flag = true;
//			});
//	}
//	else { flag = true; }
//
//	if (!flag)
//	{
//		char msg[] = SN_ALT_HANDLING;
//		UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ANSI_TO_TCHAR(msg)));
//		return;
//	}
//
//#endif

	FReadScopeLock rDestoryLock(USurfaceNavigationSystem::DestroyLock);

	BlockQueue.Empty();
	BlockQueueCount.Set(0);

	BlocksRecentlyProcessed.Empty();
	BlocksPendingAdd.Empty();
	BlocksPendingRefresh.Empty();

	{
		FWriteScopeLock wBlocksInRangeLock(BlocksInRangeLock);
		BlocksInRange.Empty();
	}

	BlockGridPointQueue.Empty();
	BlockGridPointQueueCount.Set(0);

	if (!IsInitialized())
	{
		DelayTryInitSurfaceNav();
		return;
	}

	AActor* FoundActor = nullptr;
	ASurfaceNavProxy* SurfaceNavProxy = Cast<ASurfaceNavProxy>(UGameplayStatics::GetActorOfClass(MyWorld, ASurfaceNavProxy::StaticClass()));

	// If there's no proxy yet then let's create some default values so we don't crash
	if (!SurfaceNavProxy)
	{
		SurfaceNavProxySettings::BlockSize = 1000.f;
		float BlockExtent = SurfaceNavProxySettings::BlockExtent = SurfaceNavProxySettings::BlockSize / 2.0f;

		SurfaceNavProxySettings::GridPointDistance = 80.f;
		SurfaceNavProxySettings::GridPointDistanceSq = FMath::Square(SurfaceNavProxySettings::GridPointDistance);
		SurfaceNavProxySettings::FlagNarrowCorridors = true;
		SurfaceNavProxySettings::NarrowCorridorPercentToFlagSq = SurfaceNavProxySettings::GridPointDistanceSq;

		SurfaceNavProxySettings::SingleAxisVoxelCount = truncf(SurfaceNavProxySettings::BlockSize / SurfaceNavProxySettings::GridPointDistance);
		SurfaceNavProxySettings::GridPointsPerBlock = SurfaceNavProxySettings::SingleAxisVoxelCount * SurfaceNavProxySettings::SingleAxisVoxelCount * SurfaceNavProxySettings::SingleAxisVoxelCount;
		SurfaceNavProxySettings::GridPointDistance = SurfaceNavProxySettings::BlockSize / SurfaceNavProxySettings::SingleAxisVoxelCount;
		SurfaceNavProxySettings::PaddedGridPointDistance = SurfaceNavProxySettings::GridPointDistance + SurfaceNavProxySettings::GridPointDistancePadding;

		SurfaceNavProxySettings::PaddedBlockExtent = SurfaceNavProxySettings::BlockExtent + SurfaceNavProxySettings::GridPointDistance / 2.f + SurfaceNavProxySettings::BlockExtentPadding;

		float MaxExpenseModifierSq = FMath::Square(450.f);
		float ExpenseModifierUpStepSq = FMath::Square(200.f);
		float ExpenseModifierDownStepSq = FMath::Square(70.f);

		SurfaceNavProxySettings::MaxExpenseModifierSq = MaxExpenseModifierSq;
		SurfaceNavProxySettings::ExpenseModifierUpStepSq = ExpenseModifierUpStepSq;
		SurfaceNavProxySettings::ExpenseModifierDownStepSq = ExpenseModifierDownStepSq;

		SurfaceNavProxySettings::AllowNavigationGenerationWithoutPlayer = false;
		SurfaceNavProxySettings::IsFlatWorld = true;
		SurfaceNavProxySettings::FarBlockGenerationRadius = 0.f;
		SurfaceNavProxySettings::NearBlockGenerationRadius = 6000.f;
		SurfaceNavProxySettings::MaxDistanceFromPlayerForGeneration = 10000.f;
		SurfaceNavProxySettings::CleanBlockInterval = 5.f;
		SurfaceNavProxySettings::MaxPathingIterations = 10000;
		SurfaceNavProxySettings::MaxPathRequestsPerTick = 5;
		SurfaceNavProxySettings::ObservedPathsTickInterval = 0.2f;
		SurfaceNavProxySettings::MaxDistanceFromNavigation = 300.f;
		SurfaceNavProxySettings::ShowDebugNavPointsDuringBuild = false;
		SurfaceNavProxySettings::ShowDebug_AI_Paths = false;
		SurfaceNavProxySettings::MaxExpenseModifier = 450.f;
		SurfaceNavProxySettings::ExpenseModifierUpStep = 200.f;
		SurfaceNavProxySettings::ExpenseModifierDownStep = 70.f;
		SurfaceNavProxySettings::DelayBeforePathFindingAllowed = 1.f;
		SurfaceNavProxySettings::RefreshLocationDataInterval = 0.5f;
		SurfaceNavProxySettings::ProcessComponentUpdatesInterval = 0.5f;
		SurfaceNavProxySettings::ProcessPendingDirtyNavigationInterval = 0.5f;
		SurfaceNavProxySettings::ShowNavPoints = false;
		SurfaceNavProxySettings::ShowNavGridPoints = false;
		SurfaceNavProxySettings::ShowNavBlocks = false;
		SurfaceNavProxySettings::ShowNavNeighborDirs = false;
		SurfaceNavProxySettings::ShowWhereNavigationEnds = false;
		SurfaceNavProxySettings::ShowNavLinks = false;
		SurfaceNavProxySettings::ShowNavPointNormals = false;
		SurfaceNavProxySettings::DisplayRadiusFromCamera = 2000.f;
		SurfaceNavProxySettings::MaxComponentSphereRadiusToProcess = 100000.f;

		SurfaceNavProxySettings::TraceSphereRadius = 0.0001f;
		SurfaceNavProxySettings::TraceSphereStartOffset = 0.0f;
		SurfaceNavProxySettings::TraceSphereEndOffset = 0.00004f;

		// Default to sphere since sphere tests are slightly faster.
		// Need to make sure the sphere goes out to the corners of the block
		SurfaceNavProxySettings::BlockOverlapTestShape = ESurfaceNavOverlapShape::Sphere;
		float c1 = sqrt(pow(SurfaceNavProxySettings::PaddedBlockExtent, 2) + pow(SurfaceNavProxySettings::PaddedBlockExtent, 2));
		float c2 = sqrt(pow(c1, 2) + pow(SurfaceNavProxySettings::PaddedBlockExtent, 2));
		SurfaceNavProxySettings::BlockTestShape = FCollisionShape::MakeSphere(c2);

		// Default to sphere since sphere tests are slightly faster.
		// The sphere only needs to go to the faces of the grid, not the corners.
		SurfaceNavProxySettings::GridPointOverlapTestShape = ESurfaceNavOverlapShape::Sphere;
		SurfaceNavProxySettings::GridPointTestShape = FCollisionShape::MakeSphere(SurfaceNavProxySettings::PaddedGridPointDistance);
	}

	MyWorld->GetTimerManager().SetTimer(
		TimerProcessPendingDirtyHandle,
		this,
		&USurfaceNavigationSystem::ProcessPendingDirtyQueue,
		SurfaceNavProxySettings::ProcessPendingDirtyNavigationInterval,
		true,
		1.f
	);

	MyWorld->GetTimerManager().SetTimer(
		TimerDelayedCleanupObjectsHandle,
		this,
		&USurfaceNavigationSystem::ProcessDelayedCleanupTasksAsync,
		10.f,
		true,
		10.f
	);

	SetUpdateInvokerLocationsTimer();
	SetProcessUpdatedComponentsTimer();

	// Let's enable path finding after the specified delay
	MyWorld->GetTimerManager().SetTimer(
		TimerDelayPathfindingHandle,
		this,
		&USurfaceNavigationSystem::FlagIsReadyToPathfind,
		SurfaceNavProxySettings::DelayBeforePathFindingAllowed,
		false,
		SurfaceNavProxySettings::DelayBeforePathFindingAllowed
	);

	OnComponentUpdated().Clear();
	OnComponentUpdated().AddLambda([this](const UActorComponent& Comp) {
		//NavigableComponentUpdated(Comp);
		});

	BlockOverlapsFoundDel.Unbind();
	BlockOverlapsFoundDel.BindLambda([this](const FIntVector& BlockKey)
		{
			BlockOverlapsFoundCallback(BlockKey);
		});

	ReleaseBlockDel.Unbind();
	ReleaseBlockDel.BindLambda([this](const FIntVector& BlockKey)
		{
			ReleaseBlockCallback(BlockKey);
		});

	BlockNavUpdatedDel.Unbind();
	BlockNavUpdatedDel.BindLambda([this](const FIntVector& IntLoc)
		{
			BlockNavUpdatedCallback(IntLoc);
		});

	HandleBlockMarkedDirtyDel.Unbind();
	HandleBlockMarkedDirtyDel.BindLambda([this](const FIntVector& IntLoc)
		{
			BlockMarkedDirtyCallback(IntLoc);
		});

	ActorsInBlockChangedDel.Unbind();
	ActorsInBlockChangedDel.BindLambda([this](const FIntVector& IntLoc, const FVector& BlockLocation, const FVector& CompVelocity, const FBox& CompBounds, const bool& CompIsBeingDestroyed)
		{
			//NavigableComponentUpdated(CompVelocity, CompBounds, CompIsBeingDestroyed);
		});

	GridPointFinishedDel.Unbind();
	GridPointFinishedDel.BindLambda([this](FSurfaceNavGridPoint* GridPoint, int Phase)
		{
			GridPointFinished(GridPoint, Phase);
		});

	QueueBlocksNearInvokersDel.Unbind();
	QueueBlocksNearInvokersDel.BindLambda([this](uint8 ThreadId)
		{
			PerformBackgroundWork(ThreadId);
		});

	BlockQueueWorker1 = new SurfaceNavBlockQueueWorker(1, QueueBlocksNearInvokersDel);

	if (SurfaceNavProxySettings::BackgroundWorkerThreadCount > 1)
		BlockQueueWorker2 = new SurfaceNavBlockQueueWorker(2, QueueBlocksNearInvokersDel);

	if (SurfaceNavProxySettings::BackgroundWorkerThreadCount > 2)
		BlockQueueWorker3 = new SurfaceNavBlockQueueWorker(3, QueueBlocksNearInvokersDel);

	if (SurfaceNavProxySettings::BackgroundWorkerThreadCount > 3)
		BlockQueueWorker4 = new SurfaceNavBlockQueueWorker(4, QueueBlocksNearInvokersDel);

	// For some reason nav mod volumes sometimes register themselves before the navigation object
	// is ready. So we need to find them all and register them just in case.
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANavModifierVolume::StaticClass(), FoundActors);
	for (auto& NavMod : FoundActors)
	{
		RegisterNavModVolume(Cast<ANavModifierVolume>(NavMod));
	}

	bInitDone = true;

	//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::Init(): End"));
}

void USurfaceNavigationSystem::CleanUp(const FNavigationSystem::ECleanupMode Mode)
{
	FWriteScopeLock wCleanupLock(CleanupLock);

	if (GetWorld()->GetNetMode() < ENetMode::NM_Client || bInitDone)
	{
		bool DoCleanup = NavBlocks.Num() > 0 || !CleanupBlocksLvl1Queue.IsEmpty() || !CleanupBlocksLvl2Queue.IsEmpty() || !NavBlocksReadyToDelete.IsEmpty() || !NavBlocksPendingDelete.IsEmpty();
		while (DoCleanup)
		{
			SurfaceNavCleanUp(false);
			DoCleanup = NavBlocks.Num() > 0 || !CleanupBlocksLvl1Queue.IsEmpty() || !CleanupBlocksLvl2Queue.IsEmpty() || !NavBlocksReadyToDelete.IsEmpty() || !NavBlocksPendingDelete.IsEmpty();
		}

		// Need to call 1 more time to finalize the cleanup
		SurfaceNavCleanUp();
	}

	Super::CleanUp(Mode);
}

void USurfaceNavigationSystem::SurfaceNavCleanUp(bool FinalizeCleanup)
{
	if (GetWorld()->GetNetMode() >= ENetMode::NM_Client && !bInitDone)
		return;

	//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::SurfaceNavCleanUp: Begin"));
		//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::BeginDestroy(): Write Lock - GetDestroyLock"));
		//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::SurfaceNavCleanUp(): Entered Write Lock - DestroyLock"));

	bIsCleaningUp = true;

	// Lock to force wait until runner thread processing is done.
	// This may get optimized away
	{
		FWriteScopeLock wDestoryLock(USurfaceNavigationSystem::DestroyLock);
	}

	if (BlockQueueWorker1)
		BlockQueueWorker1->Stop();

	if (BlockQueueWorker2)
		BlockQueueWorker2->Stop();

	if (BlockQueueWorker3)
		BlockQueueWorker3->Stop();

	if (BlockQueueWorker4)
		BlockQueueWorker4->Stop();

	int MaxWaitCount = 10;
	int WaitCount = 0;
	while (ThreadsProcessingCount.GetValue() > 0 && WaitCount < MaxWaitCount)
	{
		FPlatformProcess::Sleep(0.0005f);
		WaitCount++;
	}

	// Lock to force wait until runner thread processing is done.
	// This may get optimized away
	{
		FWriteScopeLock wDestoryLock(USurfaceNavigationSystem::DestroyLock);
	}


	if (BlockQueueWorker1)
	{
		delete BlockQueueWorker1;
		BlockQueueWorker1 = nullptr;
	}

	if (BlockQueueWorker2)
	{
		delete BlockQueueWorker2;
		BlockQueueWorker2 = nullptr;
	}

	if (BlockQueueWorker3)
	{
		delete BlockQueueWorker3;
		BlockQueueWorker3 = nullptr;
	}

	if (BlockQueueWorker4)
	{
		delete BlockQueueWorker4;
		BlockQueueWorker4 = nullptr;
	}

	FWriteScopeLock wDestoryLock(USurfaceNavigationSystem::DestroyLock);

	if (bCleanUpDone)
		return;

	if (IsInGameThread())
	{
		// Stop all timers first so other processing isn't happening
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	}
	else if (!bCleanUpDone && GetWorld() && !GetWorld()->bIsTearingDown)
	{
		// This must be done on the game thread
		AsyncTask(ENamedThreads::GameThread, [this]()
			{
				if (!bCleanUpDone && GetWorld() && !GetWorld()->bIsTearingDown)
					// Stop all timers first so other processing isn't happening
					GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
			});
	}

	OnComponentUpdated().Clear();

	{
		// Destroy the custom links
		FWriteScopeLock wNavLinkLock(SurfaceNavCustomLinkLock);
		for (auto& CustomLinkPair : SurfaceNavCustomLinksMap)
		{
			CustomLinkPair.Value.Destroy();
		}
		SurfaceNavCustomLinksMap.Empty();
	}

	{
		// Empty ActivePaths
		FScopeLock PathLock(&ActivePathsLock);
		ActivePaths.Empty();
	}

	VolumeBoundsChangedQueue.Empty();
	RefreshingVolumeCounter.Set(0);

	BlockQueue.Empty();
	BlockQueueCount.Set(0);

	BlocksRecentlyProcessed.Empty();
	BlocksPendingAdd.Empty();
	BlocksPendingRefresh.Empty();

	{
		FWriteScopeLock wBlocksInRangeLock(BlocksInRangeLock);
		BlocksInRange.Empty();
	}

	{
		FWriteScopeLock wLock(VolumeBoundLock);
		for (auto& NavVolBound : NavVolumeBounds)
		{
			NavVolBound.SurfaceNavBlockIndices.Empty();
		}
		NavVolumeBounds.Empty();
	}

	BlockGridPointQueue.Empty();
	BlockGridPointQueueCount.Set(0);

	UpdatedComponentsQueue.Empty();
	ReprocessComponents.Empty();

	// This will put blocks into the lvl1 block cleanup queue
	ReleaseAllBlocks();

	// Moves the blocks from cleaup queue 1 to 2
	ProcessCleanupQueues();
	// Process the blocks in queue 2
	CleanupBlocks(INT_MAX);
	// Processing the blocks has added gridpoints to cleanup queue 1
	// so move those to 2
	ProcessCleanupQueues();
	// Finally, process gridpoint cleanup queue 2
	CleanupBlocks(INT_MAX);

	// Call twice because there's two queue levels for deletes as well
	ProcessDeletesQueues();
	ProcessDeletesQueues();

	// Make sure the timers have been cleared before continuing
	/*while (!TimerClearComplete)
	{
		FPlatformProcess::Sleep(0.001f);
	}*/

	if (FinalizeCleanup)
	{
		delete NavPointPool;
		NavPointPool = nullptr;

		delete GridPointPool;
		GridPointPool = nullptr;

		delete BlockPool;
		BlockPool = nullptr;
	}
}

void USurfaceNavigationSystem::SurfaceNavRebuildAll()
{
	if (!bInitDone || bCleanUpDone || bIsCleaningUp)
		return;

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]()
		{
			FWriteScopeLock wCleanupLock(CleanupLock);

			{
				FWriteScopeLock wDestoryLock(USurfaceNavigationSystem::DestroyLock);

				if (!bInitDone || bCleanUpDone || bIsCleaningUp)
					return;

				bInitDone = false;
				bNavHasGenerated = false;
				bIsReadyToPathfind = false;
			}

			bool DoCleanup = NavBlocks.Num() > 0 || !CleanupBlocksLvl1Queue.IsEmpty() || !CleanupBlocksLvl2Queue.IsEmpty() || !NavBlocksReadyToDelete.IsEmpty() || !NavBlocksPendingDelete.IsEmpty();
			while (DoCleanup)
			{
				SurfaceNavCleanUp(false);
				DoCleanup = NavBlocks.Num() > 0 || !CleanupBlocksLvl1Queue.IsEmpty() || !CleanupBlocksLvl2Queue.IsEmpty() || !NavBlocksReadyToDelete.IsEmpty() || !NavBlocksPendingDelete.IsEmpty();
			}

			// Need to call 1 more time to finalize the cleanup
			SurfaceNavCleanUp();

			// Reset cleaning up since we were just rebuilding
			bIsCleaningUp = false;

			AsyncTask(ENamedThreads::GameThread, [this]()
				{
					SurfaceNavDoInitialSetup();
				});
		});
}

void USurfaceNavigationSystem::DisableSurfaceNavGeneration()
{
	if (!bInitDone || bCleanUpDone || bIsCleaningUp)
		return;

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]()
		{
			FWriteScopeLock wCleanupLock(CleanupLock);

			if (!bSurfaceNavGenerationEnabled)
				return;

			bSurfaceNavGenerationEnabled = false;

			{
				FWriteScopeLock wDestoryLock(USurfaceNavigationSystem::DestroyLock);

				if (!bInitDone || bCleanUpDone || bIsCleaningUp)
					return;

				bInitDone = false;
			}

			bool DoCleanup = NavBlocks.Num() > 0 || !CleanupBlocksLvl1Queue.IsEmpty() || !CleanupBlocksLvl2Queue.IsEmpty() || !NavBlocksReadyToDelete.IsEmpty() || !NavBlocksPendingDelete.IsEmpty();
			while (DoCleanup)
			{
				SurfaceNavCleanUp(false);
				DoCleanup = NavBlocks.Num() > 0 || !CleanupBlocksLvl1Queue.IsEmpty() || !CleanupBlocksLvl2Queue.IsEmpty() || !NavBlocksReadyToDelete.IsEmpty() || !NavBlocksPendingDelete.IsEmpty();
			}

			// Need to call 1 more time to finalize the cleanup
			SurfaceNavCleanUp();

			// Reset cleaning up since we were just rebuilding
			bIsCleaningUp = false;
		});
}

void USurfaceNavigationSystem::EnableSurfaceNavGeneration()
{
	FWriteScopeLock wCleanupLock(CleanupLock);

	if (bSurfaceNavGenerationEnabled)
		return;

	bSurfaceNavGenerationEnabled = true;

	SurfaceNavDoInitialSetup();
}

void USurfaceNavigationSystem::ReleaseAllBlocks()
{
	//FWriteScopeLock wNavBlocksLock(NavBlocksLock);

	//for (TPair<FIntVector, FSurfaceNavBlock*>& NavBlockPair : NavBlocks)
	for (auto It = NavBlocks.CreateIterator(); It; ++It)
	{
		FIntVector BlockIndex = It->Key;
		FSurfaceNavBlock*& NavBlock = It->Value;

		// We need to invalidate the timestamp to make sure the block can be released
		It->Value->InvalidateBlockProcessing();

		//ReleaseBlockCallback(It->Key);
	}
}

void USurfaceNavigationSystem::Tick(float DeltaSeconds)
{
	//double StartTime = FPlatformTime::Seconds();

	Super::Tick(DeltaSeconds);

	UWorld* MyWorld = GetWorld();

	if (!bInitDone || bCleanUpDone || MyWorld == nullptr || MyWorld->GetNetMode() >= ENetMode::NM_Client ||
		(bTickWhilePaused == false && MyWorld->IsPaused()))
		return;

#if WITH_EDITOR
	// If playing in PIE, then skip processing for editor
	if (IsEditorInstanceWhileRunningPIE())
		return;
#endif 

	CurPathingCount = 0;

	PurgeUnusedPaths();
	ProcessObservedPaths(DeltaSeconds);
	ProcessBlockWithUpdatedNav();
	ProcessRepathRequests();


#if WITH_EDITOR

	if ((SurfaceNavProxySettings::ShowNavBlocks || SurfaceNavProxySettings::ShowNavGridPoints ||
		SurfaceNavProxySettings::ShowNavPoints || SurfaceNavProxySettings::ShowNavNeighborDirs ||
		SurfaceNavProxySettings::ShowWhereNavigationEnds || SurfaceNavProxySettings::ShowNavLinks ||
		SurfaceNavProxySettings::ShowNavPointNormals)) // &&
		//MyWorld->WorldType == EWorldType::Editor)
	{
		DrawDebugNavData(
			SurfaceNavProxySettings::DisplayRadiusFromCamera,
			SurfaceNavProxySettings::ShowNavBlocks,
			SurfaceNavProxySettings::ShowNavGridPoints,
			SurfaceNavProxySettings::ShowNavPoints,
			SurfaceNavProxySettings::ShowNavPointNormals,
			SurfaceNavProxySettings::ShowNavNeighborDirs,
			SurfaceNavProxySettings::ShowWhereNavigationEnds,
			SurfaceNavProxySettings::ShowNavLinks,
			-1.f);
	}
#endif

	/*auto TimeSpent = FPlatformTime::Seconds() - StartTime;
	if (TimeSpent >= 0.0007f)
		UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::Tick: Code executed in %f seconds."), TimeSpent);*/
		//GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("USurfaceNavigationSystem::Tick: Code executed in %f seconds."), TimeSpent));
}

//----------------------------------------------------------------------//
// Blueprint functions
//----------------------------------------------------------------------//
USurfaceNavigationSystem* USurfaceNavigationSystem::GetSurfaceNavigationSystem(UObject* WorldContextObject)
{
	return FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(WorldContextObject);
}

void USurfaceNavigationSystem::UpdateActorData(AActor& Actor)
{
	INavRelevantInterface* NavElement = Cast<INavRelevantInterface>(&Actor);
	if (NavElement)
	{
		USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(Actor.GetWorld());
		if (NavSys)
		{
			// TODO: Need something here? 
			// 
			//NavSys->UpdateNavOctreeElement(&Actor, NavElement, FNavigationOctreeController::OctreeUpdate_Default);
		}
	}

	// Just want to see if we ever make it here.
	// May be able to remove this function.
	//check(false);
	//NavSys->OnComponentUpdated().Broadcast(Comp);
}

void USurfaceNavigationSystem::UpdateComponentData(UActorComponent& Comp)
{
	AActor* OwnerActor = Comp.GetOwner();
	if (!OwnerActor)
		return;

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(OwnerActor->GetWorld());
	if (!NavSys)
		return;

	FComponentDataCacheItem* CacheItem = NavSys->ComponentDataCache.Find(&Comp);

	if (!CacheItem)
	{
		// Not found, let's add it
		OnComponentRegistered(&Comp);
		return;
	}
	else
	{
		if (!OwnerActor->IsComponentRelevantForNavigation(&Comp) || !Comp.IsNavigationRelevant() || OwnerActor->IsA<APawn>())
		{
			// No longer nav relevant so unregister and stop processing
			OnComponentUnregistered(&Comp);
			return;
		}

		// We will only get here if the comp is nav relevant and is a scene component
		if (const USceneComponent* SceneComponent = Cast<USceneComponent>(&Comp))
		{
			// If the component is huge, then we can't update
			if (SceneComponent->Bounds.SphereRadius > SurfaceNavProxySettings::MaxComponentSphereRadiusToProcess)
				return;

			FBox BoxBounds = SceneComponent->Bounds.GetBox();

			NavSys->NavigableComponentUpdated(FVector::ZeroVector, CacheItem->Bounds, false);

			CacheItem->Bounds = BoxBounds;
			NavSys->NavigableComponentUpdated(FVector::ZeroVector, BoxBounds, false);
		}
	}
}

void USurfaceNavigationSystem::OnActorRegistered(AActor* Actor)
{
	if (Actor == nullptr)
		return;

	auto ModVolume = Cast<ANavModifierVolume>(Actor);

	if (!ModVolume)
		return;

	//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::OnActorRegistered: Begin registering nav mod volume"));

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(Actor->GetWorld());
	if (!NavSys)
	{
		//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::OnActorRegistered: NavSys not found yet"));
		return;
	}

	NavSys->RegisterNavModVolume(ModVolume);

	auto Comp = Actor->GetRootComponent();
	if (!Comp)
	{
		//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::OnActorRegistered: No root component found"));

		return;
	}

	NavSys->OnComponentUpdated().Broadcast(*Comp);
}

void USurfaceNavigationSystem::OnActorUnregistered(AActor* Actor)
{
	if (Actor == nullptr)
		return;

	auto ModVolume = Cast<ANavModifierVolume>(Actor);
	if (!ModVolume)
		return;

	//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::OnActorUnregistered: Begin un-registering nav mod volume"));

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(Actor->GetWorld());
	if (!NavSys)
	{
		//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::OnActorUnregistered: NavSys not found yet"));

		return;
	}

	NavSys->UnregisterNavModVolume(ModVolume);

	auto Comp = Actor->GetRootComponent();
	if (!Comp)
	{
		//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::OnActorUnregistered: No root component found"));

		return;
	}

	NavSys->OnComponentUpdated().Broadcast(*Comp);
}

void USurfaceNavigationSystem::OnComponentRegistered(UActorComponent* Comp)
{
	if (Comp == nullptr)
		return;

	AActor* OwnerActor = Comp->GetOwner();
	if (!OwnerActor || !OwnerActor->IsComponentRelevantForNavigation(Comp) || !Comp->IsNavigationRelevant() || OwnerActor->IsA<APawn>())
		return;

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(OwnerActor->GetWorld());
	if (!NavSys)
		return;

	if (UNavModifierComponent* ModComp = Cast<UNavModifierComponent>(Comp))
	{
		// We process modifier components a bit different
		// TODO: Need to re-evaluate this since we migrated code and refactored some things
		NavSys->RegisterNavModComponent(ModComp);
	}
	else if (const USceneComponent* SceneComponent = Cast<USceneComponent>(Comp))
	{
		// If the component is huge, then we shouldn't try to update.
		if (SceneComponent->Bounds.SphereRadius > SurfaceNavProxySettings::MaxComponentSphereRadiusToProcess)
			return;

		FBox BoxBounds = SceneComponent->Bounds.GetBox();
		NavSys->ComponentDataCache.Emplace(Comp, FComponentDataCacheItem(BoxBounds));
		NavSys->NavigableComponentUpdated(FVector::ZeroVector, BoxBounds, false);
	}
}

void USurfaceNavigationSystem::OnComponentUnregistered(UActorComponent* Comp)
{
	if (Comp == nullptr)
		return;

	AActor* OwnerActor = Comp->GetOwner();
	if (!OwnerActor)
		return;

	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(OwnerActor->GetWorld());
	if (!NavSys)
		return;

	FComponentDataCacheItem CacheItem;
	bool found = NavSys->ComponentDataCache.RemoveAndCopyValue(Comp, CacheItem);

	if (UNavModifierComponent* ModComp = Cast<UNavModifierComponent>(Comp))
	{
		NavSys->UnregisterNavModComponent(ModComp);
	}
	else if (found)
	{
		if (const USceneComponent* SceneComponent = Cast<USceneComponent>(Comp))
		{
			FBox BoxBounds = SceneComponent->Bounds.GetBox();

			// Go ahead and update the navigation within the bounds of the comp
			NavSys->NavigableComponentUpdated(FVector::ZeroVector, BoxBounds, false);

			// If the comp's prior bounds are different, then update those as well
			if (!(CacheItem.Bounds == BoxBounds))
				NavSys->NavigableComponentUpdated(FVector::ZeroVector, CacheItem.Bounds, false);
		}
	}
}

void USurfaceNavigationSystem::RegisterSurfaceNavLinkComponent(USurfaceNavLinkComponent* NavLink)
{
	SurfaceNavLinkComponents.Emplace(NavLink);
}

void USurfaceNavigationSystem::UnregisterSurfaceNavLinkComponent(USurfaceNavLinkComponent* NavLink)
{
	SurfaceNavLinkComponents.Remove(NavLink);
}

void USurfaceNavigationSystem::RegisterSurfaceNavPlanetComponent(USurfaceNavPlanetComponent* PlanetComp)
{
	FWriteScopeLock wLock(NavPlanetComponentsLock);

	SurfaceNavPlanetComponents.Emplace(PlanetComp);
}

void USurfaceNavigationSystem::UnregisterSurfaceNavPlanetComponent(USurfaceNavPlanetComponent* PlanetComp)
{
	FWriteScopeLock wLock(NavPlanetComponentsLock);

	SurfaceNavPlanetComponents.Remove(PlanetComp);
}

void USurfaceNavigationSystem::RegisterSurfaceNavInvoker(AActor*& Invoker)
{
	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(Invoker->GetWorld());
	if (NavSys)
	{
		NavSys->_RegisterSurfaceNavInvoker(Invoker);
	}
}

void USurfaceNavigationSystem::UnregisterNavigationInvoker(AActor*& Invoker)
{
	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(Invoker->GetWorld());
	if (NavSys)
	{
		NavSys->_UnregisterSurfaceNavInvoker(Invoker);
	}
}

void USurfaceNavigationSystem::_RegisterSurfaceNavInvoker(AActor*& Invoker)
{
	FWriteScopeLock wLock(NavInvokerLock);

	SurfaceNavInvokers.Emplace(Invoker);
}

void USurfaceNavigationSystem::_UnregisterSurfaceNavInvoker(AActor*& Invoker)
{
	FWriteScopeLock wLock(NavInvokerLock);

	SurfaceNavInvokers.Remove(Invoker);
}


//----------------------------------------------------------------------//
// Public querying interface                                                                
//----------------------------------------------------------------------//
FSurfaceNavPathResult USurfaceNavigationSystem::FindSurfacePathSync(const FNavAgentProperties& AgentProperties, FSurfaceNavPathFindingQueryData& Query) //, EPathFindingMode::Type Mode)
{
	//SCOPE_CYCLE_COUNTER(STAT_Navigation_PathfindingSync);
	//CSV_SCOPED_TIMING_STAT(NavigationSystem, PathfindingSync);

	/*if (Query.NavData.IsValid() == false)
	{
		Query.NavData = GetNavDataForProps(AgentProperties, Query.StartLocation);
	}*/

	// Just want to see if this ever happens
	check(false);

	// TODO: Use builder to find path
	FSurfaceNavPathResult Result(ENavigationQueryResult::Error);


	/*if (Query.NavData.IsValid())
	{
		if (Mode == EPathFindingMode::Hierarchical)
		{
			Result = Query.NavData->FindHierarchicalPath(AgentProperties, Query);
		}
		else
		{
			Result = Query.NavData->FindPath(AgentProperties, Query);
		}
	}*/

	return Result;
}

FSurfaceNavPathResult USurfaceNavigationSystem::FindSurfacePathSync(FSurfaceNavPathFindingQueryData& Query) //, EPathFindingMode::Type Mode)
{
	//SCOPE_CYCLE_COUNTER(STAT_Navigation_PathfindingSync);
	//CSV_SCOPED_TIMING_STAT(NavigationSystem, PathfindingSync);

	/*if (Query.NavData.IsValid() == false)
	{
		Query.NavData = GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	}*/


	// TODO: Use builder to find path
	FSurfaceNavPathResult Result(ENavigationQueryResult::Error);

	Result = FindPath(Query);

	//if (Query.NavData.IsValid())
	//{
	//	if (Mode == EPathFindingMode::Regular)
	//	{
	//		Result = Query.NavData->FindPath(Query.NavAgentProperties, Query);
	//	}
	//	else // EPathFindingMode::Hierarchical
	//	{
	//		Result = Query.NavData->FindHierarchicalPath(Query.NavAgentProperties, Query);
	//	}
	//}

	return Result;
}

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
TSet<const UClass*> USurfaceNavigationSystem::GetNavAreaClasses()
#else
TSet<TObjectPtr<const UClass>> USurfaceNavigationSystem::GetNavAreaClasses()
#endif
{
	return NavAreaClasses;
}

bool USurfaceNavigationSystem::ProjectPointToNavigation(const FVector& Point, const FVector& NavSourcePoint,
	const bool ForceNormalToDirFromSurface, TSharedPtr<FSurfaceNavPointBase>& OutLocation, const float& MinAngleFromGround,
	const float& MaxAngleFromGround, const float& AcceptanceRadius, UNavigationQueryFilter* FilterClass) const
{
	//SCOPE_CYCLE_COUNTER(STAT_Navigation_QueriesTimeSync);

	return ProjectPoint(Point, NavSourcePoint, Point, ForceNormalToDirFromSurface, OutLocation, MinAngleFromGround, MaxAngleFromGround, AcceptanceRadius, true /* wide search */, false);
}


float USurfaceNavigationSystem::FindClosestPlanet(const AActor* CurrentActor, AActor*& PlanetActor)
{
	float ClosestDistSq = INFINITY;

	if (CurrentActor)
		return FindClosestPlanet(CurrentActor->GetActorLocation(), PlanetActor);

	return ClosestDistSq;
}

float USurfaceNavigationSystem::FindClosestPlanet(const FVector& InLocation, AActor*& PlanetActor)
{
	float ClosestDistSq = INFINITY;

	if (!SurfaceNavProxySettings::IsFlatWorld)
	{
		FReadScopeLock rLock(NavPlanetComponentsLock);

		auto PlanetComps = GetSurfaceNavPlanetComponents();

		AActor* TempPlanetActor = nullptr;

		// Find the closest planet to this blocks location
		for (TWeakObjectPtr<USurfaceNavPlanetComponent>& PlanetComp : PlanetComps)
		{

			if (PlanetComp.IsStale(true, true) || !PlanetComp.IsValid() || !PlanetComp.Get() || !PlanetComp->IsRegistered() || !PlanetComp->IsActive() || !PlanetComp->HasBeenCreated() || PlanetComp->IsBeingDestroyed())
				continue;

			AActor* OwnerPlanetActor = PlanetComp->GetOwner();

			//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::FindClosestPlanet: Angle outside range %d"), OwnerPlanetActor->StaticClass()->typ);

			if (!OwnerPlanetActor)
				continue;

			FVector LocalPlanetLoc = OwnerPlanetActor->GetActorLocation();

			float PlanetDistSq = FVector::DistSquared(InLocation, LocalPlanetLoc);
			if (PlanetDistSq < ClosestDistSq)
			{
				ClosestDistSq = PlanetDistSq;
				TempPlanetActor = OwnerPlanetActor;
			}
		}

		if (ClosestDistSq < INFINITY && TempPlanetActor)
			PlanetActor = TempPlanetActor;
	}

	return ClosestDistSq;
}

void USurfaceNavigationSystem::RegisterNavModVolume(ANavModifierVolume* NavModVolume)
{
	FWriteScopeLock wLock(NavModVolumesLock);
	NavModVolumes.Emplace(NavModVolume);
}

void USurfaceNavigationSystem::UnregisterNavModVolume(ANavModifierVolume* NavModVolume)
{
	FWriteScopeLock wLock(NavModVolumesLock);
	NavModVolumes.Remove(NavModVolume);
}

void USurfaceNavigationSystem::RegisterNavModComponent(UNavModifierComponent* NavModComp)
{
	FWriteScopeLock wLock(NavModComponentsLock);
	NavModComponents.Emplace(NavModComp);
}

void USurfaceNavigationSystem::UnregisterNavModComponent(UNavModifierComponent* NavModComp)
{
	FWriteScopeLock wLock(NavModComponentsLock);
	NavModComponents.Remove(NavModComp);
}


#if WITH_EDITOR
void USurfaceNavigationSystem::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

// Leaving this here for future reference
void USurfaceNavigationSystem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//static const FName NAME_EnableActiveTiles = GET_MEMBER_NAME_CHECKED(UNavigationSystemV1, bGenerateNavigationOnlyAroundNavigationInvokers);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	/*if (PropertyChangedEvent.Property)
	{
		FName PropName = PropertyChangedEvent.Property->GetFName();
		if (PropName == NAME_EnableActiveTiles)
		{
			if (DefaultOctreeController.NavOctree.IsValid())
			{
				DefaultOctreeController.NavOctree->SetDataGatheringMode(DataGatheringMode);
			}

			for (auto NavData : NavDataSet)
			{
				if (NavData)
				{
					NavData->RestrictBuildingToActiveTiles(bGenerateNavigationOnlyAroundNavigationInvokers);
				}
			}
		}
	}*/
}
#endif // WITH_EDITOR

FSurfaceNavPathFindingQueryData::FSurfaceNavPathFindingQueryData(const FSurfaceNavPathFindingQueryData& Source) :
	FSurfaceNavPathFindingQueryData(Source.Owner.Get(), Source.StartLocation, Source.EndLocation, Source.NonOffsetStartLocation, Source.NonOffsetEndLocation,
		Source.FilterClass, Source.PathInstanceToFill, Source.NavDataFlags, Source.bAllowPartialPaths, Source.CostLimit,
		Source.MinAngleFromGround, Source.MaxAngleFromGround, Source.AcceptanceRadius, Source.MaxPathingIterations,
		Source.bIsForTesting, Source.PawnAngleExpenseMultipliers)
{

}

FSurfaceNavPathFindingQueryData::FSurfaceNavPathFindingQueryData(FSurfaceNavPathSharedRef PathToRecalculate) :
	FSurfaceNavPathFindingQueryData(PathToRecalculate->GetQueryData())
{
	PathInstanceToFill = PathToRecalculate;

	if (PathToRecalculate->ShouldUpdateStartPointOnRepath() && (PathToRecalculate->GetSourceActor() != nullptr))
	{
		FVector NewStartLocation;
		FVector NewStartOffsetLocation;
		PathToRecalculate->GetPathFindingStartLocations(NewStartLocation, NewStartOffsetLocation);
		if (FNavigationSystem::IsValidLocation(NewStartLocation) && FNavigationSystem::IsValidLocation(NewStartOffsetLocation))
		{
			StartLocation = NewStartOffsetLocation;
			NonOffsetStartLocation = NewStartLocation;
		}
	}

	if (PathToRecalculate->ShouldUpdateEndPointOnRepath() && (PathToRecalculate->GetGoalActor() != nullptr))
	{
		FVector NewEndLocation;
		FVector NewEndOffsetLocation;
		PathToRecalculate->GetGoalLocations(NewEndLocation, NewEndOffsetLocation);
		if (FNavigationSystem::IsValidLocation(NewEndLocation) && FNavigationSystem::IsValidLocation(NewEndOffsetLocation))
		{
			EndLocation = NewEndOffsetLocation;
			NonOffsetEndLocation = NewEndLocation;
		}
	}

}

USurfaceNavLinkComponent::USurfaceNavLinkComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void USurfaceNavigationSystem::SetUpdateInvokerLocationsTimer()
{
	GetWorld()->GetTimerManager().SetTimer(
		TimerUpdateInvokerLocationsHandle,
		this,
		&USurfaceNavigationSystem::UpdateInvokerLocations,
		SurfaceNavProxySettings::RefreshLocationDataInterval,
		true,
		1.f
	);
}

void USurfaceNavigationSystem::SetProcessUpdatedComponentsTimer()
{
	GetWorld()->GetTimerManager().SetTimer(
		TimerProcessUpdatedComponentsHandle,
		this,
		&USurfaceNavigationSystem::ProcessUpdatedComponents,
		SurfaceNavProxySettings::ProcessComponentUpdatesInterval,
		true,
		0.5f
	);
}

inline void USurfaceNavigationSystem::PurgeUnusedPaths()
{
	check(IsInGameThread());

	// Paths can be registered from async pathfinding thread 
	// while unused paths are purged in main thread (actor tick)
	FScopeLock PathLock(&ActivePathsLock);

	const int32 Count = ActivePaths.Num();
	FSurfaceNavPathWeakPtr* WeakPathPtr = (ActivePaths.GetData() + Count - 1);
	for (int32 i = Count - 1; i >= 0; --i, --WeakPathPtr)
	{
		if (!WeakPathPtr->IsValid() || !WeakPathPtr->Pin()->IsValid())
		{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
			ActivePaths.RemoveAtSwap(i, 1, /*bAllowShrinking=*/false);
#else
			ActivePaths.RemoveAtSwap(i, 1, /*bAllowShrinking=*/EAllowShrinking::No);
#endif
		}
	}
}

inline void USurfaceNavigationSystem::ProcessObservedPaths(float DeltaTime)
{
	if (NextObservedPathsTickInSeconds >= 0.f)
	{
		NextObservedPathsTickInSeconds -= DeltaTime;
		if (NextObservedPathsTickInSeconds <= 0.f)
		{
			//TSortedMap<int64, FSurfaceNavPathRecalculationRequest> TempRepathRepathRequests;
			TSortedMap<double, FSurfaceNavPathRecalculationRequest> TempRepathRepathRequests;
			TempRepathRepathRequests.Reserve(ObservedPaths.Num());

			for (int32 PathIndex = ObservedPaths.Num() - 1; PathIndex >= 0; --PathIndex)
			{
				if (ObservedPaths[PathIndex].IsValid())
				{
					FSurfaceNavPathSharedPtr SharedPath = ObservedPaths[PathIndex].Pin();
					FSurfaceNavPath* Path = SharedPath.Get();
					double SortValue;
					EPathObservationResult::Type Result = Path->TickPathObservation();
					FScopeLock PathLock(&ActivePathsLock);
					switch (Result)
					{
					case EPathObservationResult::NoLongerObserving:
						ActivePaths.Remove(ObservedPaths[PathIndex]);
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
						ObservedPaths.RemoveAtSwap(PathIndex, 1, /*bAllowShrinking=*/false);
#else
						ObservedPaths.RemoveAtSwap(PathIndex, 1, /*bAllowShrinking=*/EAllowShrinking::No);
#endif
						break;
					case EPathObservationResult::NoChange:
						// do nothing
						break;

					case EPathObservationResult::RequestRepath:
						ActivePaths.Remove(ObservedPaths[PathIndex]);

						//TempRepathRepathRequests.Emplace(SharedPath->GetTimeStamp(), FSurfaceNavPathRecalculationRequest(SharedPath, ENavPathUpdateType::GoalMoved));
						//SortValue = FVector::DistSquared(SharedPath->GetLastRepathGoalLocation(), SharedPath->GetSourceActor()->GetActorLocation());

						{
							// The main value for sorting our repath requests should be the timestamp. 
							// Add a little randomness so it's not always just the AI at the back of a pack that are
							// updating their path.
							int64 PathTimestamp = SharedPath->GetTimeStamp() * FMath::RandRange(0.9f, 1.0f);

							// We also want to look at distance to goal, but that should be a much lower weight for the calculation.
							// This will make it so the futher away something is from it's goal, the less chance it has to be at the top.
							double DistToGoal = FVector::DistSquared(SharedPath->GetLastRepathGoalLocation(), SharedPath->GetSourceActor()->GetActorLocation()) * FMath::RandRange(0.01f, 0.2f);

							SortValue = DistToGoal + DistToGoal;
						}
						TempRepathRepathRequests.Emplace(SortValue, FSurfaceNavPathRecalculationRequest(SharedPath, ENavPathUpdateType::GoalMoved));

						//RepathRequests.AddUnique(FSurfaceNavPathRecalculationRequest(SharedPath, ENavPathUpdateType::GoalMoved));
						break;

					default:
						check(false && "unhandled EPathObservationResult::Type in ANavigationData::TickActor");
						break;
					}
				}
				else
				{
					FScopeLock PathLock(&ActivePathsLock);
					ActivePaths.Remove(ObservedPaths[PathIndex]);
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
					ObservedPaths.RemoveAtSwap(PathIndex, 1, /*bAllowShrinking=*/false);
#else
					ObservedPaths.RemoveAtSwap(PathIndex, 1, /*bAllowShrinking=*/EAllowShrinking::No);
#endif
				}
			}

			RepathRequests.Reserve(TempRepathRepathRequests.Num());

			for (auto& TempRepathRepathRequestPair : TempRepathRepathRequests)
			{
				RepathRequests.AddUnique(TempRepathRepathRequestPair.Value);
			}

			if (ObservedPaths.Num() > 0)
			{
				NextObservedPathsTickInSeconds = SurfaceNavProxySettings::ObservedPathsTickInterval;
			}
		}
	}
}

inline void USurfaceNavigationSystem::ProcessRepathRequests()
{
	if (RepathRequests.Num() > 0)
	{
		if (!CanAcceptPathingRequest())
			return;

		// @todo batch-process it!

		// We don't want to saturate the pathing with just re-path requests. If we did there wouldn't
		// be available time for new pathing requests and pawns would just be stuck not moving. So let's
		// free up at least 1 request.
		// TODO: look at possible calculation to base the quantity to free up on the size of MaxPathRequestsPerTick
		const int32 MaxProcessedRequests = SurfaceNavProxySettings::MaxPathRequestsPerTick > 2 ? SurfaceNavProxySettings::MaxPathRequestsPerTick - 1 : SurfaceNavProxySettings::MaxPathRequestsPerTick;

		// make a copy of path requests and reset (remove up to MaxProcessedRequests) from navdata's array
		// this allows storing new requests in the middle of loop (e.g. used by meta path corrections)

		TArray<FSurfaceNavPathRecalculationRequest> WorkQueue(RepathRequests);
		if (WorkQueue.Num() > MaxProcessedRequests)
		{
			//UE_VLOG(this, LogNavigation, Error, TEXT("Too many repath requests! (%d/%d)"), WorkQueue.Num(), MaxProcessedRequests);

			WorkQueue.RemoveAt(MaxProcessedRequests, WorkQueue.Num() - MaxProcessedRequests);
			RepathRequests.RemoveAt(0, MaxProcessedRequests);
		}
		else
		{
			RepathRequests.Reset();
		}


		for (int32 Idx = 0; Idx < WorkQueue.Num(); Idx++)
		{
			FSurfaceNavPathRecalculationRequest& RecalcRequest = WorkQueue[Idx];

			if (RecalcRequest.Path.IsValid() == false)
				continue;

			// check if it can be updated right now
			FSurfaceNavPathSharedPtr PinnedPath = RecalcRequest.Path.Pin();
			if (PinnedPath.IsValid() == false)
			{
				continue;
			}

			const UObject* PathQuerier = PinnedPath->GetQuerier();
			const INavAgentInterface* PathNavAgent = Cast<const INavAgentInterface>(PathQuerier);
			if (PathNavAgent && PathNavAgent->ShouldPostponePathUpdates())
			{
				RepathRequests.Add(RecalcRequest);
				continue;
			}

			auto RecalcReason = RecalcRequest.Reason;

			// TODO: Need to fix by setting filter class
			FSurfaceNavPathFindingQueryData Query(PinnedPath.ToSharedRef());

			const FSurfaceNavPathResult Result = FindPath(Query.SetPathInstanceToUpdate(PinnedPath));

			// update time stamp to give observers any means of telling if it has changed
			//PinnedPath->SetTimeStamp(TimeStamp);

			// partial paths are still valid and can change to full path when moving goal gets back on navmesh
			if (Result.IsSuccessful() || Result.IsPartial())
			{
				PinnedPath->UpdateLastRepathGoalLocation();
				PinnedPath->DoneUpdating(RecalcReason); // RecalcRequest.Reason);
				if (RecalcReason == ENavPathUpdateType::NavigationChanged)
				{
					RegisterActivePath(PinnedPath);
				}
			}
			else
			{
				PinnedPath->RePathFailed();
			}

		}

	}
}

void USurfaceNavigationSystem::RequestRePath(FSurfaceNavPathSharedPtr Path, ENavPathUpdateType::Type Reason)
{
	check(IsInGameThread());
	RepathRequests.Add(FSurfaceNavPathRecalculationRequest(Path, Reason));
}

inline void USurfaceNavigationSystem::NavigableComponentUpdated(const FVector& Velocity, const FBox& BoundingBox, const bool IsBeingDestroyed)
{
	if (!bInitDone || bIsCleaningUp)
		return;

	//FWriteScopeLock wUpdatedCompQueLock(UpdatedCompQueLock);
	UpdatedComponentsQueue.Enqueue(FUpdatedComponentQueueItem(Velocity, BoundingBox, IsBeingDestroyed));
	UpdatedComponentsQueueCount.Increment();
}

inline void USurfaceNavigationSystem::NavigableComponentUpdated(const UActorComponent& Comp)
{
	check(&Comp);

	// We only care about components that are relevant to navigation
	if (!Comp.IsNavigationRelevant() || !bInitDone || bIsCleaningUp)
		return;

	if (const USceneComponent* SceneComponent = Cast<USceneComponent>(&Comp))
	{
		NavigableComponentUpdated(SceneComponent->GetComponentVelocity(), SceneComponent->Bounds.GetBox(), SceneComponent->IsBeingDestroyed());
	}
}

void USurfaceNavigationSystem::ProcessUpdatedComponents()
{
	if (bIsProcessingComponentUpdates || bIsCleaningUp)
		return;

	// Get a local copy of the player locations on game thread and pass to async thread
	//TArray<FVector> MyPlayerLocations(PlayerLocations);

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]()
		{
			FReadScopeLock rDestoryLock(USurfaceNavigationSystem::DestroyLock);

			if (bIsProcessingComponentUpdates || bIsCleaningUp)
				return;

			bIsProcessingComponentUpdates = true;

			// Get a local copy of the player locations
			TArray<FVector> MyPlayerLocations;
			{
				FReadScopeLock rPlayerLocationLock(PlayerLocationLock);
				MyPlayerLocations.Append(PlayerLocations);
			}

			int CompUpdateCount = 0;

			TSet<FIntVector> UpdatedComponentsSet;
			TSet<FUpdatedComponentQueueItem> NewReprocessComponents;

			FUpdatedComponentQueueItem CompQueItem;

			while (UpdatedComponentsQueue.Dequeue(CompQueItem))
			{
				UpdatedComponentsQueueCount.Decrement();

				if (bIsCleaningUp)
				{
					return;
				}

				ProcessUpdatedComponent(CompQueItem, MyPlayerLocations, CompUpdateCount, UpdatedComponentsSet, NewReprocessComponents);
			}

			for (auto& ReprocessComp : ReprocessComponents)
			{
				if (bIsCleaningUp)
				{
					return;
				}

				// If we're reprocessing, we want to process regardless of velocity or being destroyed because
				// the blocks need to update at some point.
				ProcessUpdatedComponent(ReprocessComp, MyPlayerLocations, CompUpdateCount, UpdatedComponentsSet,
					NewReprocessComponents, true /* ignore velocity*/, true /* ignore being destroyed */);
			}

			ReprocessComponents = MoveTemp(NewReprocessComponents);

			//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::ProcessUpdatedComponents: %d components updated nav mesh."), CompUpdateCount);

			for (const FIntVector& CompLoc : UpdatedComponentsSet)
			{
				if (bIsCleaningUp)
					continue;

				QueueBlockPendingDirty(CompLoc);
			}

			bIsProcessingComponentUpdates = false;
		});
}

void USurfaceNavigationSystem::ProcessUpdatedComponent(FUpdatedComponentQueueItem& CompQueItem, TArray<FVector> MyPlayerLocations,
	int& CompUpdateCount, TSet<FIntVector>& UpdatedComponentsSet, TSet<FUpdatedComponentQueueItem>& NewReprocessComponents,
	bool IgnoreVelocity, bool IgnoreBeingDestroyed)
{
	CompUpdateCount++;

	if ((!IgnoreVelocity && !CompQueItem.Velocity.IsNearlyZero(0.01)) ||
		(!IgnoreBeingDestroyed && CompQueItem.IsBeingDestroyed))
	{
		NewReprocessComponents.Emplace(CompQueItem);
		return;
	}

	// Need to apply the configured grid offset to the bounding box.
	// Also expand by half the grid point distance to make sure nieghboring blocks pick
	// up the changes if needed.
	const FBox CompBoundsBox = CompQueItem.Bounds.ShiftBy(-SurfaceNavProxySettings::GridOffset).ExpandBy(SurfaceNavProxySettings::GridPointDistance * 0.5f);

	// We need the min and max vector so we know what blocks to update.
	FVector MinVector = CompBoundsBox.Min;
	FVector MaxVector = CompBoundsBox.Max;

	float BlockSize = SurfaceNavProxySettings::BlockSize;
	float BlockSizeMultiplier = 1.f / BlockSize;

	double startX = round(MinVector.X * BlockSizeMultiplier);
	double endX = round(MaxVector.X * BlockSizeMultiplier);

	double startY = round(MinVector.Y * BlockSizeMultiplier);
	double endY = round(MaxVector.Y * BlockSizeMultiplier);

	double startZ = round(MinVector.Z * BlockSizeMultiplier);
	double endZ = round(MaxVector.Z * BlockSizeMultiplier);

	// Need to make sure this is far enough out to handle most cases
	// since we don't know if there will be invokers with larger allowed
	// distances than the default
	float maxDistFromPlayerSq = SurfaceNavProxySettings::MaxDistanceFromPlayerForGeneration * 5.f;

	// Sq the value so we can take advantage of the faster dist sq
	maxDistFromPlayerSq *= maxDistFromPlayerSq;

	for (double x = startX; x <= endX; x++)
	{
		for (double y = startY; y <= endY; y++)
		{
			for (double z = startZ; z <= endZ; z++)
			{
				if (bIsCleaningUp)
					return;

				// If we're allowing nav generation without player then no
				// need to process any further, just add to collection
				if (SurfaceNavProxySettings::AllowNavigationGenerationWithoutPlayer)
				{
					UpdatedComponentsSet.Emplace(FIntVector(x, y, z));
					continue;
				}

				//**********************************************
				//**********************************************
				// Make sure the invoker is within the max range of a player
				bool bIsInRangeOfPlayer = false;
				for (const FVector& pLoc : MyPlayerLocations)
				{
					auto playerDistSq = FVector::DistSquared(pLoc, FVector(x * BlockSize, y * BlockSize, z * BlockSize));
					if (playerDistSq < maxDistFromPlayerSq)
					{
						bIsInRangeOfPlayer = true;
						break;
					}
				}

				// if it's not in range then don't add to collection
				if (!bIsInRangeOfPlayer)
					continue;
				//**********************************************


				UpdatedComponentsSet.Emplace(FIntVector(x, y, z));

			}
		}
	}

}

void USurfaceNavigationSystem::QueueBlockPendingDirty(const FIntVector& IntLoc)
{
	// We want to queue them up here in case there's any back to back changes
	// to the same block. This way it'll only get updated once instead up several times.
	BlocksPendingDirty.Enqueue(IntLoc);
}

void USurfaceNavigationSystem::QueueFinishProcessingBlockGridPoint(FIntVector BlockKey)
{
	FinishProcessingBlockGridPointQueue.Enqueue(BlockKey);
	FinishProcessingBlockGridPointQueueCount.Increment();
}

void USurfaceNavigationSystem::QueueBlocksPendingDirtyWithinBounds(const FBox& InBoundsBox, const FBox& InExclusionBoundsBox)
{
	float BlockSize = SurfaceNavProxySettings::BlockSize;
	FVector GridOffset = SurfaceNavProxySettings::GridOffset;
	float GridPointDistance = SurfaceNavProxySettings::GridPointDistance;

	// Let's apply the negated grid offset so we get the proper block indices
	FBox BoundsBox = InBoundsBox.ShiftBy(-GridOffset);
	FBox ExclusionBoundsBox = InExclusionBoundsBox.ShiftBy(-GridOffset);

	// Let's expand by the grid point distance to make sure we include adjacent blocks if needed.
	// Adding a little extra to account for small precision errors.
	BoundsBox = BoundsBox.ExpandBy(GridPointDistance + KINDA_SMALL_NUMBER);
	ExclusionBoundsBox = ExclusionBoundsBox.ExpandBy(GridPointDistance - KINDA_SMALL_NUMBER);

	// Let's do a single division operation so we can multiply in the 6 lines it's needed below.
	// Multiply uses fewer instructions than division for slightly improved performance.
	double BlockSizeMultiplier = 1.0 / BlockSize;

	double HalfBlockSize = BlockSize * 0.5f;
	//double HalfBlockSize = BlockSize * 0.5f;

	// Find each of the int vector indices we need to find the overlapping blocks
	double startX = round(BoundsBox.Min.X * BlockSizeMultiplier);
	double endX = round(BoundsBox.Max.X * BlockSizeMultiplier);

	double startY = round(BoundsBox.Min.Y * BlockSizeMultiplier);
	double endY = round(BoundsBox.Max.Y * BlockSizeMultiplier);

	double startZ = round(BoundsBox.Min.Z * BlockSizeMultiplier);
	double endZ = round(BoundsBox.Max.Z * BlockSizeMultiplier);

	double x = startX;
	double y = startY;
	double z = startZ;

	// Opting for single loop instead of 3 nested loops for slight perf increase.
	// Makes it a little harder to follow, but this is a heavily used tight loop.
	bool CanIterate = true;
	while (CanIterate)
	{
		FBox BlockBox = FBox::BuildAABB(FVector(x * BlockSize, y * BlockSize, z * BlockSize), FVector(HalfBlockSize));

		// Any blocks that intersect the new bounds, intersect the old bounds and are not
		// fully in either need to be marked as dirty.
		// We don't care about new blocks being overlapped here, that's handled elsewhere.
		if (InExclusionBoundsBox.Intersect(BlockBox) &&
			(!InBoundsBox.IsInside(BlockBox) || !InExclusionBoundsBox.IsInside(BlockBox)))
		{
			const FIntVector IntLoc(x, y, z);
			QueueBlockPendingDirty(IntLoc);
		}

		z++;
		if (z <= endZ)
			continue;

		z = startZ;
		y++;
		if (y <= endY)
			continue;

		y = startY;
		x++;
		if (x <= endX)
			continue;

		CanIterate = false;
	}
}

void USurfaceNavigationSystem::ProcessPendingDirtyQueue()
{
	FIntVector IntLoc;
	while (BlocksPendingDirty.Dequeue(IntLoc))
	{
		FSurfaceNavBlock** NavBlockPtr;

		{
			FReadScopeLock rNavBlocksLock(NavBlocksLock);
			NavBlockPtr = NavBlocks.Find(IntLoc);
		}

		if (!NavBlockPtr)
			continue;

		auto& NavBlock = *NavBlockPtr;
		if (NavBlock)
		{
			//FWriteScopeLock wLock(NavBlock->NavBlockLock);
			NavBlock->MarkDirty();
		}
	}
}

void USurfaceNavigationSystem::ProcessDeletesQueues()
{
	// This function will be called directly from cleanup so lock to make
	// sure there are 2 calls running in parallel
	FWriteScopeLock wDeleteQueueLock(DeleteQueueLock);


	// ***************************************************
	// ***************************************************
	// Begin deleting nav links
	// 
	// Empty out the ready to delete queue first, simply deleting
	// each instance in the queue.
	FSurfaceNavLinkData* NavLink = nullptr;
	while (NavLinksReadyToDelete.Dequeue(NavLink))
	{
		delete NavLink;
	}

	// We're just shuffling queues to force a delay before delete
	// without having to manage a timestamp.
	NavLink = nullptr;
	while (NavLinksPendingDelete.Dequeue(NavLink))
	{
		NavLinksReadyToDelete.Enqueue(NavLink);
	}
	// ***************************************************
	// ***************************************************


	// ***************************************************
	// ***************************************************
	// Begin deleting nav points
	// 
	// Empty out the ready to delete queue first, simply deleting
	// each instance in the queue.
	TSharedPtr<FSurfaceNavPointBase> NavPointToDelete = nullptr;
	while (NavPointsReadyToDelete.Dequeue(NavPointToDelete))
	{
		if (!NavPointToDelete || NavPointToDelete->IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("USurfaceNavigationSystem::ProcessDeletesQueues(): ***Invalid NavPoint Delete"));
			continue;
		}

		if (SurfaceNavProxySettings::PerformExtraDebuggingTests && !GetWorld()->bIsTearingDown)
		{
			auto BlockSize = SurfaceNavProxySettings::BlockSize;
			float BlockSizeMultiplier = 1.f / BlockSize;
			float PaddedDistSqTestValue = FMath::Square(FMath::Max(SurfaceNavProxySettings::MaxDistanceFromNavigation, BlockSize) * 1.2f);
			auto gridPointDist = SurfaceNavProxySettings::GridPointDistance;
			auto halfGridPointDist = gridPointDist * 0.5f;

			float SearchPadding = 1.f;

			FVector TestLoc = NavPointToDelete->WorldLocation;
			int startX = roundf((TestLoc.X - halfGridPointDist) * BlockSizeMultiplier - SearchPadding);
			int endX = roundf((TestLoc.X + halfGridPointDist) * BlockSizeMultiplier + SearchPadding);

			int startY = roundf((TestLoc.Y - halfGridPointDist) * BlockSizeMultiplier - SearchPadding);
			int endY = roundf((TestLoc.Y + halfGridPointDist) * BlockSizeMultiplier + SearchPadding);

			int startZ = roundf((TestLoc.Z - halfGridPointDist) * BlockSizeMultiplier - SearchPadding);
			int endZ = roundf((TestLoc.Z + halfGridPointDist) * BlockSizeMultiplier + SearchPadding);

			bool CanIterate = true;

			int i = startX;
			int j = startY;
			int k = startZ;

			while (CanIterate)
			{
				FIntVector LocBlockIndex(i, j, k);

				FSurfaceNavBlock* const* NavBlockPtr = nullptr;

				{
					FReadScopeLock rNavBlocksLock(NavBlocksLock);
					NavBlockPtr = NavBlocks.Find(LocBlockIndex);
				}

				if (NavBlockPtr)
				{
					FSurfaceNavBlock* NavBlock = *NavBlockPtr;

					if (NavBlock && NavBlock->IsActive() && NavBlock->IsValid())
					{
						FReadScopeLock rNavBlockLock(NavBlock->NavBlockLock);

						if (NavBlock && NavBlock->IsActive() && NavBlock->IsValid())
						{
							for (int x = 0; x < NavBlock->SurfaceNavGridPoints.Num(); x++)
							{
								for (int y = 0; y < NavBlock->SurfaceNavGridPoints[x].Num(); y++)
								{
									for (int z = 0; z < NavBlock->SurfaceNavGridPoints[x][y].Num(); z++)
									{
										FSurfaceNavGridPoint*& GridPoint = NavBlock->SurfaceNavGridPoints[x][y][z];
										if (GridPoint)
										{

											for (TPair<FIntVector, TSharedPtr<FSurfaceNavPointBase>>& NavPointPair : GridPoint->NavPoints)
											{
												TSharedPtr<FSurfaceNavPointBase>& TestingNavPoint = NavPointPair.Value;
												if (TestingNavPoint)
												{

													if (TestingNavPoint->NeighborsContainNavPoint(NavPointToDelete))
														UE_LOG(LogTemp, Fatal, TEXT("USurfaceNavigationSystem::ProcessDeletesQueues(): ***Attempting to delete nav point that is still referenced"));
												}
											}
										}
									}
								}
							}
						}
					}
				}

				k++;

				if (k <= endZ)
					continue;

				k = startZ;
				j++;

				if (j <= endY)
					continue;

				j = startY;
				i++;

				if (i <= endX)
					continue;

				CanIterate = false;
			}
		}

		NavPointToDelete.Reset();
	}

	// We're just shuffling queues to force a delay before delete
	// without having to manage a timestamp.
	TSharedPtr<FSurfaceNavPointBase> NavPoint = nullptr;
	while (NavPointsPendingDelete.Dequeue(NavPoint))
	{
		NavPointsReadyToDelete.Enqueue(NavPoint);
	}
	// ***************************************************
	// ***************************************************



	// ***************************************************
	// ***************************************************
	// Begin deleting grid points
	// 
	// Empty out the ready to delete queue first, simply deleting
	// each instance in the queue.
	FSurfaceNavGridPoint* NavGridPoint = nullptr;
	while (NavGridPointsReadyToDelete.Dequeue(NavGridPoint))
	{
		if (!NavGridPoint || NavGridPoint->IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("USurfaceNavigationSystem::ProcessDeletesQueues(): ***Invalid NavGridPoint Delete"));
			continue;
		}

		delete NavGridPoint;
	}

	// We're just shuffling queues to force a delay before delete
	// without having to manage a timestamp.
	NavGridPoint = nullptr;
	while (NavGridPointsPendingDelete.Dequeue(NavGridPoint))
	{
		NavGridPointsReadyToDelete.Enqueue(NavGridPoint);
	}
	// ***************************************************
	// ***************************************************



	// ***************************************************
	// ***************************************************
	// Begin deleting blocks
	// 
	// Empty out the ready to delete queue first, simply deleting
	// each instance in the queue.
	FSurfaceNavBlock* NavBlock = nullptr;
	while (NavBlocksReadyToDelete.Dequeue(NavBlock))
	{
		if (!NavBlock || NavBlock->IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("USurfaceNavigationSystem::ProcessDeletesQueues(): ***Invalid NavBlock Delete"));
			continue;
		}

		// We don't want to delete the block until all grid points have been released
		if (NavBlock->LivingGridPointCount.GetValue() > 0)
		{
			if (NavBlock->CleanupRetryCount < 2 && (!CleanupGridPointsLvl2Queue.IsEmpty() || !CleanupGridPointsLvl1Queue.IsEmpty()))
			{
				auto BlockLoc = NavBlock->GetLocation();
				auto BlockIndex = BlockLoc / SurfaceNavProxySettings::BlockSize;
				auto IntBlockIndex = FIntVector(BlockIndex.X, BlockIndex.Y, BlockIndex.Z);
				auto BlockIndexStr = IntBlockIndex.ToString();

				if (SurfaceNavProxySettings::PerformExtraDebuggingTests)
					UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::ProcessDeletesQueues(): ***Block %s still has grid points so delaying delete"), *BlockIndexStr);

				NavBlock->CleanupRetryCount++;
				NavBlocksPendingDelete.Enqueue(NavBlock);
				continue;
			}
		}

		delete NavBlock;
	}

	// We're just shuffling queues to force a delay before delete
	// without having to manage a timestamp.
	NavBlock = nullptr;
	while (NavBlocksPendingDelete.Dequeue(NavBlock))
	{
		NavBlocksReadyToDelete.Enqueue(NavBlock);
	}
	// ***************************************************
	// ***************************************************



	PathNode* NodePoint;
	while (PathNodesReadyToDelete.Dequeue(NodePoint))
	{
		if (NodePoint)
		{
			delete NodePoint;
		}
	}

	// We're just shuffling queues to force a delay before delete
	TMap<TSharedPtr<FSurfaceNavPointBase>, PathNode*> NodePointMap;
	while (PathNodeMapsPendingDelete.Dequeue(NodePointMap))
	{
		for (TPair<TSharedPtr<FSurfaceNavPointBase>, PathNode*>& NodePointPair : NodePointMap)
		{
			PathNodesReadyToDelete.Enqueue(NodePointPair.Value);
		}
		NodePointMap.Empty();
	}
}

void USurfaceNavigationSystem::ProcessPendingDeletesQueue()
{
	//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::ProcessPendingDeletesQueue(): Begin"));

	// Don't start processing if we're already waiting on it to complete
	if (bProcessingDeleteObjectQueues)
		return;

	// Don't bother going any further if there are no items to process
	if (AreDeleteQueuesEmpty())
		return;

	bProcessingDeleteObjectQueues = true;

	ProcessDeletesQueues();

	bProcessingDeleteObjectQueues = false;

	//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::ProcessPendingDeletesQueue(): End"));
}

void USurfaceNavigationSystem::ProcessCleanupQueues()
{
	// This function will be called directly from cleanup so lock to make
	// sure there are 2 calls running in parallel
	FWriteScopeLock wCleanupPendingLock(CleanupPendingLock);

	FSurfaceNavBlock* NavBlock = nullptr;
	while (CleanupBlocksLvl1Queue.Dequeue(NavBlock))
	{
		CleanupBlocksLvl2Queue.Enqueue(NavBlock);
		CleanupLvl2QueueCount.Increment();
	}

	FSurfaceNavGridPoint* GridPoint = nullptr;
	while (CleanupGridPointsLvl1Queue.Dequeue(GridPoint))
	{
		CleanupGridPointsLvl2Queue.Enqueue(GridPoint);
	}
}

void USurfaceNavigationSystem::ProcessPendingCleanupQueue()
{
	//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::ProcessPendingDeletesQueue(): Begin"));

	// Don't start processing if we're already waiting on it to complete
	if (bProcessingCleanupObjectQueues)
		return;

	// Don't bother going any further if there are no items to process
	if (AreCleanupQueuesEmpty())
		return;

	bProcessingCleanupObjectQueues = true;

	ProcessCleanupQueues();

	bProcessingCleanupObjectQueues = false;

	//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::ProcessPendingDeletesQueue(): End"));
}

void USurfaceNavigationSystem::ProcessDelayedCleanupTasksAsync()
{
	//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::ProcessPendingDeletesQueueAsync(): Begin"));

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
		{
			ProcessPendingCleanupQueue();
			ProcessPendingDeletesQueue();
		});
}

void USurfaceNavigationSystem::PerformBackgroundWork(int ThreadId)
{
#if WITH_EDITOR
	// If playing in PIE, then skip processing for editor
	if (IsEditorInstanceWhileRunningPIE())
		return;
#endif 

	FReadScopeLock rDestoryLock(USurfaceNavigationSystem::DestroyLock);

	if (!bInitDone || bCleanUpDone || bIsCleaningUp)
		return;

	if (!bSurfaceNavGenerationEnabled)
	{
		/*BlockQueue.Empty();
		BlockQueueCount.Set(0);

		BlocksRecentlyProcessed.Empty();
		BlocksPendingAdd.Empty();
		BlocksPendingRefresh.Empty();
		BlockInRange.Empty();

		BlockGridPointQueue.Empty();
		BlockGridPointQueueCount.Set(0);

		ReleaseAllBlocks();
		CleanupBlocks(INT_MAX);*/

		return;
	}

	ThreadsProcessingCount.Increment();

	if (SurfaceNavProxySettings::BackgroundWorkerThreadCount == 1)
	{
		//FWriteScopeLock WrLock(WorkerLock1);

		CleanupBlocks(1000, true);
		QueueBlocksNearInvokers();
		ProcessChangedVolumeBounds();
		ProcessBlockQueue(1000);
		ProcessBlockGridPointQueue(500);
		FinishProcessingBlockGridPoints(500);
	}
	else if (SurfaceNavProxySettings::BackgroundWorkerThreadCount == 2)
	{
		if (ThreadId == 1)
		{
			//FWriteScopeLock WrLock(WorkerLock1);

			CleanupBlocks(INT_MAX, true);
			QueueBlocksNearInvokers();
			ProcessChangedVolumeBounds();
			ProcessBlockQueue(INT_MAX);
		}
		else
		{
			//FWriteScopeLock WrLock(WorkerLock2);

			ProcessBlockGridPointQueue(500);
			FinishProcessingBlockGridPoints(500);
		}
	}
	else if (SurfaceNavProxySettings::BackgroundWorkerThreadCount == 3)
	{
		if (ThreadId == 1)
		{
			//FWriteScopeLock WrLock(WorkerLock1);

			CleanupBlocks(INT_MAX, true);
			QueueBlocksNearInvokers();
			ProcessChangedVolumeBounds();
		}
		else if (ThreadId == 2)
		{
			//FWriteScopeLock WrLock(WorkerLock2);

			ProcessBlockQueue(INT_MAX);
		}
		else
		{
			//FWriteScopeLock WrLock(WorkerLock3);

			ProcessBlockGridPointQueue(INT_MAX);
			FinishProcessingBlockGridPoints(500);
		}
	}
	else if (SurfaceNavProxySettings::BackgroundWorkerThreadCount == 4)
	{
		if (ThreadId == 1)
		{
			//FWriteScopeLock WrLock(WorkerLock1);

			CleanupBlocks(INT_MAX, true);
			QueueBlocksNearInvokers();
			ProcessChangedVolumeBounds();
		}
		else if (ThreadId == 2)
		{
			//FWriteScopeLock WrLock(WorkerLock2);

			ProcessBlockQueue(INT_MAX);
		}
		else if (ThreadId == 3)
		{
			//FWriteScopeLock WrLock(WorkerLock3);

			ProcessBlockGridPointQueue(INT_MAX);
		}
		else
		{
			//FWriteScopeLock WrLock(WorkerLock4);

			FinishProcessingBlockGridPoints(INT_MAX);
		}
	}

	ThreadsProcessingCount.Decrement();
}

void USurfaceNavigationSystem::QueueBlocksNearInvokers()
{
	double StartTime = FPlatformTime::Seconds();

	if (!bInitDone || bCleanUpDone || bIsCleaningUp)
		return;

	TSet<FIntVector> TempLocSet;
	float BlockSize = SurfaceNavProxySettings::BlockSize;
	float blockExtent = SurfaceNavProxySettings::BlockExtent;

	// Dividing takes more cycles than multiplication so make a local var to use
	// for multiplication in the loop
	float blockSizeMultiplier = 1.f / BlockSize;

	// There's an extra bit of padding totalling GridPointDistance along each axis.
	// This means the extent's will include half GridPointDistance in padding.
	float calcBlockExtent = SurfaceNavProxySettings::PaddedBlockExtent;

	FVector gridOffset = SurfaceNavProxySettings::GridOffset;

	TempLocSet.Reserve(InvokerData.Num() * 1000);

	float UpperMinRadius = BlockSize * 2.f;
	float LowerMinRadius = BlockSize * 1.5f;

	// Get a local copy of the player locations
	TArray<FVector> MyPlayerLocations;
	{
		FReadScopeLock rPlayerLocationLock(PlayerLocationLock);
		MyPlayerLocations.Append(PlayerLocations);
	}

	TArray<FInvokerQueDataItem> MyInvokerData;
	{
		FReadScopeLock rLock(InvokerDataLock);
		MyInvokerData.Append(InvokerData);
	}

	for (auto InvokerIt = MyInvokerData.CreateIterator(); InvokerIt; ++InvokerIt)
	{
		FInvokerQueDataItem& InvokerDataItem = *InvokerIt;

		FVector InvokerLocation = InvokerDataItem.InvokerLoc;

		// Get parameter values from the invoker.
		// Use the default values if not supplied by the invoker (<= 0)
		float maxDistFromPlayer = InvokerDataItem.MaxDistanceFromPlayer <= 0 ? SurfaceNavProxySettings::MaxDistanceFromPlayerForGeneration : InvokerDataItem.MaxDistanceFromPlayer; // InvokerDataItem->MaxDistanceFromPlayer == 0 ? MaxDistanceFromPlayer : InvokerDataItem->MaxDistanceFromPlayer;
		float nearBlocRadius = InvokerDataItem.NearBlockRadius <= 0 ? SurfaceNavProxySettings::NearBlockGenerationRadius : InvokerDataItem.NearBlockRadius;
		float farBlockRadius = InvokerDataItem.FarBlockRadius <= 0 ? SurfaceNavProxySettings::FarBlockGenerationRadius : InvokerDataItem.FarBlockRadius;

		if (farBlockRadius > nearBlocRadius)
			farBlockRadius = nearBlocRadius;

		if (maxDistFromPlayer < UpperMinRadius)
			maxDistFromPlayer = UpperMinRadius;

		if (nearBlocRadius < UpperMinRadius)
			nearBlocRadius = UpperMinRadius;

		if (farBlockRadius < LowerMinRadius)
			farBlockRadius = LowerMinRadius;

		float maxDistFromPlayerSq = FMath::Square(maxDistFromPlayer);

		// Need to track closest player so we can determine what percentage to use for calculating
		// the radius for block generation
		float closestPlayerDistSq = SurfaceNavProxySettings::AllowNavigationGenerationWithoutPlayer ? 0.f : INFINITY;

		// Make sure the invoker is within the max range of the player.
		// We're not taking the generation radius into account, but may want
		// to look into that as an option.
		bool bIsInRangeOfPlayer = SurfaceNavProxySettings::AllowNavigationGenerationWithoutPlayer;
		if (!bIsInRangeOfPlayer)
		{
			for (auto PlayerLocIt = MyPlayerLocations.CreateConstIterator(); PlayerLocIt; ++PlayerLocIt)
			{
				const FVector& pLoc = *PlayerLocIt;
				auto playerDistSq = FVector::DistSquared(pLoc, InvokerLocation);
				if (playerDistSq < maxDistFromPlayerSq)
				{
					if (playerDistSq < closestPlayerDistSq)
						closestPlayerDistSq = playerDistSq;

					bIsInRangeOfPlayer = true;

					// ***************************************************
					// We don't want to break here because we also want the closest player distance, not just
					// if a player is in range.
					// ***************************************************
				}
			}
		}

		if (!bIsInRangeOfPlayer)
			continue;

		// If AllowNavigationGenerationWithoutPlayer is enabled, then use max radius, otherwise
		// calculate radius based on distance. If calculated radius is too small, then use min radius.
		float generationRadiusForDist = SurfaceNavProxySettings::AllowNavigationGenerationWithoutPlayer ?
			generationRadiusForDist = nearBlocRadius :
			FMath::Max(nearBlocRadius * (1.f - FMath::Sqrt(closestPlayerDistSq) / maxDistFromPlayer), farBlockRadius);

		float generationRadiusForDistSq = FMath::Square(generationRadiusForDist);

		float navBlocksFromInvoker = roundf(generationRadiusForDist * blockSizeMultiplier);

		double mapX = round(InvokerLocation.X * blockSizeMultiplier);
		double mapY = round(InvokerLocation.Y * blockSizeMultiplier);
		double mapZ = round(InvokerLocation.Z * blockSizeMultiplier);

		double startX = mapX - navBlocksFromInvoker;
		double endX = mapX + navBlocksFromInvoker;

		double startY = mapY - navBlocksFromInvoker;
		double endY = mapY + navBlocksFromInvoker;

		double startZ = mapZ - navBlocksFromInvoker;
		double endZ = mapZ + navBlocksFromInvoker;

		double x = startX;
		double y = startY;
		double z = startZ;

		// Opting for single loop instead of 3 nested loops for slight perf increase.
		// Makes it a little harder to follow, but this is a heavily used tight loop.
		bool CanIterate = true;
		while (CanIterate)
		{
			FIntVector BlockIndex(x, y, z);
			FVector BlockSpawnLoc(x * BlockSize, y * BlockSize, z * BlockSize);

			// Also get the offset block location as we'll need that in some cases
			FVector OffsetBlockSpawnLoc = BlockSpawnLoc + gridOffset;

			// Need to make sure the block (with offset) is within the specified radius of the invoker
			if (FVector::DistSquared(OffsetBlockSpawnLoc, InvokerLocation) < generationRadiusForDistSq)
			{
				// Need make sure the block is within the max range of the player. Initialize to
				// AllowNavigationGenerationWithoutPlayer.
				bIsInRangeOfPlayer = SurfaceNavProxySettings::AllowNavigationGenerationWithoutPlayer;

				// If it's already true due to AllowNavigationGenerationWithoutPlayer, then don't waste 
				// time checking if player is in range.
				if (!bIsInRangeOfPlayer)
				{
					for (auto PlayerLocIt = MyPlayerLocations.CreateConstIterator(); PlayerLocIt; ++PlayerLocIt)
					{
						const FVector& pLoc = *PlayerLocIt;
						if (FVector::DistSquared(pLoc, OffsetBlockSpawnLoc) < maxDistFromPlayerSq)
						{
							bIsInRangeOfPlayer = true;
							break;
						}
					}
				}

				if (bIsInRangeOfPlayer)
				{
					bool bIsInVolume = false;

					{
						FReadScopeLock rVolumeBoundLock(VolumeBoundLock);

						for (auto& NavVolBound : NavVolumeBounds)
						{
							auto BoundsBox = NavVolBound.AreaBox;

							// Use the offset block location so we get accurate results with the intersect test against
							// the nav volume bounds
							if (BoundsBox.IsValid && BoundsBox.Intersect(FBox(
								FVector(OffsetBlockSpawnLoc.X - calcBlockExtent, OffsetBlockSpawnLoc.Y - calcBlockExtent, OffsetBlockSpawnLoc.Z - calcBlockExtent),
								FVector(OffsetBlockSpawnLoc.X + calcBlockExtent, OffsetBlockSpawnLoc.Y + calcBlockExtent, OffsetBlockSpawnLoc.Z + calcBlockExtent))
							))
							{
								bool WasBlockAlreadyInBound = NavVolBound.SurfaceNavBlockIndices.Contains(BlockIndex);

								if (!WasBlockAlreadyInBound)
								{
									// Since the block is new in the bound, go ahead and mark as dirty because it may already be loaded
									// by another bounds volume and won't refresh if we don't mark as dirty.
									QueueBlockPendingDirty(BlockIndex);

									NavVolBound.SurfaceNavBlockIndices.Emplace(BlockIndex);
								}

								bIsInVolume = true;
								break;
							}
							else
							{
								NavVolBound.SurfaceNavBlockIndices.Remove(BlockIndex);
							}
						}
					}

					if (bIsInVolume)
					{
						TempLocSet.Add(BlockIndex);
						FWriteScopeLock wBlocksInRangeLock(BlocksInRangeLock);
						BlocksInRange.Emplace(BlockIndex);
					}
				}
			}


			z++;
			if (z <= endZ)
				continue;

			z = startZ;
			y++;
			if (y <= endY)
				continue;

			y = startY;
			x++;
			if (x <= endX)
				continue;

			CanIterate = false;
		}
	}

	FIntVector RecentlyProcessedBlockIndex;
	while (BlocksRecentlyProcessed.Dequeue(RecentlyProcessedBlockIndex))
	{
		BlocksPendingAdd.Remove(RecentlyProcessedBlockIndex);
		BlocksPendingRefresh.Remove(RecentlyProcessedBlockIndex);
	}

	int AddedBlockQueCount = 0;

	TSet<FSurfaceNavBlock*> BlocksToInvalidate;

	{
		FReadScopeLock rNavBlocksLock(NavBlocksLock);

		// Queue up any old blocks that need to be invalidated
		for (auto It = NavBlocks.CreateIterator(); It; ++It)
		{
			FIntVector BlockIndex = It->Key;
			FSurfaceNavBlock*& NavBlock = It->Value;

			// Make sure to only invalidate blocks that are not in TempLocSet AND
			// are in the NavBlocks collection.
			if (!TempLocSet.Contains(BlockIndex))
			{
				{
					FWriteScopeLock wBlocksInRangeLock(BlocksInRangeLock);
					BlocksInRange.Remove(BlockIndex);
				}

				if (NavBlock->IsActive())
				{
					BlocksToInvalidate.Emplace(NavBlock);
				}
			}
			else if (NavBlock->IsActive() && NavBlock->IsDirty() && !BlocksPendingRefresh.Contains(BlockIndex))
			{
				BlocksPendingRefresh.Emplace(BlockIndex);
				BlockQueue.Enqueue(BlockIndex);
				AddedBlockQueCount++;
			}
		}
	}

	// Need to break this step out to prevent deadlock
	for (auto& NavBlock : BlocksToInvalidate)
	{
		NavBlock->InvalidateBlockProcessing(true);
	}

	// Find block indices needing to be created and add them to the block queue
	for (FIntVector& Elem : TempLocSet)
	{
		bool HasNavBlock = false;

		{
			FReadScopeLock rNavBlocksLock(NavBlocksLock);
			HasNavBlock = NavBlocks.Contains(Elem);
		}

		// Here, we just want to queue new items so track the queue with the set
		if (!HasNavBlock && !BlocksPendingAdd.Contains(Elem))
		{
			BlocksPendingAdd.Emplace(Elem);
			BlockQueue.Enqueue(Elem);
			AddedBlockQueCount++;
		}
	}


	// Update the block queue count
	BlockQueueCount.Add(AddedBlockQueCount);
	LastBlockQueueCount = BlockQueueCount.GetValue();


#if WITH_EDITOR
	if (SurfaceNavProxySettings::ShowDebugStats && GEngine)
	{
		FString Msg = "DSN-Active Block Count: " + FString::FromInt(NavBlocks.Num());
		Msg += "\r\nDSN-Processing Blocks Lvl1: " + FString::FromInt(LastBlockQueueCount);
		Msg += "\r\nDSN-Processing Blocks Lvl2: " + FString::FromInt(BlockGridPointQueueCount.GetValue());
		Msg += "\r\nDSN-Processing Blocks Lvl3: " + FString::FromInt(FinishProcessingBlockGridPointQueueCount.GetValue());
		GEngine->AddOnScreenDebugMessage(1234567, 5.f, FColor::Cyan, Msg);
	}
#endif

	//double EndTime = FPlatformTime::Seconds();
	//UE_LOG(LogTemp, Warning, TEXT("QueueBlocksNearInvokers: Code executed in %f seconds."), EndTime - StartTime);

	/*auto TimeSpent = FPlatformTime::Seconds() - StartTime;
	if (TimeSpent >= 0.1f)
		UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::QueueBlocksNearInvokers: Code executed in %f seconds."), TimeSpent);*/
}

inline void USurfaceNavigationSystem::ProcessBlockQueue(int NumToProcess)
{
	if (!bInitDone || bCleanUpDone || bIsCleaningUp || GetWorld()->bIsTearingDown)
		return;

	double StartTime = FPlatformTime::Seconds();

	int count = 0;
	int Processed = 0;

	FIntVector IntLoc;
	float BlockSize = SurfaceNavProxySettings::BlockSize;
	FVector GridOffset = SurfaceNavProxySettings::GridOffset;
	TMap<FIntVector, FSurfaceNavBlock*> BlocksToAdd;
	TArray<FIntVector> AddToRecentlyProcessed;

	while (Processed < NumToProcess && BlockQueue.Dequeue(IntLoc))
		//while (BlockQueue.Dequeue(IntLoc))
	{
		count++;
		BlockQueueCount.Decrement();

		{
			FReadScopeLock rBlocksInRangeLock(BlocksInRangeLock);
			if (!BlocksInRange.Contains(IntLoc))
				continue;
		}

		FSurfaceNavBlock** NavBlockPtr = nullptr;
		{
			FReadScopeLock rNavBlocksLock(NavBlocksLock);
			NavBlockPtr = NavBlocks.Find(IntLoc);
		}

		if (NavBlockPtr && *NavBlockPtr)
		{
			if (PathingRequestCount > 0 && !IsInGameThread())
				FPlatformProcess::Sleep(0.005f);

			//FReadScopeLock rPathingLock(PathingLock);

			if (!bInitDone || bCleanUpDone || bIsCleaningUp || GetWorld()->bIsTearingDown)
			{
				BlocksRecentlyProcessed.Enqueue(IntLoc);
				return;
			}

			auto& NavBlock = *NavBlockPtr;

			NavBlock->SetWorld(GetWorld());
			if (NavBlock->Refresh(GetWorld()))
				Processed++;
			/*else
				Processed += 0.0001f;*/

			BlocksRecentlyProcessed.Enqueue(IntLoc);
		}
		else
		{
			check(GetWorld());

			FVector SpawnLoc((IntLoc.X * BlockSize) + GridOffset.X, (IntLoc.Y * BlockSize) + GridOffset.Y, (IntLoc.Z * BlockSize) + GridOffset.Z);

			if (!bInitDone || bCleanUpDone || bIsCleaningUp || GetWorld()->bIsTearingDown)
			{
				BlocksRecentlyProcessed.Enqueue(IntLoc);
				return;
			}

			/*FSurfaceNavBlock* NavBlock = BlockPool->Pop(GetWorld(), SpawnLoc, IntLoc);
			NavBlock->Activate();*/

			// Add the block to the map so it can now be tracked.
			// Make sure to activate the block as well.
			// Currently the spawn location is not offset so add that for now
			BlocksToAdd.Emplace(IntLoc,
				BlockPool->Pop(GetWorld(), SpawnLoc, IntLoc))->Activate();

			AddToRecentlyProcessed.Emplace(IntLoc);

			/*FWriteScopeLock wNavBlocksLock(NavBlocksLock);
			NavBlocks.Emplace(IntLoc, NavBlock);*/

			Processed++;
		}

	}

	{
		FWriteScopeLock wNavBlocksLock(NavBlocksLock);
		NavBlocks.Append(MoveTemp(BlocksToAdd));
	}

	for (FIntVector& MyIntLoc : AddToRecentlyProcessed)
	{
		BlocksRecentlyProcessed.Enqueue(MyIntLoc);
	}

	double TimeSpent = FPlatformTime::Seconds() - StartTime;
	if (TimeSpent > 0.003f)
	{
		double EndTime = FPlatformTime::Seconds();
		//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavProxy::ProcessBlockQueue: Code executed in %f seconds."), TimeSpent);
		//	UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavProxy::ProcessBlockQueue: Took %f seconds to process %d blocks."), TimeSpent, count);
	}
}

inline void USurfaceNavigationSystem::ProcessBlockGridPointQueue(int NumToProcess)
{
	//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::ProcessBlockGridPointQueue"));
	//auto StartTime = FPlatformTime::Seconds();

	if (!bInitDone || bCleanUpDone || bIsCleaningUp)
		return;

	int count = 0;
	FSurfaceNavBlock* NavBlock;

	while (count < NumToProcess && BlockGridPointQueue.Dequeue(NavBlock))
	{
		/*if (PathingRequestCount > 0 && !IsInGameThread())
			FPlatformProcess::Sleep(0.005f);*/

		count++;
		BlockGridPointQueueCount.Decrement();

		//FReadScopeLock rPathingLock(PathingLock);

		if (!bInitDone || bCleanUpDone || bIsCleaningUp)
			return;

		NavBlock->CheckForSurface();
	}

	/*auto TimeSpent = FPlatformTime::Seconds() - StartTime;
	if (TimeSpent >= 0.0001f)
		UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::ProcessBlockGridPointQueue: Took %f seconds to process %d blocks."), TimeSpent, count);*/
}

void USurfaceNavigationSystem::FinishProcessingBlockGridPoints(int NumToProcess)
{
	//FReadScopeLock rDestoryLock(USurfaceNavigationSystem::DestroyLock);

	if (!bInitDone || bCleanUpDone || bIsCleaningUp)
		return;

	FIntVector IntLoc;
	int count = 0;

	while (count < NumToProcess && FinishProcessingBlockGridPointQueue.Dequeue(IntLoc))
	{
		if (PathingRequestCount > 0 && !IsInGameThread())
			FPlatformProcess::Sleep(0.005f);

		count++;
		FinishProcessingBlockGridPointQueueCount.Decrement();

		//FReadScopeLock rPathingLock(PathingLock);

		if (!bInitDone || bCleanUpDone || bIsCleaningUp)
			return;

		FSurfaceNavBlock** NavBlockPtr = nullptr;

		{
			FReadScopeLock rNavBlocksLock(NavBlocksLock);
			NavBlockPtr = NavBlocks.Find(IntLoc);
		}

		if (!NavBlockPtr || !*NavBlockPtr)
			return;

		FSurfaceNavBlock* NavBlock = *NavBlockPtr;

		if (!NavBlock)
			continue;

		FWriteScopeLock wNavBlockLock(NavBlock->NavBlockLock);

		NavBlock->FinishProcessingGridPoints();
	}
}

void USurfaceNavigationSystem::ReleaseBlockCallback(const FIntVector& BlockKey)
{
	FSurfaceNavBlock* MyNavBlock = nullptr;
	FSurfaceNavBlock** MyNavBlockPtr = nullptr;

	{
		FReadScopeLock rNavBlocksLock(NavBlocksLock);
		MyNavBlockPtr = NavBlocks.Find(BlockKey);
	}

	if (MyNavBlockPtr && *MyNavBlockPtr)
	{
		MyNavBlock = *MyNavBlockPtr;
		FWriteScopeLock wNavBlocksLock(NavBlocksLock);
		NavBlocks.Remove(BlockKey);
	}

	if (MyNavBlock)
		CleanupBlocksLvl1Queue.Enqueue(MyNavBlock);
	//CleanupLvl2QueueCount.Increment();
}

void USurfaceNavigationSystem::ReleaseGridPointCallback(FSurfaceNavGridPoint* GridPoint)
{
	CleanupGridPointsLvl1Queue.Enqueue(GridPoint);
}

/** Cleans up blocks over a specified age.
	Can be a heavy call right now, can only be called from main thread due to reset calls
	removing collision. May be able to break up later with some refactoring.*/
inline void USurfaceNavigationSystem::CleanupBlocks(int NumToProcess, bool NeedsLock)
{
	auto StartTime = FPlatformTime::Seconds();

	if (NeedsLock)
	{
		FReadScopeLock rCleanupLock(CleanupLock);
		CleanupBlocks(NumToProcess);
		return;
	}

	if (NumToProcess <= 0)
		return;

	{
		// Process any current blocks in the queue
		int count = 0;
		int Processed = 0;
		FSurfaceNavBlock* NavBlock;
		while (count < NumToProcess && CleanupBlocksLvl2Queue.Dequeue(NavBlock))
		{
			if (PathingRequestCount > 0 && !IsInGameThread())
				FPlatformProcess::Sleep(0.005f);

			FReadScopeLock rPathingLock(PathingLock);

			if (NavBlock->IsReadyToRelease() && NavBlock->IsActive(false) && NavBlock->IsValid())
			{
				/*if (NavBlock->BlockKey)
				{
					FWriteScopeLock wNavBlocksLock(NavBlocksLock);
					NavBlocks.Remove(*NavBlock->BlockKey);
				}*/
				BlockPool->Release(GetWorld(), NavBlock);
				Processed++;
			}
			count++;
		}
		CleanupLvl2QueueCount.Subtract(count);
	}

	{
		// Process any current blocks in the queue
		int count = 0;
		int Processed = 0;
		FSurfaceNavGridPoint* GridPoint = nullptr;
		while (count < NumToProcess && CleanupGridPointsLvl2Queue.Dequeue(GridPoint))
		{
			// If there are pathing requests waiting then process them unless we're in the game thread.
			// Being in the game thread means the world may be tearing down.
			if (PathingRequestCount > 0 && !IsInGameThread())
				FPlatformProcess::Sleep(0.005f);

			FReadScopeLock rPathingLock(PathingLock);

			if (GridPoint && GridPoint->IsDestroying() && !GridPoint->IsDestroyed())
			{
				GridPointPool->Release(GridPoint);
				Processed++;
			}
			count++;
		}
	}

	//{
	//	// Now let's see if there's any block queued up to invalidate
	//	int count = 0;
	//	float Processed = 0.f;
	//	FIntVector BlockIndex;
	//	while (Processed < NumToProcess && InvalidateBlocksQueue.Dequeue(BlockIndex))
	//	{
	//		auto NavBlockPtr = NavBlocks.Find(BlockIndex);
	//		if (NavBlockPtr && *NavBlockPtr && !BlockInRange.Contains(BlockIndex))
	//		{
	//			FSurfaceNavBlock*& NavBlock = *NavBlockPtr;
	//			NavBlock->InvalidateBlockProcessing(true);
	//			Processed += 1.f;
	//		}
	//		count++;
	//	}
	//}

	auto TimeSpent = FPlatformTime::Seconds() - StartTime;
	if (TimeSpent > 0.001f)
	{
		double EndTime = FPlatformTime::Seconds();
		//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavProxy::CleanupBlocks: Code executed in %f seconds."), TimeSpent);
	}
}

void USurfaceNavigationSystem::BlockOverlapsFoundCallback(const FIntVector& BlockKey)
{
	BlockGridPointQueue.Enqueue(NavBlocks[BlockKey]); // navBlock);
	BlockGridPointQueueCount.Increment();
}

void USurfaceNavigationSystem::BlockNavUpdatedCallback(const FIntVector& BlockKey)
{
	FWriteScopeLock rNavBlockLock(BlocksWithUpdatedNavLock);
	BlocksWithUpdatedNav.Emplace(BlockKey);
}

void USurfaceNavigationSystem::BlockMarkedDirtyCallback(const FIntVector& BlockKey)
{
	//BlocksMarkedDirty.Enqueue(BlockKey);
}

void USurfaceNavigationSystem::ProcessBlockWithUpdatedNav()
{
	TSet<FIntVector> LocalUpdatedBlocks;
	{
		FWriteScopeLock rNavBlockLock(BlocksWithUpdatedNavLock);
		LocalUpdatedBlocks.Append(BlocksWithUpdatedNav);
		BlocksWithUpdatedNav.Empty();
	}

	if (LocalUpdatedBlocks.Num() == 0 || ActivePaths.Num() == 0)
		return;

	float BlockSize = SurfaceNavProxySettings::BlockSize;
	const int32 Count = ActivePaths.Num();
	FSurfaceNavPathWeakPtr* WeakPathPtr = (ActivePaths.GetData() + Count - 1);
	// Iterate through each path and see if it's bounds intersect any of the blocks
	// getting updated. Invalidate any that do intersect since their path may have changed.
	for (int32 i = Count - 1; i >= 0; --i, --WeakPathPtr)
	{
		if (WeakPathPtr->IsValid())
		{
			for (FIntVector& LocalUpdatedBlockIndex : LocalUpdatedBlocks)
			{
				FSurfaceNavBlock** NavBlockPtr = nullptr;

				{
					FReadScopeLock rNavBlocksLock(NavBlocksLock);
					NavBlockPtr = NavBlocks.Find(LocalUpdatedBlockIndex);
				}

				if (!NavBlockPtr)
					continue;

				auto& NavBlock = *NavBlockPtr;
				if (!NavBlock)
					continue;

				// We need to expand the block bounds a bit in case a neighboring nav point
				// in another block may be affected by the updated block
				FBox BlockBox = NavBlock->GetBounds().ExpandBy(BlockSize * 1.5f);

				FSurfaceNavPathSharedPtr SharedPathPtr = WeakPathPtr->Pin();
				if (SharedPathPtr.IsValid() && SharedPathPtr->DoesIntersectBoxFast(BlockBox))
				{
					// The path intersects the block so need to invalidate it so it repaths if needed.
					SharedPathPtr->Invalidate();
					break;
				}
			}
		}
	}
}

// TODO: This name needs to be changed and refactored since it's updating more than just invokers
void USurfaceNavigationSystem::UpdateInvokerLocations()
{
	double StartTime = FPlatformTime::Seconds();

	FReadScopeLock rDestoryLock(USurfaceNavigationSystem::DestroyLock);

	if (!bInitDone || bCleanUpDone)
		return;

#if WITH_EDITOR
	// If playing in PIE, then skip processing for editor
	if (IsEditorInstanceWhileRunningPIE())
		return;
#endif 

	// This is run through a timer, so shouldn't need to lock as there
	// will always be a delay between calls to this function. We just don't
	// want multiple running at the same time.
	if (InvokersRefreshing)
		return;
	//ACharacter test;

	InvokersRefreshing = true;

	// Refresh the volume bounds which will be needed for processing the blocks
	RefreshVolumeBounds();

	// Let's also refresh the custom links we have
	RefreshCustomLinks();

	// Refresh all player locations
	RefreshPlayerLocations();

	TArray<FInvokerQueDataItem> TempInvokerData;
	if (SurfaceNavInvokers.Num())
	{
		for (auto It = SurfaceNavInvokers.CreateIterator(); It; ++It)
		{
			TWeakObjectPtr<AActor>& InvokerActorPtr = *It;

			if (!InvokerActorPtr.IsValid())
			{
				FWriteScopeLock wLock(NavInvokerLock);
				SurfaceNavInvokers.Remove(InvokerActorPtr);
				continue;
			}

			AActor* const& InvokerActor = InvokerActorPtr.Get();

			if (!InvokerActor->HasActorBegunPlay() || InvokerActor->IsActorBeingDestroyed())
				continue;

			auto InvokerComp = Cast<USurfaceNavInvokerComponent>(InvokerActor->FindComponentByClass(USurfaceNavInvokerComponent::StaticClass()));
			if (!InvokerComp)
				continue;

			auto InvokerActorLoc = InvokerActor->GetActorLocation(); // TODO: Getting access violation sometimes

			auto test = InvokerActor->GetTransform().GetLocation();
			//InvokerActor->ApplyWorldOffset()

			TempInvokerData.Emplace(FInvokerQueDataItem(
				InvokerActorLoc,
				InvokerComp->NearBlockGenerationRadius,
				InvokerComp->FarBlockGenerationRadius,
				InvokerComp->MaxDistanceFromPlayerForGeneration
			));
		}
	}

#if WITH_EDITOR
	if (GEditor && TempInvokerData.Num() == 0)
	{
#if ENGINE_MAJOR_VERSION > 4
		UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
		if (UnrealEditorSubsystem)
		{
#endif
			FVector CameraLocation;

#if ENGINE_MAJOR_VERSION < 5
			USurfaceNavigationSystem::GetLevelViewportCameraInfo(CameraLocation);
#else
			FRotator CameraRotation;
			UnrealEditorSubsystem->GetLevelViewportCameraInfo(CameraLocation, CameraRotation);
#endif
			TempInvokerData.Emplace(FInvokerQueDataItem(
				CameraLocation,
				SurfaceNavProxySettings::DisplayRadiusFromCamera + SurfaceNavProxySettings::BlockSize,
				SurfaceNavProxySettings::DisplayRadiusFromCamera + SurfaceNavProxySettings::BlockSize,
				SurfaceNavProxySettings::MaxDistanceFromPlayerForGeneration
			));
#if ENGINE_MAJOR_VERSION > 4
		}
#endif
	}
#endif

	{
		FWriteScopeLock wLock(InvokerDataLock);
		InvokerData = MoveTemp(TempInvokerData);
	}

	InvokersRefreshing = false;

	// The thread won't start processing until this is set
	//BlockQueueWorker->bInputReady = true;

	/*auto TimeSpent = FPlatformTime::Seconds() - StartTime;
	if (TimeSpent >= 0.0005f)
		UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::UpdateInvokerLocations: Code executed in %f seconds."), TimeSpent);*/

		//UE_LOG(LogTemp, Warning, TEXT("UpdateInvokerLocations: Code executed in %f seconds."), EndTime - StartTime);
}

#if ENGINE_MAJOR_VERSION < 5 && WITH_EDITOR

bool USurfaceNavigationSystem::GetLevelViewportCameraInfo(FVector& CameraLocation)
{
	bool RetVal = false;
	CameraLocation = FVector::ZeroVector;

	if (!GEditor)
		return RetVal;

	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->IsPerspective())
		{
			CameraLocation = LevelVC->GetViewLocation();
			RetVal = true;

			break;
}
	}

	return RetVal;
}

#endif

void USurfaceNavigationSystem::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	// There may have been some values changes in the proxy which would still be saved at this point so
		// let's tell the proxy that nav needs a rebuild.
	AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), ASurfaceNavProxy::StaticClass());
	ASurfaceNavProxy* SurfaceNavProxy = Cast<ASurfaceNavProxy>(FoundActor);
	if (SurfaceNavProxy)
		SurfaceNavProxy->SurfaceNavRebuildAll();
}

inline void USurfaceNavigationSystem::RefreshVolumeBounds()
{
	// If we're already refreshing then return and try again the next timer tick.
	// This will get decremented once the queue is fully processed.
	if (RefreshingVolumeCounter.Increment() > 1)
	{
		// Need to decrement since to correct for the increment since this run
		// isn't refreshing
		RefreshingVolumeCounter.Decrement();
		return;
	}

	// Get a ref to the base nav system's nav volume bounds collection
	const TSet<FNavigationBounds>& NavBounds = GetNavigationBounds();

	TSet<FSurfaceNavBounds> KeepBounds;

	// We don't want to change the collection while it's being used elsewhere
	// so begin a write lock
	{
		FWriteScopeLock wLock(VolumeBoundLock);

		for (const FNavigationBounds& navBound : NavBounds)
		{
			// Try to find the current nav bounds volume in the saved collection
			FSurfaceNavBounds* LocalNavBound = NavVolumeBounds.Find(navBound);

			// If LocalNavBound is empty then navBound is new
			if (!LocalNavBound)
			{
				// TODO: Determine if I need a call to QueueBlocksPendingDirtyWithinBounds here

				// It's new so add it to the keep collection
				KeepBounds.Emplace(FSurfaceNavBounds(navBound));
			}
			else
			{

				// Determine if the saved bounds and the current bounds are the same
#if ENGINE_MAJOR_VERSION < 5
				bool AreaBoxesAreSame = BoxEquals(LocalNavBound->AreaBox, navBound.AreaBox, SMALL_NUMBER);
#else
				bool AreaBoxesAreSame = LocalNavBound->AreaBox.Equals(navBound.AreaBox, SMALL_NUMBER);
#endif

				if (!AreaBoxesAreSame)
				{
					VolumeBoundsChangedQueue.Enqueue(MakeTuple(navBound.AreaBox, LocalNavBound->AreaBox));
					LocalNavBound->AreaBox = navBound.AreaBox;
				}

				KeepBounds.Emplace(*LocalNavBound);
			}
		}

		NavVolumeBounds = MoveTemp(KeepBounds);
	}
}

void USurfaceNavigationSystem::ProcessChangedVolumeBounds()
{
	if (bCleanUpDone || bIsCleaningUp)
		return;

	TTuple<FBox, FBox> AreaDataItem;
	while (VolumeBoundsChangedQueue.Dequeue(AreaDataItem))
	{
		if (bCleanUpDone || bIsCleaningUp)
			return;

		QueueBlocksPendingDirtyWithinBounds(AreaDataItem.Get<0>(), AreaDataItem.Get<1>());
	}

	// Need to decrement to show that this volume refresh is done
	RefreshingVolumeCounter.Decrement();
}

inline void USurfaceNavigationSystem::RefreshCustomLinks()
{
	FReadScopeLock rDestoryLock(USurfaceNavigationSystem::DestroyLock);

	if (bCleanUpDone || bIsCleaningUp)
		return;

	if (bIsProcessingNavLinks)
		return;

	bIsProcessingNavLinks = true;

	// TODO: Need to do this more like RefreshVolumeBounds so I know when nav links are added/removed
	// and can make needed calls to update the navigation

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
	const TMap<uint32, FNavigationSystem::FCustomLinkOwnerInfo> LocalCustomLinks = GetCustomLinks();
#else
	const TMap<FNavLinkId, FNavigationSystem::FCustomLinkOwnerInfo> LocalCustomLinks = GetCustomLinks();
#endif

	TArray<FSurfaceNavLinkData*> CustomLinks;
	CustomLinks.Reserve(LocalCustomLinks.Num());

	// Iterate through all nav links and store essential data in a collection
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
	for (const TPair<uint32, FNavigationSystem::FCustomLinkOwnerInfo>& CustomLinkKeyPair : LocalCustomLinks)
#else
	for (const TPair<FNavLinkId, FNavigationSystem::FCustomLinkOwnerInfo>& CustomLinkKeyPair : LocalCustomLinks)
#endif	
	{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
		int64 LinkId = CustomLinkKeyPair.Key;
#else
		int64 LinkId = CustomLinkKeyPair.Key.GetId();
#endif

		FNavigationSystem::FCustomLinkOwnerInfo CustomLinkInfo = CustomLinkKeyPair.Value;
		if (CustomLinkInfo.IsValid())
		{
			INavLinkCustomInterface* CustomLink = CustomLinkInfo.LinkInterface;

			UObject* CustomLinkOb = CustomLink->GetLinkOwner();
			UActorComponent* OwnerComp = Cast<UActorComponent>(CustomLinkOb);

			// We need the components actor so we can get the proper transform of the points
			AActor* OwnerActor = OwnerComp ? OwnerComp->GetOwner() : Cast<AActor>(CustomLinkOb);

			// No owner so skip
			if (!OwnerActor)
				continue;

			ENavLinkDirection::Type LinkDir = ENavLinkDirection::BothWays;
			FVector RelativePtA, RelativePtB;
			CustomLink->GetLinkData(RelativePtA, RelativePtB, LinkDir);

			// Use the owner to transform the relative position to world space
			const FTransform OwnerActorTM = OwnerActor->GetTransform();
			FVector WorldPtA = OwnerActorTM.TransformPosition(RelativePtA);
			FVector WorldPtB = OwnerActorTM.TransformPosition(RelativePtB);

			auto NavArea = CustomLink->GetLinkAreaClass()->GetDefaultObject<UNavArea>();

			bool IsRelevant = true;
			UNavLinkCustomComponent* NavLinkComp = Cast<UNavLinkCustomComponent>(OwnerComp);
			if (NavLinkComp)
				IsRelevant = NavLinkComp->IsNavigationRelevant();

			auto NavLinkData = new FSurfaceNavLinkData(LinkId, WorldPtA, WorldPtB, LinkDir, SurfaceNavProxySettings::GridPointDistance * 1.25f, NavArea, IsRelevant);
			CustomLinks.Emplace(NavLinkData);

			// If this is a Nav Link Proxy, then we may also have a collection of 
			// simple links to process
			auto LinkProxy = Cast<ANavLinkProxy>(OwnerActor);
			if (LinkProxy)
			{
				for (FNavigationLink& PointLink : LinkProxy->PointLinks)
				{
					WorldPtA = OwnerActorTM.TransformPosition(PointLink.Left);
					WorldPtB = OwnerActorTM.TransformPosition(PointLink.Right);
					LinkDir = PointLink.Direction;

					NavArea = PointLink.GetAreaClass()->GetDefaultObject<UNavArea>();

					auto CustomPointLink = new FSurfaceNavLinkData(WorldPtA, WorldPtB, LinkDir, PointLink.SnapRadius, NavArea, true, LinkId);

					// Since this is a simple link without an ID, let's set the ID to the negative of the hash
					// of the object.
					CustomPointLink->LinkID = -((int64)CustomPointLink->GetHash());

					// Add the new link to the collection
					CustomLinks.Emplace(CustomPointLink);
				}
			}
		}
	}

	// TODO: This section may not ever be getting used. Need to verify.
	auto LocalNavLinkComponents = GetSurfaceNavLinkComponents();
	for (const auto& NavLinkComp : LocalNavLinkComponents)
	{
		// We need the components actor so we can get the proper transform of the points
		AActor* OwnerActor = NavLinkComp ? NavLinkComp->GetOwner() : nullptr;

		// No owner so skip
		if (!OwnerActor)
			continue;

		// Use the owner to transform the relative position to world space
		const FTransform OwnerActorTM = OwnerActor->GetTransform();

		bool IsRelevant = NavLinkComp->IsNavigationRelevant();

		for (FNavigationLink& PointLink : NavLinkComp->Links)
		{
			FVector WorldPtA = OwnerActorTM.TransformPosition(PointLink.Left);
			FVector WorldPtB = OwnerActorTM.TransformPosition(PointLink.Right);
			ENavLinkDirection::Type LinkDir = PointLink.Direction;

			auto NavArea = PointLink.GetAreaClass()->GetDefaultObject<UNavArea>();

			auto CustomPointLink = new FSurfaceNavLinkData(WorldPtA, WorldPtB, LinkDir, PointLink.SnapRadius, NavArea, IsRelevant, -1);

			// Since this is a simple link without an ID, let's set the ID to the negative of the hash
			// of the object.
			CustomPointLink->LinkID = -((int64)CustomPointLink->GetHash());

			// Add the new link to the collection
			CustomLinks.Emplace(CustomPointLink);
		}
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, CustomLinks]()
		{
			if (bCleanUpDone || bIsCleaningUp)
			{
				for (auto& CustomLink : CustomLinks)
				{
					CustomLink->Destroy();
					delete CustomLink;
				}

				bIsProcessingNavLinks = false;
				return;
			}

			//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::RefreshCustomLinks(): Read Lock - GetDestroyLock"));
			FReadScopeLock rDestoryLock(USurfaceNavigationSystem::DestroyLock);

			if (bCleanUpDone || bIsCleaningUp)
			{
				for (auto& CustomLink : CustomLinks)
				{
					CustomLink->Destroy();
					delete CustomLink;
				}

				bIsProcessingNavLinks = false;
				return;
			}

			FWriteScopeLock wLock(SurfaceNavCustomLinkLock);

			TArray<int64> RemoveLinkKeys;
			RemoveLinkKeys.Reserve(SurfaceNavCustomLinksMap.Num());
			SurfaceNavCustomLinksMap.GetKeys(RemoveLinkKeys);

			auto TraceSphere = FCollisionShape::MakeSphere(10.f);
			auto TraceSphereQueryParams = FCollisionQueryParams();
			TraceSphereQueryParams.bTraceComplex = false;
			TraceSphereQueryParams.bReturnPhysicalMaterial = false;
			TraceSphereQueryParams.bSkipNarrowPhase = true;
			auto DefFQuat = FQuat();

			// Loop through the nav links we got from the nav system and determine if they changed
			// then take appropriate action
			for (auto MyCustomLink : CustomLinks)
			{
				auto LinkId = MyCustomLink->LinkID;

				auto CurCustomLink = SurfaceNavCustomLinksMap.Find(LinkId);

				// if we don't have this nav link in the collection yet, then add it
				// and queue any intersecting blocks to mark them dirty
				if (!CurCustomLink)
				{
					// Let's try to get a normal for point A
					FHitResult OutHit;
					if (GetWorld()->SweepSingleByProfile
					(
						OutHit,
						MyCustomLink->WorldPtA,
						MyCustomLink->WorldPtA,
						DefFQuat,
						FName("IgnoreOnlyPawn"),
						TraceSphere,
						TraceSphereQueryParams
					))
					{
						MyCustomLink->LinkNavPointA->HitNormal = OutHit.ImpactNormal;
					}

					// Let's try to get a normal for point B
					if (GetWorld()->SweepSingleByProfile
					(
						OutHit,
						MyCustomLink->WorldPtB,
						MyCustomLink->WorldPtB,
						DefFQuat,
						FName("IgnoreOnlyPawn"),
						TraceSphere,
						TraceSphereQueryParams
					))
					{
						MyCustomLink->LinkNavPointB->HitNormal = OutHit.ImpactNormal;
					}

					// Add to our own custom nav link collection
					SurfaceNavCustomLinksMap.Emplace(LinkId, FSurfaceNavLinkData(MyCustomLink->LinkID, MyCustomLink->WorldPtA, MyCustomLink->WorldPtB, MyCustomLink->LinkDir, MyCustomLink->Radius, MyCustomLink->LinkNavPointA->NavArea, MyCustomLink->bIsRelevant, MyCustomLink->ParentLinkId));

					// Find all intersecting blocks and mark them dirty
					QueueBlocksAffectedByLink(MyCustomLink);

				}
				// if the nav link is already in the collection then check it's positions and mark
				// block dirty if the positions changed.
				else
				{
					// This key was found so remove it from the remove keys collection
					RemoveLinkKeys.Remove(LinkId);

					// If their hashes are the same then nothing has changed so skip
					if (CurCustomLink->GetHash() == MyCustomLink->GetHash())
					{
						// Need to destroy and delete the temporary data that was created
						MyCustomLink->Destroy();
						delete MyCustomLink;
						continue;
					}

					// Find all intersecting blocks for the previous points and mark them dirty
					QueueBlocksAffectedByLink(CurCustomLink);

					// Update the current link object with the new point data
					CurCustomLink->Update(MyCustomLink->WorldPtA, MyCustomLink->WorldPtB, MyCustomLink->LinkDir, MyCustomLink->Radius, MyCustomLink->LinkNavPointA->NavArea, MyCustomLink->bIsRelevant);

					// Let's try to get a normal for point A
					FHitResult OutHit;
					if (GetWorld()->SweepSingleByProfile
					(
						OutHit,
						CurCustomLink->WorldPtA,
						CurCustomLink->WorldPtA,
						DefFQuat,
						FName("IgnoreOnlyPawn"),
						TraceSphere,
						TraceSphereQueryParams
					))
					{
						CurCustomLink->LinkNavPointA->HitNormal = OutHit.ImpactNormal;
					}

					// Let's try to get a normal for point B
					if (GetWorld()->SweepSingleByProfile
					(
						OutHit,
						CurCustomLink->WorldPtB,
						CurCustomLink->WorldPtB,
						DefFQuat,
						FName("IgnoreOnlyPawn"),
						TraceSphere,
						TraceSphereQueryParams
					))
					{
						CurCustomLink->LinkNavPointB->HitNormal = OutHit.ImpactNormal;
					}

					// Find all intersecting blocks for the new points and mark them dirty
					QueueBlocksAffectedByLink(MyCustomLink);
				}

				MyCustomLink->Destroy();
				QueueNavLinkToDelete(MyCustomLink, this);
			}

			for (auto& RemoveLinkKey : RemoveLinkKeys)
			{
				auto CustomLink = SurfaceNavCustomLinksMap.Find(RemoveLinkKey);

				if (!CustomLink)
					continue;

				SurfaceNavCustomLinksMap.Remove(RemoveLinkKey);

				QueueBlocksAffectedByLink(CustomLink);

				CustomLink->Destroy();

				// TODO: Deleting this object can lead to a crash, need to look into this
				//QueueNavLinkToDelete(CustomLink, this);
				//delete CustomLink;
			}

			bIsProcessingNavLinks = false;
		});
}

void USurfaceNavigationSystem::QueueBlocksAffectedByLink(FSurfaceNavLinkData* NavLink)
{
	FBox NewLinkBounds(ForceInitToZero);
	NewLinkBounds += NavLink->WorldPtA;
	NewLinkBounds += NavLink->WorldPtB;

	// Shift the bounds by negative the grid offset so we get the correct block index.
	// Also expand by half the grid point distance in case we're near the edge of a block.
	NewLinkBounds = NewLinkBounds.ShiftBy(-SurfaceNavProxySettings::GridOffset).ExpandBy(SurfaceNavProxySettings::GridPointDistance * 0.5f);

	auto BlockSize = SurfaceNavProxySettings::BlockSize;
	auto gridPointDistance = SurfaceNavProxySettings::GridPointDistance;

	double startX = round((NewLinkBounds.Min.X - gridPointDistance) / BlockSize);
	double endX = round((NewLinkBounds.Max.X + gridPointDistance) / BlockSize);

	double startY = round((NewLinkBounds.Min.Y - gridPointDistance) / BlockSize);
	double endY = round((NewLinkBounds.Max.Y + gridPointDistance) / BlockSize);

	double startZ = round((NewLinkBounds.Min.Z - gridPointDistance) / BlockSize);
	double endZ = round((NewLinkBounds.Max.Z + gridPointDistance) / BlockSize);

	double x = startX;
	double y = startY;
	double z = startZ;

	// Opting for single loop instead of 3 nested loops for slight perf increase.
	// Makes it a little harder to follow, but this is a heavily used tight loop.
	bool CanIterate = true;
	while (CanIterate)
	{
		FIntVector BlockIndex(x, y, z);
		QueueBlockPendingDirty(BlockIndex);

		z++;
		if (z <= endZ)
			continue;

		z = startZ;
		y++;
		if (y <= endY)
			continue;

		y = startY;
		x++;
		if (x <= endX)
			continue;

		CanIterate = false;
	}
}

void USurfaceNavigationSystem::RefreshPlayerLocations()
{
	auto world = GetWorld();

	if (world != NULL)
	{
		TArray<FVector> NewPlayerLocations;
		NewPlayerLocations.Reserve(PlayerLocations.Num());
		//int CurrentPlayerLocCount = PlayerLocations.Num();
		//int NewCount = 0;

		for (FConstPlayerControllerIterator iter = world->GetPlayerControllerIterator(); iter; ++iter)
		{
			auto player = Cast<APlayerController>(*iter);
			if (!player)
				continue;

			auto Pawn = player->GetPawn();
			if (!Pawn)
				continue;

			//NewCount++;

			NewPlayerLocations.Emplace(Pawn->GetActorLocation());

			/*if (NewCount > CurrentPlayerLocCount)
				PlayerLocations.Emplace(Pawn->GetActorLocation());
			else
				PlayerLocations[NewCount - 1] = Pawn->GetActorLocation();*/
		}


#if WITH_EDITOR
		if (GEditor && NewPlayerLocations.Num() == 0)
		{
#if ENGINE_MAJOR_VERSION > 4
			UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
			if (UnrealEditorSubsystem)
			{
#endif
				FVector CameraLocation;

#if ENGINE_MAJOR_VERSION < 5
				USurfaceNavigationSystem::GetLevelViewportCameraInfo(CameraLocation);
#else
				FRotator CameraRotation;
				UnrealEditorSubsystem->GetLevelViewportCameraInfo(CameraLocation, CameraRotation);
#endif

				//NewCount++;

				NewPlayerLocations.Emplace(CameraLocation);

				/*if (NewCount > CurrentPlayerLocCount)
					PlayerLocations.Emplace(CameraLocation);
				else
					PlayerLocations[NewCount - 1] = CameraLocation;*/

#if ENGINE_MAJOR_VERSION > 4
			}
#endif
		}
#endif

		FWriteScopeLock wPlayerLocationLock(PlayerLocationLock);
		PlayerLocations = MoveTemp(NewPlayerLocations);

		// Clean up any loose ends by removing old excess player locations
		/*if (NewCount < CurrentPlayerLocCount)
			PlayerLocations.RemoveAtSwap(NewCount, CurrentPlayerLocCount - NewCount);*/
	}
}

TArray<FSurfaceNavGridPoint*> USurfaceNavigationSystem::GetGridPointsInBox(FBox TestBox)
{
	// Need to get an offset of this box to use when comparing against calculated
	// block indices and locations.
	FBox OffsetTestBox = TestBox.ShiftBy(-SurfaceNavProxySettings::GridOffset);

	TArray<FSurfaceNavGridPoint*> GridPointsResult;

	float BlockSize = SurfaceNavProxySettings::BlockSize;
	float blockExtent = SurfaceNavProxySettings::BlockExtent;

	// Dividing takes more cycles than multiplication so make a local var to use
	// for multiplication in the loop
	float blockSizeMultiplier = 1.f / BlockSize;

	FVector CenterLoc = OffsetTestBox.GetCenter();
	double mapX = round(CenterLoc.X * blockSizeMultiplier);
	double mapY = round(CenterLoc.Y * blockSizeMultiplier);
	double mapZ = round(CenterLoc.Z * blockSizeMultiplier);

	FVector BoxExtent = OffsetTestBox.GetExtent();
	float NavBlocksFromCenterX = roundf((BoxExtent.X + BlockSize + blockExtent) * blockSizeMultiplier);
	float NavBlocksFromCenterY = roundf((BoxExtent.Y + BlockSize + blockExtent) * blockSizeMultiplier);
	float NavBlocksFromCenterZ = roundf((BoxExtent.Z + BlockSize + blockExtent) * blockSizeMultiplier);

	float startX = mapX - NavBlocksFromCenterX;
	float endX = mapX + NavBlocksFromCenterX;

	float startY = mapY - NavBlocksFromCenterY;
	float endY = mapY + NavBlocksFromCenterY;

	float startZ = mapZ - NavBlocksFromCenterZ;
	float endZ = mapZ + NavBlocksFromCenterZ;

	float x = startX;
	float y = startY;
	float z = startZ;

	// Using a while with a flag instead of nested for loops using x, y, z
	// for slight perf boost.
	bool CanIterate = true;
	while (CanIterate)
	{
		FVector BlockSpawnLoc(x * BlockSize, y * BlockSize, z * BlockSize);
		FBox BlockBox(BlockSpawnLoc - blockExtent, BlockSpawnLoc + blockExtent);

		// Use the offset test box for this test since we're working with a calculated
		// block location.
#if ENGINE_MAJOR_VERSION < 5
		float SqDistToBlock = ComputeSquaredDistanceToBox(BlockBox, OffsetTestBox);
#else
		float SqDistToBlock = BlockBox.ComputeSquaredDistanceToBox(OffsetTestBox);
#endif

		if (SqDistToBlock <= 0.f)
		{
			FSurfaceNavBlock** NavBlockPtr = nullptr;

			{
				FReadScopeLock rNavBlocksLock(NavBlocksLock);
				NavBlockPtr = NavBlocks.Find(FIntVector(x, y, z));
			}

			if (NavBlockPtr && *NavBlockPtr)
			{
				FSurfaceNavBlock* NavBlock = *NavBlockPtr;
				if (NavBlock && NavBlock->IsValid())
				{
					FReadScopeLock rLock(NavBlock->NavBlockLock);

					if (NavBlock && NavBlock->IsValid())
					{
						auto GridPoints = NavBlock->SurfaceNavGridPoints;
						for (int i = 0; i < GridPoints.Num(); i++)
							for (int j = 0; j < GridPoints[i].Num(); j++)
								for (int k = 0; k < GridPoints[i][j].Num(); k++)
								{
									auto GridPoint = GridPoints[i][j][k];
									if (GridPoint && GridPoint->IsValid())
									{
										// Use the non-offset box location to find the distance since we're working with a real,
										// stored, non-calculated grid point location.
										float GridPointDistSq = TestBox.ComputeSquaredDistanceToPoint(GridPoint->WorldLocation);
										if (GridPointDistSq <= 0.f)
										{
											GridPointsResult.Emplace(GridPoint);
										}
									}
								}
					}
				}
			}
		}


		z++;
		if (z <= endZ)
			continue;

		z = startZ;
		y++;
		if (y <= endY)
			continue;

		y = startY;
		x++;
		if (x <= endX)
			continue;

		CanIterate = false;
}

	return GridPointsResult;
}

TArray<FSurfaceNavGridPoint*> USurfaceNavigationSystem::GetGridPointsInRadius(FVector CenterLoc, float Radius)
{
	// Need to get an offset of this location to use when comparing against calculated
	// block indices and locations.
	FVector OffsetCenterLoc = CenterLoc - SurfaceNavProxySettings::GridOffset;

	TArray<FSurfaceNavGridPoint*> GridPointsResult;

	float BlockSize = SurfaceNavProxySettings::BlockSize;
	float blockExtent = SurfaceNavProxySettings::BlockExtent;

	// Dividing takes more cycles than multiplication so make a local var to use
	// for multiplication in the loop
	float blockSizeMultiplier = 1.f / BlockSize;

	float RadiusSq = FMath::Square(Radius);
	FBox OffsetTestBox(OffsetCenterLoc - Radius, OffsetCenterLoc + Radius);

	double mapX = round(OffsetCenterLoc.X * blockSizeMultiplier);
	double mapY = round(OffsetCenterLoc.Y * blockSizeMultiplier);
	double mapZ = round(OffsetCenterLoc.Z * blockSizeMultiplier);

	float NavBlocksFromCenter = roundf((Radius + BlockSize + blockExtent) * blockSizeMultiplier);

	float startX = mapX - NavBlocksFromCenter;
	float endX = mapX + NavBlocksFromCenter;

	float startY = mapY - NavBlocksFromCenter;
	float endY = mapY + NavBlocksFromCenter;

	float startZ = mapZ - NavBlocksFromCenter;
	float endZ = mapZ + NavBlocksFromCenter;

	float x = startX;
	float y = startY;
	float z = startZ;

	// Using a while with a flag instead of nested for loops using x, y, z
	// for slight perf boost.
	bool CanIterate = true;
	while (CanIterate)
	{
		FVector BlockSpawnLoc(x * BlockSize, y * BlockSize, z * BlockSize);
		FBox BlockBox(BlockSpawnLoc - blockExtent, BlockSpawnLoc + blockExtent);

#if ENGINE_MAJOR_VERSION < 5
		float SqDistToBlock = ComputeSquaredDistanceToBox(BlockBox, OffsetTestBox);
#else
		float SqDistToBlock = BlockBox.ComputeSquaredDistanceToBox(OffsetTestBox);
#endif

		if (SqDistToBlock <= 0.f)
		{
			FSurfaceNavBlock** NavBlockPtr = nullptr;

			{
				FReadScopeLock rNavBlocksLock(NavBlocksLock);
				NavBlockPtr = NavBlocks.Find(FIntVector(x, y, z));
			}

			if (NavBlockPtr && *NavBlockPtr)
			{
				FSurfaceNavBlock* NavBlock = *NavBlockPtr;
				if (NavBlock && NavBlock->IsValid())
				{
					FReadScopeLock rLock(NavBlock->NavBlockLock);

					if (NavBlock && NavBlock->IsValid())
					{
						auto GridPoints = NavBlock->SurfaceNavGridPoints;
						for (int i = 0; i < GridPoints.Num(); i++)
							for (int j = 0; j < GridPoints[i].Num(); j++)
								for (int k = 0; k < GridPoints[i][j].Num(); k++)
								{
									auto GridPoint = GridPoints[i][j][k];
									if (GridPoint && GridPoint->IsValid())
									{
										float GridPointDistSq = FVector::DistSquared(CenterLoc, GridPoint->WorldLocation);
										if (GridPointDistSq <= RadiusSq)
										{
											GridPointsResult.Emplace(GridPoint);
										}
									}
								}
					}
				}
			}
		}


		z++;
		if (z <= endZ)
			continue;

		z = startZ;
		y++;
		if (y <= endY)
			continue;

		y = startY;
		x++;
		if (x <= endX)
			continue;

		CanIterate = false;
	}

	return GridPointsResult;
}

TArray<FSurfaceNavPoint*> USurfaceNavigationSystem::GetNavPointsInRadius(FVector CenterLoc, float Radius)
{
	TArray<FSurfaceNavPoint*> NavPointsResult;

	return NavPointsResult;
}

void USurfaceNavigationSystem::DrawDebugNavData(const float DrawRadius,
	const bool ShowBlocks, const bool ShowGridPoints, const bool ShowNavPoints, const bool ShowNavPointNormals,
	const bool ShowNeighborDirLines, const bool ShowWhereNavigationEnds, const bool ShowNavLinkPoints, const float Duration)
{
#if WITH_EDITOR
	if (!GEditor)
		return;

	//if (GEngine)
	//{
	//	//LastBlockQueueCount
	//	FString Msg = "DSN-Blocks to Process: " + FString::FromInt(LastBlockQueueCount);
	//	GEngine->AddOnScreenDebugMessage(1234567, -1.f, FColor::Cyan, Msg);

	//	Msg = "DSN-Block Count: " + FString::FromInt(NavBlocks.Num());
	//	GEngine->AddOnScreenDebugMessage(1234568, -1.f, FColor::Cyan, Msg);
	//}

	FColor DrawColorNormal = FLinearColor(0.f, 0.8f, 0.f, 1.0f).ToFColor(true);

	FVector CameraLocation;

#if ENGINE_MAJOR_VERSION > 4
	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
	if (UnrealEditorSubsystem)
	{
#endif

#if ENGINE_MAJOR_VERSION < 5
		USurfaceNavigationSystem::GetLevelViewportCameraInfo(CameraLocation);
#else
		FRotator CameraRotation;
		UnrealEditorSubsystem->GetLevelViewportCameraInfo(CameraLocation, CameraRotation);
#endif

		bool bIsSimulating = false;
		if (GEditor->GameViewport && GEditor->GameViewport->GetGameViewport())
			bIsSimulating = GEditor->GameViewport->GetGameViewport()->GetPlayInEditorIsSimulate();

		// Check if we're not simulating and have a player controller
		if (!bIsSimulating && GetWorld() && GetWorld()->GetFirstPlayerController())
		{
			CameraLocation = GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();

			// Let's see if the debug camera is active
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (PC->CheatManager && PC->CheatManager->DebugCameraControllerRef)
			{
				ADebugCameraController* const DCC = PC->CheatManager->DebugCameraControllerRef;
				if (DCC && DCC->OriginalPlayer && DCC->GetSpectatorPawn())
				{
					CameraLocation = DCC->GetSpectatorPawn()->GetActorLocation();
				}
			}
		}

		//UE_LOG(LogTemp, Warning, TEXT("USurfaceNavigationSystem::DrawDebugNavData: CameraLocation=%s"), *CameraLocation.ToString());
#if ENGINE_MAJOR_VERSION > 4
	}
#endif

	float BlockLabelDistSq = FMath::Square(SurfaceNavProxySettings::BlockSize * 2.f);
	float DisplayRadiusSq = FMath::Square(DrawRadius);
	float PaddedDisplayRadiusSq = FMath::Square(DrawRadius + SurfaceNavProxySettings::BlockExtent);

	AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), ASurfaceNavProxy::StaticClass());
	ASurfaceNavProxy* SurfaceNavProxy = Cast<ASurfaceNavProxy>(FoundActor);

	TMap<FIntVector, FSurfaceNavBlock*> MyNavBlocks;
	{
		FReadScopeLock rNavBlocksLock(NavBlocksLock);
		MyNavBlocks = NavBlocks;
	}


	for (const TPair<FIntVector, FSurfaceNavBlock*>& BlockPair : MyNavBlocks)
	{
		FSurfaceNavBlock* const& NavBlock = BlockPair.Value;
		FReadScopeLock rLock(NavBlock->NavBlockLock);

		if (!NavBlock || !NavBlock->IsActive() || !NavBlock->IsValid())
			continue;

		if (FVector::DistSquared(NavBlock->GetLocation(), CameraLocation) > PaddedDisplayRadiusSq)
			continue;

		TArray<TArray<TArray<FSurfaceNavGridPoint*>>> GridPoints = NavBlock->SurfaceNavGridPoints;

		if (ShowBlocks)
		{
			DrawDebugBox(
				GetWorld(),
				NavBlock->GetLocation(),
				FVector(SurfaceNavProxySettings::BlockExtent),
				FColor::Red,
				false,
				Duration,
				0U,
				2.0f);

			if (SurfaceNavProxy && FVector::DistSquared(NavBlock->GetLocation(), CameraLocation) <= BlockLabelDistSq)
			{

			}
		}

		TSet<FIntVector> GridPointsProcessed;

		if (!true && !ShowGridPoints && !ShowNavPoints && !ShowNeighborDirLines && !ShowWhereNavigationEnds && !ShowNavLinkPoints)
			continue;

		for (int i = 0; i < GridPoints.Num(); i++)
			for (int j = 0; j < GridPoints[i].Num(); j++)
				for (int k = 0; k < GridPoints[i][j].Num(); k++)
				{
					auto GridPoint = GridPoints[i][j][k];
					if (!GridPoint || !GridPoint->IsValid())
						continue;

					check(!GridPointsProcessed.Contains(GridPoint->VectorIndex));

					GridPointsProcessed.Emplace(GridPoint->VectorIndex);

					bool IsOrphaned = GridPoint->IsOrphaned();
					bool IsNarrow = GridPoint->IsNarrow();

					if (IsOrphaned)
						continue;

					if (FVector::DistSquared(GridPoint->WorldLocation, CameraLocation) > DisplayRadiusSq)
						continue;

					TSet<FIntVector> NavPointsProcessed;

					int NavPointCount = 0;
					//for (const TPair<FIntVector, FSurfaceNavPoint*>& NavPointPair : GridPoint->NavPoints)
					for (auto It = GridPoint->NavPoints.CreateConstIterator(); It; ++It)
					{
						FReadScopeLock rNavPointLock(NavBlock->NavPointLock);

						auto NavPointPair = *It;

						if (NavPointPair.Value)
						{
							check(!NavPointsProcessed.Contains(NavPointPair.Key));

							NavPointsProcessed.Emplace(NavPointPair.Key);

							NavPointCount++;

							//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::ShowNavPoints: IsOrphaned = %d"), NavPointPair.Value->OwningGridPoint->IsOrphaned());

							if (ShowNavPoints)
							{
								DrawDebugPoint
								(
									GetWorld(),
									NavPointPair.Value->WorldLocation,
									10.f,
									IsOrphaned ? FColor::Red : IsNarrow ? FColor::Cyan : NavPointPair.Value->NavArea->DrawColor,
									false,
									Duration
								);
							}

							if (ShowNavPointNormals)
							{
								FVector ColorVector = NavPointPair.Value->GetNormal(true).GetAbs() * 255.f;

								DrawDebugDirectionalArrow
								(
									GetWorld(),
									NavPointPair.Value->WorldLocation,
									NavPointPair.Value->WorldLocation + (NavPointPair.Value->GetNormal(true) * 20.f),
									50.f,
									FColor(ColorVector.X, ColorVector.Y, ColorVector.Z),
									false,
									Duration,
									0,
									1.f
								);
							}

							if (ShowNeighborDirLines || ShowWhereNavigationEnds)
							{
								for (int NeighborIndex = 0; NeighborIndex < 8; NeighborIndex++)
								{
									TSharedPtr<FSurfaceNavPointBase> Neighbor = nullptr;
									NavPointPair.Value->GetNeighborAt(NeighborIndex, Neighbor);
									if (ShowNeighborDirLines && Neighbor)
										NeighborDebugLine(NavPointPair.Value->WorldLocation, Neighbor->WorldLocation, Duration);

									if (ShowWhereNavigationEnds && !Neighbor && NeighborIndex < 4)
									{
										FVector VectorToNeighbor = NavPointPair.Value->GetNeighborVectorAtIndex(NeighborIndex);
										FVector LocationOffset = NavPointPair.Value->WorldLocation + (VectorToNeighbor * 20.f);
										DrawDebugDirectionalArrow
										(
											GetWorld(),
											NavPointPair.Value->WorldLocation,
											NavPointPair.Value->WorldLocation + (VectorToNeighbor * 20.f),
											16.f,
											FColor::Red,
											false,
											Duration,
											0,
											1.f
										);
										DrawDebugBox(GetWorld(), LocationOffset, FVector(10.f), FColor::Red, false, Duration, 0, 1.f);
									}
								}
							}

							FReadScopeLock rNavLinkLock(SurfaceNavCustomLinkLock);

							if (ShowNavLinkPoints && ShowNavPoints && NavPointPair.Value->CustomLinkNeighbors.Num() > 0)
							{
								for (const auto& CustomLinkNeighbor : NavPointPair.Value->CustomLinkNeighbors)
								{
									if (auto PinnedCustomLinkNeighbor = CustomLinkNeighbor.Pin())
									{
										if (!PinnedCustomLinkNeighbor->IsValid())
											continue;

										DrawDebugLine(
											GetWorld(),
											NavPointPair.Value->WorldLocation,
											PinnedCustomLinkNeighbor->WorldLocation,
											(IsOrphaned ? FColor::Red : IsNarrow ? FColor::Cyan : NavPointPair.Value->NavArea->DrawColor).WithAlpha(0.f),
											false,
											Duration,
											0,
											1.f
										);
									}
									/*DrawDebugPoint
									(
										GetWorld(),
										*CustomLinkNeighbor->WorldLocation,
										10.f,
										IsOrphaned ? FColor::Red : IsNarrow ? FColor::Cyan : NavPointPair.Value->NavArea->DrawColor,
										false,
										Duration
									);*/
								}
							}
						}
					}

					////UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::ShowNavPoints: IsOrphaned = %d"), IsOrphaned);

					if (ShowGridPoints && NavPointCount > 0)
					{
						// Keeping this here because I like this view better sometimes when working through bugs,
						// but it may confuse others not intimately familiar with how the navigation generation works.
						// This can give the impression that a grid of voxel like cubes are used, but that is not
						// completely accurate.
						/*DrawDebugBox(
							GetWorld(),
							*GridPoint->WorldLocation,
							FVector(SurfaceNavProxySettings::GridPointDistance / 2.f - 2.f),
							IsOrphaned ? FColor::Red : IsNarrow ? FColor::Cyan : FColorList::DarkGreen,
							false,
							Duration,
							0U,
							1.0f);*/

						DrawDebugCircle
						(
							GetWorld(),
							GridPoint->WorldLocation,
							10.f,
							4,
							IsOrphaned ? FColor::Red : IsNarrow ? FColor::Cyan : FColorList::DarkGreen,
							false,
							Duration,
							0,
							1.f
						);
					}
				}
	}

	if (ShowNavLinkPoints)
	{
		FReadScopeLock rNavLinkLock(SurfaceNavCustomLinkLock);

		for (auto& SurfaceNavCustomLinkPair : SurfaceNavCustomLinksMap)
		{
			if (!SurfaceNavCustomLinkPair.Value.bIsRelevant)
				continue;

			if (FVector::DistSquared(SurfaceNavCustomLinkPair.Value.LinkNavPointA->WorldLocation, CameraLocation) > DisplayRadiusSq &&
				FVector::DistSquared(SurfaceNavCustomLinkPair.Value.LinkNavPointB->WorldLocation, CameraLocation) > DisplayRadiusSq)
				continue;

			FVector OffsetVector = (SurfaceNavCustomLinkPair.Value.LinkNavPointB->WorldLocation - SurfaceNavCustomLinkPair.Value.LinkNavPointA->WorldLocation).GetSafeNormal() * 24.f;

			DrawDebugSphere
			(
				GetWorld(),
				SurfaceNavCustomLinkPair.Value.LinkNavPointA->WorldLocation,
				18.f,
				4,
				SurfaceNavCustomLinkPair.Value.LinkNavPointA->NavArea->DrawColor,
				false,
				Duration,
				0,
				1.0f
			);

			DrawDebugSphere
			(
				GetWorld(),
				SurfaceNavCustomLinkPair.Value.LinkNavPointB->WorldLocation,
				18.f,
				4,
				SurfaceNavCustomLinkPair.Value.LinkNavPointB->NavArea->DrawColor,
				false,
				Duration,
				0,
				1.0f
			);

			if (SurfaceNavCustomLinkPair.Value.LinkNavPointA->GetOtherLinkPoint() != nullptr)
			{

				DrawDebugDirectionalArrow
				(
					GetWorld(),
					SurfaceNavCustomLinkPair.Value.LinkNavPointA->WorldLocation + OffsetVector,
					SurfaceNavCustomLinkPair.Value.LinkNavPointB->WorldLocation - OffsetVector,
					100.f,
					SurfaceNavCustomLinkPair.Value.LinkNavPointA->NavArea->DrawColor,
					false,
					Duration,
					0,
					1.f
				);
			}

			if (SurfaceNavCustomLinkPair.Value.LinkNavPointB->GetOtherLinkPoint() != nullptr)
			{

				DrawDebugDirectionalArrow
				(
					GetWorld(),
					SurfaceNavCustomLinkPair.Value.LinkNavPointB->WorldLocation - OffsetVector,
					SurfaceNavCustomLinkPair.Value.LinkNavPointA->WorldLocation + OffsetVector,
					100.f,
					SurfaceNavCustomLinkPair.Value.LinkNavPointB->NavArea->DrawColor,
					false,
					Duration,
					0,
					1.f
				);
			}

			if (ShowNavPointNormals)
			{
				FVector LinkPointNormal = SurfaceNavCustomLinkPair.Value.LinkNavPointA->GetNormal();
				FVector ColorVector = LinkPointNormal.GetAbs() * 255.f;

				DrawDebugDirectionalArrow
				(
					GetWorld(),
					SurfaceNavCustomLinkPair.Value.LinkNavPointA->WorldLocation,
					//*NavPointPair.Value->WorldLocation + (*NavPointPair.Value->Normal * 20.f),
					SurfaceNavCustomLinkPair.Value.LinkNavPointA->WorldLocation + (LinkPointNormal * 20.f),
					50.f,
					FColor(ColorVector.X, ColorVector.Y, ColorVector.Z),
					false,
					Duration,
					0,
					1.f
				);

				LinkPointNormal = SurfaceNavCustomLinkPair.Value.LinkNavPointB->GetNormal();
				ColorVector = SurfaceNavCustomLinkPair.Value.LinkNavPointB->GetNormal().GetAbs() * 255.f;

				DrawDebugDirectionalArrow
				(
					GetWorld(),
					SurfaceNavCustomLinkPair.Value.LinkNavPointB->WorldLocation,
					//*NavPointPair.Value->WorldLocation + (*NavPointPair.Value->Normal * 20.f),
					SurfaceNavCustomLinkPair.Value.LinkNavPointB->WorldLocation + (LinkPointNormal * 20.f),
					50.f,
					FColor(ColorVector.X, ColorVector.Y, ColorVector.Z),
					false,
					Duration,
					0,
					1.f
				);
			}
		}
	}

#endif
}

inline void USurfaceNavigationSystem::NeighborDebugLine(const FVector& StartLocation, const FVector& EndLocation, const float Duration) const
{
#if WITH_EDITOR
	if (&EndLocation == nullptr || &StartLocation == nullptr)
		return;

	auto EndOfLine = (EndLocation - StartLocation);
	EndOfLine.Normalize();
	EndOfLine = EndOfLine * 20.f + StartLocation;

	FVector DiffNormalized = (StartLocation - EndLocation).GetSafeNormal().GetAbs() * 255.f; // *127.5f;

	DrawDebugLine(
		GetWorld(),
		StartLocation,
		EndOfLine,
		FColor(FMath::Abs(DiffNormalized.X), FMath::Abs(DiffNormalized.Y), FMath::Abs(DiffNormalized.Z)),
		false,
		Duration,
		0,
		1.f
	);
#endif
}


#pragma region Build Nav Point Neighbors

inline void USurfaceNavigationSystem::GridPointFinished(FSurfaceNavGridPoint* CurGridPoint, int Phase)
{
	const FSurfaceNavGridPoint* CurGridPointConst = CurGridPoint;

	FSurfaceNavBlock*& CurBlock = CurGridPoint->OwningBlock;
	if (!CurBlock)
		return;

	FIntVector BlockIndex = *CurBlock->BlockKey;
	FIntVector GridPointIndex = CurGridPoint->VectorIndex;

	auto VoxelSize = CurBlock->SurfaceNavGridPoints.Num();

	//***************************************
	//Each navpoint can have up to four neighbors and we need to define how we're mapping
	//the index for each axis so we can be consisten
	//
	//For X axis (-1 & +1)
	// -Y = 0
	// +Y = 1
	// -Z = 2
	// +Z = 3
	//
	//For Y axis (-1 & +1)
	// -X = 0
	// +X = 1
	// -Z = 2
	// +Z = 3
	//
	//For Z axis (-1 & +1)
	// -X = 0
	// +X = 1
	// -Y = 2
	// +Y = 3
	//***************************************


	NegXAxisFindNeighbors(BlockIndex, GridPointIndex, VoxelSize, CurGridPointConst, *CurBlock, Phase);
	PosXAxisFindNeighbors(BlockIndex, GridPointIndex, VoxelSize, CurGridPointConst, *CurBlock, Phase);

	NegYAxisFindNeighbors(BlockIndex, GridPointIndex, VoxelSize, CurGridPointConst, *CurBlock, Phase);
	PosYAxisFindNeighbors(BlockIndex, GridPointIndex, VoxelSize, CurGridPointConst, *CurBlock, Phase);

	NegZAxisFindNeighbors(BlockIndex, GridPointIndex, VoxelSize, CurGridPointConst, *CurBlock, Phase);
	PosZAxisFindNeighbors(BlockIndex, GridPointIndex, VoxelSize, CurGridPointConst, *CurBlock, Phase);

	// Flag indicating that at least some navigation has finished generating
	bNavHasGenerated = true;
}

inline void USurfaceNavigationSystem::NegXAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase)
{
	FIntVector NavPointIndex(-1, 0, 0);
	auto CurNavPointPtr = CurGridPoint->NavPoints.Find(NavPointIndex);
	if (!CurNavPointPtr)
		return;

	auto CurNavPoint = *CurNavPointPtr;

	if (CurNavPoint)
	{
		if (Phase == 1) // Adjacent neighbors
		{
			// -X Axis to Find -Y
			NegAxisFindNeg(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, XAxisNegYIndex, NegYDir, YAxisNegXIndex, NegXDir, XAxisPosYIndex, PosYDir, YAxisPosXIndex, XAxisIndex, YAxisIndex);

			// -X Axis to Find +Y
			NegAxisFindPos(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, XAxisPosYIndex, PosYDir, YAxisNegXIndex, NegXDir, XAxisNegYIndex, NegYDir, YAxisPosXIndex, XAxisIndex, YAxisIndex);

			// -X Axis to Find -Z
			NegAxisFindNeg(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, XAxisNegZIndex, NegZDir, ZAxisNegXIndex, NegXDir, XAxisPosZIndex, PosZDir, ZAxisPosXIndex, XAxisIndex, ZAxisIndex);

			// -X Axis to Find +Z
			NegAxisFindPos(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, XAxisPosZIndex, PosZDir, ZAxisNegXIndex, NegXDir, XAxisNegZIndex, NegZDir, ZAxisPosXIndex, XAxisIndex, ZAxisIndex);
		}
		else // Diagonal neighbors
		{
			NegAxisFindDiagNegNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, XAxisNegYNegZIndex, NegXDir, PosXDir, NegYDir, NegZDir, XAxisPosYPosZIndex, XAxisIndex, YAxisIndex, ZAxisIndex);
			NegAxisFindDiagPosNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, XAxisPosYNegZIndex, NegXDir, PosXDir, PosYDir, NegZDir, XAxisNegYPosZIndex, XAxisIndex, YAxisIndex, ZAxisIndex);
			NegAxisFindDiagNegPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, XAxisNegYPosZIndex, NegXDir, PosXDir, NegYDir, PosZDir, XAxisPosYNegZIndex, XAxisIndex, YAxisIndex, ZAxisIndex);
			NegAxisFindDiagPosPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, XAxisPosYPosZIndex, NegXDir, PosXDir, PosYDir, PosZDir, XAxisNegYNegZIndex, XAxisIndex, YAxisIndex, ZAxisIndex);
		}
	}
}

inline void USurfaceNavigationSystem::PosXAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase)
{
	FIntVector NavPointIndex(1, 0, 0);
	auto CurNavPointPtr = CurGridPoint->NavPoints.Find(NavPointIndex);
	if (!CurNavPointPtr)
		return;

	auto CurNavPoint = *CurNavPointPtr;

	if (CurNavPoint)
	{
		if (Phase == 1) // Adjacent neighbors
		{
			// +X Axis to Find -Y
			PosAxisFindNeg(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, XAxisNegYIndex, NegYDir, YAxisPosXIndex, PosXDir, XAxisPosYIndex, PosYDir, YAxisNegXIndex, XAxisIndex, YAxisIndex);

			// +X Axis to Find +Y
			PosAxisFindPos(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, XAxisPosYIndex, PosYDir, YAxisPosXIndex, PosXDir, XAxisNegYIndex, NegYDir, YAxisNegXIndex, XAxisIndex, YAxisIndex);

			// +X Axis to Find -Z
			PosAxisFindNeg(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, XAxisNegZIndex, NegZDir, ZAxisPosXIndex, PosXDir, XAxisPosZIndex, PosZDir, ZAxisNegXIndex, XAxisIndex, ZAxisIndex);

			// +X Axis to Find +Z
			PosAxisFindPos(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, XAxisPosZIndex, PosZDir, ZAxisPosXIndex, PosXDir, XAxisNegZIndex, NegZDir, ZAxisNegXIndex, XAxisIndex, ZAxisIndex);
		}
		else // Diagonal neighbors
		{
			PosAxisFindDiagNegNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, XAxisNegYNegZIndex, PosXDir, NegXDir, NegYDir, NegZDir, XAxisPosYPosZIndex, XAxisIndex, YAxisIndex, ZAxisIndex);
			PosAxisFindDiagPosNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, XAxisPosYNegZIndex, PosXDir, NegXDir, PosYDir, NegZDir, XAxisNegYPosZIndex, XAxisIndex, YAxisIndex, ZAxisIndex);
			PosAxisFindDiagNegPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, XAxisNegYPosZIndex, PosXDir, NegXDir, NegYDir, PosZDir, XAxisPosYNegZIndex, XAxisIndex, YAxisIndex, ZAxisIndex);
			PosAxisFindDiagPosPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, XAxisPosYPosZIndex, PosXDir, NegXDir, PosYDir, PosZDir, XAxisNegYNegZIndex, XAxisIndex, YAxisIndex, ZAxisIndex);
		}
	}
}

inline void USurfaceNavigationSystem::NegYAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase)
{
	FIntVector NavPointIndex(0, -1, 0);
	auto CurNavPointPtr = CurGridPoint->NavPoints.Find(NavPointIndex);
	if (!CurNavPointPtr)
		return;

	auto CurNavPoint = *CurNavPointPtr;

	if (CurNavPoint)
	{
		if (Phase == 1) // Adjacent neighbors
		{
			// -Y Axis to Find -X
			NegAxisFindNeg(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, YAxisNegXIndex, NegXDir, XAxisNegYIndex, NegYDir, YAxisPosXIndex, PosXDir, XAxisPosYIndex, YAxisIndex, XAxisIndex);

			// -Y Axis to Find +X
			NegAxisFindPos(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, YAxisPosXIndex, PosXDir, XAxisNegYIndex, NegYDir, YAxisNegXIndex, NegXDir, XAxisPosYIndex, YAxisIndex, XAxisIndex);

			// -Y Axis to Find -Z
			NegAxisFindNeg(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, YAxisNegZIndex, NegZDir, ZAxisNegYIndex, NegYDir, YAxisPosZIndex, PosZDir, ZAxisPosYIndex, YAxisIndex, ZAxisIndex);

			// -Y Axis to Find +Z
			NegAxisFindPos(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, YAxisPosZIndex, PosZDir, ZAxisNegYIndex, NegYDir, YAxisNegZIndex, NegZDir, ZAxisPosYIndex, YAxisIndex, ZAxisIndex);
		}
		else // Diagonal neighbors
		{
			NegAxisFindDiagNegNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, YAxisNegXNegZIndex, NegYDir, PosYDir, NegXDir, NegZDir, YAxisPosXPosZIndex, YAxisIndex, XAxisIndex, ZAxisIndex);
			NegAxisFindDiagPosNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, YAxisPosXNegZIndex, NegYDir, PosYDir, PosXDir, NegZDir, YAxisNegXPosZIndex, YAxisIndex, XAxisIndex, ZAxisIndex);
			NegAxisFindDiagNegPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, YAxisNegXPosZIndex, NegYDir, PosYDir, NegXDir, PosZDir, YAxisPosXNegZIndex, YAxisIndex, XAxisIndex, ZAxisIndex);
			NegAxisFindDiagPosPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, YAxisPosXPosZIndex, NegYDir, PosYDir, PosXDir, PosZDir, YAxisNegXNegZIndex, YAxisIndex, XAxisIndex, ZAxisIndex);
		}
	}
}

inline void USurfaceNavigationSystem::PosYAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase)
{
	FIntVector NavPointIndex(0, 1, 0);
	auto CurNavPointPtr = CurGridPoint->NavPoints.Find(NavPointIndex);
	if (!CurNavPointPtr)
		return;

	auto CurNavPoint = *CurNavPointPtr;

	if (CurNavPoint)
	{
		if (Phase == 1) // Adjacent neighbors
		{
			// +Y Axis to Find -X
			PosAxisFindNeg(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, YAxisNegXIndex, NegXDir, XAxisPosYIndex, PosYDir, YAxisPosXIndex, PosXDir, XAxisNegYIndex, YAxisIndex, XAxisIndex);

			// +Y Axis to Find +X
			PosAxisFindPos(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, YAxisPosXIndex, PosXDir, XAxisPosYIndex, PosYDir, YAxisNegXIndex, NegXDir, XAxisNegYIndex, YAxisIndex, XAxisIndex);

			// +Y Axis to Find -Z
			PosAxisFindNeg(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, YAxisNegZIndex, NegZDir, ZAxisPosYIndex, PosYDir, YAxisPosZIndex, PosZDir, ZAxisNegYIndex, YAxisIndex, ZAxisIndex);

			// +Y Axis to Find +Z
			PosAxisFindPos(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, YAxisPosZIndex, PosZDir, ZAxisPosYIndex, PosYDir, YAxisNegZIndex, NegZDir, ZAxisNegYIndex, YAxisIndex, ZAxisIndex);
		}
		else // Diagonal neighbors
		{
			PosAxisFindDiagNegNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, YAxisNegXNegZIndex, PosYDir, NegYDir, NegXDir, NegZDir, YAxisPosXPosZIndex, YAxisIndex, XAxisIndex, ZAxisIndex);
			PosAxisFindDiagPosNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, YAxisPosXNegZIndex, PosYDir, NegYDir, PosXDir, NegZDir, YAxisNegXPosZIndex, YAxisIndex, XAxisIndex, ZAxisIndex);
			PosAxisFindDiagNegPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, YAxisNegXPosZIndex, PosYDir, NegYDir, NegXDir, PosZDir, YAxisPosXNegZIndex, YAxisIndex, XAxisIndex, ZAxisIndex);
			PosAxisFindDiagPosPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, YAxisPosXPosZIndex, PosYDir, NegYDir, PosXDir, PosZDir, YAxisNegXNegZIndex, YAxisIndex, XAxisIndex, ZAxisIndex);
		}
	}
}

inline void USurfaceNavigationSystem::NegZAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase)
{
	FIntVector NavPointIndex(0, 0, -1);
	auto CurNavPointPtr = CurGridPoint->NavPoints.Find(NavPointIndex);
	if (!CurNavPointPtr)
		return;

	auto CurNavPoint = *CurNavPointPtr;

	if (CurNavPoint)
	{
		if (Phase == 1) // Adjacent neighbors
		{
			// -Z Axis to Find -X
			NegAxisFindNeg(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, ZAxisNegXIndex, NegXDir, XAxisNegZIndex, NegZDir, ZAxisPosXIndex, PosXDir, XAxisPosZIndex, ZAxisIndex, XAxisIndex);

			// -Z Axis to Find +X
			NegAxisFindPos(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, ZAxisPosXIndex, PosXDir, XAxisNegZIndex, NegZDir, ZAxisNegXIndex, NegXDir, XAxisPosZIndex, ZAxisIndex, XAxisIndex);

			// -Z Axis to Find -Y
			NegAxisFindNeg(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, ZAxisNegYIndex, NegYDir, YAxisNegZIndex, NegZDir, ZAxisPosYIndex, PosYDir, YAxisPosZIndex, ZAxisIndex, YAxisIndex);

			// -Z Axis to Find +Y
			NegAxisFindPos(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurGridPoint, CurBlock, ZAxisPosYIndex, PosYDir, YAxisNegZIndex, NegZDir, ZAxisNegYIndex, NegYDir, YAxisPosZIndex, ZAxisIndex, YAxisIndex);
		}
		else // Diagonal neighbors
		{
			NegAxisFindDiagNegNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, ZAxisNegXNegYIndex, NegZDir, PosZDir, NegXDir, NegYDir, ZAxisPosXPosYIndex, ZAxisIndex, XAxisIndex, YAxisIndex);
			NegAxisFindDiagPosNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, ZAxisPosXNegYIndex, NegZDir, PosZDir, PosXDir, NegYDir, ZAxisNegXPosYIndex, ZAxisIndex, XAxisIndex, YAxisIndex);
			NegAxisFindDiagNegPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, ZAxisNegXPosYIndex, NegZDir, PosZDir, NegXDir, PosYDir, ZAxisPosXNegYIndex, ZAxisIndex, XAxisIndex, YAxisIndex);
			NegAxisFindDiagPosPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, ZAxisPosXPosYIndex, NegZDir, PosZDir, PosXDir, PosYDir, ZAxisNegXNegYIndex, ZAxisIndex, XAxisIndex, YAxisIndex);
		}
	}
}

inline void USurfaceNavigationSystem::PosZAxisFindNeighbors(const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize, const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, int Phase)
{
	FIntVector NavPointIndex(0, 0, 1);
	auto CurNavPointPtr = CurGridPoint->NavPoints.Find(NavPointIndex);
	if (!CurNavPointPtr)
		return;

	auto CurNavPoint = *CurNavPointPtr;

	if (CurNavPoint)
	{
		if (Phase == 1) // Adjacent neighbors
		{
			// +Z Axis to Find -X
			PosAxisFindNeg(
				CurNavPoint
				, BlockIndex
				, GridPointIndex
				, VoxelSize
				, CurGridPoint
				, CurBlock
				, ZAxisNegXIndex // 0
				, NegXDir // -1, 0, 0
				, XAxisPosZIndex // 3
				, PosZDir // 0, 0, 1
				, ZAxisPosXIndex // 1
				, PosXDir // 1, 0, 0
				, XAxisNegZIndex // 2
				, ZAxisIndex // 2
				, XAxisIndex // 0
			);

			// +Z Axis to Find +X
			PosAxisFindPos(
				CurNavPoint
				, BlockIndex
				, GridPointIndex
				, VoxelSize
				, CurGridPoint
				, CurBlock
				, ZAxisPosXIndex // 1
				, PosXDir // 1, 0, 0
				, XAxisPosZIndex // 3
				, PosZDir // 0, 0, 1
				, ZAxisNegXIndex // 0
				, NegXDir // -1, 0, 0
				, XAxisNegZIndex // 2
				, ZAxisIndex // 2
				, XAxisIndex // 0
			);

			// +Z Axis to Find -Y
			PosAxisFindNeg(
				CurNavPoint
				, BlockIndex
				, GridPointIndex
				, VoxelSize
				, CurGridPoint
				, CurBlock
				, ZAxisNegYIndex // 2 
				, NegYDir // 0, -1, 0
				, YAxisPosZIndex // 3
				, PosZDir // 0, 0, 1
				, ZAxisPosYIndex // 3
				, PosYDir // 0, 1, 0
				, YAxisNegZIndex // 2
				, ZAxisIndex // 2
				, YAxisIndex // 1
			);

			// +Z Axis to Find +Y
			PosAxisFindPos(
				CurNavPoint
				, BlockIndex
				, GridPointIndex
				, VoxelSize
				, CurGridPoint
				, CurBlock
				, ZAxisPosYIndex // 3
				, PosYDir // 0, 1, 0
				, YAxisPosZIndex // 3
				, PosZDir // 0, 0, 1
				, ZAxisNegYIndex // 2
				, NegYDir // 0, -1, 0
				, YAxisNegZIndex // 2
				, ZAxisIndex // 2
				, YAxisIndex // 1
			);
		}
		else // Diagonal neighbors
		{
			PosAxisFindDiagNegNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, ZAxisNegXNegYIndex, PosZDir, NegZDir, NegXDir, NegYDir, ZAxisPosXPosYIndex, ZAxisIndex, XAxisIndex, YAxisIndex);
			PosAxisFindDiagPosNegAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, ZAxisPosXNegYIndex, PosZDir, NegZDir, PosXDir, NegYDir, ZAxisNegXPosYIndex, ZAxisIndex, XAxisIndex, YAxisIndex);
			PosAxisFindDiagNegPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, ZAxisNegXPosYIndex, PosZDir, NegZDir, NegXDir, PosYDir, ZAxisPosXNegYIndex, ZAxisIndex, XAxisIndex, YAxisIndex);
			PosAxisFindDiagPosPosAlt(CurNavPoint, BlockIndex, GridPointIndex, VoxelSize, CurBlock, ZAxisPosXPosYIndex, PosZDir, NegZDir, PosXDir, PosYDir, ZAxisNegXNegYIndex, ZAxisIndex, XAxisIndex, YAxisIndex);
		}
	}
}

inline void USurfaceNavigationSystem::PosAxisFindNeg(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir1, const int& TestAxisIndex1,
	const FIntVector& TestDir2, const int& TestAxisIndex2, const FIntVector& TestDir3, const int& TestAxisIndex3, const int& PrimaryGridPointAxisIndex, const int& SecondaryGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;
	int PriorityIndex = 0;

	if (!DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, CurGridPoint, TestDir1, TestAxisIndex1))
	{
		if (NewNeighbor)
			check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);

		TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

		auto offsetIndex = BlockIndex;
		auto OtherBlock = &CurBlock;

		DecrementGridPointIndex(MyGridPointIndex[SecondaryGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, TestDir1);
		PriorityIndex++;
		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
			// Test GridPointSibling in the same direction as the axis we're testing
			if (!DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir2, TestAxisIndex2))
			{
				if (NewNeighbor)
					check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);

				// Increment or decrement in the direction of the axis we're testing
				IncrementGridPointIndex(MyGridPointIndex[PrimaryGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, TestDir2);
				PriorityIndex++;
				if (OtherBlock)
				{
					GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
					// Test in the opposite direction we're trying to find
					DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir3, TestAxisIndex3);
					if (NewNeighbor)
						check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);
				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

inline void USurfaceNavigationSystem::PosAxisFindPos(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir1, const int& TestAxisIndex1,
	const FIntVector& TestDir2, const int& TestAxisIndex2, const FIntVector& TestDir3, const int& TestAxisIndex3, const int& PrimaryGridPointAxisIndex, const int& SecondaryGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;
	int PriorityIndex = 0;

	if (!DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, CurGridPoint, TestDir1, TestAxisIndex1))
	{
		if (NewNeighbor)
			check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);

		TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

		auto offsetIndex = BlockIndex;
		FSurfaceNavBlock* OtherBlock = &CurBlock;

		IncrementGridPointIndex(MyGridPointIndex[SecondaryGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, TestDir1);
		PriorityIndex++;
		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
			// Test GridPointSibling in the same direction as the axis we're testing
			if (!DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir2, TestAxisIndex2))
			{
				if (NewNeighbor)
					check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);

				// Increment or decrement in the direction of the axis we're testing
				IncrementGridPointIndex(MyGridPointIndex[PrimaryGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, TestDir2);
				PriorityIndex++;
				if (OtherBlock)
				{
					GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
					// Test in the opposite direction we're trying to find
					DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir3, TestAxisIndex3);
					if (NewNeighbor)
						check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);

				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

inline void USurfaceNavigationSystem::NegAxisFindNeg(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir1, const int& TestAxisIndex1,
	const FIntVector& TestDir2, const int& TestAxisIndex2, const FIntVector& TestDir3, const int& TestAxisIndex3, const int& PrimaryGridPointAxisIndex, const int& SecondaryGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;
	int PriorityIndex = 0;

	if (!DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, CurGridPoint, TestDir1, TestAxisIndex1))
	{
		if (NewNeighbor)
			check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);

		TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

		auto offsetIndex = BlockIndex;
		FSurfaceNavBlock* OtherBlock = &CurBlock;

		DecrementGridPointIndex(MyGridPointIndex[SecondaryGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, TestDir1);
		PriorityIndex++;
		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
			// Test GridPointSibling in the same direction as the axis we're testing
			if (!DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir2, TestAxisIndex2))
			{
				if (NewNeighbor)
					check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);

				// Increment or decrement in the direction of the axis we're testing
				DecrementGridPointIndex(MyGridPointIndex[PrimaryGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, TestDir2);
				PriorityIndex++;
				if (OtherBlock)
				{
					GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
					// Test in the opposite direction we're trying to find
					DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir3, TestAxisIndex3);
					if (NewNeighbor)
						check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);

				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

inline void USurfaceNavigationSystem::NegAxisFindPos(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	const FSurfaceNavGridPoint*& CurGridPoint, FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir1, const int& TestAxisIndex1,
	const FIntVector& TestDir2, const int& TestAxisIndex2, const FIntVector& TestDir3, const int& TestAxisIndex3, const int& PrimaryGridPointAxisIndex, const int& SecondaryGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;
	int PriorityIndex = 0;

	if (!DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, CurGridPoint, TestDir1, TestAxisIndex1))
	{
		if (NewNeighbor)
			check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);

		TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

		auto offsetIndex = BlockIndex;
		FSurfaceNavBlock* OtherBlock = &CurBlock;

		IncrementGridPointIndex(MyGridPointIndex[SecondaryGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, TestDir1);
		PriorityIndex++;
		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
			// Test GridPointSibling in the same direction as the axis we're testing
			if (!DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir2, TestAxisIndex2))
			{
				if (NewNeighbor)
					check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);

				// Increment or decrement in the direction of the axis we're testing
				DecrementGridPointIndex(MyGridPointIndex[PrimaryGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, TestDir2);
				PriorityIndex++;
				if (OtherBlock)
				{
					GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
					// Test in the opposite direction we're trying to find
					DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir3, TestAxisIndex3);
					if (NewNeighbor)
						check(NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);
				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}


void USurfaceNavigationSystem::NegAxisFindDiagNegNegAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
	const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;

	auto offsetIndex = BlockIndex;
	FSurfaceNavBlock* OtherBlock = &CurBlock;
	TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

	// Only 1 priority index for diagonal neighbors at this time so will always be zero
	int PriorityIndex = 0;

	/* 4 = 0, 2 - neg,neg
	*  5 = 1, 2 - pos,neg
	*  6 = 0, 3 - neg,pos
	*  7 = 1, 3 - pos,pos
	*/

	// We're checking a diagonal so there are hits in either of axis directions that
	// make up this diagonal direction then we won't use it.
	FSurfaceNavGridPoint* OwningGridPoint = CurNavPoint->GetOwningGridPoint();
	if (!OwningGridPoint->HasHitInAxisDir(Axis2Dir) && !OwningGridPoint->HasHitInAxisDir(Axis3Dir))
	{
		// Decrement second axis
		DecrementGridPointIndex(MyGridPointIndex[SecondGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis2Dir);

		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = nullptr;

			// Get the sibling gridpoint at the updated index
			GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];

			// Make sure the sibling gridpoint is valid and not an orphan
			if (GridPointSibling && !GridPointSibling->IsOrphaned()) // && !GridPointSibling->IsNarrow())
			{
				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1 = nullptr;
				CurNavPoint->GetNeighborAt(0, ExpectedNeighor1);

				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1Neighbor = nullptr;
				if (ExpectedNeighor1)
					ExpectedNeighor1->GetNeighborAt(1, ExpectedNeighor1Neighbor);

				if (ExpectedNeighor1Neighbor == CurNavPoint)
				{
					//if (OtherBlock && OtherBlock->orph)
					// Decrement third axis
					DecrementGridPointIndex(MyGridPointIndex[ThirdGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis3Dir);

					if (OtherBlock)
					{
						// Get the sibling gridpoint at the updated index
						GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
						if (GridPointSibling && !GridPointSibling->IsOrphaned() && // !GridPointSibling->IsNarrow() &&
							!GridPointSibling->HasHitInAxisDir(Axis2Dir * -1) && !GridPointSibling->HasHitInAxisDir(Axis3Dir * -1))
						{
							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2 = nullptr;
							ExpectedNeighor1->GetNeighborAt(2, ExpectedNeighor2);

							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2Neighbor = nullptr;
							if (ExpectedNeighor2)
								ExpectedNeighor2->GetNeighborAt(3, ExpectedNeighor2Neighbor);

							if (ExpectedNeighor2Neighbor == ExpectedNeighor1)
							{
								if (DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir, TestAxisIndex1))
								{
									UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
									return;
								}
							}
						}
					}
				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

void USurfaceNavigationSystem::NegAxisFindDiagPosNegAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
	const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;

	auto offsetIndex = BlockIndex;
	FSurfaceNavBlock* OtherBlock = &CurBlock;
	TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

	// Only 1 priority index for diagonal neighbors at this time so will always be zero
	int PriorityIndex = 0;

	/* 4 = 0, 2
	*  5 = 1, 2
	*  6 = 0, 3
	*  7 = 1, 3
	*/

	// We're checking a diagonal so there are hits in either of axis directions that
	// make up this diagonal direction then we won't use it.
	FSurfaceNavGridPoint* OwningGridPoint = CurNavPoint->GetOwningGridPoint();
	if (!OwningGridPoint->HasHitInAxisDir(Axis2Dir) && !OwningGridPoint->HasHitInAxisDir(Axis3Dir))
	{
		// Decrement second axis
		IncrementGridPointIndex(MyGridPointIndex[SecondGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis2Dir);

		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = nullptr;

			// Get the sibling gridpoint at the updated index
			GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];

			// Make sure the sibling gridpoint is valid and not an orphan
			if (GridPointSibling && !GridPointSibling->IsOrphaned()) // && !GridPointSibling->IsNarrow())
			{
				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1 = nullptr;
				CurNavPoint->GetNeighborAt(1, ExpectedNeighor1);

				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1Neighbor = nullptr;
				if (ExpectedNeighor1)
					ExpectedNeighor1->GetNeighborAt(0, ExpectedNeighor1Neighbor);

				if (ExpectedNeighor1Neighbor == CurNavPoint)
				{

					//if (OtherBlock && OtherBlock->orph)
					// Decrement third axis
					DecrementGridPointIndex(MyGridPointIndex[ThirdGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis3Dir);

					if (OtherBlock)
					{
						// Get the sibling gridpoint at the updated index
						GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
						if (GridPointSibling && !GridPointSibling->IsOrphaned() && // !GridPointSibling->IsNarrow() &&
							!GridPointSibling->HasHitInAxisDir(Axis2Dir * -1) && !GridPointSibling->HasHitInAxisDir(Axis3Dir * -1))
						{
							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2 = nullptr;
							ExpectedNeighor1->GetNeighborAt(2, ExpectedNeighor2);

							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2Neighbor = nullptr;
							if (ExpectedNeighor2)
								ExpectedNeighor2->GetNeighborAt(3, ExpectedNeighor2Neighbor);

							if (ExpectedNeighor2Neighbor == ExpectedNeighor1)
							{
								if (DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir, TestAxisIndex1))
								{
									UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
									return;
								}
							}
						}
					}
				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

void USurfaceNavigationSystem::NegAxisFindDiagNegPosAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
	const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;

	auto offsetIndex = BlockIndex;
	FSurfaceNavBlock* OtherBlock = &CurBlock;
	TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

	// Only 1 priority index for diagonal neighbors at this time so will always be zero
	int PriorityIndex = 0;

	/* 4 = 0, 2
	*  5 = 1, 2
	*  6 = 0, 3
	*  7 = 1, 3
	*/

	// We're checking a diagonal so there are hits in either of axis directions that
	// make up this diagonal direction then we won't use it.
	FSurfaceNavGridPoint* OwningGridPoint = CurNavPoint->GetOwningGridPoint();
	if (!OwningGridPoint->HasHitInAxisDir(Axis2Dir) && !OwningGridPoint->HasHitInAxisDir(Axis3Dir))
	{
		// Decrement second axis
		DecrementGridPointIndex(MyGridPointIndex[SecondGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis2Dir);

		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = nullptr;

			// Get the sibling gridpoint at the updated index
			GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];

			// Make sure the sibling gridpoint is valid and not an orphan
			if (GridPointSibling && !GridPointSibling->IsOrphaned()) // && !GridPointSibling->IsNarrow())
			{
				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1 = nullptr;
				CurNavPoint->GetNeighborAt(0, ExpectedNeighor1);

				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1Neighbor = nullptr;
				if (ExpectedNeighor1)
					ExpectedNeighor1->GetNeighborAt(1, ExpectedNeighor1Neighbor);

				if (ExpectedNeighor1Neighbor == CurNavPoint)
				{

					//if (OtherBlock && OtherBlock->orph)
					// Decrement third axis
					IncrementGridPointIndex(MyGridPointIndex[ThirdGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis3Dir);

					if (OtherBlock)
					{
						// Get the sibling gridpoint at the updated index
						GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
						if (GridPointSibling && !GridPointSibling->IsOrphaned() && // !GridPointSibling->IsNarrow() &&
							!GridPointSibling->HasHitInAxisDir(Axis2Dir * -1) && !GridPointSibling->HasHitInAxisDir(Axis3Dir * -1))
						{
							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2 = nullptr;
							ExpectedNeighor1->GetNeighborAt(3, ExpectedNeighor2);

							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2Neighbor = nullptr;
							if (ExpectedNeighor2)
								ExpectedNeighor2->GetNeighborAt(2, ExpectedNeighor2Neighbor);

							if (ExpectedNeighor2Neighbor == ExpectedNeighor1)
							{
								if (DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir, TestAxisIndex1))
								{
									UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
									return;
								}
							}
						}
					}
				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

void USurfaceNavigationSystem::NegAxisFindDiagPosPosAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
	const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;

	auto offsetIndex = BlockIndex;
	FSurfaceNavBlock* OtherBlock = &CurBlock;
	TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

	// Only 1 priority index for diagonal neighbors at this time so will always be zero
	int PriorityIndex = 0;

	/* 4 = 0, 2
	*  5 = 1, 2
	*  6 = 0, 3
	*  7 = 1, 3
	*/

	// We're checking a diagonal so there are hits in either of axis directions that
	// make up this diagonal direction then we won't use it.
	FSurfaceNavGridPoint* OwningGridPoint = CurNavPoint->GetOwningGridPoint();
	if (!OwningGridPoint->HasHitInAxisDir(Axis2Dir) && !OwningGridPoint->HasHitInAxisDir(Axis3Dir))
	{
		// Decrement second axis
		IncrementGridPointIndex(MyGridPointIndex[SecondGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis2Dir);

		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = nullptr;

			// Get the sibling gridpoint at the updated index
			GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];

			// Make sure the sibling gridpoint is valid and not an orphan
			if (GridPointSibling && !GridPointSibling->IsOrphaned()) // && !GridPointSibling->IsNarrow())
			{
				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1 = nullptr;
				CurNavPoint->GetNeighborAt(1, ExpectedNeighor1);

				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1Neighbor = nullptr;
				if (ExpectedNeighor1)
					ExpectedNeighor1->GetNeighborAt(0, ExpectedNeighor1Neighbor);

				if (ExpectedNeighor1Neighbor == CurNavPoint)
				{

					//if (OtherBlock && OtherBlock->orph)
					// Decrement third axis
					IncrementGridPointIndex(MyGridPointIndex[ThirdGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis3Dir);

					if (OtherBlock)
					{
						// Get the sibling gridpoint at the updated index
						GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
						if (GridPointSibling && !GridPointSibling->IsOrphaned() && // !GridPointSibling->IsNarrow() &&
							!GridPointSibling->HasHitInAxisDir(Axis2Dir * -1) && !GridPointSibling->HasHitInAxisDir(Axis3Dir * -1))
						{
							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2 = nullptr;
							ExpectedNeighor1->GetNeighborAt(3, ExpectedNeighor2);

							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2Neighbor = nullptr;
							if (ExpectedNeighor2)
								ExpectedNeighor2->GetNeighborAt(2, ExpectedNeighor2Neighbor);

							if (ExpectedNeighor2Neighbor == ExpectedNeighor1)
							{
								if (DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir, TestAxisIndex1))
								{
									UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
									return;
								}
							}
						}
					}
				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

void USurfaceNavigationSystem::PosAxisFindDiagNegNegAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
	const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;

	auto offsetIndex = BlockIndex;
	FSurfaceNavBlock* OtherBlock = &CurBlock;
	TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

	// Only 1 priority index for diagonal neighbors at this time so will always be zero
	int PriorityIndex = 0;

	/* 4 = 0, 2 - neg,neg
	*  5 = 1, 2 - pos,neg
	*  6 = 0, 3 - neg,pos
	*  7 = 1, 3 - pos,pos
	*/

	// We're checking a diagonal so there are hits in either of axis directions that
	// make up this diagonal direction then we won't use it.
	FSurfaceNavGridPoint* OwningGridPoint = CurNavPoint->GetOwningGridPoint();
	if (!OwningGridPoint->HasHitInAxisDir(Axis2Dir) && !OwningGridPoint->HasHitInAxisDir(Axis3Dir))
	{
		// Decrement second axis
		DecrementGridPointIndex(MyGridPointIndex[SecondGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis2Dir);

		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = nullptr;

			// Get the sibling gridpoint at the updated index
			GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];

			// Make sure the sibling gridpoint is valid and not an orphan
			if (GridPointSibling && !GridPointSibling->IsOrphaned()) // && !GridPointSibling->IsNarrow())
			{
				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1 = nullptr;
				CurNavPoint->GetNeighborAt(0, ExpectedNeighor1);

				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1Neighbor = nullptr;
				if (ExpectedNeighor1)
					ExpectedNeighor1->GetNeighborAt(1, ExpectedNeighor1Neighbor);

				if (ExpectedNeighor1Neighbor == CurNavPoint)
				{
					//if (OtherBlock && OtherBlock->orph)
					// Decrement third axis
					DecrementGridPointIndex(MyGridPointIndex[ThirdGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis3Dir);

					if (OtherBlock)
					{
						// Get the sibling gridpoint at the updated index
						GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
						if (GridPointSibling && !GridPointSibling->IsOrphaned() && // !GridPointSibling->IsNarrow() &&
							!GridPointSibling->HasHitInAxisDir(Axis2Dir * -1) && !GridPointSibling->HasHitInAxisDir(Axis3Dir * -1))
						{
							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2 = nullptr;
							ExpectedNeighor1->GetNeighborAt(2, ExpectedNeighor2);

							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2Neighbor = nullptr;
							if (ExpectedNeighor2)
								ExpectedNeighor2->GetNeighborAt(3, ExpectedNeighor2Neighbor);

							if (ExpectedNeighor2Neighbor == ExpectedNeighor1)
							{
								if (DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir, TestAxisIndex1))
								{
									UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
									return;
								}
							}
						}
					}
				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

void USurfaceNavigationSystem::PosAxisFindDiagPosNegAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
	const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;

	auto offsetIndex = BlockIndex;
	FSurfaceNavBlock* OtherBlock = &CurBlock;
	TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

	// Only 1 priority index for diagonal neighbors at this time so will always be zero
	int PriorityIndex = 0;

	/* 4 = 0, 2
	*  5 = 1, 2
	*  6 = 0, 3
	*  7 = 1, 3
	*/

	// We're checking a diagonal so there are hits in either of axis directions that
	// make up this diagonal direction then we won't use it.
	FSurfaceNavGridPoint* OwningGridPoint = CurNavPoint->GetOwningGridPoint();
	if (!OwningGridPoint->HasHitInAxisDir(Axis2Dir) && !OwningGridPoint->HasHitInAxisDir(Axis3Dir))
	{
		// Decrement second axis
		IncrementGridPointIndex(MyGridPointIndex[SecondGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis2Dir);

		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = nullptr;

			// Get the sibling gridpoint at the updated index
			GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];

			// Make sure the sibling gridpoint is valid and not an orphan
			if (GridPointSibling && !GridPointSibling->IsOrphaned()) // && !GridPointSibling->IsNarrow())
			{
				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1 = nullptr;
				CurNavPoint->GetNeighborAt(1, ExpectedNeighor1);

				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1Neighbor = nullptr;
				if (ExpectedNeighor1)
					ExpectedNeighor1->GetNeighborAt(0, ExpectedNeighor1Neighbor);

				if (ExpectedNeighor1Neighbor == CurNavPoint)
				{

					//if (OtherBlock && OtherBlock->orph)
					// Decrement third axis
					DecrementGridPointIndex(MyGridPointIndex[ThirdGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis3Dir);

					if (OtherBlock)
					{
						// Get the sibling gridpoint at the updated index
						GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
						if (GridPointSibling && !GridPointSibling->IsOrphaned() && // !GridPointSibling->IsNarrow() &&
							!GridPointSibling->HasHitInAxisDir(Axis2Dir * -1) && !GridPointSibling->HasHitInAxisDir(Axis3Dir * -1))
						{
							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2 = nullptr;
							ExpectedNeighor1->GetNeighborAt(2, ExpectedNeighor2);

							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2Neighbor = nullptr;
							if (ExpectedNeighor2)
								ExpectedNeighor2->GetNeighborAt(3, ExpectedNeighor2Neighbor);

							if (ExpectedNeighor2Neighbor == ExpectedNeighor1)
							{
								if (DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir, TestAxisIndex1))
								{
									UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
									return;
								}
							}
						}
					}
				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

void USurfaceNavigationSystem::PosAxisFindDiagNegPosAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
	const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;

	auto offsetIndex = BlockIndex;
	FSurfaceNavBlock* OtherBlock = &CurBlock;
	TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

	// Only 1 priority index for diagonal neighbors at this time so will always be zero
	int PriorityIndex = 0;

	/* 4 = 0, 2
	*  5 = 1, 2
	*  6 = 0, 3
	*  7 = 1, 3
	*/

	// We're checking a diagonal so there are hits in either of axis directions that
	// make up this diagonal direction then we won't use it.
	FSurfaceNavGridPoint* OwningGridPoint = CurNavPoint->GetOwningGridPoint();
	if (!OwningGridPoint->HasHitInAxisDir(Axis2Dir) && !OwningGridPoint->HasHitInAxisDir(Axis3Dir))
	{
		// Decrement second axis
		DecrementGridPointIndex(MyGridPointIndex[SecondGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis2Dir);

		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = nullptr;

			// Get the sibling gridpoint at the updated index
			GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];

			// Make sure the sibling gridpoint is valid and not an orphan
			if (GridPointSibling && !GridPointSibling->IsOrphaned()) // && !GridPointSibling->IsNarrow())
			{
				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1 = nullptr;
				CurNavPoint->GetNeighborAt(0, ExpectedNeighor1);

				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1Neighbor = nullptr;
				if (ExpectedNeighor1)
					ExpectedNeighor1->GetNeighborAt(1, ExpectedNeighor1Neighbor);

				if (ExpectedNeighor1Neighbor == CurNavPoint)
				{

					//if (OtherBlock && OtherBlock->orph)
					// Decrement third axis
					IncrementGridPointIndex(MyGridPointIndex[ThirdGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis3Dir);

					if (OtherBlock)
					{
						// Get the sibling gridpoint at the updated index
						GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
						if (GridPointSibling && !GridPointSibling->IsOrphaned() && // !GridPointSibling->IsNarrow() &&
							!GridPointSibling->HasHitInAxisDir(Axis2Dir * -1) && !GridPointSibling->HasHitInAxisDir(Axis3Dir * -1))
						{
							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2 = nullptr;
							ExpectedNeighor1->GetNeighborAt(3, ExpectedNeighor2);

							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2Neighbor = nullptr;
							if (ExpectedNeighor2)
								ExpectedNeighor2->GetNeighborAt(2, ExpectedNeighor2Neighbor);

							if (ExpectedNeighor2Neighbor == ExpectedNeighor1)
							{
								if (DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir, TestAxisIndex1))
								{
									UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
									return;
								}
							}
						}
					}
				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

void USurfaceNavigationSystem::PosAxisFindDiagPosPosAlt(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const FIntVector& BlockIndex, const FIntVector& GridPointIndex, const int& VoxelSize,
	FSurfaceNavBlock& CurBlock, const int& WorkingNeighborIndex, const FIntVector& TestDir, const FIntVector& Axis1Dir, const FIntVector& Axis2Dir, const FIntVector& Axis3Dir,
	const int& TestAxisIndex1, const int& PrimaryGridPointAxisIndex, const int& SecondGridPointAxisIndex, const int& ThirdGridPointAxisIndex)
{
	TSharedPtr<FSurfaceNavPointBase> NewNeighbor = nullptr;

	int MyIndexForNewNeighbor = -1;

	auto offsetIndex = BlockIndex;
	FSurfaceNavBlock* OtherBlock = &CurBlock;
	TArray<int> MyGridPointIndex = { GridPointIndex.X, GridPointIndex.Y, GridPointIndex.Z };

	// Only 1 priority index for diagonal neighbors at this time so will always be zero
	int PriorityIndex = 0;

	/* 4 = 0, 2
	*  5 = 1, 2
	*  6 = 0, 3
	*  7 = 1, 3
	*/

	// We're checking a diagonal so there are hits in either of axis directions that
	// make up this diagonal direction then we won't use it.
	FSurfaceNavGridPoint* OwningGridPoint = CurNavPoint->GetOwningGridPoint();
	if (!OwningGridPoint->HasHitInAxisDir(Axis2Dir) && !OwningGridPoint->HasHitInAxisDir(Axis3Dir))
	{
		// Decrement second axis
		IncrementGridPointIndex(MyGridPointIndex[SecondGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis2Dir);

		if (OtherBlock)
		{
			const FSurfaceNavGridPoint* GridPointSibling = nullptr;

			// Get the sibling gridpoint at the updated index
			GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];

			// Make sure the sibling gridpoint is valid and not an orphan
			if (GridPointSibling && !GridPointSibling->IsOrphaned()) // && !GridPointSibling->IsNarrow())
			{
				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1 = nullptr;
				CurNavPoint->GetNeighborAt(1, ExpectedNeighor1);

				TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor1Neighbor = nullptr;
				if (ExpectedNeighor1)
					ExpectedNeighor1->GetNeighborAt(0, ExpectedNeighor1Neighbor);

				if (ExpectedNeighor1Neighbor == CurNavPoint)
				{

					//if (OtherBlock && OtherBlock->orph)
					// Decrement third axis
					IncrementGridPointIndex(MyGridPointIndex[ThirdGridPointAxisIndex], offsetIndex, OtherBlock, VoxelSize, Axis3Dir);

					if (OtherBlock)
					{
						// Get the sibling gridpoint at the updated index
						GridPointSibling = OtherBlock->SurfaceNavGridPoints[MyGridPointIndex[0]][MyGridPointIndex[1]][MyGridPointIndex[2]];
						if (GridPointSibling && !GridPointSibling->IsOrphaned() && // !GridPointSibling->IsNarrow() &&
							!GridPointSibling->HasHitInAxisDir(Axis2Dir * -1) && !GridPointSibling->HasHitInAxisDir(Axis3Dir * -1))
						{
							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2 = nullptr;
							ExpectedNeighor1->GetNeighborAt(3, ExpectedNeighor2);

							TSharedPtr<FSurfaceNavPointBase> ExpectedNeighor2Neighbor = nullptr;
							if (ExpectedNeighor2)
								ExpectedNeighor2->GetNeighborAt(2, ExpectedNeighor2Neighbor);

							if (ExpectedNeighor2Neighbor == ExpectedNeighor1)
							{
								if (DetermineNewNeighbor(NewNeighbor, MyIndexForNewNeighbor, GridPointSibling, TestDir, TestAxisIndex1))
								{
									UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
									return;
								}
							}
						}
					}
				}
			}
		}
	}

	// TODO: Is there a way we can update the old neighbor...or just wait for it to update itself?
	UpdateNavNeighbors(CurNavPoint, WorkingNeighborIndex, NewNeighbor, MyIndexForNewNeighbor, PriorityIndex);
}

inline void USurfaceNavigationSystem::DecrementGridPointIndex(int& GridPointIndex, FIntVector& OffsetIndex, FSurfaceNavBlock*& OtherBlock, const int& VoxelSize, const FIntVector& SubtractFromIndex)
{
	if (GridPointIndex > 0)
	{
		GridPointIndex--;
	}
	else
	{
		GridPointIndex = VoxelSize - 1;
		OffsetIndex = OffsetIndex + SubtractFromIndex;

		FSurfaceNavBlock** OtherBlockPtr = nullptr;
		{
			FReadScopeLock rNavBlocksLock(NavBlocksLock);
			OtherBlockPtr = NavBlocks.Find(OffsetIndex);
		}

		OtherBlock = !OtherBlockPtr ? nullptr : *OtherBlockPtr;
	}
}

inline void USurfaceNavigationSystem::IncrementGridPointIndex(int& GridPointIndex, FIntVector& OffsetIndex, FSurfaceNavBlock*& OtherBlock, const int& VoxelSize, const FIntVector& AddToIndex)
{
	if (GridPointIndex < VoxelSize - 1)
	{
		GridPointIndex++;
	}
	else
	{
		GridPointIndex = 0;
		OffsetIndex = OffsetIndex + AddToIndex;

		FSurfaceNavBlock** OtherBlockPtr = nullptr;

		{
			FReadScopeLock rNavBlocksLock(NavBlocksLock);
			OtherBlockPtr = NavBlocks.Find(OffsetIndex);
		}

		OtherBlock = !OtherBlockPtr ? nullptr : *OtherBlockPtr;
	}
}

inline bool USurfaceNavigationSystem::DetermineNewNeighbor(TSharedPtr<FSurfaceNavPointBase>& NewNeighbor, int& MyIndexForNewNeighbor, const FSurfaceNavGridPoint*& GridPoint, const FIntVector& NavPointIndex, const int& NewNeighborIndex)
{
	if (GridPoint)
	{
		TSharedPtr<FSurfaceNavPointBase> const* NewNeighborPtr = GridPoint->NavPoints.Find(NavPointIndex);

		NewNeighbor = !NewNeighborPtr ? nullptr : *NewNeighborPtr;

		if (NewNeighbor)
		{
			if (NewNeighbor->IsValid() && GridPoint == NewNeighbor->GetOwningGridPoint())
			{
				MyIndexForNewNeighbor = NewNeighborIndex;

				return true;
			}
		}
	}

	return false;
}

void USurfaceNavigationSystem::UpdateNavNeighbors(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, const int& WorkingNeighborIndex, TSharedPtr<FSurfaceNavPointBase>& NewNeighbor,
	const int& MyIndexForNewNeighbor, const int& PriorityIndex) //, const bool IsDiagonal)
{
	check(WorkingNeighborIndex >= 0 && WorkingNeighborIndex < 8);
	check(MyIndexForNewNeighbor >= -1 && MyIndexForNewNeighbor < 8);

	if (NewNeighbor)
		NewNeighbor->ClearNeighbor(MyIndexForNewNeighbor, PriorityIndex, NewNeighbor->GetOwningGridPoint()->OwningBlock != CurNavPoint->GetOwningGridPoint()->OwningBlock);

	CurNavPoint->ClearNeighbor(WorkingNeighborIndex, PriorityIndex);

	// No longer check if is orphan here. Checking for orphan can isolate this point which could have a neighbor update afterwards
	// which could have made this point navigable without the orphan check. So just don't check for orphan here. It's fine because pathing
	// always checks for orphan as well.
	if (NewNeighbor)
	{
		//FWriteScopeLock wNavPointLock(NewNeighbor->OwningGridPoint->OwningBlock->NavPointLock);
		if (NewNeighbor->IsValid())
		{
			//Update current nav point's neighbor
			CurNavPoint->SetNeighbor(NewNeighbor, WorkingNeighborIndex, PriorityIndex);
			NewNeighbor->SetNeighbor(CurNavPoint, MyIndexForNewNeighbor, PriorityIndex, NewNeighbor->GetOwningGridPoint()->OwningBlock != CurNavPoint->GetOwningGridPoint()->OwningBlock);
		}

		// TODO: Need to complete this section to fix minor nav gen bug
		// If we're updating a axis aligned neighbor, we may need to also tell the neighbor
		// to update a couple of it's diagonal neighbors.
		if (WorkingNeighborIndex < 4)
		{
		}

		// Force the neighbor nav point to calculate it's normal
		NewNeighbor->CalcNormalFromNeighbors();
	}

	// Force the current nav point to calculate it's normal
	CurNavPoint->CalcNormalFromNeighbors();

	if (NewNeighbor && CurNavPoint->GetOwningGridPoint()->OwningBlock != NewNeighbor->GetOwningGridPoint()->OwningBlock)
		BlockNavUpdatedCallback(*NewNeighbor->GetOwningGridPoint()->OwningBlock->BlockKey);
}

#pragma endregion


#pragma region Pathing Functions


bool USurfaceNavigationSystem::ProjectPoint(const FVector& TargetLoc, const FVector& SourceLoc, const FVector& NonOffsetTargetLoc,
	const bool ForceNormalToDirFromSurface, TSharedPtr<FSurfaceNavPointBase>& OutLocation,
	const float& MinAngleFromGround, const float& MaxAngleFromGround, const float& AcceptanceRadius, const bool AllowWideSearch,
	const bool RequireWithinAcceptanceRadius) const
{
	auto StartNavPoint = FindClosestNavPointToLocation(TargetLoc, SourceLoc, NonOffsetTargetLoc, ForceNormalToDirFromSurface,
		MinAngleFromGround, MaxAngleFromGround, AcceptanceRadius, AllowWideSearch, RequireWithinAcceptanceRadius);

	if (!StartNavPoint)
		return false;

	const UNavigationQueryFilter* FilterClass = nullptr;
	OutLocation = DetermineCheapestNavPoint(StartNavPoint, MinAngleFromGround, MaxAngleFromGround, FilterClass);

	return true;
}


TSharedPtr<FSurfaceNavPointBase> USurfaceNavigationSystem::FindClosestNavPointToLocation(const FVector& TargetLoc, const FVector& SourceLoc,
	const FVector& NonOffsetTargetLoc, const bool ForceNormalToDirFromSurface,
	const float& MinAngleFromGround, const float& MaxAngleFromGround, const float& AcceptanceRadius, const bool AllowWideSearch,
	const bool RequireWithinAcceptanceRadius) const
{
	//auto StartTime = FPlatformTime::Seconds();

	auto BlockSize = SurfaceNavProxySettings::BlockSize;
	float BlockSizeMultiplier = 1.f / BlockSize;
	float PaddedDistSqTestValue = FMath::Square(FMath::Max(SurfaceNavProxySettings::MaxDistanceFromNavigation, BlockSize) * 1.2f);
	auto gridPointDist = SurfaceNavProxySettings::GridPointDistance;
	auto halfGridPointDist = gridPointDist * 0.5f;

	// We don't want more than the 9 closest blocks with surfaces
	int MaxBlocks = 9;

	float SearchPadding = AllowWideSearch ? 3.f : 1.f;

	FVector TestLoc = SourceLoc;

	if (TestLoc != TargetLoc)
	{
		FVector DirVector = TargetLoc - SourceLoc;
		FVector DirNormal = DirVector.GetSafeNormal();

		bool bBlockFound = true;
		while (bBlockFound)
		{
			// Test if we're already in allowed distance of target and if so then stop
			float TargetDistSq = FVector::DistSquared(TestLoc, TargetLoc);
			if (TargetDistSq <= PaddedDistSqTestValue)
				break;

			const FVector TempLoc = TestLoc + DirNormal * BlockSize;

			FIntVector LocBlockIndex((TempLoc - SurfaceNavProxySettings::GridOffset) * BlockSizeMultiplier);

			FSurfaceNavBlock* const* NavBlockPtr = nullptr;

			{
				FReadScopeLock rNavBlocksLock(NavBlocksLock);
				NavBlockPtr = NavBlocks.Find(LocBlockIndex);
			}

			if (NavBlockPtr && *NavBlockPtr)
				TestLoc = TempLoc;
			else
				bBlockFound = false;
		}
	}

	FVector OffsetTestLoc = TestLoc - SurfaceNavProxySettings::GridOffset;
	int startX = roundf((OffsetTestLoc.X - halfGridPointDist) * BlockSizeMultiplier - SearchPadding);
	int endX = roundf((OffsetTestLoc.X + halfGridPointDist) * BlockSizeMultiplier + SearchPadding);

	int startY = roundf((OffsetTestLoc.Y - halfGridPointDist) * BlockSizeMultiplier - SearchPadding);
	int endY = roundf((OffsetTestLoc.Y + halfGridPointDist) * BlockSizeMultiplier + SearchPadding);

	int startZ = roundf((OffsetTestLoc.Z - halfGridPointDist) * BlockSizeMultiplier - SearchPadding);
	int endZ = roundf((OffsetTestLoc.Z + halfGridPointDist) * BlockSizeMultiplier + SearchPadding);

	TSet<FSurfaceNavBlock*> ClosestBlocks;
	TMap<FSurfaceNavBlock*, float> BlockDistances;

	bool CanIterate = true;

	int i = startX;
	int j = startY;
	int k = startZ;

	// Using the target location, which is offset, can lead to grabbing "valid" points under the terrain in certain
	// situations like when the pawn is going over a corner forcing it's offset location to be under the terrain. By
	// using the average of the offset loc and non-offset loc, we may get better accuracy when selecting the nav point.
	FVector AvgTargetLoc = (NonOffsetTargetLoc + TargetLoc) * 0.5f;

	// Let's try to find closest blocks immediately around the location
	while (CanIterate)
	{
		FIntVector LocBlockIndex(i, j, k);

		FSurfaceNavBlock* const* NavBlockPtr = nullptr;

		{
			FReadScopeLock rNavBlocksLock(NavBlocksLock);
			NavBlockPtr = NavBlocks.Find(LocBlockIndex);
		}

		if (NavBlockPtr)
		{
			FSurfaceNavBlock* NavBlock = *NavBlockPtr;

			if (NavBlock && NavBlock->IsValid() && NavBlock->IsAvailableForPathing() && NavBlock->HasFoundSurface())
			{
				auto BlockLocation = NavBlock->GetLocation();
				auto BlockDist = FVector::DistSquared(AvgTargetLoc, BlockLocation);

				auto OldDistPtr = BlockDistances.Find(NavBlock);
				if (OldDistPtr)
				{
					if (BlockDist < *OldDistPtr)
					{
						BlockDistances[NavBlock] = BlockDist;
					}
				}
				else
				{
					BlockDistances.Emplace(NavBlock, BlockDist);
				}
			}
		}


		k++;

		if (k <= endZ)
			continue;

		k = startZ;
		j++;

		if (j <= endY)
			continue;

		j = startY;
		i++;

		if (i <= endX)
			continue;

		CanIterate = false;
	}

	if (BlockDistances.Num() == 0)
		return nullptr;


	BlockDistances.ValueSort([](const float& A, const float& B) { return A < B; });


	// TODO: Need something here that finds the closest available NavBlocks if no blocks found above

	// Let's find the closest Nav Point to the location
	TSharedPtr<FSurfaceNavPointBase> ClosestNavPoint = nullptr;

	double ClosestNavPointDist = INFINITY;
	int counter = 0;

	for (const TPair<FSurfaceNavBlock*, float>& ClosestBlockPair : BlockDistances)
	{
		if (counter >= MaxBlocks)
			break;

		CanIterate = true;

		int iX = 0;
		int iY = 0;
		int iZ = 0;

		auto& BlockGridPoints = ClosestBlockPair.Key->SurfaceNavGridPoints;

		while (CanIterate)
		{
			auto GridPointFinal = BlockGridPoints[iX][iY][iZ];

			if (GridPointFinal && GridPointFinal->IsValid() && GridPointFinal->OwningBlock &&
				GridPointFinal->OwningBlock->IsAvailableForPathing() && GridPointFinal->OwningBlock->IsValid() &&
				!GridPointFinal->IsOrphaned() && !GridPointFinal->IsNarrow())
			{
				//for (const TPair<FIntVector, FSurfaceNavPoint*>& NavPointPair : GridPointFinal->NavPoints)
				for (auto It = GridPointFinal->NavPoints.CreateConstIterator(); It; ++It)
				{
					auto& NavPoint = It.Value();

					if (!GridPointFinal || !GridPointFinal->IsValid() || !GridPointFinal->OwningBlock ||
						!GridPointFinal->OwningBlock->IsAvailableForPathing() || !GridPointFinal->OwningBlock->IsValid() ||
						!NavPoint->IsValid() || !NavPoint->HasNeighbors())
					{
						//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::FindClosestNavPointToLocation: No neighors found"));
						continue;
					}

					// Make sure the angle is within the allowed range
					if (NavPoint->GetAngleFromGround() < MinAngleFromGround || NavPoint->GetAngleFromGround() > MaxAngleFromGround)
					{
						//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::FindClosestNavPointToLocation: Angle outside range %d"), angle);
						continue;
					}

					const FVector& navPointLocation = NavPoint->WorldLocation;

					float GroundVectorDelta;
					if (ForceNormalToDirFromSurface)
					{
						GroundVectorDelta = (NonOffsetTargetLoc - navPointLocation).GetSafeNormal() | NavPoint->GetNormal();
						// Let's try to prevent points on the other side of walls/floors from getting selected
						if (GroundVectorDelta < 0.f)
							continue;
					}

					// Score the nav point by lowest distance
					auto navPointDist = FVector::DistSquared(AvgTargetLoc, navPointLocation);
					if (navPointDist < ClosestNavPointDist)
					{
						ClosestNavPointDist = navPointDist;
						ClosestNavPoint = NavPoint;
					}
				}
			}

			iZ++;

			if (iZ < BlockGridPoints[iX][iY].Num())
				continue;

			iZ = 0;
			iY++;

			if (iY < BlockGridPoints[iX].Num())
				continue;

			iY = 0;
			iX++;

			if (iX < BlockGridPoints.Num())
				continue;

			CanIterate = false;
		}

		counter++;
	}

	/*auto TimeSpent = FPlatformTime::Seconds() - StartTime;
	if (TimeSpent >= 0.001f)
		UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::FindClosestNavPointToLocation: Processed in %fs"), TimeSpent);*/

		// Get the squared max distance from nav
	float MaxDistanceFromNavigationSq = FMath::Square(SurfaceNavProxySettings::MaxDistanceFromNavigation);

	// Return the closest nav point if the accptance radius isn't required,
	// or, if it is required, then return closest point if within max distance from nav
	if (!RequireWithinAcceptanceRadius || ClosestNavPointDist < MaxDistanceFromNavigationSq)
		return ClosestNavPoint;
	else
	{
		return nullptr;
	}
}

bool USurfaceNavigationSystem::TestPathSync(FSurfaceNavPathFindingQueryData Query, int32* NumVisitedNodes)
{
	bool Result = false;

	FSurfaceNavPathResult PathResult = FindPath(Query);
	if (PathResult.IsSuccessful())
	{
		*NumVisitedNodes = PathResult.Path->GetPathPoints().Num();
		Result = true;
	}

	return Result;
}

void USurfaceNavigationSystem::CanReachTarget(UObject* WorldContextObject, const FVector& PathStart,
	const FVector& PathEnd, bool& OutSuccess, TSubclassOf<UNavigationQueryFilter> FilterClass,
	const float InMinAngleLimit, const float InMaxAngleLimit, const float InAcceptanceRadius,
	const int InMaxPathingIterations)
{
	OutSuccess = false;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(World);
	if (NavSys)
	{
		// Build the query object to use for pathing
		FSurfaceNavPathFindingQueryData Query(WorldContextObject, PathStart, PathEnd, PathStart, PathEnd, FilterClass, NULL,
			0, false, FLT_MAX, InMinAngleLimit, InMaxAngleLimit, InAcceptanceRadius, InMaxPathingIterations, true);

		OutSuccess = NavSys->CanReachTarget(Query);
	}
}

bool USurfaceNavigationSystem::CanReachTarget(FSurfaceNavPathFindingQueryData Query)
{
	bool Result = false;

	// Make sure we're not outside the allowed angle limit range
	Query.MinAngleFromGround = FMath::Clamp(Query.MinAngleFromGround, 0.f, 180.f);
	Query.MaxAngleFromGround = FMath::Clamp(Query.MaxAngleFromGround, 0.f, 180.f);

	// Make sure the max is actually higher
	Query.MaxAngleFromGround = FMath::Max(Query.MinAngleFromGround, Query.MaxAngleFromGround);

	// We want to know if we can reach the target so no partials
	Query.SetAllowPartialPaths(false);

	// Mark for testing so it doesn't increase pathing cost
	Query.bIsForTesting = true;

	FSurfaceNavPathResult PathResult = FindPath(Query);
	if (PathResult.IsSuccessful())
		Result = true;

	return Result;
}

void USurfaceNavigationSystem::GetPathToTarget(UObject* WorldContextObject, const FVector& PathStart,
	const FVector& PathEnd, bool& OutSuccess, TArray<FSurfaceNavPathPointData>& OutPathData,
	TSubclassOf<UNavigationQueryFilter> FilterClass, const float InMinAngleLimit, const float InMaxAngleLimit,
	const float InAcceptanceRadius, const int InMaxPathingIterations)
{
	OutSuccess = false;
	OutPathData = {};

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(World);
	if (NavSys)
	{
		// Build the query object to use for pathing
		FSurfaceNavPathFindingQueryData Query(WorldContextObject, PathStart, PathEnd, PathStart, PathEnd, FilterClass, NULL,
			0, false, FLT_MAX, InMinAngleLimit, InMaxAngleLimit, InAcceptanceRadius, InMaxPathingIterations, true);

		NavSys->GetPathToTarget(Query, OutSuccess, OutPathData);
	}
}

void USurfaceNavigationSystem::GetPathToTarget(FSurfaceNavPathFindingQueryData InQuery, bool& OutSuccess, TArray<FSurfaceNavPathPointData>& OutPathData)
{
	// Make sure we're not outside the allowed angle limit range
	InQuery.MinAngleFromGround = FMath::Clamp(InQuery.MinAngleFromGround, 0.f, 180.f);
	InQuery.MaxAngleFromGround = FMath::Clamp(InQuery.MaxAngleFromGround, 0.f, 180.f);

	// Make sure the max is actually higher
	InQuery.MaxAngleFromGround = FMath::Max(InQuery.MinAngleFromGround, InQuery.MaxAngleFromGround);

	// We want to know if we can reach the target so no partials
	InQuery.SetAllowPartialPaths(false);

	// Mark for testing so it doesn't increase pathing cost
	InQuery.bIsForTesting = true;

	FSurfaceNavPathResult PathResult = FindPath(InQuery);
	if (PathResult.IsSuccessful())
	{
		PathResult.Path->GetRawPathData(OutSuccess, OutPathData);
	}
}

FSurfaceNavPathResult USurfaceNavigationSystem::FindPath(FSurfaceNavPathFindingQueryData& Query)
{
	//auto StartTime = FPlatformTime::Seconds();

	FSurfaceNavPathResult Result(ENavigationQueryResult::Error);

	if (bIsReadyToPathfind && bSurfaceNavGenerationEnabled && (Query.bIsForTesting || CurPathingCount < SurfaceNavProxySettings::MaxPathRequestsPerTick))
	{
		if (!Query.bIsForTesting)
			CurPathingCount++;

		TSharedPtr<FSurfaceNavPointBase> EndNavPoint;

		TArray<FVector> PathPoints;
		TArray<float> PointExpenses;
		TArray<FVector> PointNormals;
		TArray<int64> CustomLinkIDs;

		{
			PathingRequestCount++;
			FWriteScopeLock wPathingLock(PathingLock);

			TSharedPtr<FSurfaceNavPointBase> StartNavPoint = FindClosestNavPointToLocation(Query.StartLocation, Query.StartLocation, Query.NonOffsetStartLocation, true, Query.MinAngleFromGround, Query.MaxAngleFromGround, Query.AcceptanceRadius);

			if (!StartNavPoint)
			{
				PathingRequestCount--;
				return Result;
			}

			// Let's try to find a nav point around the end location first, but it's possible
			// there may not be an invoker close enough to get valid nav data. We'll do a wide 
			// search to give the best chance of success.
			EndNavPoint = FindClosestNavPointToLocation(Query.EndLocation, Query.EndLocation, Query.NonOffsetEndLocation, false, Query.MinAngleFromGround, Query.MaxAngleFromGround, Query.AcceptanceRadius, true /* wide search */, false /* require acceptance radius */);

			// If we don't find an end nav point with the prior attempt, then let's try it a different way.		
			if (!EndNavPoint)
				// We're passing in the start location for the second parameter so that the blocks searched will
				// be around the source to ensure the path is for loaded blocks around the source. We'll do
				// another wide search to give the best chance of success.			
				EndNavPoint = FindClosestNavPointToLocation(Query.EndLocation, Query.StartLocation, Query.NonOffsetEndLocation, false, Query.MinAngleFromGround, Query.MaxAngleFromGround, Query.AcceptanceRadius, true /* wide search */, false /* require acceptance radius */);

			if (!EndNavPoint)
			{
				PathingRequestCount--;
				return Result;
			}


			// *************************************
			// *************************************
			// Let's see if any of the immediate neighbors of the start and end navpoints are cheaper cost and
			// if so then use those cheaper cost nav points.
			// TODO: This may not be a good idea with a larger grid point distance as they could be too far away from each other.
			// *************************************

			const UNavigationQueryFilter* FilterClass = Query.FilterClass ? Query.FilterClass.GetDefaultObject() : nullptr;

			// Let's do the start nav point first
			StartNavPoint = DetermineCheapestNavPoint(StartNavPoint, Query.MinAngleFromGround, Query.MaxAngleFromGround, FilterClass); // , CheapestNavPoint);

			// Now do the same for the end nav point
			EndNavPoint = DetermineCheapestNavPoint(EndNavPoint, Query.MinAngleFromGround, Query.MaxAngleFromGround, FilterClass); // , CheapestNavPoint);		

			// *************************************
			// *************************************


			// Debug code for seeing the location of the found end nav point
			/*DrawDebugSphere
			(
				GetWorld(),
				EndNavPoint->WorldLocation,
				30.f,
				8,
				FColor::Red,
				false,
				360.f,
				0,
				2.0f
			);*/



			BuildNavPointsForPath(PathPoints, PointExpenses, PointNormals, CustomLinkIDs, StartNavPoint, EndNavPoint, Query);

			PathingRequestCount--;
		}

		if (PathPoints.Num() > 1)
		{
			AActor* OwnerActor = nullptr;

			if (!Query.bIsForTesting && Query.PathInstanceToFill && Query.PathInstanceToFill.IsValid())
			{
				Result.Path = Query.PathInstanceToFill;
				Result.Path->ResetForRepath();
				Result.Path->UpdatePathData(PathPoints, PointExpenses, PointNormals, CustomLinkIDs);
			}
			else
			{
				auto SurfacePath = new FSurfaceNavPath(PathPoints, PointExpenses, PointNormals, CustomLinkIDs, OwnerActor);
				Result.Path = TSharedPtr<FSurfaceNavPath, ESPMode::ThreadSafe>(SurfacePath);
			}

			if (!Query.bIsForTesting)
				RegisterActivePath(Result.Path);

			Result.Path->SetQueryData(Query);

			bool EndpointsMatch = PathPoints.Last().Equals(EndNavPoint->WorldLocation);
			float DistFromTargetToEndpointSq = FVector::DistSquared(EndNavPoint->WorldLocation, Query.EndLocation);
			float AcceptanceRadiusSq = FMath::Square(Query.AcceptanceRadius) * 1.1f;
			Result.Path->SetIsPartial(!EndpointsMatch || DistFromTargetToEndpointSq > AcceptanceRadiusSq);
			float TimeStamp = GetWorldTimeStamp();

			// update time stamp to give observers any means of telling if it has changed
			Result.Path->SetTimeStamp(TimeStamp);

			if (!Result.IsPartial())
				Result.Result = ENavigationQueryResult::Success;
			else
				Result.Result = Query.bAllowPartialPaths ? ENavigationQueryResult::Success : ENavigationQueryResult::Fail;


			// partial paths are still valid and can change to full path when moving goal gets back on navmesh
			if (Result.IsSuccessful() || Result.IsPartial())
			{
				Result.Path->UpdateLastRepathGoalLocation();
				Result.Path->DoneUpdating(ENavPathUpdateType::NavigationChanged);

			}
			else
			{
				Result.Path->RePathFailed();
			}
		}
	}
	else
	{
		Result.Result = ENavigationQueryResult::Fail;
	}

	/*auto TimeSpent = FPlatformTime::Seconds() - StartTime;
	if (TimeSpent >= 0.001f)
		UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::FindPath: Processed in %fs"), TimeSpent);*/

	return Result;
}

inline TSharedPtr<FSurfaceNavPointBase> USurfaceNavigationSystem::DetermineCheapestNavPoint(TSharedPtr<FSurfaceNavPointBase> NavPoint,
	const float& MinAngleFromGround, const float& MaxAngleFromGround, const UNavigationQueryFilter*& FilterClass) const //, FSurfaceNavPoint*& CheapestNavPoint)
{
	TSharedPtr<FSurfaceNavPointBase> CheapestNavPoint = NavPoint;

	TSharedPtr<FSurfaceNavPointBase> NavPointNeighbor = nullptr;
	NavPoint->GetNeighborAt(0, NavPointNeighbor);
	DetermineCheaperNavPoint(NavPointNeighbor, CheapestNavPoint, MinAngleFromGround, MaxAngleFromGround, FilterClass);

	NavPointNeighbor = nullptr;
	NavPoint->GetNeighborAt(1, NavPointNeighbor);
	DetermineCheaperNavPoint(NavPointNeighbor, CheapestNavPoint, MinAngleFromGround, MaxAngleFromGround, FilterClass);

	NavPointNeighbor = nullptr;
	NavPoint->GetNeighborAt(2, NavPointNeighbor);
	DetermineCheaperNavPoint(NavPointNeighbor, CheapestNavPoint, MinAngleFromGround, MaxAngleFromGround, FilterClass);

	NavPointNeighbor = nullptr;
	NavPoint->GetNeighborAt(3, NavPointNeighbor);
	DetermineCheaperNavPoint(NavPointNeighbor, CheapestNavPoint, MinAngleFromGround, MaxAngleFromGround, FilterClass);

	NavPointNeighbor = nullptr;
	NavPoint->GetNeighborAt(4, NavPointNeighbor);
	DetermineCheaperNavPoint(NavPointNeighbor, CheapestNavPoint, MinAngleFromGround, MaxAngleFromGround, FilterClass);

	NavPointNeighbor = nullptr;
	NavPoint->GetNeighborAt(5, NavPointNeighbor);
	DetermineCheaperNavPoint(NavPointNeighbor, CheapestNavPoint, MinAngleFromGround, MaxAngleFromGround, FilterClass);

	NavPointNeighbor = nullptr;
	NavPoint->GetNeighborAt(6, NavPointNeighbor);
	DetermineCheaperNavPoint(NavPointNeighbor, CheapestNavPoint, MinAngleFromGround, MaxAngleFromGround, FilterClass);

	NavPointNeighbor = nullptr;
	NavPoint->GetNeighborAt(7, NavPointNeighbor);
	DetermineCheaperNavPoint(NavPointNeighbor, CheapestNavPoint, MinAngleFromGround, MaxAngleFromGround, FilterClass);

	return CheapestNavPoint;
}

inline void USurfaceNavigationSystem::DetermineCheaperNavPoint(TSharedPtr<FSurfaceNavPointBase> NavPointNeighbor, TSharedPtr<FSurfaceNavPointBase>& CheapestNavPoint,
	const float& MinAngleFromGround, const float& MaxAngleFromGround, const UNavigationQueryFilter*& FilterClass) const
{
	if (!NavPointNeighbor || !NavPointNeighbor->IsValid() || !NavPointNeighbor->HasNeighbors())
		return;

	FSurfaceNavGridPoint* OwningGridPoint = NavPointNeighbor->GetOwningGridPoint();
	if (OwningGridPoint->IsOrphaned() || OwningGridPoint->IsNarrow())
		return;

	double NavPointNeighborCost = NavPointNeighbor->NavArea->DefaultCost;
	double CheapestNavPointCost = CheapestNavPoint->NavArea->DefaultCost;

	if (FilterClass)
	{
		auto NavPointNeighborFilterArea = FilterClass->Areas.FindByPredicate([NavPointNeighbor](const FNavigationFilterArea& NavFilterArea) { return NavFilterArea.AreaClass.GetDefaultObject() == NavPointNeighbor->NavArea; });
		if (NavPointNeighborFilterArea)
			NavPointNeighborCost = NavPointNeighborFilterArea->bIsExcluded ? INFINITY : NavPointNeighborFilterArea->TravelCostOverride;

		auto CheapestNavPointFilterArea = FilterClass->Areas.FindByPredicate([CheapestNavPoint](const FNavigationFilterArea& NavFilterArea) { return NavFilterArea.AreaClass.GetDefaultObject() == CheapestNavPoint->NavArea; });
		if (CheapestNavPointFilterArea)
			CheapestNavPointCost = CheapestNavPointFilterArea->bIsExcluded ? INFINITY : CheapestNavPointFilterArea->TravelCostOverride;
	}

	if (NavPointNeighborCost < CheapestNavPointCost&& NavPointNeighbor->GetAngleFromGround() >= MinAngleFromGround &&
		NavPointNeighbor->GetAngleFromGround() <= MaxAngleFromGround)
		CheapestNavPoint = NavPointNeighbor;
}

inline void USurfaceNavigationSystem::BuildNavPointsForPath(TArray<FVector>& PathPoints, TArray<float>& PointExpenses, TArray<FVector>& PointNormals, TArray<int64>& CustomLinkIDs,
	TSharedPtr<FSurfaceNavPointBase>& StartNavPoint, TSharedPtr<FSurfaceNavPointBase>& EndNavPoint, FSurfaceNavPathFindingQueryData& Query)
{
	//auto StartTime = FPlatformTime::Seconds();

	PathNode* FinalNode = nullptr;

	// If the queries max pathing iterations is 0, then set it to what the grid builder has.
	// It will be used later
	Query.MaxPathingIterations = Query.MaxPathingIterations > 0 ? Query.MaxPathingIterations : SurfaceNavProxySettings::MaxPathingIterations;

	if (Query.MaxPathingIterations == SurfaceNavProxySettings::MaxPathingIterations)
	{
		Query.MaxPathingIterations = SurfaceNavProxySettings::MaxPathingIterations + 1;
	}

	DeterminePath(StartNavPoint, EndNavPoint, Query, FinalNode);


	if (!FinalNode)
		return;


	// TODO: Need to review pathing logic to try to resolve this issue there.
	// A partial path shouldn't end up further away from the goal as the AI wouldn't know if that's a viable path
	// unless it ended up closer to the goal.
	/*double DistSqToEndFromStart = FVector::DistSquared(StartNavPoint->WorldLocation, EndNavPoint->WorldLocation);
	double DistSqToEndFromNode = FVector::DistSquared(FinalNode->NavPoint->WorldLocation, EndNavPoint->WorldLocation);
	if ((EndNavPoint != FinalNode->NavPoint && DistSqToEndFromNode > DistSqToEndFromStart))
		return;*/


	PathNode* CurrentNode = FinalNode;

	int MyMaxPathingIterations = Query.MaxPathingIterations;

	//Can't be larger than MaxPathingIterations
	PathPoints.SetNumZeroed(MyMaxPathingIterations);
	PointExpenses.SetNumZeroed(MyMaxPathingIterations);
	PointNormals.SetNumZeroed(MyMaxPathingIterations);
	CustomLinkIDs.SetNumZeroed(MyMaxPathingIterations);

	int CurIndex = MyMaxPathingIterations;

	FVector PrevLoc = FVector::ZeroVector;
	FVector PrevOffsetLoc = FVector::ZeroVector;
	int Counter = 0;

	const UNavArea* DefaultAreaOb = static_cast<const UNavArea*>(FNavigationSystem::GetDefaultWalkableArea().GetDefaultObject());
	const float NavDefaultCost = DefaultAreaOb ? DefaultAreaOb->DefaultCost : 1.0f;

	while (CurrentNode)
	{
		TSharedPtr<FSurfaceNavPointBase> CurNavPoint = CurrentNode->NavPoint;

		// Since this point was selected, let's increase it's expense slightly
		if (!Query.bIsForTesting)
			CurNavPoint->IncreaseExpense();

		CurIndex--;

		PathPoints[CurIndex] = CurNavPoint->WorldLocation;
		PointExpenses[CurIndex] = CurNavPoint->NavArea ? CurNavPoint->NavArea->DefaultCost : NavDefaultCost;
		PointNormals[CurIndex] = CurNavPoint->GetNormal();
		CustomLinkIDs[CurIndex] = CurNavPoint->GetLinkId();

		CurrentNode = CurrentNode->parent;

		Counter++;
	}

	// Remove all the excess empty items from the beginning of the array
	PathPoints.RemoveAt(0, CurIndex);
	PointExpenses.RemoveAt(0, CurIndex);
	PointNormals.RemoveAt(0, CurIndex);
	CustomLinkIDs.RemoveAt(0, CurIndex);


	/*auto TimeSpent = FPlatformTime::Seconds() - StartTime;
	if (TimeSpent >= 0.001f)
		UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::BuildNavPointsForPath: Processed in %fs"), TimeSpent);*/
}

inline void USurfaceNavigationSystem::DeterminePath(TSharedPtr<FSurfaceNavPointBase>& StartNavPoint, TSharedPtr<FSurfaceNavPointBase>& EndNavPoint, FSurfaceNavPathFindingQueryData& Query,
	PathNode*& CurrentNode)
{
	//auto StartTime = FPlatformTime::Seconds();

	FVector EndLocation = EndNavPoint->WorldLocation;
	FVector StartLocation = StartNavPoint->WorldLocation;

	PathNode* EndNode = new PathNode(EndNavPoint);

	PathNode* StartNode = new PathNode(StartNavPoint);

	StartNode->fLocalGoal = 0.0f;
	StartNode->fGlobalGoal = FVector::DistSquared(StartLocation, EndLocation);

	TSortedMap<double, PathNode*> PointsNotTestedMap;
	PointsNotTestedMap.Reserve(Query.MaxPathingIterations);
	PointsNotTestedMap.Emplace(StartNode->fGlobalGoal, StartNode);

	TMap<TSharedPtr<FSurfaceNavPointBase>, PathNode*> NodePoints;
	NodePoints.Reserve(Query.MaxPathingIterations);
	NodePoints.Emplace(StartNavPoint, StartNode);
	NodePoints.Emplace(EndNavPoint, EndNode);

	auto FilterClass = Query.GetNavigationFilter().GetDefaultObject();

	float MinAngleFromGround = Query.MinAngleFromGround;
	float MaxAngleFromGround = Query.MaxAngleFromGround;

	CurrentNode = StartNode;

	PathNode* ShortestPathNode = StartNode;

	uint32 Counter = 0;
	bool bPathComplete = false;

	while (PointsNotTestedMap.Num() > 0 && CurrentNode->NavPoint != EndNavPoint)
	{
		//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::DeterminePath: Sorting %d points"), PointsNotTested.Num());

		for (auto It = PointsNotTestedMap.CreateIterator(); It; ++It)
		{
			if (!It.Value()->bVisited)
			{
				CurrentNode = It.Value();
				break;
			}

			It.RemoveCurrent();
		}

		// ...or abort because there are no valid nodes left to test
		if (PointsNotTestedMap.Num() == 0)
			break;

		CurrentNode->bVisited = true; // We only explore a node once
		TSharedPtr<FSurfaceNavPointBase>& CurNavPoint = CurrentNode->NavPoint;

		if (CurNavPoint && CurNavPoint->IsValid())
		{
			const FVector& curLoc = CurNavPoint->WorldLocation;

			// Regular nav points will have an owning grid point
			FSurfaceNavGridPoint* OwningGridPoint = CurNavPoint->GetOwningGridPoint();
			if (OwningGridPoint)
			{
				//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::DeterminePath(): Read Lock - NavBlockLock"));
				//FReadScopeLock rLock(CurNavPoint->OwningGridPoint->OwningBlock->NavBlockLock);

				if (OwningGridPoint->IsValid() && OwningGridPoint->OwningBlock && OwningGridPoint->OwningBlock->IsActive() && OwningGridPoint->OwningBlock->IsAvailableForPathing())
					ProcessNavPointForPathing(CurNavPoint, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
						EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);

			}
			// Nav points for nav links will have a link ID that's not zero
			else if (CurNavPoint->GetLinkId() != 0)
			{
				ProcessNavPointForPathing(CurNavPoint, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
					EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);

			}
		}

		// If we surpass max iterations then stop
		if (Counter >= Query.MaxPathingIterations)
			break;
	}

	double ShortestPathDistSq = FVector::DistSquared(ShortestPathNode->NavPoint->WorldLocation, Query.EndLocation);
	double CurrentPathDistSq = FVector::DistSquared(CurrentNode->NavPoint->WorldLocation, Query.EndLocation);

	// If the shortest path node is the start node then we didn't go anywhere so clear the
	// current node
	if (ShortestPathNode == StartNode && ShortestPathDistSq < CurrentPathDistSq)
		CurrentNode = nullptr;
	// ***************************************************************
	// In cases where the iteration limit is hit, the pathing will be cut off so current node
	// may not be the closest node.
	// ***************************************************************
	// If the shortest path has traveled less (global goal) and is closer to the end point (global - local)
	// then use shortest path node as current node.
	else if (ShortestPathDistSq < CurrentPathDistSq || (ShortestPathNode->fGlobalGoal < CurrentNode->fGlobalGoal &&
		(ShortestPathNode->fGlobalGoal - ShortestPathNode->fLocalGoal) < (CurrentNode->fGlobalGoal - CurrentNode->fLocalGoal)))
		CurrentNode = ShortestPathNode;

	// Need to queue this for cleanup to prevent memory leak.
	// There's a delay so the remaining pathfinding will process before this happens
	QueuePathNodeMapToDelete(NodePoints, this);
}

inline void USurfaceNavigationSystem::ProcessNavPointForPathing(TSharedPtr<FSurfaceNavPointBase>& CurNavPoint, TMap<TSharedPtr<FSurfaceNavPointBase>, PathNode*>& NodePoints, TSortedMap<double, PathNode*>& PointsNotTestedMap,
	UNavigationQueryFilter*& FilterClass, const FVector& curLoc, PathNode*& CurrentNode, PathNode*& ShortestPathNode, const FVector& EndLocation,
	const float& MinAngleFromGround, const float& MaxAngleFromGround, uint32& Counter, FSurfaceNavPathFindingQueryData& Query)
{
	// If this is a link point, then test the other side of the link 
	if (CurNavPoint->GetOtherLinkPoint()) {
		TSharedPtr<FSurfaceNavPointBase> OtherLinkPoint = CurNavPoint->GetOtherLinkPoint();
		PathingIterateNavPointNeighbor(OtherLinkPoint, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
			EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);
	}

	// If this nav point has neighbors related to a nav link, then they will be here.
	// This could be the neighbors from a nav link point, or the nav points that are
	// neighbors with the link.
	if (CurNavPoint->CustomLinkNeighbors.Num() > 0)
	{
		for (auto& LinkNeighbor : CurNavPoint->CustomLinkNeighbors)
		{
			if (auto PinnedLinkNeighbor = LinkNeighbor.Pin())
				PathingIterateNavPointNeighbor(PinnedLinkNeighbor, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
					EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);
		}
	}

	//	// Always return if narrow, we only want to test the opposing narrow point
	//	return;
	//}

	// **********************************
	// **********************************
	// Only 8 neighbors so unwrap the loop
	// **********************************

	TSharedPtr<FSurfaceNavPointBase> NavPointNeighbor = nullptr;
	CurNavPoint->GetNeighborAt(0, NavPointNeighbor);
	PathingIterateNavPointNeighborLockProxy(NavPointNeighbor, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
		EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);

	NavPointNeighbor = nullptr;
	CurNavPoint->GetNeighborAt(1, NavPointNeighbor);
	PathingIterateNavPointNeighborLockProxy(NavPointNeighbor, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
		EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);

	NavPointNeighbor = nullptr;
	CurNavPoint->GetNeighborAt(2, NavPointNeighbor);
	PathingIterateNavPointNeighborLockProxy(NavPointNeighbor, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
		EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);

	NavPointNeighbor = nullptr;
	CurNavPoint->GetNeighborAt(3, NavPointNeighbor);
	PathingIterateNavPointNeighborLockProxy(NavPointNeighbor, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
		EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);

	NavPointNeighbor = nullptr;
	CurNavPoint->GetNeighborAt(4, NavPointNeighbor);
	PathingIterateNavPointNeighborLockProxy(NavPointNeighbor, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
		EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);

	NavPointNeighbor = nullptr;
	CurNavPoint->GetNeighborAt(5, NavPointNeighbor);
	PathingIterateNavPointNeighborLockProxy(NavPointNeighbor, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
		EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);

	NavPointNeighbor = nullptr;
	CurNavPoint->GetNeighborAt(6, NavPointNeighbor);
	PathingIterateNavPointNeighborLockProxy(NavPointNeighbor, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
		EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);

	NavPointNeighbor = nullptr;
	CurNavPoint->GetNeighborAt(7, NavPointNeighbor);
	PathingIterateNavPointNeighborLockProxy(NavPointNeighbor, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
		EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);

	// This won't apply to the last point and that's fine
	CurNavPoint->DecreaseExpense();

	// **********************************
	// **********************************
}

inline void USurfaceNavigationSystem::PathingIterateNavPointNeighborLockProxy(TSharedPtr<FSurfaceNavPointBase>& navPoint, TMap<TSharedPtr<FSurfaceNavPointBase>, PathNode*>& NodePoints, TSortedMap<double, PathNode*>& PointsNotTestedMap,
	UNavigationQueryFilter*& FilterClass, const FVector& curLoc, PathNode*& CurrentNode, PathNode*& ShortestPathNode, const FVector& EndLocation,
	const float& MinAngleFromGround, const float& MaxAngleFromGround, uint32& Counter, FSurfaceNavPathFindingQueryData& Query)
{
	if (navPoint && navPoint->IsValid())
	{
		FSurfaceNavGridPoint* OwningGridPoint = navPoint->GetOwningGridPoint();
		if (OwningGridPoint && OwningGridPoint->OwningBlock && OwningGridPoint->OwningBlock->IsActive())
		{
			//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::PathingIterateNavPointNeighborLockProxy(): Read Lock - NavBlockLock"));
			//FReadScopeLock rLock(navPoint->OwningGridPoint->OwningBlock->NavBlockLock);

			if (OwningGridPoint->OwningBlock->IsAvailableForPathing())
				PathingIterateNavPointNeighbor(navPoint, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
					EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);
		}
		else
		{
			PathingIterateNavPointNeighbor(navPoint, NodePoints, PointsNotTestedMap, FilterClass, curLoc, CurrentNode, ShortestPathNode,
				EndLocation, MinAngleFromGround, MaxAngleFromGround, Counter, Query);
		}
	}
}

inline void USurfaceNavigationSystem::PathingIterateNavPointNeighbor(TSharedPtr<FSurfaceNavPointBase>& navPoint, TMap<TSharedPtr<FSurfaceNavPointBase>, PathNode*>& NodePoints,
	TSortedMap<double, PathNode*>& PointsNotTestedMap, UNavigationQueryFilter* FilterClass, const FVector& curLoc, PathNode*& CurrentNode,
	PathNode*& ShortestPathNode, const FVector& EndLocation, const float& MinAngleFromGround,
	const float& MaxAngleFromGround, uint32& Counter, FSurfaceNavPathFindingQueryData& Query)
{
	if (!navPoint || !navPoint->IsValid() || !navPoint->HasNeighbors()) // || (!navPoint->OwningGridPoint && !navPoint->OtherLinkPoint))
		return;

	// If this nav point is not a nav link and is orphaned or is narrow then skip
	FSurfaceNavGridPoint* OwningGridPoint = navPoint->GetOwningGridPoint();
	if (navPoint->GetLinkId() == 0 && (!OwningGridPoint || !OwningGridPoint->IsValid() || OwningGridPoint->IsOrphaned() || OwningGridPoint->IsNarrow()))
		return;

	// Make sure the point is at an angle that the AI is allowed to navigate on
	if (navPoint->GetAngleFromGround() < MinAngleFromGround || navPoint->GetAngleFromGround() > MaxAngleFromGround)
		return;

	//bool bIsForNarrowGap = false;
	//CurrentNode->NavPoint->OwningGridPoint->IsNarrow() && 
	/*if (navPoint->OwningGridPoint->IsNarrow())
		if (!AllowNarrow)
			return;
		else
			bIsForNarrowGap = true;*/

	PathNode* nodePoint = nullptr;

	auto nodePointPtr = NodePoints.Find(navPoint); // nullptr;
	if (nodePointPtr)
		nodePoint = *nodePointPtr;

	/*if (NodePoints.Contains(navPoint))
	nodePoint = NodePoints[navPoint];
	else*/

	if (!nodePoint)
	{
		nodePoint = new PathNode(navPoint);
		//nodePoint->NavPoint = navPoint;
		NodePoints.Emplace(navPoint, nodePoint);
		//PointsNotTested.Emplace(nodePoint);
	}

	/*else if (!nodePoint->bVisited)
	{
		PointsNotTested.Emplace(nodePoint);
	}*/


	// Calculate the neighbours potential lowest parent distance
	auto navPointLoc = navPoint->WorldLocation;

	const auto& NavArea = navPoint->NavArea;
	double DefaultCost = NavArea->DefaultCost;
	double FixedAreaEnteringCost = NavArea->GetFixedAreaEnteringCost();

	/*if (DefaultCost > 5000000.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::PathingIterateNavPointNeighbor: Cost is %d"), DefaultCost);
	}*/

	if (FilterClass)
	{
		//int AreaIndex = 0;
		auto NavFilterArea = FilterClass->Areas.FindByPredicate([NavArea](const FNavigationFilterArea& NavFilterArea) { return NavFilterArea.AreaClass.GetDefaultObject() == NavArea; });
		if (NavFilterArea)
		{
			if (NavFilterArea->bIsExcluded)
			{
				DefaultCost = INFINITY;
				FixedAreaEnteringCost = INFINITY;
			}
			else
			{
				if (NavFilterArea->bOverrideTravelCost)
					DefaultCost = NavFilterArea->TravelCostOverride;

				if (NavFilterArea->bOverrideEnteringCost)
					FixedAreaEnteringCost = NavFilterArea->EnteringCostOverride;
			}
		}
	}

	if (Query.PawnAngleExpenseMultipliers.Num() > 0)
	{
		for (FPawnAngleExpenseMultiplier& AngleExpMultiplier : Query.PawnAngleExpenseMultipliers)
		{
			if (navPoint->GetAngleFromGround() >= AngleExpMultiplier.MinAngle && navPoint->GetAngleFromGround() <= AngleExpMultiplier.MaxAngle)
			{
				DefaultCost *= AngleExpMultiplier.ExpenseMultiplier;
				break;
			}
		}
	}

	// Working with squared distances/costs so square these as well
	DefaultCost *= DefaultCost;
	FixedAreaEnteringCost *= FixedAreaEnteringCost;

	//UE_LOG(LogTemp, Warning, TEXT("ASurfaceNavGridBuilder::DeterminePath: DefCost = %f, EnterCost = %f"), DefaultCost, FixedAreaEnteringCost);

	double DistToPoint = FVector::DistSquared(curLoc, navPointLoc) * DefaultCost;
	//double DistToPoint = FVector::Dist(curLoc, navPointLoc) * DefaultCost;

	if (!Query.bIsForTesting)
		DistToPoint += navPoint->GetExpenseModifier();

	// TODO: Right now I'm just adding the enter cost because I'm having trouble finding
	// UE code which makes use of this value...need more research to determine how to handle it
	if (CurrentNode->NavPoint->NavArea != navPoint->NavArea)
	{
		DistToPoint += FixedAreaEnteringCost;
	}

	//float DistToPointPercent = FMath::Min<float>(DistToPoint / StandardizedPointCost, 1.f);

	double fPossiblyLowerGoal = CurrentNode->fLocalGoal + DistToPoint; // +(DistToPointPercent * navPoint->NavArea->DefaultCost);

	// If choosing to path through this node is a lower distance than what 
	// the neighbour currently has set, update the neighbour to use this node
	// as the path source, and set its distance scores as necessary
	if (fPossiblyLowerGoal < nodePoint->fLocalGoal)
	{
		nodePoint->parent = CurrentNode;
		nodePoint->fLocalGoal = fPossiblyLowerGoal;

		// The best path length to the neighbour being tested has changed, so
		// update the neighbour's score. The heuristic is used to globally bias
		// the path algorithm, so it knows if its getting better or worse. At some
		// point the algo will realise this path is worse and abandon it, and then go
		// and search along the next best path.
		nodePoint->fGlobalGoal = nodePoint->fLocalGoal + FVector::DistSquared(navPointLoc, EndLocation);
		if (nodePoint->fGlobalGoal < ShortestPathNode->fGlobalGoal)
		{
			// Let's delete the original ShortestPathNode ptr to prevent memory leak
			/*if (ShortestPathNode->fGlobalGoal == INFINITY)
				delete ShortestPathNode;*/

			ShortestPathNode = nodePoint;
		}
	}

	if (!nodePoint->bVisited)
		PointsNotTestedMap.Emplace(nodePoint->fGlobalGoal, nodePoint);

	Counter++;
}

inline bool USurfaceNavigationSystem::FindRandomNavPointNearby_Original(FSurfaceNavPathFindingQueryData& Query, FVector& RandomLocation, const float SearchRadius, const FVector TetherPoint)
{
	//auto StartTime = FPlatformTime::Seconds();
	float SearchRadiusSq = SearchRadius * SearchRadius;

	// *************************************
	// *************************************


	UNavArea* SourceNavArea = nullptr;
	//auto ClosestNavPointToSource = FindClosestNavPointToLocation(Query.StartLocation, Query.StartLocation);

	// Get the closest nav point to the starting location in order to get the closest nav area
	if (auto ClosestNavPointToSource = FindClosestNavPointToLocation(Query.StartLocation, Query.StartLocation, Query.StartLocation, true, Query.MinAngleFromGround, Query.MaxAngleFromGround, Query.AcceptanceRadius, false /* wide search */, false /* require acceptance radius */))
		SourceNavArea = ClosestNavPointToSource->NavArea;

	auto BlockSize = SurfaceNavProxySettings::BlockSize;
	//auto blockExtent = SurfaceNavProxySettings::BlockExtent;
	auto gridPointDist = SurfaceNavProxySettings::GridPointDistance;
	auto halfGridPointDist = gridPointDist / 2.f;
	auto AxisGridSize = truncf(BlockSize / gridPointDist);

	const bool UsingTether = !TetherPoint.IsZero();
	const auto& SourceLoc = UsingTether ? TetherPoint : Query.StartLocation;

	float SearchPadding = 0.f;

	FVector OffsetSourceLoc = SourceLoc - SurfaceNavProxySettings::GridOffset;
	int startX = roundf((OffsetSourceLoc.X - halfGridPointDist - SearchRadius) / BlockSize - SearchPadding);
	int endX = roundf((OffsetSourceLoc.X + halfGridPointDist + SearchRadius) / BlockSize + SearchPadding);

	int startY = roundf((OffsetSourceLoc.Y - halfGridPointDist - SearchRadius) / BlockSize - SearchPadding);
	int endY = roundf((OffsetSourceLoc.Y + halfGridPointDist + SearchRadius) / BlockSize + SearchPadding);

	int startZ = roundf((OffsetSourceLoc.Z - halfGridPointDist - SearchRadius) / BlockSize - SearchPadding);
	int endZ = roundf((OffsetSourceLoc.Z + halfGridPointDist + SearchRadius) / BlockSize + SearchPadding);



	// This will be used later to determine if we select a nav point or not.
	// This calculation may need to be adjusted to get the desired wandering results.
	auto SelectionThreshold = 100.f / (AxisGridSize * AxisGridSize * (endX - startX)) * 10.f;

	auto FilterClass = Query.GetNavigationFilter().GetDefaultObject();

	bool CanIterateBlocks = true;

	int iBlock = startX;
	int jBlock = startY;
	int kBlock = startZ;

	TSharedPtr<FSurfaceNavPointBase> CheapestRandomNavPoint = nullptr;
	float CheapestCost = INFINITY;

	bool AcceptCurrentValue = false;

	// Square the search radius to use below
	//SearchRadius *= SearchRadius;

	bool ValidNavPointsProcessed = false;

	TArray<TSharedPtr<FSurfaceNavPointBase>> CheapestNavPoints;

	// Let's try to find closest blocks immediately around the location
	while (CanIterateBlocks)
	{
		FIntVector LocBlockIndex(iBlock, jBlock, kBlock);

		FSurfaceNavBlock** NavBlockPtr = nullptr;

		{
			FReadScopeLock rNavBlocksLock(NavBlocksLock);
			NavBlockPtr = NavBlocks.Find(LocBlockIndex);
		}

		if (NavBlockPtr) // || !NavBlock->HasFoundSurface())
		{
			FSurfaceNavBlock* NavBlock = *NavBlockPtr;

			if (NavBlock && NavBlock->HasFoundSurface())
			{
				auto CanIterateGridPoints = true;

				int xGridPoint = 0;
				int yGridPoint = 0;
				int zGridPoint = 0;

				auto& BlockGridPoints = NavBlock->SurfaceNavGridPoints;

				while (CanIterateGridPoints)
				{
					auto GridPointFinal = BlockGridPoints[xGridPoint][yGridPoint][zGridPoint];

					if (GridPointFinal && GridPointFinal->NavPoints.Num() > 0 && !GridPointFinal->IsOrphaned() && !GridPointFinal->IsNarrow())
					{
						// If the distance is greater than the provided search radius then skip
						auto DistFromSource = FVector::DistSquared(GridPointFinal->WorldLocation, SourceLoc);
						if (DistFromSource <= SearchRadiusSq)
						{
							for (const TPair<FIntVector, TSharedPtr<FSurfaceNavPointBase>>& NavPointPair : GridPointFinal->NavPoints)
							{
								auto& NavPoint = NavPointPair.Value;

								if (NavPoint->HasNeighbors())
								{
									ValidNavPointsProcessed = true;

									//// Add some randomness to get a random navigation point
									//// Adjust this number to tweak selection
									//if (FMath::FRandRange(0.f, 100.f) > SelectionThreshold) // RangeMax)
									//	continue;

									const auto& NavArea = NavPoint->NavArea;
									auto NavPointCost = NavArea->DefaultCost;
									auto FixedAreaEnteringCost = NavArea->GetFixedAreaEnteringCost();

									if (FilterClass)
									{
										int AreaIndex = 0;
										auto NavFilterArea = FilterClass->Areas.FindByPredicate([NavArea](const FNavigationFilterArea& NavFilterArea) { return NavFilterArea.AreaClass.GetDefaultObject() == NavArea; });
										if (NavFilterArea)
										{
											if (NavFilterArea->bIsExcluded)
											{
												NavPointCost = INFINITY;
												FixedAreaEnteringCost = INFINITY;
											}
											else
											{
												if (NavFilterArea->bOverrideTravelCost)
													NavPointCost = NavFilterArea->TravelCostOverride;

												if (NavFilterArea->bOverrideEnteringCost)
													FixedAreaEnteringCost = NavFilterArea->EnteringCostOverride;
											}
										}
									}

									// If the source's nav area is different than the current nav area, then add the entering cost
									if (SourceNavArea && SourceNavArea != NavArea)
									{
										NavPointCost += FixedAreaEnteringCost;
									}

									if (NavPointCost > CheapestCost)
										continue;

									if (NavPointCost < CheapestCost)
									{
										CheapestNavPoints.Empty();
										CheapestCost = NavPointCost;
									}

									CheapestNavPoints.Emplace(NavPoint);

								}
							}
						}
					}

					zGridPoint++;

					if (zGridPoint < AxisGridSize)
						continue;

					zGridPoint = 0;
					yGridPoint++;

					if (yGridPoint < AxisGridSize)
						continue;

					yGridPoint = 0;
					xGridPoint++;

					if (xGridPoint < AxisGridSize)
						continue;

					CanIterateGridPoints = false;

				}

				//counter++;
			}
		}

		kBlock++;

		if (kBlock <= endZ)
			continue;

		kBlock = startZ;
		jBlock++;

		if (jBlock <= endY)
			continue;

		jBlock = startY;
		iBlock++;

		if (iBlock <= endX)
			continue;
		//}

		CanIterateBlocks = false;
	}

	/*if (BlockDistances.Num() == 0)
		return Result;*/


		//BlockDistances.ValueSort([](const float& A, const float& B) { return A < B; });


		// TODO: Need something here that finds the closest available NavBlocks if no blocks found above

		// Let's find the closest Nav Point to the location

		//double CheapestNavPointCost = -1;

	if (CheapestNavPoints.Num() > 0)
	{
		RandomLocation = CheapestNavPoints[FMath::RandRange(0, CheapestNavPoints.Num() - 1)]->WorldLocation;
		return true;
	}

	//// If we found a random point then let's return it
	//if (CheapestRandomNavPoint)
	//	Result = *CheapestRandomNavPoint->WorldLocation;

	// If we didn't find a random point and no nav point were processed and we're using tether
	// then let's find the closest point to the tether we can to get this AI back home.
	else if (!ValidNavPointsProcessed && UsingTether)
	{
		auto ClosestNavPointToTether = FindClosestNavPointToLocation(TetherPoint, Query.StartLocation, TetherPoint, false, Query.MinAngleFromGround, Query.MaxAngleFromGround, true, false);
		if (ClosestNavPointToTether)
		{
			RandomLocation = ClosestNavPointToTether->WorldLocation;
			return true;
		}
	}

	// *************************************
	// *************************************


	return false;
}

inline bool USurfaceNavigationSystem::FindRandomNavPointNearby_RandPath(FSurfaceNavPathFindingQueryData& Query, FVector& RandomLocation, const float SearchRadius, const FVector TetherPoint)
{
	//auto StartTime = FPlatformTime::Seconds();

	if (bIsReadyToPathfind)
	{
		auto StartNavPoint = FindClosestNavPointToLocation(Query.StartLocation, Query.StartLocation, Query.NonOffsetStartLocation, true, Query.MinAngleFromGround, Query.MaxAngleFromGround, Query.AcceptanceRadius);

		const bool UsingTether = !TetherPoint.IsZero();
		const auto& SourceLoc = UsingTether ? TetherPoint : Query.StartLocation;

		// Get a random location in the range of the search radius
		FVector EndPoint(
			SourceLoc.X + FMath::FRandRange(-SearchRadius, SearchRadius),
			SourceLoc.Y + FMath::FRandRange(-SearchRadius, SearchRadius),
			SourceLoc.Z + FMath::FRandRange(-SearchRadius, SearchRadius));


		// We're passing in the start location for the second parameter so that the blocks searched will
		// be around the source to ensure the path is for loaded blocks around the source.
		// This will prevent it attempting to path across gaps in the navigation.
		auto EndNavPoint = FindClosestNavPointToLocation(EndPoint, Query.StartLocation, EndPoint, false, Query.MinAngleFromGround, Query.MaxAngleFromGround, Query.AcceptanceRadius, true /* wide search */, false /* require acceptance radius */);

		if (StartNavPoint && EndNavPoint)
		{
			// *************************************
			// Let's see if any of the immediate neighbors of the start and end navpoints are cheaper cost and
			// if so then use those cheaper cost nav points.
			// TODO: This may not be a good idea with a larger grid point distance as they could be too far away from each other.
			// *************************************

			const UNavigationQueryFilter* FilterClass = Query.FilterClass ? Query.FilterClass.GetDefaultObject() : nullptr;

			// Let's do the start nav point first
			StartNavPoint = DetermineCheapestNavPoint(StartNavPoint, Query.MinAngleFromGround, Query.MaxAngleFromGround, FilterClass); // , CheapestNavPoint);

			// Now do the same for the end nav point
			EndNavPoint = DetermineCheapestNavPoint(EndNavPoint, Query.MinAngleFromGround, Query.MaxAngleFromGround, FilterClass); // , CheapestNavPoint);

			PathNode* FinalNode = nullptr;
			DeterminePath(StartNavPoint, EndNavPoint, Query, FinalNode);

			if (FinalNode)
			{
				RandomLocation = FinalNode->NavPoint->WorldLocation;
				return true;
			}

		}
	}

	return false;
}

inline bool USurfaceNavigationSystem::FindRandomNavPointNearby_SimpleRandPoint(FSurfaceNavPathFindingQueryData& Query, FVector& RandomLocation, const float SearchRadius, const FVector TetherPoint)
{
	if (bIsReadyToPathfind && bSurfaceNavGenerationEnabled)
	{
		const bool UsingTether = !TetherPoint.IsZero();
		const auto& SourceLoc = UsingTether ? TetherPoint : Query.StartLocation;

		// Get a random location in the range of the search radius
		RandomLocation = FVector(
			SourceLoc.X + FMath::FRandRange(-SearchRadius, SearchRadius),
			SourceLoc.Y + FMath::FRandRange(-SearchRadius, SearchRadius),
			SourceLoc.Z + FMath::FRandRange(-SearchRadius, SearchRadius));

		return true;
	}

	return false;
}

bool USurfaceNavigationSystem::FindRandomNavPointNearby(FSurfaceNavPathFindingQueryData& Query, FVector& RandomLocation, const float SearchRadius, const FVector TetherPoint)
{
	if (!IsReady())
		return false;

	return FindRandomNavPointNearby_SimpleRandPoint(Query, RandomLocation, SearchRadius, TetherPoint);
}

bool USurfaceNavigationSystem::K2_SurfaceNavFindRandomNavPointNearby(UObject* WorldContextObject, FVector& RandomLocation, const float Radius, const FVector Origin)
{
	bool bResult = false;
	RandomLocation = Origin;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(World);
	if (NavSys)
	{
		FSurfaceNavPathFindingQueryData Query;
		Query.StartLocation = Origin;
		if (NavSys->FindRandomNavPointNearby(Query, RandomLocation, Radius, Origin))
		{
			bResult = true;
		}
	}

	return bResult;
}

bool USurfaceNavigationSystem::GetRandomReachableSurfaceNavPointNearby(FSurfaceNavPathFindingQueryData Query, FVector& RandomLocation, const float SearchRadius)
{
	bool bResult = false;

	if (FindRandomNavPointNearby(Query, RandomLocation, SearchRadius))
	{
		FSurfaceNavPathResult PathResult = FindPath(Query);
		if (PathResult.IsSuccessful())
		{
			RandomLocation = PathResult.Path->GetEndLocation();
			if (RandomLocation != FNavigationSystem::InvalidLocation)
				bResult = true;
		}
	}

	return bResult;
}

void USurfaceNavigationSystem::DrawPath(TArray<FVector>& PathPoints)
{
#if WITH_EDITOR

	int LastIndex = PathPoints.Num() - 1;
	int CurIndex = 0;
	for (auto& PathPoint : PathPoints)
	{
		DrawDebugPoint
		(
			GetWorld(),
			PathPoint,
			CurIndex == 0 || CurIndex == LastIndex ? 25.f : 15.f,
			CurIndex == 0 ? FColor::Green : CurIndex == LastIndex ? FColor::Red : FColor::Purple,
			false,
			1.f
		);

		CurIndex++;
	}
#endif
}

#pragma endregion

PathNode::PathNode(TSharedPtr<FSurfaceNavPointBase> InitNavPoint) : NavPoint(InitNavPoint)
{

}

ENavigationQueryResult::Type USurfaceNavigationSystem::GetSurfaceNavPathLength(UObject* WorldContextObject,
	const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, TSubclassOf<UNavigationQueryFilter> FilterClass,
	const float InMinAngleLimit, const float InMaxAngleLimit, const float InAcceptanceRadius, const int InMaxPathingIterations)
{
	float PathLength = 0.f;

	// Make sure we're not outside the allowed angle limit range
	float NewMinAngleLimit = FMath::Clamp(InMinAngleLimit, 0.f, 180.f);
	float NewMaxAngleLimit = FMath::Clamp(InMaxAngleLimit, 0.f, 180.f);

	// Make sure the max is actually higher
	NewMaxAngleLimit = FMath::Max(NewMinAngleLimit, NewMaxAngleLimit);

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	USurfaceNavigationSystem* NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(World);
	if (NavSys)
	{
		TSharedPtr<FSurfaceNavPointBase> StartNavPoint = nullptr;
		TSharedPtr<FSurfaceNavPointBase> EndNavPoint = nullptr;

		// Let's try to get these locations projected onto the navigation with wide search enabled.
		bool StartPointSuccess = NavSys->ProjectPoint(PathStart, PathStart, PathStart, false, StartNavPoint, InMinAngleLimit, InMaxAngleLimit, InAcceptanceRadius, true, false);
		bool EndPointSuccess = NavSys->ProjectPoint(PathEnd, PathEnd, PathEnd, false, EndNavPoint, InMinAngleLimit, InMaxAngleLimit, InAcceptanceRadius, true, false);

		// Can only continue if we found nearby navigation data
		if (StartPointSuccess && EndPointSuccess)
		{
			// Just need the locations for the next part
			FVector NewPathStart = StartNavPoint->WorldLocation;
			FVector NewPathEnd = EndNavPoint->WorldLocation;

			// Build the query object to use for pathing
			FSurfaceNavPathFindingQueryData Query(WorldContextObject, NewPathStart, NewPathEnd, NewPathStart, NewPathEnd, FilterClass, NULL,
				0, false, FLT_MAX, InMinAngleLimit, InMaxAngleLimit, InAcceptanceRadius, InMaxPathingIterations, true);

			// Since we want the lenght of the complete path, don't allow partials
			Query.SetAllowPartialPaths(false);

			return NavSys->GetSurfaceNavPathLength(OutPathLength, Query);
		}
	}

	return ENavigationQueryResult::Error;
}

ENavigationQueryResult::Type USurfaceNavigationSystem::GetSurfaceNavPathLength(float& OutPathLength,
	FSurfaceNavPathFindingQueryData& QueryData)
{
	FSurfaceNavPathResult Result = FindPath(QueryData);
	OutPathLength = (Result.IsSuccessful()) ? Result.Path->GetLength() : BIG_NUMBER;

	return Result.Result;
}


#pragma region NAV LINK COMPONENT

void USurfaceNavLinkComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	auto NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	if (NavSys)
	{
		NavSys->RegisterSurfaceNavLinkComponent(this);
	}
}

void USurfaceNavLinkComponent::Deactivate()
{
	auto NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	if (NavSys)
	{
		NavSys->UnregisterSurfaceNavLinkComponent(this);
	}

	Super::Deactivate();
}

void USurfaceNavLinkComponent::DestroyComponent(bool bPromoteChildren)
{
	auto NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	if (NavSys)
	{
		NavSys->UnregisterSurfaceNavLinkComponent(this);
	}

	Super::DestroyComponent(bPromoteChildren);
}

void USurfaceNavLinkComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	auto NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	if (NavSys)
	{
		NavSys->UnregisterSurfaceNavLinkComponent(this);
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void USurfaceNavLinkComponent::OnRegister()
{
	Super::OnRegister();

	auto NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	if (NavSys)
	{
		NavSys->RegisterSurfaceNavLinkComponent(this);
	}
}

void USurfaceNavLinkComponent::OnUnregister()
{
	auto NavSys = FNavigationSystem::GetCurrent<USurfaceNavigationSystem>(GetWorld());

	if (NavSys)
	{
		NavSys->UnregisterSurfaceNavLinkComponent(this);
	}

	Super::OnUnregister();
}

#pragma endregion
