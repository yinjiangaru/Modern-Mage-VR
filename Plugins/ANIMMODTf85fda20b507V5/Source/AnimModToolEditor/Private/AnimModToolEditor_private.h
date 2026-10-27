// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "UObject/ObjectMacros.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimSequence.h"
#include "AnimModToolEditor_private.generated.h"

enum class EAlphaBlendOption : uint8;

//--------------------------------------------------------------------
// UAnimSequenceCustom
//--------------------------------------------------------------------

UCLASS()
class UAnimSequenceCustom : public UAnimSequence
{
	GENERATED_UCLASS_BODY()
};

//--------------------------------------------------------------------
// FRepeatFrame
//--------------------------------------------------------------------

USTRUCT()
struct FRepeatFrame
{
	GENERATED_BODY()

public:

	UPROPERTY()
	int32 FrameNum;

	UPROPERTY()
	int32 RepeatNum;
};

//--------------------------------------------------------------------
// UFloatCurveWrapper
//--------------------------------------------------------------------

UCLASS()
class UAnimModToolConfig : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY()
	uint8 bReverse : 1;
	
	UPROPERTY()
	uint8 bReverseClearRootBoneZeroFrameOffset : 1;
	
	UPROPERTY()
	uint8 bMirror : 1;
	
	UPROPERTY()
	uint8 bRepeatFrames : 1;

	UPROPERTY()
	uint8 bMakeLoopedDynamic : 1;
	
	UPROPERTY()
	uint8 bRemoveFrames : 1;
	
	UPROPERTY()
	uint8 bBlendWithOther : 1;
	
	UPROPERTY()
	uint8 bUseStaticFrameFromTarget : 1;
	
	UPROPERTY()
	uint8 bInsertAsNewFrames : 1;
	
	UPROPERTY()
	uint8 bCurveChanged : 1;
	
	UPROPERTY()
	uint8 bTimeManipulation : 1;
	
	UPROPERTY()
	uint8 bHandPoses : 1;
	
	UPROPERTY()
	uint8 bHandPoses_Left : 1;
	
	UPROPERTY()
	uint8 bHandPoses_Right : 1;

	UPROPERTY()
	uint8 HandPoses_UE4 : 1;

	UPROPERTY()
	TMap<FGuid, int32> RepeatFramesKeys;
	
	UPROPERTY()
	TMap<FGuid, int32> RepeatFramesValues;

	UPROPERTY()
	int32 MakeLoopedDynamicStartBlendFramesNum;

	UPROPERTY()
	int32 MakeLoopedDynamicEndBlendFramesNum;

	UPROPERTY()
	float MakeLoopedDynamicBalance;

	UPROPERTY()
	EAlphaBlendOption MakeLoopedDynamicBlendOption;

	UPROPERTY()
	FName MakeLoopedDynamicBoneName;

	UPROPERTY()
	int32 RemoveFramesStartFrame;
	
	UPROPERTY()
	int32 RemoveFramesFramesNum;

	UPROPERTY()
	TMap<FGuid, FName> Blend_StartBones;
	
	UPROPERTY()
	TMap<FGuid, FName> Blend_EndBones;

	UPROPERTY()
	int32 Blend_StartFrame;
	
	UPROPERTY()
	int32 Blend_Target_StartFrame;
	
	UPROPERTY()
	int32 Blend_Target_FramesNum;
	
	UPROPERTY()
	int32 Blend_AFramesNum;
	
	UPROPERTY()
	int32 Blend_BFramesNum;
	
	UPROPERTY()
	TObjectPtr<UAnimSequence> Blend_Target;
	
	UPROPERTY()
	EAlphaBlendOption Blend_Target_ABlendOption;
	
	UPROPERTY()
	EAlphaBlendOption Blend_Target_BBlendOption;
	
	UPROPERTY()
	int32 HandPoses_PoseFrame = 0;
	
	UPROPERTY()
	int32 HandPoses_StartFrame;
	
	UPROPERTY()
	int32 HandPoses_FramesNum;
	
	UPROPERTY()
	int32 HandPoses_StartBlend_FramesNum;
	
	UPROPERTY()
	EAlphaBlendOption HandPoses_StartBlend_BlendOption;
	
	UPROPERTY()
	int32 HandPoses_EndBlend_FramesNum;
	
	UPROPERTY()
	EAlphaBlendOption HandPoses_EndBlend_BlendOption;

	UPROPERTY()
	FFloatCurve TimeManipulationFloatCurve;
};