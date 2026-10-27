// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "SAnimationScrubPanel_COPY.h"
#include "Widgets/SBoxPanel.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimBlueprint.h"
#include "AnimPreviewInstance.h"
#include "SScrubControlPanel.h"
#include "ScopedTransaction.h"
#include "Animation/BlendSpace.h"
#include "AnimationEditorPreviewScene_COPY.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequenceHelpers.h"

#define LOCTEXT_NAMESPACE "AnimationScrubPanel"

void SAnimationScrubPanel_COPY::Construct( const SAnimationScrubPanel_COPY::FArguments& InArgs, const TSharedRef<IPersonaPreviewScene>& InPreviewScene)
{
	bSliderBeingDragged = false;
	LockedSequence = InArgs._LockedSequence;
	OnSetInputViewRange = InArgs._OnSetInputViewRange;

	PreviewScenePtr = InPreviewScene;

	this->ChildSlot
	[
		SNew(SHorizontalBox)
		.AddMetaData<FTagMetaData>(TEXT("AnimScrub.Scrub"))
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Fill) 
		.VAlign(VAlign_Center)
		.FillWidth(1)
		.Padding(0.0f)
		[
			SAssignNew(ScrubControlPanel, SScrubControlPanel)
			.IsEnabled(true)//this, &SAnimationScrubPanel_COPY::DoesSyncViewport)
			.Value(this, &SAnimationScrubPanel_COPY::GetScrubValue)
			.NumOfKeys(this, &SAnimationScrubPanel_COPY::GetNumberOfKeys)
			.SequenceLength(this, &SAnimationScrubPanel_COPY::GetSequenceLength)
			.DisplayDrag(this, &SAnimationScrubPanel_COPY::GetDisplayDrag)
			.OnValueChanged(this, &SAnimationScrubPanel_COPY::OnValueChanged)
			.OnBeginSliderMovement(this, &SAnimationScrubPanel_COPY::OnBeginSliderMovement)
			.OnEndSliderMovement(this, &SAnimationScrubPanel_COPY::OnEndSliderMovement)
			.OnClickedForwardPlay(this, &SAnimationScrubPanel_COPY::OnClick_Forward)
			.OnClickedForwardStep(this, &SAnimationScrubPanel_COPY::OnClick_Forward_Step)
			.OnClickedForwardEnd(this, &SAnimationScrubPanel_COPY::OnClick_Forward_End)
			.OnClickedBackwardPlay(this, &SAnimationScrubPanel_COPY::OnClick_Backward)
			.OnClickedBackwardStep(this, &SAnimationScrubPanel_COPY::OnClick_Backward_Step)
			.OnClickedBackwardEnd(this, &SAnimationScrubPanel_COPY::OnClick_Backward_End)
			.OnClickedToggleLoop(this, &SAnimationScrubPanel_COPY::OnClick_ToggleLoop)
			.OnClickedRecord(this, &SAnimationScrubPanel_COPY::OnClick_Record)
			.OnGetLooping(this, &SAnimationScrubPanel_COPY::IsLoopStatusOn)
			.OnGetPlaybackMode(this, &SAnimationScrubPanel_COPY::GetPlaybackMode)
			.OnGetRecording(this, &SAnimationScrubPanel_COPY::IsRecording)
			.ViewInputMin(InArgs._ViewInputMin)
			.ViewInputMax(InArgs._ViewInputMax)
			.bDisplayAnimScrubBarEditing(InArgs._bDisplayAnimScrubBarEditing)
			.OnSetInputViewRange(InArgs._OnSetInputViewRange)
			.OnCropAnimSequence( this, &SAnimationScrubPanel_COPY::OnCropAnimSequence )
			.OnAddAnimSequence( this, &SAnimationScrubPanel_COPY::OnInsertAnimSequence )
			.OnAppendAnimSequence(this, &SAnimationScrubPanel_COPY::OnAppendAnimSequence )
			.OnReZeroAnimSequence( this, &SAnimationScrubPanel_COPY::OnReZeroAnimSequence )
			.bAllowZoom(InArgs._bAllowZoom)
			.IsRealtimeStreamingMode(this, &SAnimationScrubPanel_COPY::IsRealtimeStreamingMode)
		]
	];
}

