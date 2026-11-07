// Copyright 2024 BitProtectStudio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Runtime/Core/Public/HAL/Runnable.h"
#include "Components/AudioComponent.h"
#include "FlyInsectsSystemActor.generated.h"

USTRUCT(BlueprintType)
struct FFlyInsectsAudio
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    int instanceIndex = 0;

    UPROPERTY()
    float insectDistance = 0.f;

    FFlyInsectsAudio()
    {
    };
};

USTRUCT(BlueprintType)
struct FlyInsectData
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    int instanceIndex = 0;

    UPROPERTY()
    FVector velocity = FVector::ZeroVector;

    UPROPERTY()
    FTransform transform;

    UPROPERTY()
    bool bIsCanIdle = false;
    UPROPERTY()
    bool bIsIdleNow = false;

    UPROPERTY()
    float actionTime = 0.f;
    UPROPERTY()
    float maxActionTime = 0.f;

    UPROPERTY()
    float idleTime = 0.f;
    UPROPERTY()
    float maxIdleTime = 0.f;
    UPROPERTY()
    float findIdlePointTime = 0.f;
    UPROPERTY()
    TArray<AActor*> attackedActors;

    UPROPERTY()
    bool bIsDead = false;

    UPROPERTY()
    FVector idlePoint = FVector::ZeroVector;

    UPROPERTY()
    FVector idleNormal = FVector::ZeroVector;
    
    FlyInsectData()
    {
    };
};

USTRUCT(BlueprintType)
struct FSwarmFlyInsectsArrays
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    int instanceIndex;

    UPROPERTY()
    TArray<FlyInsectData> flyInsectsArr;

    FSwarmFlyInsectsArrays()
    {
        instanceIndex = 0;
        flyInsectsArr.Empty();
    };

    void AddData(int index, TArray<FlyInsectData> newInsectData)
    {
        instanceIndex = index;
        flyInsectsArr = newInsectData;
    };
};

UENUM(BlueprintType)
enum class EPriorityFlyInsect: uint8
{
    Normal UMETA(DisplayName = "Normal"),
    AboveNormal UMETA(DisplayName = "AboveNormal"),
    BelowNormal UMETA(DisplayName = "BelowNormal"),
    Highest UMETA(DisplayName = "Highest"),
    Lowest UMETA(DisplayName = "Lowest"),
    SlightlyBelowNormal UMETA(DisplayName = "SlightlyBelowNormal"),
    TimeCritical UMETA(DisplayName = "TimeCritical")
};

USTRUCT(BlueprintType)
struct FlyInsectsParameters
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    EPriorityFlyInsect threadPriority = EPriorityFlyInsect::Normal; 
    // Use for only more insect!
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    bool ExperimentalOptimization = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    bool bUseTerrarium = true;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    float insectWanderInRandomRadius = 500.0f;
    // If you want add all not mobile actors for avoidance. 
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bAutoAddComponentsInArray = true;
    // Use this if used streaming level and Auto Add Components
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bLowLevelValid = false;
    // If need react on pawn.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bReactOnPawn = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bFollowToPawn = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bFollowWithCohesion = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bFollowWithAlignment = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bAttackLikeSwarm = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bCanAttackPawn = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    float followScale = 50.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    float attackRadius = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    float damageValue = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    TSubclassOf<class UDamageType> damageType;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bIdleInsectsEqualNearby = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bUseComplexTraceFindIdlePoint = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    bool bForceGroundIdlePoint = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float checkDistanceFindIdlePoint = 250.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    float moveSpeedInterpInThread = 5.f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    float maxIdleTime = 10.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    float idleTimeOffset = 3.0f;
    // If bForceGroundIdlePoint = false, and if fly insect cant find idle point. After this time fly insect fly to ground.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    float maxTimeFindIdlePoint = 30.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    float maxActionTime = 20.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    float actionTimeOffset = 10.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float insectMaxSpeed = 40.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float insectOffsetSpeed = 5.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float escapeMaxSpeedMultiply = 2.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float insectMateAwarenessRadius = 400.0f;
    float insectMateAwarenessRadiusSquare = 0.f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float alignScale = 0.4f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float cohesionScale = 0.6f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float separationScale = 5.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float fleeScale = 10.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float fleeScaleAvoidance = 10.0f;

    float avoidancePrimitiveDistance = 50.f;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float separationRadius = 6.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float insectMateRotationRate = 0.6f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float escapeMateRotationRate = 20.f;

    float insectMaxSteeringForce = 100.0f;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float insectEnemyAwarenessRadius = 200.0f;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float insectFollowPawnAwarenessRadius = 500.0f;

    float fleeScaleTerrarium = 10.0f;
    float strengthTerrariumOffsetValue = 100.f;

    bool bManualLand = false;

    UPROPERTY()
    float attackRadiusSquared = 0.f;

    FlyInsectsParameters()
    {
        attackRadiusSquared = FMath::Square(attackRadius);
    };
};

UCLASS()
class FLYINSECTSSYSTEM_API AFlyInsectsSystemActor : public AActor
{
    GENERATED_BODY()

public:


    // constructor
    AFlyInsectsSystemActor();

    // Called every frame
    virtual void Tick(float DeltaTime) override;

    //************************************************************************
    // Component                                                                  
    //************************************************************************

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Component")
    class USphereComponent* SphereComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Component")
    class UBoxComponent* BoxComponent;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Component")
    class UInstancedStaticMeshComponent* StaticMeshInstanceComponent;

    //************************************************************************

    void GenerateFlyInsectsThread();

    void AddAvoidanceComponentsTimer();

    // Add an instance to this component. Transform is given in world space. 
    void AddInsectsWorldSpace(const FTransform& worldTransform);

