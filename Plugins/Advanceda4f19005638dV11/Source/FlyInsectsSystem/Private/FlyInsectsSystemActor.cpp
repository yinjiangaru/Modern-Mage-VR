// Copyright 2024 BitProtectStudio. All Rights Reserved.

#include "FlyInsectsSystemActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/Core/Public/HAL/RunnableThread.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine.h"

AFlyInsectsSystemActor::AFlyInsectsSystemActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	RootComponent = SphereComponent;
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SphereComponent->SetGenerateOverlapEvents(false);

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetupAttachment(RootComponent);
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	BoxComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	BoxComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Overlap);
	BoxComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);

	BoxComponent->SetBoxExtent(FVector(1000.f, 1000.f, 1000.f), true);

	StaticMeshInstanceComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StaticMeshInstanceComponent"));
	StaticMeshInstanceComponent->SetupAttachment(RootComponent);
	StaticMeshInstanceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshInstanceComponent->SetGenerateOverlapEvents(false);
}

FlyInsectsThread::FlyInsectsThread(AActor* newActor)
{
	m_Kill = false;
	m_Pause = false;

	//Initialize FEvent (as a cross platform (Confirmed Mac/Windows))
	m_semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);

	EThreadPriority threadPriority_;

	switch (flyInsectParametersTHR.threadPriority)
	{
	case EPriorityFlyInsect::Normal:
		threadPriority_ = TPri_Normal;
		break;
	case EPriorityFlyInsect::Highest:
		threadPriority_ = TPri_Highest;
		break;
	case EPriorityFlyInsect::Lowest:
		threadPriority_ = TPri_Lowest;
		break;
	case EPriorityFlyInsect::AboveNormal:
		threadPriority_ = TPri_AboveNormal;
		break;
	case EPriorityFlyInsect::BelowNormal:
		threadPriority_ = TPri_BelowNormal;
		break;
	case EPriorityFlyInsect::SlightlyBelowNormal:
		threadPriority_ = TPri_SlightlyBelowNormal;
		break;
	case EPriorityFlyInsect::TimeCritical:
		threadPriority_ = TPri_TimeCritical;
		break;

	default:
		threadPriority_ = TPri_AboveNormal;
		break;
	}

	ownerActor = newActor;

	Thread = FRunnableThread::Create(this, (TEXT("%s_FSomeRunnable"), *newActor->GetName()), 0, threadPriority_);
}

FlyInsectsThread::~FlyInsectsThread()
{
	if (m_semaphore)
	{
		//Cleanup the FEvent
		FGenericPlatformProcess::ReturnSynchEventToPool(m_semaphore);
		m_semaphore = nullptr;
	}

	if (Thread)
	{
		//Cleanup the worker thread
		delete Thread;
		Thread = nullptr;
	}
}

void FlyInsectsThread::EnsureCompletion()
{
	Stop();

	if (Thread)
	{
		Thread->WaitForCompletion();
	}
}

void FlyInsectsThread::PauseThread()
{
	m_Pause = true;
}

void FlyInsectsThread::ContinueThread()
{
	m_Pause = false;

	if (m_semaphore)
	{
		//Here is a FEvent signal "Trigger()" -> it will wake up the thread.
		m_semaphore->Trigger();
	}
}

bool FlyInsectsThread::Init()
{
	return true;
}