FReply SAnimationScrubPanel_COPY::OnClick_Forward_Step()
{
	UDebugSkelMeshComponent* SMC = GetPreviewScene()->GetPreviewMeshComponent();

	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		bool bShouldStepCloth = FMath::Abs(PreviewInstance->GetLength() - PreviewInstance->GetCurrentTime()) > SMALL_NUMBER;

		PreviewInstance->SetPlaying(false);
		PreviewInstance->StepForward();

		if(SMC && bShouldStepCloth)
		{
			SMC->bPerformSingleClothingTick = true;
		}
	}
	else if (SMC)
	{
		// BlendSpaces and Animation Blueprints combine animations so there's no such thing as a frame. However, 1/30 is a sensible/common rate.
		const float FixedFrameRate = 30.0f;

		// Advance a single frame, leaving it paused afterwards
		SMC->GlobalAnimRateScale = 1.0f;
		GetPreviewScene()->Tick(1.0f / FixedFrameRate); 
		SMC->GlobalAnimRateScale = 0.0f;
	}

	return FReply::Handled();
}

FReply SAnimationScrubPanel_COPY::OnClick_Forward_End()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(PreviewInstance->GetLength(), false);
	}

	return FReply::Handled();
}

FReply SAnimationScrubPanel_COPY::OnClick_Backward_Step()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	UDebugSkelMeshComponent* SMC = GetPreviewScene()->GetPreviewMeshComponent();
	if (PreviewInstance)
	{
		bool bShouldStepCloth = PreviewInstance->GetCurrentTime() > SMALL_NUMBER;

		PreviewInstance->SetPlaying(false);
		PreviewInstance->StepBackward();

		if(SMC && bShouldStepCloth)
		{
			SMC->bPerformSingleClothingTick = true;
		}
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel_COPY::OnClick_Backward_End()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(0.f, false);
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel_COPY::OnClick_Forward()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	UDebugSkelMeshComponent* SMC = GetPreviewScene()->GetPreviewMeshComponent();

	if (PreviewInstance)
	{
		bool bIsReverse = PreviewInstance->IsReverse();
		bool bIsPlaying = PreviewInstance->IsPlaying();
		// if current bIsReverse and bIsPlaying, we'd like to just turn off reverse
		if (bIsReverse && bIsPlaying)
		{
			PreviewInstance->SetReverse(false);
		}
		// already playing, simply pause
		else if (bIsPlaying) 
		{
			PreviewInstance->SetPlaying(false);
			
			if(SMC && SMC->bPauseClothingSimulationWithAnim)
			{
				SMC->SuspendClothingSimulation();
			}
		}
		// if not playing, play forward
		else 
		{
			//if we're at the end of the animation, jump back to the beginning before playing
			if ( GetScrubValue() >= GetSequenceLength() )
			{
				PreviewInstance->SetPosition(0.0f, false);
			}

			PreviewInstance->SetReverse(false);
			PreviewInstance->SetPlaying(true);

			if(SMC && SMC->bPauseClothingSimulationWithAnim)
			{
				SMC->ResumeClothingSimulation();
			}
		}
	}
	else if(SMC)
	{
		SMC->GlobalAnimRateScale = (SMC->GlobalAnimRateScale > 0.0f) ? 0.0f : 1.0f;
	}

	return FReply::Handled();
}

