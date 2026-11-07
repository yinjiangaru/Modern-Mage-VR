// Copyright BMK Games LLC. All Rights Reserved.

//#pragma once

//#include "CoreMinimal.h"
//#include "UObject/ObjectMacros.h"
//#include "UObject/Object.h"
//#include "Engine/EngineTypes.h"
//#include "AITypes.h"
//#include "Navigation/PathFollowingComponent.h"
//#include "Blueprint/AIAsyncTaskBlueprintProxy.h"
//#include "SurfaceNavAIAsyncTaskBlueprintProxy.generated.h"
//
//class ASurfaceNavAIController;
//
////DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSurfaceNavAISimpleDelegate, EPathFollowingResult::Type, MovementResult);
//
//UCLASS(MinimalAPI)
//class USurfaceNavAIAsyncTaskBlueprintProxy : public UAIAsyncTaskBlueprintProxy
//{
//	GENERATED_UCLASS_BODY()
//
//		/*UPROPERTY(BlueprintAssignable)
//		FOSurfaceNavAISimpleDelegate OnSurfaceNavSuccess;
//
//	UPROPERTY(BlueprintAssignable)
//		FOSurfaceNavAISimpleDelegate OnSurfaceNavFail;*/
//
//public:
//	UFUNCTION()
//		void OnSurfaceNavMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type MovementResult);
//
//	void OnSurfaceNavNoPath();
//	void OnSurfaceNavAtGoal();
//
//	//~ Begin UObject Interface
//	virtual void BeginDestroy() override;
//	//~ End UObject Interface
//
//	TWeakObjectPtr<ASurfaceNavAIController> AIController;
//	FAIRequestID MoveRequestId;
//	TWeakObjectPtr<UWorld> MyWorld;
//
//	FTimerHandle TimerHandle_OnInstantFinish;
//};