uint32 FlyInsectsThread::Run()
{
	//Initial wait before starting
	FPlatformProcess::Sleep(FMath::RandRange(0.05f, 0.5f));

	threadDeltaTime = 0.f;

	while (!m_Kill)
	{
		if (m_Pause)
		{
			//FEvent->Wait(); will "sleep" the thread until it will get a signal "Trigger()"
			m_semaphore->Wait();

			if (m_Kill)
			{
				return 0;
			}
		}
		else
		{
			// threadDeltaTime = FTimespan(FPlatformTime::Cycles64() - lastFrameTime).GetTotalSeconds();
			// lastFrameTime = FPlatformTime::Cycles64();

			// Fixing motion judder.
			threadDeltaTime = ownerActor->GetWorld()->GetTimeSeconds() - lastFrameTime;
			lastFrameTime = ownerActor->GetWorld()->GetTimeSeconds();
			
			//TArray<FlyInsectData> flyInsectsArr_ = flyInsectsThreadMembersArr;

			TArray<FlyInsectData> flyInsectsArr_ = flyInsectsThreadMembersArr;

			for (int32 flyInsectID_ = 0; flyInsectID_ < flyInsectsArr_.Num(); ++flyInsectID_)
			{
				bool bIsAvoidance_(false);
				FlyInsectData& flyInsect_ = flyInsectsArr_[flyInsectID_];

				FVector const flyInsectLocation_ = flyInsect_.transform.GetLocation();

				FVector followVec_ = FVector::ZeroVector;
				FVector cohesionVec_ = FVector::ZeroVector;
				FVector alignmentVec_ = FVector::ZeroVector;
				FVector separationVec_ = FVector::ZeroVector;
				FVector fleeVec_ = FVector::ZeroVector;
				FVector newVelocity_ = FVector::ZeroVector;
				bool tryFindGroundIdlePoint_ = false;

				FHitResult traceHitRes_(ForceInit);

				if (!flyInsectParametersTHR.bManualLand)
				{
					// Calculate idle time.
					if (!flyInsect_.bIsIdleNow)
					{
						flyInsect_.actionTime += threadDeltaTime;

						if (flyInsect_.actionTime > flyInsect_.maxActionTime)
						{
							flyInsect_.bIsCanIdle = true;
						}

						// If fly insect cant fine idle point.
						if (flyInsect_.bIsCanIdle)
						{
							flyInsect_.findIdlePointTime += threadDeltaTime;

							if (flyInsect_.findIdlePointTime >= flyInsectParametersTHR.maxTimeFindIdlePoint)
							{
								tryFindGroundIdlePoint_ = true;
							}
						}
					}
					else
					{
						flyInsect_.idleTime += threadDeltaTime;

						if (flyInsect_.idleTime > flyInsect_.maxIdleTime)
						{
							ResetIdleTime(flyInsect_);
						}
					}
				}

				// Find idle point.
				if (flyInsect_.bIsCanIdle && flyInsect_.idlePoint == FVector::ZeroVector || flyInsectParametersTHR.bManualLand)
				{
					//	FVector startV_ = flyInsectLocation_;
					//  FVector endV_ = UKismetMathLibrary::GetForwardVector(flyInsect_.transform.Rotator()) * 50.f + startV_;
					FVector endV_ = flyInsect_.transform.GetUnitAxis(EAxis::X);
					//       endV_.Normalize();
					endV_ = endV_ * flyInsectParametersTHR.checkDistanceFindIdlePoint + flyInsectLocation_;

					traceHitRes_ = CheckTrace(flyInsectLocation_, endV_);

					if (traceHitRes_.bBlockingHit && traceHitRes_.Location != FVector::ZeroVector)
					{
						// If use Terrarium can idle only in BoxComponent
						if (flyInsectParametersTHR.bUseTerrarium)
						{
							if (UKismetMathLibrary::IsPointInBox(traceHitRes_.Location, BoxComponentRef->GetComponentLocation(), BoxComponentRef->GetScaledBoxExtent()))
							{
								flyInsect_.idlePoint = traceHitRes_.Normal * 2.f + traceHitRes_.Location; //traceHitRes_.Location;
								flyInsect_.idleNormal = traceHitRes_.Normal;

								// Set rand action time on next action time.
								flyInsect_.maxActionTime = flyInsectParametersTHR.maxActionTime + FMath::RandRange(-flyInsectParametersTHR.actionTimeOffset, flyInsectParametersTHR.actionTimeOffset);

								// Reset action time.
								flyInsect_.actionTime = 0.f;
							}
						}
						else
						{
							flyInsect_.idlePoint = traceHitRes_.Location;
							flyInsect_.idleNormal = traceHitRes_.Normal;

							// Set rand action time on next action time.
							flyInsect_.maxActionTime = flyInsectParametersTHR.maxActionTime + FMath::RandRange(-flyInsectParametersTHR.actionTimeOffset, flyInsectParametersTHR.actionTimeOffset);

							// Reset action time.
							flyInsect_.actionTime = 0.f;
						}
					}
				}

				if (flyInsectParametersTHR.bIdleInsectsEqualNearby)
				{
					// Follow to nearby.
					// Other forces need nearby insect
					TArray<int32> mates_ = GetNearbyInsects(flyInsectID_);

					if (flyInsectParametersTHR.cohesionScale > 0.0f)
					{
						// Cohesion - staying near nearby insect
						cohesionVec_ = SteeringCohesion(flyInsect_, mates_) * flyInsectParametersTHR.cohesionScale;
					}

					if (flyInsectParametersTHR.alignScale > 0.0f)
					{
						// Alignment =  aligning with the heading of nearby insect
						alignmentVec_ = SteeringAlign(flyInsect_, mates_) * flyInsectParametersTHR.alignScale;
					}

					if (flyInsectParametersTHR.separationScale > 0.0f)
					{
						// Separation = trying to not get too close to insect
						separationVec_ = SteeringSeparate(flyInsect_, mates_) * flyInsectParametersTHR.separationScale;
					}
				}
				else if (!flyInsect_.bIsCanIdle)
				{
					// Follow to nearby.
					// Other forces need nearby insect
					TArray<int32> mates_ = GetNearbyInsects(flyInsectID_);

					if (flyInsectParametersTHR.cohesionScale > 0.0f)
					{
						// Cohesion - staying near nearby insect
						cohesionVec_ = SteeringCohesion(flyInsect_, mates_) * flyInsectParametersTHR.cohesionScale;
					}

					if (flyInsectParametersTHR.alignScale > 0.0f)
					{
						// Alignment =  aligning with the heading of nearby insect
						alignmentVec_ = SteeringAlign(flyInsect_, mates_) * flyInsectParametersTHR.alignScale;
					}

					if (flyInsectParametersTHR.separationScale > 0.0f)
					{
						// Separation = trying to not get too close to insect
						separationVec_ = SteeringSeparate(flyInsect_, mates_) * flyInsectParametersTHR.separationScale;
					}
				}

				// Flee = running away from enemies!
				if (flyInsectParametersTHR.fleeScale > 0.0f && !flyInsectParametersTHR.bFollowToPawn)
				{
					fleeVec_ = SteeringFlee(flyInsect_) * flyInsectParametersTHR.fleeScale;
					if (fleeVec_ != FVector::ZeroVector)
					{
						bIsAvoidance_ = true;
						ResetIdleTime(flyInsect_);
					}
				}

				if (flyInsect_.bIsCanIdle && flyInsectParametersTHR.bForceGroundIdlePoint || tryFindGroundIdlePoint_)
				{
					cohesionVec_ = SteeringGround(flyInsect_) * flyInsectParametersTHR.fleeScaleTerrarium;
				}

				// Follow to pawn
				if (flyInsectParametersTHR.bFollowToPawn)
				{
					followVec_ = SteeringFollowPawn(flyInsect_);
					if (followVec_ != FVector::ZeroVector)
					{
						if (flyInsectParametersTHR.bAttackLikeSwarm)
						{
							bIsAvoidance_ = true;
						}
						ResetIdleTime(flyInsect_);
					}
				}

				// Flee = running away from primitive object collision from root component!
				if (flyInsectParametersTHR.fleeScaleAvoidance > 0.f && allOverlappingComponentsArrTHR.Num() > 0 && flyInsectParametersTHR.bAutoAddComponentsInArray)
				{
					FVector avoidVec_ = SteeringAvoidanceComponent(flyInsect_, flyInsectLocation_) * flyInsectParametersTHR.fleeScaleAvoidance;

					if (avoidVec_ != FVector::ZeroVector)
					{
						bIsAvoidance_ = true;
						fleeVec_ = avoidVec_;
					}
				}

				// Avoidance Aquarium. 
				if (flyInsectParametersTHR.fleeScaleTerrarium > 0.f && flyInsectParametersTHR.bUseTerrarium)
				{
					if (!UKismetMathLibrary::IsPointInBox(flyInsectLocation_, BoxComponentRef->GetComponentLocation(), BoxComponentRef->GetScaledBoxExtent()))
					{
						// Flee = running away to Terrarium center.
						fleeVec_ = SteeringTerrarium(flyInsect_) * flyInsectParametersTHR.fleeScaleTerrarium;
						if (fleeVec_ != FVector::ZeroVector)
						{
							bIsAvoidance_ = true;
						}
					}
				}

				// Fly around insectWanderInRandomRadius
				if (!flyInsectParametersTHR.bUseTerrarium)
				{
					if ((flyInsectLocation_ - BoxComponentRef->GetComponentLocation()).Size() > flyInsectParametersTHR.insectWanderInRandomRadius)
					{
						// Flee = running away to Terrarium center.
						fleeVec_ = SteeringCenter(flyInsect_) * flyInsectParametersTHR.fleeScaleTerrarium;
						if (fleeVec_ != FVector::ZeroVector)
						{
							bIsAvoidance_ = true;
						}
					}
				}

				// newVelocity_ += fleeVec_;

				if (followVec_ != FVector::ZeroVector)
				{
					newVelocity_ += followVec_;
					newVelocity_ += separationVec_;
					if (flyInsectParametersTHR.bFollowWithCohesion)
					{
						newVelocity_ += cohesionVec_;
					}
					if (flyInsectParametersTHR.bFollowWithAlignment)
					{
						newVelocity_ += alignmentVec_;
					}
				}
				else if (fleeVec_ != FVector::ZeroVector)
				{
					newVelocity_ += fleeVec_;
				}
				else
				{
					newVelocity_ += separationVec_;
					newVelocity_ += cohesionVec_;
					newVelocity_ += alignmentVec_;
				}


				// Truncate the new force calculated in newVelocity so we don't go crazy
				newVelocity_ = newVelocity_.GetClampedToSize(0.0f, flyInsectParametersTHR.insectMaxSteeringForce);

				FVector targetVelocity_ = flyInsect_.velocity + newVelocity_;

				float insectRotRate_(flyInsectParametersTHR.insectMateRotationRate);
				if (bIsAvoidance_)
				{
					insectRotRate_ = flyInsectParametersTHR.escapeMateRotationRate;
				}

				// Set move velocity.
				FVector forward_ = flyInsect_.transform.GetUnitAxis(EAxis::X);
				forward_.Normalize();
				flyInsect_.velocity = forward_ * targetVelocity_.Size();

				// Clamp our new velocity to be within min->max speeds
				if (flyInsect_.velocity.Size() > flyInsectParametersTHR.insectMaxSpeed)
				{
					flyInsect_.velocity = flyInsect_.velocity.GetSafeNormal() * FMath::RandRange(
						flyInsectParametersTHR.insectMaxSpeed - flyInsectParametersTHR.insectOffsetSpeed,
						flyInsectParametersTHR.insectMaxSpeed + flyInsectParametersTHR.insectOffsetSpeed);
				}

				// If need escape from danger actor.
				if (bIsAvoidance_)
				{
					flyInsect_.velocity = flyInsect_.velocity * flyInsectParametersTHR.escapeMaxSpeedMultiply;
				}


				if (flyInsect_.idlePoint == FVector::ZeroVector)
				{
					// Move insect.
					FVector setSpeed_ = flyInsectLocation_ + flyInsect_.velocity;
					flyInsect_.transform.SetLocation(setSpeed_);

					// Rotate the insect towards the velocity direction vector
					FRotator rot = FRotationMatrix::MakeFromX((flyInsectLocation_ + targetVelocity_) - flyInsectLocation_).Rotator();
					FRotator final = FMath::RInterpTo(flyInsect_.transform.Rotator(), rot, threadDeltaTime, insectRotRate_);
					flyInsect_.transform.SetRotation(final.Quaternion());

					flyInsect_.bIsIdleNow = false;
				}
				else
				{
					// Move to idle point
					flyInsect_.transform.SetLocation(flyInsect_.idlePoint);

					// Rotate to normal idle point
					FRotator runRotate_ = RotationFromNormal(UKismetMathLibrary::GetForwardVector(flyInsect_.transform.Rotator()), flyInsect_.idleNormal);
					flyInsect_.transform.SetRotation(runRotate_.Quaternion());

					flyInsect_.bIsIdleNow = true;
				}


				// Save all parameters
				//	flyInsectsArr_[flyInsectID_] = flyInsect_;
			}


			if (flyInsectParametersTHR.ExperimentalOptimization)
			{
				threadDeltaTime += threadSleepTime;
			}

			//Critical section:
			m_mutex.Lock();
			//We are locking our FCriticalSection so no other thread will access it
			//And thus it is a thread-safe access now

			flyInsectsThreadMembersArr = flyInsectsArr_;

			//Unlock FCriticalSection so other threads may use it.
			m_mutex.Unlock();

			//Pause Condition - if we RandomVectors contains more vectors than m_amount we shall pause the thread to release system resources.
			m_Pause = true;

			if (flyInsectParametersTHR.ExperimentalOptimization)
			{
				//A little sleep between the chunks (So CPU will rest a bit -- (may be omitted))
				FPlatformProcess::Sleep(threadSleepTime);
			}
		}
	}
	return 0;
}