FReply SAnimationScrubPanel_COPY::OnClick_Backward()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		bool bIsReverse = PreviewInstance->IsReverse();
		bool bIsPlaying = PreviewInstance->IsPlaying();
		// if currently playing forward, just simply turn on reverse
		if (!bIsReverse && bIsPlaying)
		{
			PreviewInstance->SetReverse(true);
		}
		else if (bIsPlaying)
		{
			PreviewInstance->SetPlaying(false);
		}
		else
		{
			//if we're at the beginning of the animation, jump back to the end before playing
			if ( GetScrubValue() <= 0.0f )
			{
				PreviewInstance->SetPosition(GetSequenceLength(), false);
			}

			PreviewInstance->SetPlaying(true);
			PreviewInstance->SetReverse(true);
		}
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel_COPY::OnClick_ToggleLoop()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		bool bIsLooping = PreviewInstance->IsLooping();
		PreviewInstance->SetLooping(!bIsLooping);
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel_COPY::OnClick_Record()
{
	StaticCastSharedRef<FAnimationEditorPreviewScene_COPY>(GetPreviewScene())->RecordAnimation();

	return FReply::Handled();
}

bool SAnimationScrubPanel_COPY::IsLoopStatusOn() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	return (PreviewInstance && PreviewInstance->IsLooping());
}

EPlaybackMode::Type SAnimationScrubPanel_COPY::GetPlaybackMode() const
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		if (PreviewInstance->IsPlaying())
		{
			return PreviewInstance->IsReverse() ? EPlaybackMode::PlayingReverse : EPlaybackMode::PlayingForward;
		}
		return EPlaybackMode::Stopped;
	}
	else if (UDebugSkelMeshComponent* SMC = GetPreviewScene()->GetPreviewMeshComponent())
	{
		return (SMC->GlobalAnimRateScale > 0.0f) ? EPlaybackMode::PlayingForward : EPlaybackMode::Stopped;
	}
	
	return EPlaybackMode::Stopped;
}

bool SAnimationScrubPanel_COPY::IsRecording() const
{
	return StaticCastSharedRef<FAnimationEditorPreviewScene_COPY>(GetPreviewScene())->IsRecording();
}

bool SAnimationScrubPanel_COPY::IsRealtimeStreamingMode() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	return ( ! (PreviewInstance && PreviewInstance->GetCurrentAsset()) );
}

void SAnimationScrubPanel_COPY::OnValueChanged(float NewValue)
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		PreviewInstance->SetPosition(NewValue);
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			DebugData->SetSnapshotIndexByTime(Instance, NewValue);
		}
	}
}

// make sure viewport is freshes
void SAnimationScrubPanel_COPY::OnBeginSliderMovement()
{
	bSliderBeingDragged = true;

	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		PreviewInstance->SetPlaying(false);
	}
}

void SAnimationScrubPanel_COPY::OnEndSliderMovement(float NewValue)
{
	bSliderBeingDragged = false;
}

uint32 SAnimationScrubPanel_COPY::GetNumberOfKeys() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
		float Length = PreviewInstance->GetLength();
		// if anim sequence, use correct num frames
		int32 NumKeys = (int32) (Length/0.0333f); 
		if (PreviewInstance->GetCurrentAsset())
		{
			if (PreviewInstance->GetCurrentAsset()->IsA(UAnimSequenceBase::StaticClass()))
			{
				NumKeys = CastChecked<UAnimSequenceBase>(PreviewInstance->GetCurrentAsset())->GetNumberOfSampledKeys();
			}
			else if(PreviewInstance->GetCurrentAsset()->IsA(UBlendSpace::StaticClass()))
			{
				// Blendspaces dont display frame notches, so just return 0 here
				NumKeys = 0;
			}
		}
		return NumKeys;
	}
	else if (LockedSequence)
	{
		return LockedSequence->GetNumberOfSampledKeys();
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			return DebugData->GetSnapshotLengthInFrames();
		}
	}

	return 1;
}

float SAnimationScrubPanel_COPY::GetSequenceLength() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
		return PreviewInstance->GetLength();
	}
	else if (LockedSequence)
	{
		return LockedSequence->GetPlayLength();
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			return static_cast<float>(Instance->LifeTimer);
		}
	}

	return 0.f;
}

bool SAnimationScrubPanel_COPY::DoesSyncViewport() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();

	return (( LockedSequence==nullptr && PreviewInstance ) || ( LockedSequence && PreviewInstance && PreviewInstance->GetCurrentAsset() == LockedSequence ));
}