    UFUNCTION(BlueprintCallable, Category = "Insects System")
    TArray<FTransform> ApplyDamage_BP(FVector setDamagePoint, float setDamageRadius);

    UFUNCTION(BlueprintCallable, Category = "Insects System")
    bool GetIsInsectsOnPause();

    UFUNCTION(BlueprintCallable, Category = "Insects System")
    void SetPause(bool bSetPause, bool bClearInstance);

    void DivideFlyInsectsArrayForThreads();

    UFUNCTION(BlueprintCallable, Category = "Fly Insects Parameters")
    void SetInsectManualLand(bool bTakeOffOnlyOnCommand);

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    UStaticMesh* StaticMesh;
    // Recommended - 1 Thread = (2000 - 2500) mates. 
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn", meta=(ClampMin="1", ClampMax="32"))
    int maxUseThreads = 1;
    // Recommended - 1 Thread = (2000 - 2500) mates.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    int32 flyInsectsInstances = 1000;
    // Random mesh scale.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    float minMeshScale = 1.f;
    // Random mesh scale.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Spawn")
    float maxMeshScale = 2.f;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float interpMoveAnimRate = 200.f;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    bool bSmoothRotation = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    float interpRotateAnimRate = 25.f;
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Fly Insects Parameters")
    FlyInsectsParameters flyInsectsParameters;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Fly Insects Parameters")
    TArray<AActor*> avoidanceActorRootArr;
    UPROPERTY(BlueprintReadOnly, Category = "Advanced Fly Insects Parameters")
    TArray<FlyInsectData> mFlyInsectsData;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Insects Spawn", meta=(ClampMin="0", ClampMax="2"))
    int maxSounds = 2;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Insects Spawn")
    USoundBase* insectSound = nullptr;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Insects Spawn")
    bool bEnableSound = true;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced Insects Spawn")
    bool bIsDebugSound = false;

    bool bManualLand = false;
    
    
    int32 numInsect;

    UPROPERTY()
    TArray<UPrimitiveComponent*> allOverlappingComponentsArr;

    TArray<FSwarmFlyInsectsArrays> allFlyInsectsArrays;

    UPROPERTY()
    TArray<AActor*> dangerActors;

protected:

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void BeginDestroy() override;

    FTimerHandle addAvoidanceActor_Timer;

    UPROPERTY()
    TArray<class UAudioComponent*> allAudioComponentsArr;


private:

    TArray<class FlyInsectsThread*> flyInsectsActorPoolThreadArr;

    bool bIsPaused = false;
};

// Thread
class FlyInsectsThread : public FRunnable
{
public:

    //================================= THREAD =====================================
    
    //Constructor
    FlyInsectsThread(AActor *newActor);
    //Destructor
    virtual ~FlyInsectsThread() override;

    //Use this method to kill the thread!!
    void EnsureCompletion();
    //Pause the thread 
    void PauseThread();
    //Continue/UnPause the thread
    void ContinueThread();
    //FRunnable interface.
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    bool IsThreadPaused() const;

    //================================= FLY INSECTS =====================================
    class UBoxComponent* BoxComponentRef;

    TArray<FlyInsectData> GetFlyInsectsData();

    void InitFlyInsectsParameters(TArray<FlyInsectData> setFlyInsectsArr, FlyInsectsParameters newParameters, UBoxComponent* SetBoxComponent);

    void SetInsectManualLandTHR(bool setManualLand);

    void SetOverlappingComponents(TArray<UPrimitiveComponent*> overlappingComponentsArr, TArray<AActor*> dangerActors);

    FVector SteeringTerrarium(const FlyInsectData& insect) const;
    FVector SteeringCenter(const FlyInsectData& insect) const;

    FVector SteeringGround(const FlyInsectData& insect) const;
    
    FVector SteeringAvoidanceComponent(const FlyInsectData& insect,const FVector& insectLocation) const;
    TArray<int32> GetNearbyInsects(int32 insect);
    FVector SteeringAlign(FlyInsectData& insect, TArray<int32>& insectMates);
    FVector SteeringSeparate(const FlyInsectData& insect, TArray<int32>& insectMates);
    FVector SteeringCohesion(const FlyInsectData& insect, TArray<int32>& insectMates);
    FVector SteeringFlee(const FlyInsectData& insect);
    FVector SteeringFollowPawn(FlyInsectData& insect);

    TArray<FlyInsectData> flyInsectsThreadMembersArr;
    FlyInsectsParameters flyInsectParametersTHR;
    TArray<UPrimitiveComponent*> allOverlappingComponentsArrTHR;
    TArray<AActor*> dangerActorsTHR;
    TArray<AActor*> enemyActorsArrTHR;

    
    float threadDeltaTime = 0.f;
    float lastFrameTime = 0.f;
    //================================= FLY INSECTS =====================================

    void SetPoolThread(TArray<class FlyInsectsThread*> setPoolThreadArr);

    AActor *ownerActor;

private:

    //Thread to run the worker FRunnable on
    FRunnableThread* Thread;

    FCriticalSection m_mutex;
    FEvent* m_semaphore;

    int m_chunkCount;
    int m_amount;
    int m_MinInt;
    int m_MaxInt;

    float threadSleepTime = 0.01f;

    //As the name states those members are Thread safe
    FThreadSafeBool m_Kill;
    FThreadSafeBool m_Pause;

    TArray<class FlyInsectsThread*> PoolThreadArr;

    FHitResult CheckTrace(const FVector &startV, FVector &endV) const;

    static FRotator RotationFromNormal(const FVector& insectForward, const FVector& HitNormal);

    void ResetIdleTime(FlyInsectData& setInsect) const;
};