void FlyInsectsThread::Stop()
{
	m_Kill = true; //Thread kill condition "while (!m_Kill){...}"
	m_Pause = false;

	if (m_semaphore)
	{
		//We shall signal "Trigger" the FEvent (in case the Thread is sleeping it shall wake up!!)
		m_semaphore->Trigger();
	}
}

bool FlyInsectsThread::IsThreadPaused() const
{
	return (bool)m_Pause;
}

TArray<FlyInsectData> FlyInsectsThread::GetFlyInsectsData()
{
	m_mutex.Lock();

	TArray<FlyInsectData> actualArray_ = flyInsectsThreadMembersArr;

	// Clear attacked actors in thread
	if (flyInsectParametersTHR.bCanAttackPawn)
	{
		for (int i = 0; i < flyInsectsThreadMembersArr.Num(); ++i)
		{
			flyInsectsThreadMembersArr[i].attackedActors.Empty();
		}
	}

	this->ContinueThread();

	m_mutex.Unlock();

	return actualArray_;
}

void FlyInsectsThread::InitFlyInsectsParameters(TArray<FlyInsectData> setFlyInsectsArr, FlyInsectsParameters newParameters, UBoxComponent* SetBoxComponent)
{
	flyInsectParametersTHR = newParameters;
	BoxComponentRef = SetBoxComponent;
	flyInsectsThreadMembersArr = setFlyInsectsArr;
}