void SAnimationScrubPanel_COPY::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bSliderBeingDragged)
	{
		GetPreviewScene()->InvalidateViews();
	}
}

class UAnimSingleNodeInstance* SAnimationScrubPanel_COPY::GetPreviewInstance() const
{
	UDebugSkelMeshComponent* PreviewMeshComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewMeshComponent && PreviewMeshComponent->IsPreviewOn()? PreviewMeshComponent->PreviewInstance : nullptr;
}

float SAnimationScrubPanel_COPY::GetScrubValue() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
		if (PreviewInstance)
		{
			return PreviewInstance->GetCurrentTime(); 
		}
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			return static_cast<float>(Instance->CurrentLifeTimerScrubPosition);
		}
	}

	return 0.f;
}

void SAnimationScrubPanel_COPY::ReplaceLockedSequence(class UAnimSequenceBase* NewLockedSequence)
{
	LockedSequence = NewLockedSequence;
}

UAnimInstance* SAnimationScrubPanel_COPY::GetAnimInstanceWithBlueprint() const
{
	if (UDebugSkelMeshComponent* DebugComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		UAnimInstance* Instance = DebugComponent->GetAnimInstance();

		if ((Instance != nullptr) && (Instance->GetClass()->ClassGeneratedBy != nullptr))
		{
			return Instance;
		}
	}

	return nullptr;
}

bool SAnimationScrubPanel_COPY::GetAnimBlueprintDebugData(UAnimInstance*& Instance, FAnimBlueprintDebugData*& DebugInfo) const
{
	Instance = GetAnimInstanceWithBlueprint();

	if (Instance != nullptr)
	{
		// Avoid updating the instance if we're replaying the past
		if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(Instance->GetClass()))
		{
			if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
			{
				if (Blueprint->GetObjectBeingDebugged() == Instance)
				{
					DebugInfo = &(AnimBlueprintClass->GetAnimBlueprintDebugData());
					return true;
				}
			}
		}
	}

	return false;
}

void SAnimationScrubPanel_COPY::OnCropAnimSequence( bool bFromStart, float CurrentTime )
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance)
	{
		float Length = PreviewInstance->GetLength();
		if (PreviewInstance->GetCurrentAsset())
		{
			UAnimSequence* AnimSequence = Cast<UAnimSequence>( PreviewInstance->GetCurrentAsset() );
			if( AnimSequence )
			{
				const FScopedTransaction Transaction( LOCTEXT("CropAnimSequence", "Crop Animation Sequence") );

				//Call modify to restore slider position
				PreviewInstance->Modify();

				//Call modify to restore anim sequence current state
				AnimSequence->Modify();


				const float TrimStart = bFromStart ? 0.f : CurrentTime;
				const float TrimEnd = bFromStart ? CurrentTime : AnimSequence->GetPlayLength();

				// Trim off the user-selected part of the raw anim data.
				UE::Anim::AnimationData::Trim(AnimSequence, TrimStart, TrimEnd);

				//Resetting slider position to the first frame
				PreviewInstance->SetPosition( 0.0f, false );

				OnSetInputViewRange.ExecuteIfBound(0, AnimSequence->GetPlayLength());
			}
		}
	}
}

void SAnimationScrubPanel_COPY::OnAppendAnimSequence( bool bFromStart, int32 NumOfFrames )
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance && PreviewInstance->GetCurrentAsset())
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(PreviewInstance->GetCurrentAsset());
		if(AnimSequence)
		{
			const FScopedTransaction Transaction(LOCTEXT("InsertAnimSequence", "Insert Animation Sequence"));

			//Call modify to restore slider position
			PreviewInstance->Modify();

			//Call modify to restore anim sequence current state
			AnimSequence->Modify();

			// Crop the raw anim data.
			int32 StartFrame = (bFromStart)? 0 : AnimSequence->GetDataModel()->GetNumberOfFrames() - 1;
			int32 EndFrame = StartFrame + NumOfFrames;
			int32 CopyFrame = StartFrame;
			UE::Anim::AnimationData::DuplicateKeys(AnimSequence, StartFrame, NumOfFrames, CopyFrame);

			OnSetInputViewRange.ExecuteIfBound(0, AnimSequence->GetPlayLength());
		}
	}
}