void FlyInsectsThread::SetInsectManualLandTHR(bool setManualLand)
{
	m_mutex.Lock();

	flyInsectParametersTHR.bManualLand = setManualLand;

	this->ContinueThread();

	m_mutex.Unlock();
}

void FlyInsectsThread::SetOverlappingComponents(TArray<UPrimitiveComponent*> overlappingComponentsArr, TArray<AActor*> dangerActors)
{
	allOverlappingComponentsArrTHR = overlappingComponentsArr;
	dangerActorsTHR = dangerActors;
}

void AFlyInsectsSystemActor::BeginPlay()
{
	Super::BeginPlay();

	StaticMeshInstanceComponent->SetWorldScale3D(FVector(1.f, 1.f, 1.f));

	StaticMeshInstanceComponent->SetStaticMesh(StaticMesh);

	flyInsectsParameters.insectMateAwarenessRadiusSquare = FMath::Square(flyInsectsParameters.insectMateAwarenessRadius);

	FVector const startLoc_(GetActorLocation());

	for (int i = 0; i < flyInsectsInstances; ++i)
	{
		FVector const direction_(FMath::VRand());
		FRotator const rotation_(UKismetMathLibrary::RandomRotator());
		float const distance_(FMath::RandRange(0.01f, SphereComponent->GetScaledSphereRadius()));
		FVector const newLoc_(startLoc_ + direction_ * distance_);
		float const randScale_(FMath::RandRange(minMeshScale, maxMeshScale));

		AddInsectsWorldSpace(FTransform(rotation_, newLoc_, FVector(randScale_, randScale_, randScale_)));
	}

	if (maxSounds > 0 && insectSound != nullptr && bEnableSound)
	{
		for (int i = 0; i < maxSounds; ++i)
		{
			UAudioComponent* newAudioComp_ = NewObject<UAudioComponent>(this, UAudioComponent::StaticClass(), TEXT("DynamicSpawnedUAudioComponent"));
			if (newAudioComp_)
			{
				newAudioComp_->RegisterComponent();
				newAudioComp_->SetSound(insectSound);
				newAudioComp_->Play();
				allAudioComponentsArr.Add(newAudioComp_);
			}
		}
	}

	if (flyInsectsParameters.bAutoAddComponentsInArray || flyInsectsParameters.bReactOnPawn)
	{
		GetWorldTimerManager().SetTimer(addAvoidanceActor_Timer, this, &AFlyInsectsSystemActor::AddAvoidanceComponentsTimer, 1.f, true, 0.5f);
	}
	DivideFlyInsectsArrayForThreads();

	for (int i = 0; i < flyInsectsActorPoolThreadArr.Num(); i++)
	{
		flyInsectsActorPoolThreadArr[i] = nullptr;
	}
	GenerateFlyInsectsThread();
}

void AFlyInsectsSystemActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int i = 0; i < flyInsectsActorPoolThreadArr.Num(); i++)
	{
		if (flyInsectsActorPoolThreadArr[i])
		{
			flyInsectsActorPoolThreadArr[i]->EnsureCompletion();
			delete flyInsectsActorPoolThreadArr[i];
			flyInsectsActorPoolThreadArr[i] = nullptr;
		}
	}
	flyInsectsActorPoolThreadArr.Empty();

	Super::EndPlay(EndPlayReason);
}

void AFlyInsectsSystemActor::BeginDestroy()
{
	for (int i = 0; i < flyInsectsActorPoolThreadArr.Num(); i++)
	{
		if (flyInsectsActorPoolThreadArr[i])
		{
			flyInsectsActorPoolThreadArr[i]->EnsureCompletion();
			delete flyInsectsActorPoolThreadArr[i];
			flyInsectsActorPoolThreadArr[i] = nullptr;
		}
	}
	flyInsectsActorPoolThreadArr.Empty();

	Super::BeginDestroy();
}

void AFlyInsectsSystemActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsPaused)
	{
		return;
	}

	if (!StaticMeshInstanceComponent) return;

	TArray<FlyInsectData> flyInsectsDataArr_;

	for (int i = 0; i < flyInsectsActorPoolThreadArr.Num(); i++)
	{
		if (flyInsectsActorPoolThreadArr[i])
		{
			flyInsectsDataArr_.Append(flyInsectsActorPoolThreadArr[i]->GetFlyInsectsData());
		}
	}

	bool aliveInstance_(false);

	TArray<FFlyInsectsAudio> allInsectsAudioArr_;

	// Move insects. 
	for (int32 insectID_ = 0; insectID_ < flyInsectsDataArr_.Num(); ++insectID_)
	{
		if (!mFlyInsectsData[insectID_].bIsDead)
		{
			aliveInstance_ = true;


			if (flyInsectsParameters.bCanAttackPawn)
			{
				for (int attackedID_ = 0; attackedID_ < flyInsectsDataArr_[insectID_].attackedActors.Num(); ++attackedID_)
				{
					UGameplayStatics::ApplyDamage(flyInsectsDataArr_[insectID_].attackedActors[attackedID_], flyInsectsParameters.damageValue,
					                              nullptr, this, flyInsectsParameters.damageType);
				}
			}

			// Interpolate Vector and Rotate
			mFlyInsectsData[insectID_].transform.SetLocation(FMath::VInterpConstantTo(mFlyInsectsData[insectID_].transform.GetLocation(),
			                                                                          flyInsectsDataArr_[insectID_].transform.GetLocation(), DeltaTime, interpMoveAnimRate));

			if (bSmoothRotation)
			{
				mFlyInsectsData[insectID_].transform.SetRotation(FMath::RInterpTo(mFlyInsectsData[insectID_].transform.Rotator(),
				                                                                  flyInsectsDataArr_[insectID_].transform.Rotator(), DeltaTime, interpRotateAnimRate).Quaternion());
			}
			else
			{
				mFlyInsectsData[insectID_].transform.SetRotation(FQuat(flyInsectsDataArr_[insectID_].transform.Rotator()));
			}


			StaticMeshInstanceComponent->UpdateInstanceTransform(mFlyInsectsData[insectID_].instanceIndex, mFlyInsectsData[insectID_].transform, true, false);

			if (maxSounds > 0 && insectSound != nullptr && bEnableSound)
			{
				APlayerCameraManager* playerCam_ = UGameplayStatics::GetPlayerCameraManager(this, 0);
				if (playerCam_)
				{
					FVector camLoc_ = playerCam_->GetCameraLocation();

					FFlyInsectsAudio insectsAudio_;
					insectsAudio_.instanceIndex = insectID_;
					insectsAudio_.insectDistance = (camLoc_ - mFlyInsectsData[insectID_].transform.GetLocation()).SizeSquared();
					allInsectsAudioArr_.Add(insectsAudio_);
				}
			}
		}
	}

	if (maxSounds > allInsectsAudioArr_.Num() && bEnableSound)
	{
		maxSounds = allInsectsAudioArr_.Num();
	}

	if (maxSounds > 0 && insectSound != nullptr && bEnableSound)
	{
		TArray<float> allDistanceToInsectsArr_;
		for (int i = 0; i < allInsectsAudioArr_.Num(); ++i)
		{
			allDistanceToInsectsArr_.Add(allInsectsAudioArr_[i].insectDistance);
		}
		for (int i = 0; i < maxSounds - 1; ++i)
		{
			int index_;
			float minVal_;
			UKismetMathLibrary::MinOfFloatArray(allDistanceToInsectsArr_, index_, minVal_);

			allAudioComponentsArr[i]->SetWorldLocation(mFlyInsectsData[allInsectsAudioArr_[index_].instanceIndex].transform.GetLocation());

			if (bIsDebugSound)
			{
				DrawDebugSphere(GetWorld(), mFlyInsectsData[allInsectsAudioArr_[index_].instanceIndex].transform.GetLocation(),
				                30.f, 8, FColor::Green, false, 0.f, 0, 1.f);

				DrawDebugString(GetWorld(), mFlyInsectsData[allInsectsAudioArr_[index_].instanceIndex].transform.GetLocation(), TEXT("Sound is here!"),
				                nullptr, FColor::Orange, 0, true, 1);
			}

			allDistanceToInsectsArr_.RemoveAt(index_);
			allInsectsAudioArr_.RemoveAt(index_);
		}
	}

	// If all instance death destroy actor.
	if (!aliveInstance_)
	{
		for (int i = 0; i < flyInsectsActorPoolThreadArr.Num(); i++)
		{
			flyInsectsActorPoolThreadArr[i]->Stop();
		}
		Destroy();
	}

	StaticMeshInstanceComponent->MarkRenderStateDirty();
}

void AFlyInsectsSystemActor::GenerateFlyInsectsThread()
{
	if (maxUseThreads > flyInsectsInstances)
	{
		flyInsectsActorPoolThreadArr.Add(new FlyInsectsThread(this));
		flyInsectsActorPoolThreadArr[0]->InitFlyInsectsParameters(allFlyInsectsArrays[0].flyInsectsArr, flyInsectsParameters, BoxComponent);
		flyInsectsActorPoolThreadArr[0]->SetPoolThread(flyInsectsActorPoolThreadArr);
		return;
	}
	for (int i = 0; i < maxUseThreads; i++)
	{
		flyInsectsActorPoolThreadArr.Add(new FlyInsectsThread(this));
		flyInsectsActorPoolThreadArr[i]->InitFlyInsectsParameters(allFlyInsectsArrays[i].flyInsectsArr, flyInsectsParameters, BoxComponent);
	}

	// Set pool thread for control insect from first thread.
	for (int i = 0; i < flyInsectsActorPoolThreadArr.Num(); i++)
	{
		flyInsectsActorPoolThreadArr[i]->SetPoolThread(flyInsectsActorPoolThreadArr);
	}
}

void AFlyInsectsSystemActor::AddAvoidanceComponentsTimer()
{
	if (bIsPaused)
	{
		return;
	}

	if (flyInsectsParameters.bAutoAddComponentsInArray)
	{
		allOverlappingComponentsArr.Empty();
		dangerActors.Empty();

		TArray<UPrimitiveComponent*> overlappingComponentsArr_;

		BoxComponent->GetOverlappingComponents(overlappingComponentsArr_);

		for (int i = 0; i < overlappingComponentsArr_.Num(); i++)
		{
			APawn* checkPawn_ = Cast<APawn>(overlappingComponentsArr_[i]->GetOwner());
			if (checkPawn_)
			{
				if (flyInsectsParameters.bReactOnPawn)
				{
					dangerActors.AddUnique(checkPawn_);
				}
			}
			else
			{
				allOverlappingComponentsArr.Add(overlappingComponentsArr_[i]);
			}
		}
	}

	for (int i = 0; i < flyInsectsActorPoolThreadArr.Num(); i++)
	{
		if (flyInsectsActorPoolThreadArr[i])
		{
			flyInsectsActorPoolThreadArr[i]->SetOverlappingComponents(allOverlappingComponentsArr, dangerActors);
		}
	}
}