void SAnimationScrubPanel_COPY::OnInsertAnimSequence( bool bBefore, int32 CurrentFrame )
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance && PreviewInstance->GetCurrentAsset())
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(PreviewInstance->GetCurrentAsset());
		if(AnimSequence)
		{
			const FScopedTransaction Transaction(LOCTEXT("InsertAnimSequence", "Insert Animation Sequence"));

			//Call modify to restore slider position
			PreviewInstance->Modify();

			//Call modify to restore anim sequence current state
			AnimSequence->Modify();

			// Duplicate specified key
			const int32 StartFrame = (bBefore)? CurrentFrame : CurrentFrame + 1;
			UE::Anim::AnimationData::DuplicateKeys(AnimSequence, StartFrame, 1, CurrentFrame);

			OnSetInputViewRange.ExecuteIfBound(0, AnimSequence->GetPlayLength());
		}
	}
}

void SAnimationScrubPanel_COPY::OnReZeroAnimSequence(int32 FrameIndex)
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance)
	{
		UDebugSkelMeshComponent* PreviewSkelComp = GetPreviewScene()->GetPreviewMeshComponent();

		if (PreviewInstance->GetCurrentAsset() && PreviewSkelComp )
		{
			if(UAnimSequence* AnimSequence = Cast<UAnimSequence>( PreviewInstance->GetCurrentAsset()))
			{
				if (const USkeleton* Skeleton = AnimSequence->GetSkeleton())
				{
					const FName RootBoneName = Skeleton->GetReferenceSkeleton().GetBoneName(0);

					if(AnimSequence->GetDataModel()->IsValidBoneTrackName(RootBoneName))
					{
						TArray<FVector3f> PosKeys;
						TArray<FQuat4f> RotKeys;
						TArray<FVector3f> ScaleKeys;

						TArray<FTransform> BoneTransforms;
						AnimSequence->GetDataModel()->GetBoneTrackTransforms(RootBoneName, BoneTransforms);

						PosKeys.SetNum(BoneTransforms.Num());
						RotKeys.SetNum(BoneTransforms.Num());
						ScaleKeys.SetNum(BoneTransforms.Num());

						// Find vector that would translate current root bone location onto origin.
						FVector FrameTransform = FVector::ZeroVector;
						if (FrameIndex == INDEX_NONE)
						{
							// Use current transform
							FrameTransform = PreviewSkelComp->GetComponentSpaceTransforms()[0].GetLocation();
						}
						else if(BoneTransforms.IsValidIndex(FrameIndex))
						{
							// Use transform at frame
							FrameTransform = BoneTransforms[FrameIndex].GetLocation();
						}

						FVector ApplyTranslation = -1.f * FrameTransform;

						// Convert into world space
						const FVector WorldApplyTranslation = PreviewSkelComp->GetComponentTransform().TransformVector(ApplyTranslation);
						ApplyTranslation = PreviewSkelComp->GetComponentTransform().InverseTransformVector(WorldApplyTranslation);

						for(int32 KeyIndex = 0; KeyIndex < BoneTransforms.Num(); KeyIndex++)
						{
							PosKeys[KeyIndex] = FVector3f(BoneTransforms[KeyIndex].GetLocation() + ApplyTranslation);
							RotKeys[KeyIndex] = FQuat4f(BoneTransforms[KeyIndex].GetRotation());
							ScaleKeys[KeyIndex] = FVector3f(BoneTransforms[KeyIndex].GetScale3D());
						}

						IAnimationDataController& Controller = AnimSequence->GetController();
						Controller.SetBoneTrackKeys(RootBoneName, PosKeys, RotKeys, ScaleKeys);
					}
				}
			}
		}
	}
}

bool SAnimationScrubPanel_COPY::GetDisplayDrag() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance && PreviewInstance->GetCurrentAsset())
	{
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