FVector FlyInsectsThread::SteeringTerrarium(const FlyInsectData& insect) const
{
	FRotator const rotationToCenter_(UKismetMathLibrary::FindLookAtRotation(insect.transform.GetLocation(), BoxComponentRef->GetComponentLocation()));
	FVector direction_ = UKismetMathLibrary::Conv_RotatorToVector(rotationToCenter_);
	direction_.Normalize();
	FVector newVec_ = direction_ * ((flyInsectParametersTHR.insectEnemyAwarenessRadius / flyInsectParametersTHR.strengthTerrariumOffsetValue) * flyInsectParametersTHR.fleeScaleTerrarium);
	return newVec_;
}

FVector FlyInsectsThread::SteeringCenter(const FlyInsectData& insect) const
{
	FRotator const rotationToCenter_(UKismetMathLibrary::FindLookAtRotation(insect.transform.GetLocation(), BoxComponentRef->GetComponentLocation()));
	FVector direction_ = UKismetMathLibrary::Conv_RotatorToVector(rotationToCenter_);
	direction_.Normalize();
	FVector newVec_ = direction_ * ((flyInsectParametersTHR.insectEnemyAwarenessRadius / flyInsectParametersTHR.strengthTerrariumOffsetValue) * flyInsectParametersTHR.fleeScaleTerrarium);
	return newVec_;
}

FVector FlyInsectsThread::SteeringGround(const FlyInsectData& insect) const
{
	FVector targetV_ = insect.transform.GetLocation();
	targetV_.Z -= 300.f;
	FRotator const rotationToGround_(UKismetMathLibrary::FindLookAtRotation(insect.transform.GetLocation(), targetV_));
	FVector direction_ = UKismetMathLibrary::Conv_RotatorToVector(rotationToGround_);
	direction_.Normalize();
	FVector newVec_ = direction_ * ((flyInsectParametersTHR.insectEnemyAwarenessRadius / flyInsectParametersTHR.strengthTerrariumOffsetValue) * flyInsectParametersTHR.fleeScaleTerrarium);
	return newVec_;
}

FVector FlyInsectsThread::SteeringAvoidanceComponent(const FlyInsectData& insect, const FVector& insectLocation) const
{
	FVector newVec_(FVector::ZeroVector);

	for (int i = 0; i < allOverlappingComponentsArrTHR.Num(); ++i)
	{
		// Check Low Level.
		if (flyInsectParametersTHR.bLowLevelValid)
		{
			if (allOverlappingComponentsArrTHR[i]->IsValidLowLevel() && IsValid(allOverlappingComponentsArrTHR[i]))
			{
				UPrimitiveComponent* primComp_(Cast<UPrimitiveComponent>(allOverlappingComponentsArrTHR[i]));
				if (primComp_)
				{
					FVector closestPoint_;
					primComp_->GetClosestPointOnCollision(insectLocation, closestPoint_);

					if ((closestPoint_ - insectLocation).Size() < flyInsectParametersTHR.avoidancePrimitiveDistance + insect.velocity.Size())
					{
						FRotator const rotationToCenter_(UKismetMathLibrary::FindLookAtRotation(closestPoint_, insectLocation));
						FVector direction_ = UKismetMathLibrary::Conv_RotatorToVector(rotationToCenter_);
						direction_.Normalize();

						newVec_ = direction_ * ((flyInsectParametersTHR.insectEnemyAwarenessRadius / flyInsectParametersTHR.strengthTerrariumOffsetValue) * flyInsectParametersTHR.fleeScaleTerrarium);
					}
				}
			}
		}
		else
		{
			if (IsValid(allOverlappingComponentsArrTHR[i]))
			{
				UPrimitiveComponent* primComp_(Cast<UPrimitiveComponent>(allOverlappingComponentsArrTHR[i]));
				if (primComp_)
				{
					FVector closestPoint_;
					primComp_->GetClosestPointOnCollision(insectLocation, closestPoint_);

					if ((closestPoint_ - insectLocation).Size() < flyInsectParametersTHR.avoidancePrimitiveDistance + insect.velocity.Size())
					{
						FRotator const rotationToCenter_(UKismetMathLibrary::FindLookAtRotation(closestPoint_, insectLocation));
						FVector direction_ = UKismetMathLibrary::Conv_RotatorToVector(rotationToCenter_);
						direction_.Normalize();

						newVec_ = direction_ * ((flyInsectParametersTHR.insectEnemyAwarenessRadius / flyInsectParametersTHR.strengthTerrariumOffsetValue) * flyInsectParametersTHR.fleeScaleTerrarium);
					}
				}
			}
		}
	}

	return newVec_;
}

FVector FlyInsectsThread::SteeringAlign(FlyInsectData& insect, TArray<int32>& insectMates)
{
	FVector vel_ = FVector(0, 0, 0);
	if (insectMates.Num() == 0) return vel_;

	for (int32 i = 0; i < insectMates.Num(); i++)
	{
		vel_ += flyInsectsThreadMembersArr[insectMates[i]].velocity;
	}
	vel_ /= insectMates.Num();

	return vel_;
}

FVector FlyInsectsThread::SteeringSeparate(const FlyInsectData& insect, TArray<int32>& insectMates)
{
	FVector force_ = FVector(0, 0, 0);
	if (insectMates.Num() == 0) return force_;

	for (int32 i = 0; i < insectMates.Num(); i++)
	{
		FVector diff_ = insect.transform.GetLocation() - flyInsectsThreadMembersArr[insectMates[i]].transform.GetLocation();
		float const scale_ = diff_.Size();
		diff_.Normalize();
		diff_ = diff_ * (flyInsectParametersTHR.separationRadius / scale_);
		force_ += diff_;
	}
	return force_;
}

FVector FlyInsectsThread::SteeringCohesion(const FlyInsectData& insect, TArray<int32>& insectMates)
{
	FVector avgPos_ = FVector(0, 0, 0);
	if (insectMates.Num() == 0) return avgPos_;

	for (int32 i = 0; i < insectMates.Num(); i++)
	{
		avgPos_ += flyInsectsThreadMembersArr[insectMates[i]].transform.GetLocation();
	}

	avgPos_ /= insectMates.Num();

	return avgPos_ - insect.transform.GetLocation();
}

FVector FlyInsectsThread::SteeringFlee(const FlyInsectData& insect)
{
	FVector newVec_ = FVector(0, 0, 0);

	for (int i = 0; i < dangerActorsTHR.Num(); ++i)
	{
		if (dangerActorsTHR[i])
		{
			// calculate flee from this threat
			FVector fromEnemy_ = insect.transform.GetLocation() - dangerActorsTHR[i]->GetActorLocation();
			float const distanceToEnemy_ = fromEnemy_.Size();
			fromEnemy_.Normalize();

			// enemy inside our enemy awareness threshold, so evade them
			if (distanceToEnemy_ < flyInsectParametersTHR.insectEnemyAwarenessRadius)
			{
				newVec_ += fromEnemy_ * ((flyInsectParametersTHR.insectEnemyAwarenessRadius / distanceToEnemy_) * flyInsectParametersTHR.fleeScale);
			}
		}
	}
	return newVec_;
}

FVector FlyInsectsThread::SteeringFollowPawn(FlyInsectData& insect)
{
	FVector newVec_ = FVector(0, 0, 0);

	for (int i = 0; i < dangerActorsTHR.Num(); ++i)
	{
		if (dangerActorsTHR[i])
		{
			// calculate flee from this threat
			FVector fromEnemy_ = dangerActorsTHR[i]->GetActorLocation() - insect.transform.GetLocation();
			float distanceToEnemy_ = fromEnemy_.Size();

			// enemy inside our enemy awareness threshold, so evade them
			if (distanceToEnemy_ < flyInsectParametersTHR.insectFollowPawnAwarenessRadius)
			{
				// Add attacked actors in array.
				if (flyInsectParametersTHR.bCanAttackPawn)
				{
					if ((dangerActorsTHR[i]->GetActorLocation() - insect.transform.GetLocation()).SizeSquared() < flyInsectParametersTHR.attackRadiusSquared)
					{
						insect.attackedActors.AddUnique(dangerActorsTHR[i]);

						// calculate flee from this threat
						fromEnemy_ = insect.transform.GetLocation() - dangerActorsTHR[i]->GetActorLocation();
						distanceToEnemy_ = fromEnemy_.Size();
						fromEnemy_.Normalize();

						newVec_ = fromEnemy_ * ((flyInsectParametersTHR.insectEnemyAwarenessRadius / distanceToEnemy_) * flyInsectParametersTHR.fleeScale);
					}
					else
					{
						fromEnemy_.Normalize();

						newVec_ += fromEnemy_ * ((flyInsectParametersTHR.insectFollowPawnAwarenessRadius / distanceToEnemy_) * flyInsectParametersTHR.followScale);
					}
				}
			}
		}
	}
	return newVec_;
}

void FlyInsectsThread::SetPoolThread(TArray<FlyInsectsThread*> setPoolThreadArr)
{
	PoolThreadArr = setPoolThreadArr;
}

FHitResult FlyInsectsThread::CheckTrace(const FVector& startV, FVector& endV) const
{
	FCollisionQueryParams traceParams(TEXT("TraceCheck"), false, ownerActor);
	//	traceParams.bTraceAsyncScene = true;
	traceParams.bReturnPhysicalMaterial = false;

	// Not tracing complex uses the rough collision instead making tiny objects easier to select.
	traceParams.bTraceComplex = flyInsectParametersTHR.bUseComplexTraceFindIdlePoint;

	FHitResult hitTrace(ForceInit);
	ownerActor->GetWorld()->LineTraceSingleByChannel(hitTrace, startV, endV, ECollisionChannel::ECC_Visibility, traceParams);

	return hitTrace;
}

void AFlyInsectsSystemActor::DivideFlyInsectsArrayForThreads()
{
	TArray<FlyInsectData> tempInsectsData_;
	FSwarmFlyInsectsArrays newInsectData_;

	if (maxUseThreads > flyInsectsInstances)
	{
		newInsectData_.AddData(0, tempInsectsData_);
		allFlyInsectsArrays.Add(newInsectData_);
		allFlyInsectsArrays[0].flyInsectsArr = mFlyInsectsData;
		return;
	}

	int insectID_(0);
	int insectsPart_ = mFlyInsectsData.Num() / maxUseThreads;
	int const insectsPartIncrement_ = insectsPart_;

	for (int i = 0; i < maxUseThreads; i++)
	{
		for (; insectID_ < insectsPart_; insectID_++)
		{
			tempInsectsData_.Add(mFlyInsectsData[insectID_]);
		}

		newInsectData_.AddData(i, tempInsectsData_);
		allFlyInsectsArrays.Add(newInsectData_);
		tempInsectsData_.Empty();

		insectsPart_ += insectsPartIncrement_;
	}

	// Adding the remainder of the division.
	if (insectID_ < mFlyInsectsData.Num() - 1 && insectID_ == (insectsPartIncrement_ * maxUseThreads))
	{
		for (; insectID_ < mFlyInsectsData.Num(); insectID_++)
		{
			allFlyInsectsArrays[maxUseThreads - 1].flyInsectsArr.Add(mFlyInsectsData[insectID_]);
		}
	}
}

void AFlyInsectsSystemActor::SetInsectManualLand(bool setManualLand)
{
	bManualLand = setManualLand;

	for (int i = 0; i < flyInsectsActorPoolThreadArr.Num(); ++i)
	{
		flyInsectsActorPoolThreadArr[i]->SetInsectManualLandTHR(bManualLand);
	}
}


void AFlyInsectsSystemActor::AddInsectsWorldSpace(const FTransform& worldTransform)
{
	StaticMeshInstanceComponent->AddInstance(worldTransform, true);

	FlyInsectData flyInsect_;
	flyInsect_.instanceIndex = numInsect;
	flyInsect_.transform = worldTransform;

	// Set rand idle time and action time.
	flyInsect_.maxIdleTime = flyInsectsParameters.maxIdleTime + FMath::RandRange(-flyInsectsParameters.idleTimeOffset, flyInsectsParameters.idleTimeOffset);
	flyInsect_.maxActionTime = flyInsectsParameters.maxActionTime + FMath::RandRange(-flyInsectsParameters.actionTimeOffset,
	                                                                                 flyInsectsParameters.actionTimeOffset);

	mFlyInsectsData.Add(flyInsect_);
	numInsect++;
}

TArray<FTransform> AFlyInsectsSystemActor::ApplyDamage_BP(FVector setDamagePoint, float setDamageRadius)
{
	TArray<FTransform> deadTransformArr_;

	if (bIsPaused)
	{
		return deadTransformArr_;
	}

	for (int i = 0; i < mFlyInsectsData.Num(); ++i)
	{
		if (!mFlyInsectsData[i].bIsDead)
		{
			if ((mFlyInsectsData[i].transform.GetLocation() - setDamagePoint).Size() <= setDamageRadius)
			{
				deadTransformArr_.Add(mFlyInsectsData[i].transform);
				mFlyInsectsData[i].transform.SetScale3D(FVector::ZeroVector);
				StaticMeshInstanceComponent->UpdateInstanceTransform(mFlyInsectsData[i].instanceIndex, mFlyInsectsData[i].transform, true, false);
				mFlyInsectsData[i].bIsDead = true;
			}
		}
	}
	StaticMeshInstanceComponent->MarkRenderStateDirty();
	return deadTransformArr_;
}

bool AFlyInsectsSystemActor::GetIsInsectsOnPause()
{
	return bIsPaused;
}

void AFlyInsectsSystemActor::SetPause(bool bSetPause, bool bClearInstance)
{
	bIsPaused = bSetPause;

	for (int i = 0; i < flyInsectsActorPoolThreadArr.Num(); ++i)
	{
		if (flyInsectsActorPoolThreadArr[i])
		{
			if (bIsPaused)
			{
				if (!flyInsectsActorPoolThreadArr[i]->IsThreadPaused())
				{
					flyInsectsActorPoolThreadArr[i]->PauseThread();
				}
			}
			else
			{
				if (flyInsectsActorPoolThreadArr[i]->IsThreadPaused())
				{
					flyInsectsActorPoolThreadArr[i]->ContinueThread();
				}
			}
		}
	}

	if (maxSounds > 0 && insectSound != nullptr && bEnableSound)
	{
		for (int i = 0; i < allAudioComponentsArr.Num(); ++i)
		{
			if (bIsPaused)
			{
				allAudioComponentsArr[i]->Stop();
			}
			else
			{
				allAudioComponentsArr[i]->Play();
			}
		}
	}

	if (bIsPaused)
	{
		if (bClearInstance)
		{
			StaticMeshInstanceComponent->ClearInstances();
		}
	}
	else
	{
		if (StaticMeshInstanceComponent->GetInstanceCount() == 0)
		{
			for (int i = 0; i < mFlyInsectsData.Num(); ++i)
			{
				StaticMeshInstanceComponent->AddInstance(mFlyInsectsData[i].transform, true);
			}
		}
	}
}

TArray<int32> FlyInsectsThread::GetNearbyInsects(int32 insect)
{
	TArray<int32> mates_;
	if (insect > flyInsectsThreadMembersArr.Num()) return mates_;
	if (insect < 0) return mates_;

	for (int32 i = 0; i < flyInsectsThreadMembersArr.Num(); i++)
	{
		if (i != insect)
		{
			if (flyInsectParametersTHR.bIdleInsectsEqualNearby)
			{
				FVector diff_ = flyInsectsThreadMembersArr[i].transform.GetLocation() - flyInsectsThreadMembersArr[insect].transform.GetLocation();
				if (FMath::Abs(diff_.SizeSquared()) < flyInsectParametersTHR.insectMateAwarenessRadiusSquare)
				{
					mates_.Add(i);
				}
			}
			else
			{
				FVector diff_ = flyInsectsThreadMembersArr[i].transform.GetLocation() - flyInsectsThreadMembersArr[insect].transform.GetLocation();
				if (FMath::Abs(diff_.SizeSquared()) < flyInsectParametersTHR.insectMateAwarenessRadiusSquare)
				{
					mates_.Add(i);
				}
			}
		}
	}

	return mates_;
}

FRotator FlyInsectsThread::RotationFromNormal(const FVector& insectForward, const FVector& HitNormal)
{
	return FRotationMatrix::MakeFromZY(HitNormal, FVector::CrossProduct(HitNormal, insectForward)).Rotator();
}

void FlyInsectsThread::ResetIdleTime(FlyInsectData& setInsect) const
{
	// Set rand idle time on next idle time.
	setInsect.maxIdleTime = flyInsectParametersTHR.maxIdleTime +
		FMath::RandRange(-flyInsectParametersTHR.idleTimeOffset, flyInsectParametersTHR.idleTimeOffset);

	// Reset idle time.
	setInsect.bIsCanIdle = false;
	setInsect.idlePoint = FVector::ZeroVector;
	setInsect.idleTime = 0.f;

	setInsect.findIdlePointTime = 0.f;
}
