// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "AnimModToolEditor.h"
#include "AnimModToolEditor_private.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"
#include "Interfaces/IPluginManager.h"
#include "Framework/Application/SlateApplication.h"
#include "TickableEditorObject.h"
#include "EditorReimportHandler.h"
#include "IPersonaToolkit.h"
#include "IPersonaPreviewScene.h"
#include "PersonaModule.h"
#include "PersonaToolMenuContext.h"
#include "PersonaTabs.h"
#include "IAnimSequenceCurveEditor.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "ContentBrowserModule.h"
#include "Factories/MirrorDataTableFactory.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequenceHelpers.h"
#include "AnimationRuntime.h"
#include "AlphaBlend.h"
#include "Generators/MovieSceneEasingCurves.h"

#include "Engine/SkeletalMeshSocket.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/STileView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SSlider.h"
#include "BoneSelectionWidget.h"
#include <ISkeletonEditorModule.h>
#include "SSequenceEditor_COPY.h"
#include "AnimSequenceTimelineCommands_COPY.h"
#include "SAnimSequenceCurveEditor_COPY.h"
#include "CurveEditor.h"
#include "Animation/AnimRootMotionProvider.h"

#define LOCTEXT_NAMESPACE "FAnimModToolEditorModule"

const int32 HAND_POSES_NUM = 60;

typedef IAnimationDataModel DataModelType;

const float slotPadding = 8.f;
const float contentPadding = 12.f;
const float internalPadding = 2.f;

const float TileSizeX = 55;
const float TileSizeY = 64;

enum class EModifierFilter
{
	SIMPLE,
	DEFERRED,
	ALL
};

void TimeManipulation(const DataModelType* Model, IAnimationDataController& Controller, const FFloatCurve* timeManipulationCurve)
{
	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	const int32 Num = Model->GetNumberOfKeys();
	BoneTransforms.Reserve(Num);

	const FFrameRate& frameRate = Model->GetFrameRate();
	const double secondsInFrame = frameRate.AsSeconds(1);

	const int32 SUBDIVISION = 100;
	const double step = secondsInFrame / SUBDIVISION;

	double newPlayLength = 0;
	TMap<int32, double> curveValues;

	for (int32 i = 0; i < Num; i++)
	{
		for (int32 j = 0; j < SUBDIVISION; j++)
		{
			const double value = timeManipulationCurve->Evaluate(secondsInFrame * i + secondsInFrame * j / SUBDIVISION);
			newPlayLength += value * step;
			curveValues.Add(SUBDIVISION * i + j, newPlayLength);
		}
	}

	const double secondsInFrameInv = 1 / secondsInFrame;
	const int32 NewNum = FMath::FloorToInt(newPlayLength * secondsInFrameInv) + 1;

	TArray<FVector> PosKeys;
	PosKeys.SetNum(NewNum);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(NewNum);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(NewNum);

	TArray<bool> SetKeys;
	SetKeys.SetNum(NewNum);
	TArray<double> SetKeysDistance;
	SetKeysDistance.SetNum(NewNum);

	for (const FName& TrackName : TrackNames)
	{
		BoneTransforms.Reset();
		Model->GetBoneTrackTransforms(TrackName, BoneTransforms);

		for (size_t i = 0; i < NewNum; i++)
		{
			SetKeys[i] = false;
			SetKeysDistance[i] = 100000;
		}

		for (size_t i = 0; i < Num; i++)
		{
			const int32 minFrame = FMath::FloorToInt(curveValues[SUBDIVISION * i] * secondsInFrameInv);
			const int32 maxFrame = FMath::FloorToInt(curveValues[SUBDIVISION * i + SUBDIVISION - 1] * secondsInFrameInv);
			const int32 j = i == Num - 1 ? maxFrame : minFrame;

			if (!SetKeys[j])
			{
				PosKeys[j] = BoneTransforms[i].GetLocation();
				RotKeys[j] = BoneTransforms[i].GetRotation();
				ScaleKeys[j] = BoneTransforms[i].GetScale3D();

				SetKeys[j] = true;
			}
		}

		int32 firstSetKey = INDEX_NONE;
		int32 prevSetKey = INDEX_NONE;

		for (size_t i = 0; i < NewNum; i++)
		{
			if (SetKeys[i])
			{
				if (firstSetKey == INDEX_NONE)
				{
					firstSetKey = i;
				}

				if (prevSetKey != INDEX_NONE && i > prevSetKey + 1)
				{
					for (size_t k = prevSetKey + 1; k < i; k++)
					{
						const float alpha = (k - prevSetKey) * 1.0 / (i - prevSetKey);

						PosKeys[k] = FMath::Lerp(PosKeys[prevSetKey], PosKeys[i], alpha);
						RotKeys[k] = FQuat::Slerp(RotKeys[prevSetKey], RotKeys[i], alpha);
						ScaleKeys[k] = FMath::Lerp(ScaleKeys[prevSetKey], ScaleKeys[i], alpha);
					}
				}

				prevSetKey = i;
			}
		}

		if (!SetKeys[0])
		{
			for (size_t i = 0; i < firstSetKey; i++)
			{
				PosKeys[i] = PosKeys[firstSetKey];
				RotKeys[i] = RotKeys[firstSetKey];
				ScaleKeys[i] = ScaleKeys[firstSetKey];
			}
		}

		if (!SetKeys[NewNum - 1])
		{
			for (size_t i = prevSetKey + 1; i < NewNum - 1; i++)
			{
				PosKeys[i] = PosKeys[prevSetKey];
				RotKeys[i] = RotKeys[prevSetKey];
				ScaleKeys[i] = ScaleKeys[prevSetKey];
			}
		}

		Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
	}

	Controller.ResizeInFrames(NewNum, 0, NewNum - 1);
}

void SetFrames(UAnimSequence* target, const UAnimSequence* source)
{
	IAnimationDataController& Controller = target->GetController();

	DataModelType* targetModel = target->GetDataModel();

	const int32 targetNum = targetModel->GetNumberOfKeys();

	const DataModelType* sourceModel = source->GetDataModel();

	TArray<FName> TrackNames;
	sourceModel->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	const int32 Num = sourceModel->GetNumberOfKeys();
	BoneTransforms.Reserve(Num);

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num);

	for (const FName& TrackName : TrackNames)
	{
		BoneTransforms.Reset();
		sourceModel->GetBoneTrackTransforms(TrackName, BoneTransforms);


		for (int32 TransformIndex = 0; TransformIndex < Num; ++TransformIndex)
		{
			PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
			RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
			ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
		}

		Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
	}

	if (targetNum != Num)
	{
		Controller.ResizeInFrames(Num - 1, 0, Num - 1);
	}
}

void InsertFrames(const DataModelType* Model, IAnimationDataController& Controller, const int32 StartFrameIndex, const int32 NumFrames)
{
	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	const int32 Num = Model->GetNumberOfKeys();
	BoneTransforms.Reserve(Num);

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num + NumFrames);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num + NumFrames);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num + NumFrames);

	for (const FName& TrackName : TrackNames)
	{
		BoneTransforms.Reset();
		Model->GetBoneTrackTransforms(TrackName, BoneTransforms);

		for (int32 TransformIndex = 0; TransformIndex < Num + NumFrames; ++TransformIndex)
		{
			if (TransformIndex < StartFrameIndex)
			{
				PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
				RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
				ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
			}
			if (TransformIndex >= StartFrameIndex && TransformIndex < StartFrameIndex + NumFrames)
			{
				PosKeys[TransformIndex] = BoneTransforms[StartFrameIndex].GetLocation();
				RotKeys[TransformIndex] = BoneTransforms[StartFrameIndex].GetRotation();
				ScaleKeys[TransformIndex] = BoneTransforms[StartFrameIndex].GetScale3D();
			}
			else if (TransformIndex >= StartFrameIndex + NumFrames)
			{
				PosKeys[TransformIndex] = BoneTransforms[TransformIndex - NumFrames].GetLocation();
				RotKeys[TransformIndex] = BoneTransforms[TransformIndex - NumFrames].GetRotation();
				ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex - NumFrames].GetScale3D();
			}
		}

		Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
	}

	Controller.ResizeInFrames(Num + NumFrames - 1, StartFrameIndex, StartFrameIndex + NumFrames);
}

void RemoveFrames(const DataModelType* Model, IAnimationDataController& Controller, const int32 StartFrameIndex, const int32 NumFrames)
{
	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	const int32 Num = Model->GetNumberOfKeys();
	BoneTransforms.Reserve(Num);

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num - NumFrames);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num - NumFrames);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num - NumFrames);

	for (const FName& TrackName : TrackNames)
	{
		BoneTransforms.Reset();
		Model->GetBoneTrackTransforms(TrackName, BoneTransforms);

		for (int32 TransformIndex = 0; TransformIndex < Num - NumFrames; ++TransformIndex)
		{
			if (TransformIndex < StartFrameIndex)
			{
				PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
				RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
				ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
			}
			if (TransformIndex >= StartFrameIndex)
			{
				PosKeys[TransformIndex] = BoneTransforms[TransformIndex + NumFrames].GetLocation();
				RotKeys[TransformIndex] = BoneTransforms[TransformIndex + NumFrames].GetRotation();
				ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex + NumFrames].GetScale3D();
			}
		}

		Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
	}

	Controller.ResizeInFrames(Num - NumFrames - 1, StartFrameIndex, StartFrameIndex + NumFrames);
}

void ReverseFrames(const DataModelType* Model, IAnimationDataController& Controller, const int32 StartFrameIndex, const int32 NumFrames)
{
	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	const int32 Num = Model->GetNumberOfKeys();
	BoneTransforms.Reserve(Num);

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num);

	for (const FName& TrackName : TrackNames)
	{
		BoneTransforms.Reset();
		Model->GetBoneTrackTransforms(TrackName, BoneTransforms);

		for (int32 TransformIndex = 0; TransformIndex < Num; ++TransformIndex)
		{
			if (TransformIndex >= StartFrameIndex && TransformIndex <= StartFrameIndex + NumFrames / 2)
			{
				PosKeys[TransformIndex] = BoneTransforms[StartFrameIndex + NumFrames - TransformIndex].GetLocation();
				RotKeys[TransformIndex] = BoneTransforms[StartFrameIndex + NumFrames - TransformIndex].GetRotation();
				ScaleKeys[TransformIndex] = BoneTransforms[StartFrameIndex + NumFrames - TransformIndex].GetScale3D();

				PosKeys[StartFrameIndex + NumFrames - TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
				RotKeys[StartFrameIndex + NumFrames - TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
				ScaleKeys[StartFrameIndex + NumFrames - TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
			}
			else if (TransformIndex < StartFrameIndex && TransformIndex > StartFrameIndex + NumFrames)
			{
				PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
				RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
				ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
			}
		}

		Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
	}
}

void OffsetFrames(const DataModelType* Model, IAnimationDataController& Controller, const int32 StartFrameIndex, const int32 NumFrames, const int SourceFrameIndex, const FName trackName)
{
	TArray<FTransform> BoneTransforms;
	const int32 Num = Model->GetNumberOfKeys();
	BoneTransforms.Reserve(Num);

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num);

	{
		BoneTransforms.Reset();
		Model->GetBoneTrackTransforms(trackName, BoneTransforms);

		FVector sourcePos = BoneTransforms[SourceFrameIndex].GetLocation();

		for (int32 TransformIndex = 0; TransformIndex < Num; ++TransformIndex)
		{
			if (TransformIndex >= StartFrameIndex && TransformIndex < StartFrameIndex + NumFrames)
			{
				PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation() - sourcePos;
			}
			else
			{
				PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
			}

			RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
			ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
		}

		Controller.SetBoneTrackKeys(trackName, PosKeys, RotKeys, ScaleKeys);
	}
}

void SetFrames(const DataModelType* Model, IAnimationDataController& Controller, const int32 StartFrameIndex, const int32 NumFrames, const TArray<FCompactPose>& poseData, const TMap<FName, int32>& nameIndexMapping)
{
	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	const int32 Num = Model->GetNumberOfKeys();
	BoneTransforms.Reserve(Num);

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num);

	for (const FName& TrackName : TrackNames)
	{
		BoneTransforms.Reset();
		Model->GetBoneTrackTransforms(TrackName, BoneTransforms);

		int32 boneIndex = nameIndexMapping[TrackName];

		for (int32 TransformIndex = 0; TransformIndex < Num; ++TransformIndex)
		{
			if (TransformIndex >= StartFrameIndex && TransformIndex < StartFrameIndex + NumFrames)
			{
				PosKeys[TransformIndex] = poseData[TransformIndex].GetBones()[boneIndex].GetLocation();
				RotKeys[TransformIndex] = poseData[TransformIndex].GetBones()[boneIndex].GetRotation();
				ScaleKeys[TransformIndex] = poseData[TransformIndex].GetBones()[boneIndex].GetScale3D();
			}
			else
			{
				PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
				RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
				ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
			}
		}

		Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
	}
}

void SetFrames(const DataModelType* Model, IAnimationDataController& Controller, const int32 StartFrameIndex, const int32 NumFrames, const DataModelType* TargetModel, const int32 TargetStartFrameIndex, const TSet<FName>& filteredBoneNames)
{
	const int32 Num = Model->GetNumberOfKeys();

	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	BoneTransforms.Reserve(Num);

	TArray<FName> TargetTrackNames;
	TargetModel->GetBoneTrackNames(TargetTrackNames);

	TArray<FTransform> TargetBoneTransforms;
	TargetBoneTransforms.Reserve(TargetModel->GetNumberOfKeys());

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num);

	const bool noFilter = filteredBoneNames.Num() == 0;

	for (const FName& TrackName : TrackNames)
	{
		if (filteredBoneNames.Contains(TrackName) || noFilter)
		{
			BoneTransforms.Reset();
			Model->GetBoneTrackTransforms(TrackName, BoneTransforms);

			if (TargetTrackNames.Contains(TrackName))
			{
				TargetBoneTransforms.Reset();
				TargetModel->GetBoneTrackTransforms(TrackName, TargetBoneTransforms);

				for (int32 TransformIndex = 0; TransformIndex < Num; ++TransformIndex)
				{
					if (TransformIndex >= StartFrameIndex && TransformIndex < StartFrameIndex + NumFrames)
					{
						PosKeys[TransformIndex] = TargetBoneTransforms[TargetStartFrameIndex + TransformIndex - StartFrameIndex].GetLocation();
						RotKeys[TransformIndex] = TargetBoneTransforms[TargetStartFrameIndex + TransformIndex - StartFrameIndex].GetRotation();
						ScaleKeys[TransformIndex] = TargetBoneTransforms[TargetStartFrameIndex + TransformIndex - StartFrameIndex].GetScale3D();
					}
					else
					{
						PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
						RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
						ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
					}
				}

				Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
			}
		}
	}
}

void SetFrames_HandPoses(const DataModelType* Model, IAnimationDataController& Controller, const int32 StartFrameIndex, const int32 NumFrames, const DataModelType* TargetModel, const int32 TargetStartFrameIndex, const TSet<FName>& filteredBoneNames)
{
	const int32 Num = Model->GetNumberOfKeys();

	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	BoneTransforms.Reserve(Num);

	TArray<FName> TargetTrackNames;
	TargetModel->GetBoneTrackNames(TargetTrackNames);

	TArray<FTransform> TargetBoneTransforms;
	TargetBoneTransforms.Reserve(TargetModel->GetNumberOfKeys());

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num);

	const bool noFilter = filteredBoneNames.Num() == 0;

	for (const FName& TrackName : TrackNames)
	{
		if (filteredBoneNames.Contains(TrackName) || noFilter)
		{
			BoneTransforms.Reset();
			Model->GetBoneTrackTransforms(TrackName, BoneTransforms);

			if (TargetTrackNames.Contains(TrackName))
			{
				TargetBoneTransforms.Reset();
				TargetModel->GetBoneTrackTransforms(TrackName, TargetBoneTransforms);

				for (int32 TransformIndex = 0; TransformIndex < Num; ++TransformIndex)
				{
					if (TransformIndex >= StartFrameIndex && TransformIndex < StartFrameIndex + NumFrames + 1)
					{
						PosKeys[TransformIndex] = TargetBoneTransforms[TargetStartFrameIndex].GetLocation();
						RotKeys[TransformIndex] = TargetBoneTransforms[TargetStartFrameIndex].GetRotation();
						ScaleKeys[TransformIndex] = TargetBoneTransforms[TargetStartFrameIndex].GetScale3D();
					}
					else
					{
						PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
						RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
						ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
					}
				}

				Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
			}
		}
	}
}

void LerpFrames(const DataModelType* Model, IAnimationDataController& Controller, const int32 StartFrameIndex, const int32 NumFrames, const int aFrameIndex, const int bFrameIndex, const TSet<FName>& filteredBoneNames, const EAlphaBlendOption alphaBlendOption)
{
	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	const int32 Num = Model->GetNumberOfKeys();
	BoneTransforms.Reserve(Num);

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num);

	const bool noFilter = filteredBoneNames.Num() == 0;

	for (const FName& TrackName : TrackNames)
	{
		if (filteredBoneNames.Contains(TrackName) || noFilter)
		{
			BoneTransforms.Reset();
			Model->GetBoneTrackTransforms(TrackName, BoneTransforms);

			FVector posA = BoneTransforms[aFrameIndex].GetLocation();
			FQuat rotA = BoneTransforms[aFrameIndex].GetRotation();
			FVector scaleA = BoneTransforms[aFrameIndex].GetScale3D();

			FVector posB = BoneTransforms[bFrameIndex].GetLocation();
			FQuat rotB = BoneTransforms[bFrameIndex].GetRotation();
			FVector scaleB = BoneTransforms[bFrameIndex].GetScale3D();

			for (int32 TransformIndex = 0; TransformIndex < Num; ++TransformIndex)
			{
				if (TransformIndex > StartFrameIndex && TransformIndex <= StartFrameIndex + NumFrames)
				{
					const float alpha = (TransformIndex - StartFrameIndex) * 1.f / NumFrames;

					const float blendOption = FAlphaBlend::AlphaToBlendOption(alpha, alphaBlendOption);

					PosKeys[TransformIndex] = FVector(FMath::Lerp(posA, posB, blendOption));
					RotKeys[TransformIndex] = FQuat(FQuat::Slerp(rotA, rotB, blendOption));
					ScaleKeys[TransformIndex] = FVector(FMath::Lerp(scaleA, scaleB, blendOption));
				}
				else
				{
					PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
					RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
					ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
				}
			}

			Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
		}
	}
}

void LerpFramesDynamic(const DataModelType* Model, IAnimationDataController& Controller, const int32 aFrameIndex, const int bFrameIndex, const float balanceAlpha, const int blendFirstFromFrameIndex, const int blendFirstFramesNum, const int blendSecondFromFrameIndex, const int blendSecondFramesNum, const EAlphaBlendOption alphaBlendOption, const TSet<FName>& filteredBoneNames)
{
	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	const int32 Num = Model->GetNumberOfKeys();
	BoneTransforms.Reserve(Num);

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num);

	const bool noFilter = filteredBoneNames.Num() == 0;

	float actualBalanceAlpha = balanceAlpha;

	if (blendFirstFramesNum == 0)
	{
		actualBalanceAlpha = 0;
	}

	if (blendSecondFramesNum == 0)
	{
		actualBalanceAlpha = 1;
	}

	for (const FName& TrackName : TrackNames)
	{
		if (filteredBoneNames.Contains(TrackName) || noFilter)
		{
			BoneTransforms.Reset();
			Model->GetBoneTrackTransforms(TrackName, BoneTransforms);

			FVector posA = BoneTransforms[aFrameIndex].GetLocation();
			FQuat rotA = BoneTransforms[aFrameIndex].GetRotation();
			FVector scaleA = BoneTransforms[aFrameIndex].GetScale3D();

			FVector posC = BoneTransforms[bFrameIndex].GetLocation();
			FQuat rotC = BoneTransforms[bFrameIndex].GetRotation();
			FVector scaleC = BoneTransforms[bFrameIndex].GetScale3D();

			FVector posB = FVector(FMath::Lerp(posA, posC, actualBalanceAlpha));
			FQuat rotB = FQuat(FQuat::Slerp(rotA, rotC, actualBalanceAlpha));
			FVector scaleB = FVector(FMath::Lerp(scaleA, scaleC, actualBalanceAlpha));

			for (int32 TransformIndex = 0; TransformIndex < Num; ++TransformIndex)
			{
				if (TransformIndex > blendFirstFromFrameIndex && TransformIndex <= blendFirstFromFrameIndex + blendFirstFramesNum)
				{
					const float alpha = (TransformIndex - blendFirstFromFrameIndex) * 1.f / blendFirstFramesNum;
					const float blendOption = FAlphaBlend::AlphaToBlendOption(alpha, alphaBlendOption);

					PosKeys[TransformIndex] = FVector(FMath::Lerp(posA, posB, blendOption));
					RotKeys[TransformIndex] = FQuat(FQuat::Slerp(rotA, rotB, blendOption));
					ScaleKeys[TransformIndex] = FVector(FMath::Lerp(scaleA, scaleB, blendOption));
				}
				else if (TransformIndex >= blendSecondFromFrameIndex && TransformIndex <= blendSecondFromFrameIndex + blendSecondFramesNum)
				{
					const float alpha = (TransformIndex - blendSecondFromFrameIndex) * 1.f / blendSecondFramesNum;
					const float blendOption = FAlphaBlend::AlphaToBlendOption(alpha, alphaBlendOption);

					PosKeys[TransformIndex] = FVector(FMath::Lerp(posB, posC, blendOption));
					RotKeys[TransformIndex] = FQuat(FQuat::Slerp(rotB, rotC, blendOption));
					ScaleKeys[TransformIndex] = FVector(FMath::Lerp(scaleB, scaleC, blendOption));
				}
				else
				{
					PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
					RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
					ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
				}
			}

			Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
		}
	}
}

void FillFilteredBoneNames(const TMap<FName, int32>& nameIndexMapping, const FReferenceSkeleton& RefSkeleton, TSet<FName>& filteredBoneNames, const FName startBoneName, const FName endBoneName)
{
	if (nameIndexMapping.Contains(startBoneName))
	{
		TQueue<int32> bonesQueue;
		bonesQueue.Enqueue(nameIndexMapping[startBoneName]);

		const int32 endBoneIndex = endBoneName == NAME_None ? INDEX_NONE : nameIndexMapping[endBoneName];

		int32 boneIndex = INDEX_NONE;
		while (bonesQueue.Dequeue(boneIndex))
		{
			filteredBoneNames.FindOrAdd(RefSkeleton.GetBoneName(boneIndex));

			if (endBoneName == NAME_None || endBoneIndex != boneIndex)
			{
				TArray<int32> directChildBones;
				RefSkeleton.GetDirectChildBones(boneIndex, directChildBones);

				for (const int32 directChildBone : directChildBones)
				{
					bonesQueue.Enqueue(directChildBone);
				}
			}
		}
	}
}

EMovieSceneBuiltInEasing Convert(EAlphaBlendOption in)
{
	switch (in)
	{
	case EAlphaBlendOption::Linear: return EMovieSceneBuiltInEasing::Linear;
	case EAlphaBlendOption::Cubic: return EMovieSceneBuiltInEasing::Cubic;
	case EAlphaBlendOption::CubicInOut: return EMovieSceneBuiltInEasing::CubicInOut;
	case EAlphaBlendOption::HermiteCubic: return EMovieSceneBuiltInEasing::HermiteCubicInOut;
	case EAlphaBlendOption::Sinusoidal: return EMovieSceneBuiltInEasing::SinInOut;
	case EAlphaBlendOption::QuadraticInOut: return EMovieSceneBuiltInEasing::QuadInOut;
	case EAlphaBlendOption::QuarticInOut: return EMovieSceneBuiltInEasing::QuartInOut;
	case EAlphaBlendOption::QuinticInOut: return EMovieSceneBuiltInEasing::QuintInOut;
	case EAlphaBlendOption::CircularIn: return EMovieSceneBuiltInEasing::CircIn;
	case EAlphaBlendOption::CircularOut: return EMovieSceneBuiltInEasing::CircOut;
	case EAlphaBlendOption::CircularInOut: return EMovieSceneBuiltInEasing::CircInOut;
	case EAlphaBlendOption::ExpIn: return EMovieSceneBuiltInEasing::ExpoIn;
	case EAlphaBlendOption::ExpOut: return EMovieSceneBuiltInEasing::ExpoOut;
	case EAlphaBlendOption::ExpInOut: return EMovieSceneBuiltInEasing::ExpoInOut;
	case EAlphaBlendOption::Custom: return EMovieSceneBuiltInEasing::Custom;
	}

	return EMovieSceneBuiltInEasing::Linear;
}

EAlphaBlendOption Convert(EMovieSceneBuiltInEasing in)
{
	switch (in)
	{
	case EMovieSceneBuiltInEasing::Linear: return EAlphaBlendOption::Linear;
	case EMovieSceneBuiltInEasing::Cubic: return EAlphaBlendOption::Cubic;
	case EMovieSceneBuiltInEasing::CubicInOut: return EAlphaBlendOption::CubicInOut;
	case EMovieSceneBuiltInEasing::HermiteCubicInOut: return EAlphaBlendOption::HermiteCubic;
	case EMovieSceneBuiltInEasing::SinInOut: return EAlphaBlendOption::Sinusoidal;
	case EMovieSceneBuiltInEasing::QuadInOut: return EAlphaBlendOption::QuadraticInOut;
	case EMovieSceneBuiltInEasing::QuartInOut: return EAlphaBlendOption::QuarticInOut;
	case EMovieSceneBuiltInEasing::QuintInOut: return EAlphaBlendOption::QuinticInOut;
	case EMovieSceneBuiltInEasing::CircIn: return EAlphaBlendOption::CircularIn;
	case EMovieSceneBuiltInEasing::CircOut: return EAlphaBlendOption::CircularOut;
	case EMovieSceneBuiltInEasing::CircInOut: return EAlphaBlendOption::CircularInOut;
	case EMovieSceneBuiltInEasing::ExpoIn: return EAlphaBlendOption::ExpIn;
	case EMovieSceneBuiltInEasing::ExpoOut: return EAlphaBlendOption::ExpOut;
	case EMovieSceneBuiltInEasing::ExpoInOut: return EAlphaBlendOption::ExpInOut;
	case EMovieSceneBuiltInEasing::Custom: return EAlphaBlendOption::Custom;
	}

	return EAlphaBlendOption::Linear;
}

//--------------------------------------------------------------------
// SBuiltInFunctionVisualizerCustom
//--------------------------------------------------------------------

class SBuiltInFunctionVisualizerCustom : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBuiltInFunctionVisualizerCustom) {}
	SLATE_END_ARGS();

	virtual ~SBuiltInFunctionVisualizerCustom() = default;

	void Construct(const FArguments& InArgs, EMovieSceneBuiltInEasing InValue);

	void SetType(EMovieSceneBuiltInEasing InValue);
	void SetAnimationDuration(float TimeInSeconds) { AnimationDuration = TimeInSeconds; }

private:
	void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override final;
	void OnMouseLeave(const FPointerEvent& MouseEvent) override final;

	EActiveTimerReturnType TickInterp(const double InCurrentTime, const float InDeltaTime);

	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

	TSharedPtr<FActiveTimerHandle> TimerHandle;
	double MouseOverTime;
	EMovieSceneBuiltInEasing EasingType;

	FVector2D InterpValue;
	TArray<FVector2D> Samples;
	float AnimationDuration = 0.5f; /** Preview animation duration in seconds. */
};

void SBuiltInFunctionVisualizerCustom::SetType(EMovieSceneBuiltInEasing InValue)
{
	InterpValue = FVector2D::ZeroVector;

	UMovieSceneBuiltInEasingFunction* DefaultObject = GetMutableDefault<UMovieSceneBuiltInEasingFunction>();
	EMovieSceneBuiltInEasing DefaultType = DefaultObject->Type;

	DefaultObject->Type = InValue;

	Samples.Reset();
	float Interp = 0.f;
	while (Interp <= 1.f)
	{
		Samples.Add(FVector2D(Interp, DefaultObject->Evaluate(Interp)));
		Interp += 0.01f;
	}

	DefaultObject->Type = DefaultType;
	EasingType = InValue;
}

void SBuiltInFunctionVisualizerCustom::Construct(const FArguments& InArgs, EMovieSceneBuiltInEasing InValue)
{
	SetType(InValue);

	ChildSlot
		[
			SNew(SOverlay)
		];
}

void SBuiltInFunctionVisualizerCustom::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!TimerHandle.IsValid())
	{
		MouseOverTime = FSlateApplication::Get().GetCurrentTime();
		TimerHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SBuiltInFunctionVisualizerCustom::TickInterp));
	}
}

void SBuiltInFunctionVisualizerCustom::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	if (TimerHandle.IsValid())
	{
		InterpValue = FVector2D::ZeroVector;
		UnRegisterActiveTimer(TimerHandle.ToSharedRef());
		TimerHandle = nullptr;
	}
}

EActiveTimerReturnType SBuiltInFunctionVisualizerCustom::TickInterp(const double InCurrentTime, const float InDeltaTime)
{
	static float InterpInPad = .25f, InterpOutPad = .5f;

	float TotalInterpTime = InterpInPad + AnimationDuration + InterpOutPad;
	InterpValue.X = FMath::Clamp((FMath::Fmod(float(InCurrentTime - MouseOverTime), TotalInterpTime) - InterpInPad) / AnimationDuration, 0.f, 1.f);

	UMovieSceneBuiltInEasingFunction* DefaultObject = GetMutableDefault<UMovieSceneBuiltInEasingFunction>();

	EMovieSceneBuiltInEasing DefaultType = DefaultObject->Type;
	DefaultObject->Type = EasingType;

	InterpValue.Y = DefaultObject->Evaluate(InterpValue.X);
	DefaultObject->Type = DefaultType;

	return EActiveTimerReturnType::Continue;
}

int32 SBuiltInFunctionVisualizerCustom::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	float VerticalPad = 0.2f;
	FVector2D InverseVerticalSize(AllottedGeometry.Size.X, -AllottedGeometry.Size.Y);

	const float VerticalBottom = AllottedGeometry.Size.Y - AllottedGeometry.Size.Y * VerticalPad * .5f;
	const float CurveHeight = AllottedGeometry.Size.Y * (1.f - VerticalPad);
	const float CurveWidth = AllottedGeometry.Size.X - 5.f;

	TArray<FVector2D> Points;
	for (FVector2D Sample : Samples)
	{
		FVector2D Offset(5.f, VerticalBottom);
		Points.Add(Offset + FVector2D(
			CurveWidth * Sample.X,
			-CurveHeight * Sample.Y
		));
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None);

	if (TimerHandle.IsValid())
	{
		FVector2D PointOffset(0.f, VerticalBottom - CurveHeight * InterpValue.Y - 4.f);

		static const FSlateBrush* InterpPointBrush = FAppStyle::GetBrush("Sequencer.InterpLine");
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.MakeChild(
				FVector2D(AllottedGeometry.Size.X, 7.f),
				FSlateLayoutTransform(PointOffset)
			).ToPaintGeometry(),
			InterpPointBrush,
			ESlateDrawEffect::None,
			FLinearColor::Green
		);
	}

	return LayerId + 1;
}

//--------------------------------------------------------------------
// SEasingFunctionGridWidgetCustom
//--------------------------------------------------------------------

DECLARE_DELEGATE_OneParam(FEasingFunctionGridWidgetCustom_OnClicked, EMovieSceneBuiltInEasing);

class SEasingFunctionGridWidgetCustom : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEasingFunctionGridWidgetCustom) {}

		/** The easing curve filter containing all curve types that should be excluded.In case the filter is empty, all curve types will be shown. */
		SLATE_ATTRIBUTE(TSet<EMovieSceneBuiltInEasing>, FilterExclude)
		SLATE_EVENT(FEasingFunctionGridWidgetCustom_OnClicked, OnTypeChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	struct FGroup
	{
		FString GroupName;
		TArray<EMovieSceneBuiltInEasing, TInlineAllocator<3>> Values;
	};

	FGroup& FindOrAddGroup(TArray<FGroup>& Groups, const FString& GroupName);
	TArray<FGroup> ConstructGroups(const TSet<EMovieSceneBuiltInEasing>& FilterExclude);

	FReply OnTypeButtonClicked(EMovieSceneBuiltInEasing type);

	FEasingFunctionGridWidgetCustom_OnClicked OnClickedDelegate;
	TAttribute<TSet<EMovieSceneBuiltInEasing>> FilterExcludeAttribute;
};

SEasingFunctionGridWidgetCustom::FGroup& SEasingFunctionGridWidgetCustom::FindOrAddGroup(TArray<FGroup>& Groups, const FString& GroupName)
{
	for (int32 Index = Groups.Num() - 1; Index >= 0; --Index)
	{
		if (Groups[Index].GroupName == GroupName)
		{
			return Groups[Index];
		}
	}

	Groups.Emplace();
	Groups.Last().GroupName = GroupName;
	return Groups.Last();
};

TArray<SEasingFunctionGridWidgetCustom::FGroup> SEasingFunctionGridWidgetCustom::ConstructGroups(const TSet<EMovieSceneBuiltInEasing>& FilterExclude)
{
	const UEnum* EasingEnum = StaticEnum<EMovieSceneBuiltInEasing>();
	check(EasingEnum)

		TArray<FGroup> Groups;

	for (int32 NameIndex = 0; NameIndex < EasingEnum->NumEnums() - 1; ++NameIndex)
	{
		const FString& Grouping = EasingEnum->GetMetaData(TEXT("Grouping"), NameIndex);
		EMovieSceneBuiltInEasing Value = (EMovieSceneBuiltInEasing)EasingEnum->GetValueByIndex(NameIndex);

		if (FilterExclude.IsEmpty() || FilterExclude.Find(Value) == nullptr)
		{
			FindOrAddGroup(Groups, Grouping).Values.Add(Value);
		}
	}

	return Groups;
}

FReply SEasingFunctionGridWidgetCustom::OnTypeButtonClicked(EMovieSceneBuiltInEasing type)
{
	OnClickedDelegate.ExecuteIfBound(type);
	return FReply::Handled();
}

void SEasingFunctionGridWidgetCustom::Construct(const FArguments& InArgs)
{
	FilterExcludeAttribute = InArgs._FilterExclude;
	OnClickedDelegate = InArgs._OnTypeChanged;

	const UEnum* EasingEnum = StaticEnum<EMovieSceneBuiltInEasing>();
	check(EasingEnum)

		TArray<FGroup> Groups = ConstructGroups(FilterExcludeAttribute.Get());

	TSharedRef<SGridPanel> Grid = SNew(SGridPanel);

	int32 RowIndex = 0;
	for (const FGroup& Group : Groups)
	{
		for (int32 ColumnIndex = 0; ColumnIndex < Group.Values.Num(); ++ColumnIndex)
		{
			EMovieSceneBuiltInEasing Value = Group.Values[ColumnIndex];

			Grid->AddSlot(ColumnIndex, RowIndex)
				[
					SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						.OnClicked(this, &SEasingFunctionGridWidgetCustom::OnTypeButtonClicked, Value)
						[
							SNew(SBox)
								.WidthOverride(100.f)
								.HeightOverride(50.f)
								[
									SNew(SBuiltInFunctionVisualizerCustom, Value)
								]
						]
				];

			Grid->AddSlot(ColumnIndex, RowIndex + 1)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
						.Text(EasingEnum->GetDisplayNameTextByValue((int64)Value))
				];
		}

		RowIndex += 2;
	}

	ChildSlot
		[
			Grid
		];
}

//--------------------------------------------------------------------
// SBuiltInFunctionVisualizerWithTextCustom
//--------------------------------------------------------------------

class SBuiltInFunctionVisualizerWithTextCustom : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBuiltInFunctionVisualizerWithTextCustom) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, EMovieSceneBuiltInEasing InValue);
	void SetType(EMovieSceneBuiltInEasing InValue);

private:
	TSharedPtr<SBuiltInFunctionVisualizerCustom> FunctionVisualizer;
	TSharedPtr<STextBlock> FunctionName;
};

void SBuiltInFunctionVisualizerWithTextCustom::Construct(const FArguments& InArgs, EMovieSceneBuiltInEasing InValue)
{
	const UEnum* EasingEnum = StaticEnum<EMovieSceneBuiltInEasing>();
	check(EasingEnum);

	FunctionVisualizer = SNew(SBuiltInFunctionVisualizerCustom, InValue);
	FunctionName = SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(EasingEnum->GetDisplayNameTextByValue((int64)InValue));

	ChildSlot
		[
			SNew(SVerticalBox)

				// Add curve
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
						.WidthOverride(100.0f)
						.HeightOverride(50.0f)
						[
							FunctionVisualizer.ToSharedRef()
						]
				]

				// Add curve type name
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					FunctionName.ToSharedRef()
				]
		];
}

void SBuiltInFunctionVisualizerWithTextCustom::SetType(EMovieSceneBuiltInEasing InValue)
{
	FunctionVisualizer->SetType(InValue);

	const UEnum* EasingEnum = StaticEnum<EMovieSceneBuiltInEasing>();
	check(EasingEnum);
	FunctionName->SetText(EasingEnum->GetDisplayNameTextByValue((int64)InValue));
}

//--------------------------------------------------------------------
// UAnimSequenceCustom
//--------------------------------------------------------------------

UAnimSequenceCustom::UAnimSequenceCustom(const FObjectInitializer& objectInitializer) :Super(objectInitializer)
{
	//bBlockCompressionRequests = true;
}

//--------------------------------------------------------------------
// SComboBox_COPY
//--------------------------------------------------------------------

class SComboButtonCustom : public SComboButton
{
public:

	void SetButtonContent(TSharedRef<SWidget> widget)
	{
		ButtonContentSlot->DetachWidget();
		ButtonContentSlot->AttachWidget(widget);
	}
};

//--------------------------------------------------------------------
// SComboBox_COPY
//--------------------------------------------------------------------

template< typename OptionType >
class SComboBox_COPY : public SComboButton
{
public:

	typedef TListTypeTraits< OptionType > ListTypeTraits;
	typedef typename TListTypeTraits< OptionType >::NullableType NullableOptionType;

	/** Type of list used for showing menu options. */
	typedef STileView< OptionType > STileViewType;
	/** Delegate type used to generate widgets that represent Options */
	typedef typename TSlateDelegates< OptionType >::FOnGenerateWidget FOnGenerateWidget;
	typedef typename TSlateDelegates< NullableOptionType >::FOnSelectionChanged FOnSelectionChanged;

	SLATE_BEGIN_ARGS(SComboBox_COPY)
		: _Content()
		, _ComboBoxStyle(&FAppStyle::Get().GetWidgetStyle< FComboBoxStyle >("ComboBox"))
		, _ButtonStyle(nullptr)
		, _ItemStyle(&FAppStyle::Get().GetWidgetStyle< FTableRowStyle >("ComboBox.Row"))
		, _ScrollBarStyle(&FAppStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar"))
		, _ContentPadding(_ComboBoxStyle->ContentPadding)
		, _ForegroundColor(FSlateColor::UseStyle())
		, _OptionsSource()
		, _OnSelectionChanged()
		, _OnGenerateWidget()
		, _InitiallySelectedItem(ListTypeTraits::MakeNullPtr())
		, _Method()
		, _MaxListHeight(450.0f)
		, _HasDownArrow(true)
		, _EnableGamepadNavigationMode(false)
		, _IsFocusable(true)
		{}

		/** Slot for this button's content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_STYLE_ARGUMENT(FComboBoxStyle, ComboBoxStyle)

		/** The visual style of the button part of the combo box (overrides ComboBoxStyle) */
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)

		SLATE_STYLE_ARGUMENT(FTableRowStyle, ItemStyle)

		SLATE_STYLE_ARGUMENT(FScrollBarStyle, ScrollBarStyle)

		SLATE_ATTRIBUTE(FMargin, ContentPadding)
		SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)

		SLATE_ARGUMENT(const TArray< OptionType >*, OptionsSource)
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
		SLATE_EVENT(FOnGenerateWidget, OnGenerateWidget)

		/** Called when combo box is opened, before list is actually created */
		SLATE_EVENT(FOnComboBoxOpening, OnComboBoxOpening)

		/** The custom scrollbar to use in the ListView */
		SLATE_ARGUMENT(TSharedPtr<SScrollBar>, CustomScrollbar)

		/** The option that should be selected when the combo box is first created */
		SLATE_ARGUMENT(NullableOptionType, InitiallySelectedItem)

		SLATE_ARGUMENT(TOptional<EPopupMethod>, Method)

		/** The max height of the combo box menu */
		SLATE_ARGUMENT(float, MaxListHeight)

		/** The sound to play when the button is pressed (overrides ComboBoxStyle) */
		SLATE_ARGUMENT(TOptional<FSlateSound>, PressedSoundOverride)

		/** The sound to play when the selection changes (overrides ComboBoxStyle) */
		SLATE_ARGUMENT(TOptional<FSlateSound>, SelectionChangeSoundOverride)

		/**
		 * When false, the down arrow is not generated and it is up to the API consumer
		 * to make their own visual hint that this is a drop down.
		 */
		SLATE_ARGUMENT(bool, HasDownArrow)

		/**
		 *  When false, directional keys will change the selection. When true, ComboBox
		 *	must be activated and will only capture arrow input while activated.
		*/
		SLATE_ARGUMENT(bool, EnableGamepadNavigationMode)

		/** When true, allows the combo box to receive keyboard focus */
		SLATE_ARGUMENT(bool, IsFocusable)

		/** True if this combo's menu should be collapsed when our parent receives focus, false (default) otherwise */
		SLATE_ARGUMENT(bool, CollapseMenuOnParentFocus)

	SLATE_END_ARGS()

	/**
	 * Construct the widget from a declaration
	 *
	 * @param InArgs   Declaration from which to construct the combo box
	 */
	void Construct(const FArguments& InArgs)
	{
		check(InArgs._ComboBoxStyle);

		ItemStyle = InArgs._ItemStyle;
		ComboBoxStyle = InArgs._ComboBoxStyle;
		MenuRowPadding = ComboBoxStyle->MenuRowPadding;

		// Work out which values we should use based on whether we were given an override, or should use the style's version
		const FComboButtonStyle& OurComboButtonStyle = ComboBoxStyle->ComboButtonStyle;
		const FButtonStyle* const OurButtonStyle = InArgs._ButtonStyle ? InArgs._ButtonStyle : &OurComboButtonStyle.ButtonStyle;
		PressedSound = InArgs._PressedSoundOverride.Get(ComboBoxStyle->PressedSlateSound);
		SelectionChangeSound = InArgs._SelectionChangeSoundOverride.Get(ComboBoxStyle->SelectionChangeSlateSound);

		this->OnComboBoxOpening = InArgs._OnComboBoxOpening;
		this->OnSelectionChanged = InArgs._OnSelectionChanged;
		this->OnGenerateWidget = InArgs._OnGenerateWidget;
		this->EnableGamepadNavigationMode = InArgs._EnableGamepadNavigationMode;
		this->bControllerInputCaptured = false;

		OptionsSource = InArgs._OptionsSource;
		CustomScrollbar = InArgs._CustomScrollbar;

		ComboBoxMenuContent =
			SNew(SBox)
			.MaxDesiredHeight(InArgs._MaxListHeight)
			[
				SNew(SOverlay)

					+SOverlay::Slot()
					[
						SNew(SImage).ColorAndOpacity(FLinearColor::Black)
					]

					+ SOverlay::Slot()
					[
						SAssignNew(this->TileView, STileViewType)
							.ListItemsSource(InArgs._OptionsSource)
							.OnGenerateTile(this, &SComboBox_COPY< OptionType >::GenerateMenuItemRow)
							.OnSelectionChanged(this, &SComboBox_COPY< OptionType >::OnSelectionChanged_Internal)
							.SelectionMode(ESelectionMode::Single)
							.ScrollBarStyle(InArgs._ScrollBarStyle)
							.ExternalScrollbar(InArgs._CustomScrollbar)
							.ItemHeight(TileSizeY)
							.ItemWidth(TileSizeY)
					]
			];

		// Set up content
		TSharedPtr<SWidget> ButtonContent = InArgs._Content.Widget;
		if (InArgs._Content.Widget == SNullWidget::NullWidget)
		{
			SAssignNew(ButtonContent, STextBlock)
				.Text(NSLOCTEXT("SComboBox_COPY", "ContentWarning", "No Content Provided"))
				.ColorAndOpacity(FLinearColor::Red);
		}


		SComboButton::Construct(SComboButton::FArguments()
			.ComboButtonStyle(&OurComboButtonStyle)
			.ButtonStyle(OurButtonStyle)
			.Method(InArgs._Method)
			.ButtonContent()
			[
				ButtonContent.ToSharedRef()
			]
			.MenuContent()
			[
				ComboBoxMenuContent.ToSharedRef()
			]
			.HasDownArrow(InArgs._HasDownArrow)
			.ContentPadding(InArgs._ContentPadding)
			.ForegroundColor(InArgs._ForegroundColor)
			.OnMenuOpenChanged(this, &SComboBox_COPY< OptionType >::OnMenuOpenChanged)
			.IsFocusable(InArgs._IsFocusable)
			.CollapseMenuOnParentFocus(InArgs._CollapseMenuOnParentFocus)
		);
		SetMenuContentWidgetToFocus(TileView);

		// Need to establish the selected item at point of construction so its available for querying
		// NB: If you need a selection to fire use SetItemSelection rather than setting an IntiallySelectedItem
		SelectedItem = InArgs._InitiallySelectedItem;
		if (TListTypeTraits<OptionType>::IsPtrValid(SelectedItem))
		{
			OptionType ValidatedItem = TListTypeTraits<OptionType>::NullableItemTypeConvertToItemType(SelectedItem);
			TileView->Private_SetItemSelection(ValidatedItem, true);
			TileView->RequestScrollIntoView(ValidatedItem, 0);
		}

	}

	SComboBox_COPY()
	{
#if WITH_ACCESSIBILITY
		AccessibleBehavior = EAccessibleBehavior::Auto;
		bCanChildrenBeAccessible = true;
#endif
	}

#if WITH_ACCESSIBILITY
protected:
	friend class FSlateAccessibleComboBox;
	/**
	* An accessible implementation of SComboBox_COPY to expose to platform accessibility APIs.
	* We inherit from IAccessibleProperty as Windows will use the interface to read out
	* the value associated with the combo box. Convenient place to return the value of the currently selected option.
	* For subclasses of SComboBox_COPY, inherit and override the necessary functions
	*/
	class FSlateAccessibleComboBox
		: public FSlateAccessibleWidget
		, public IAccessibleProperty
	{
	public:
		FSlateAccessibleComboBox(TWeakPtr<SWidget> InWidget)
			: FSlateAccessibleWidget(InWidget, EAccessibleWidgetType::ComboBox)
		{}

		// IAccessibleWidget
		virtual IAccessibleProperty* AsProperty() override
		{
			return this;
		}
		// ~

		// IAccessibleProperty
		virtual FString GetValue() const override
		{
			if (Widget.IsValid())
			{
				TSharedPtr<SComboBox_COPY<OptionType>> ComboBox = StaticCastSharedPtr<SComboBox_COPY<OptionType>>(Widget.Pin());
				if (TListTypeTraits<OptionType>::IsPtrValid(ComboBox->SelectedItem))
				{
					OptionType SelectedOption = TListTypeTraits<OptionType>::NullableItemTypeConvertToItemType(ComboBox->SelectedItem);
					TSharedPtr<ITableRow> SelectedTableRow = ComboBox->TileView->WidgetFromItem(SelectedOption);
					if (SelectedTableRow.IsValid())
					{
						TSharedRef<SWidget> TableRowWidget = SelectedTableRow->AsWidget();
						return TableRowWidget->GetAccessibleText().ToString();
					}
				}
			}
			return FText::GetEmpty().ToString();
		}

		virtual FVariant GetValueAsVariant() const override
		{
			return FVariant(GetValue());
		}
		// ~
	};

public:
	virtual TSharedRef<FSlateAccessibleWidget> CreateAccessibleWidget() override
	{
		return MakeShareable<FSlateAccessibleWidget>(new SComboBox_COPY<OptionType>::FSlateAccessibleComboBox(SharedThis(this)));
	}

	virtual TOptional<FText> GetDefaultAccessibleText(EAccessibleType AccessibleType) const
	{
		// current behaviour will red out the  templated type of the combo box which is verbose and unhelpful 
		// This coupled with UIA type will announce Combo Box twice, but it's the best we can do for now if there's no label
		//@TODOAccessibility: Give a better name
		static FString Name(TEXT("Combo Box"));
		return FText::FromString(Name);
	}
#endif

	void ClearSelection()
	{
		TileView->ClearSelection();
	}

	void SetSelectedItem(NullableOptionType InSelectedItem)
	{
		if (TListTypeTraits<OptionType>::IsPtrValid(InSelectedItem))
		{
			OptionType InSelected = TListTypeTraits<OptionType>::NullableItemTypeConvertToItemType(InSelectedItem);
			TileView->SetSelection(InSelected);
		}
		else
		{
			TileView->ClearSelection();
		}
	}

	void SetEnableGamepadNavigationMode(bool InEnableGamepadNavigationMode)
	{
		this->EnableGamepadNavigationMode = InEnableGamepadNavigationMode;
	}

	void SetMaxHeight(float InMaxHeight)
	{
		ComboBoxMenuContent->SetMaxDesiredHeight(InMaxHeight);
	}

	void SetStyle(const FComboBoxStyle* InStyle)
	{
		if (ComboBoxStyle != InStyle)
		{
			ComboBoxStyle = InStyle;
			InvalidateStyle();
		}
	}

	void InvalidateStyle()
	{
		Invalidate(EInvalidateWidgetReason::Layout);
	}

	void SetItemStyle(const FTableRowStyle* InItemStyle)
	{
		if (ItemStyle != InItemStyle)
		{
			ItemStyle = InItemStyle;
			InvalidateItemStyle();
		}
	}

	void InvalidateItemStyle()
	{
		Invalidate(EInvalidateWidgetReason::Layout);
	}

	/** @return the item currently selected by the combo box. */
	NullableOptionType GetSelectedItem()
	{
		return SelectedItem;
	}


	/**
	 * Requests a list refresh after updating options
	 * Call SetSelectedItem to update the selected item if required
	 * @see SetSelectedItem
	 */
	void RefreshOptions()
	{
		TileView->RequestListRefresh();
	}

protected:
	/** Handle key presses that SListView ignores */
	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (IsInteractable())
		{
			const EUINavigationAction NavAction = FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent);
			const EUINavigation NavDirection = FSlateApplication::Get().GetNavigationDirectionFromKey(InKeyEvent);
			if (EnableGamepadNavigationMode)
			{
				// The controller's bottom face button must be pressed once to begin manipulating the combobox's value.
				// Navigation away from the widget is prevented until the button has been pressed again or focus is lost.
				if (NavAction == EUINavigationAction::Accept)
				{
					if (bControllerInputCaptured == false)
					{
						// Begin capturing controller input and open the ListView
						bControllerInputCaptured = true;
						PlayPressedSound();
						OnComboBoxOpening.ExecuteIfBound();
						return SComboButton::OnButtonClicked();
					}
					else
					{
						// Set selection to the selected item on the list and close
						bControllerInputCaptured = false;

						// Re-select first selected item, just in case it was selected by navigation previously
						TArray<OptionType> SelectedItems = TileView->GetSelectedItems();
						if (SelectedItems.Num() > 0)
						{
							OnSelectionChanged_Internal(SelectedItems[0], ESelectInfo::Direct);
						}

						// Set focus back to ComboBox
						FReply Reply = FReply::Handled();
						Reply.SetUserFocus(this->AsShared(), EFocusCause::SetDirectly);
						return Reply;
					}

				}
				else if (NavAction == EUINavigationAction::Back || InKeyEvent.GetKey() == EKeys::BackSpace)
				{
					const bool bWasInputCaptured = bControllerInputCaptured;

					OnMenuOpenChanged(false);
					if (bWasInputCaptured)
					{
						return FReply::Handled();
					}
				}
				else
				{
					if (bControllerInputCaptured)
					{
						return FReply::Handled();
					}
				}
			}
			else
			{
				if (NavDirection == EUINavigation::Up)
				{
					NullableOptionType NullableSelected = GetSelectedItem();
					if (TListTypeTraits<OptionType>::IsPtrValid(NullableSelected))
					{
						OptionType ActuallySelected = TListTypeTraits<OptionType>::NullableItemTypeConvertToItemType(NullableSelected);
						const int32 SelectionIndex = OptionsSource->Find(ActuallySelected);
						if (SelectionIndex >= 1)
						{
							// Select an item on the prev row
							SetSelectedItem((*OptionsSource)[SelectionIndex - 1]);
						}
					}

					return FReply::Handled();
				}
				else if (NavDirection == EUINavigation::Down)
				{
					NullableOptionType NullableSelected = GetSelectedItem();
					if (TListTypeTraits<OptionType>::IsPtrValid(NullableSelected))
					{
						OptionType ActuallySelected = TListTypeTraits<OptionType>::NullableItemTypeConvertToItemType(NullableSelected);
						const int32 SelectionIndex = OptionsSource->Find(ActuallySelected);
						if (SelectionIndex < OptionsSource->Num() - 1)
						{
							// Select an item on the next row
							SetSelectedItem((*OptionsSource)[SelectionIndex + 1]);
						}
					}
					return FReply::Handled();
				}

				return SComboButton::OnKeyDown(MyGeometry, InKeyEvent);
			}
		}
		return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

	virtual bool SupportsKeyboardFocus() const override
	{
		return bIsFocusable;
	}

	virtual bool IsInteractable() const
	{
		return IsEnabled();
	}

private:

	/** Generate a row for the InItem in the combo box's list (passed in as OwnerTable). Do this by calling the user-specified OnGenerateWidget */
	TSharedRef<ITableRow> GenerateMenuItemRow(OptionType InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		if (OnGenerateWidget.IsBound())
		{
			return SNew(SComboRow<OptionType>, OwnerTable)
				.Style(ItemStyle)
				.Padding(MenuRowPadding)
				[
					OnGenerateWidget.Execute(InItem)
				];
		}
		else
		{
			return SNew(SComboRow<OptionType>, OwnerTable)
				[
					SNew(STextBlock).Text(NSLOCTEXT("SlateCore", "ComboBoxMissingOnGenerateWidgetMethod", "Please provide a .OnGenerateWidget() handler."))
				];

		}
	}

	//** Called if the menu is closed
	void OnMenuOpenChanged(bool bOpen)
	{
		if (bOpen == false)
		{
			bControllerInputCaptured = false;

			if (TListTypeTraits<OptionType>::IsPtrValid(SelectedItem))
			{
				// Ensure the ListView selection is set back to the last committed selection
				OptionType ActuallySelected = TListTypeTraits<OptionType>::NullableItemTypeConvertToItemType(SelectedItem);

				TileView->SetSelection(ActuallySelected, ESelectInfo::OnNavigation);
				TileView->RequestScrollIntoView(ActuallySelected, 0);
			}

			// Set focus back to ComboBox for users focusing the ListView that just closed
			FSlateApplication::Get().ForEachUser([this](FSlateUser& User)
				{
					TSharedRef<SWidget> ThisRef = this->AsShared();
					if (User.IsWidgetInFocusPath(this->TileView))
					{
						User.SetFocus(ThisRef);
					}
				});

		}
	}

	/** Invoked when the selection in the list changes */
	void OnSelectionChanged_Internal(NullableOptionType ProposedSelection, ESelectInfo::Type SelectInfo)
	{
		// Ensure that the proposed selection is different
		if (SelectInfo != ESelectInfo::OnNavigation)
		{
			// Ensure that the proposed selection is different from selected
			if (ProposedSelection != SelectedItem)
			{
				PlaySelectionChangeSound();
				SelectedItem = ProposedSelection;
				OnSelectionChanged.ExecuteIfBound(ProposedSelection, SelectInfo);
			}
			// close combo even if user reselected item
			this->SetIsOpen(false);
		}
	}

	/** Handle clicking on the content menu */
	virtual FReply OnButtonClicked() override
	{
		// if user clicked to close the combo menu
		if (this->IsOpen())
		{
			// Re-select first selected item, just in case it was selected by navigation previously
			TArray<OptionType> SelectedItems = TileView->GetSelectedItems();
			if (SelectedItems.Num() > 0)
			{
				OnSelectionChanged_Internal(SelectedItems[0], ESelectInfo::Direct);
			}
		}
		else
		{
			PlayPressedSound();
			OnComboBoxOpening.ExecuteIfBound();
		}

		return SComboButton::OnButtonClicked();
	}

	FReply OnKeyDownHandler(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		if (InKeyEvent.GetKey() == EKeys::Enter)
		{
			// Select the first selected item on hitting enter
			TArray<OptionType> SelectedItems = TileView->GetSelectedItems();
			if (SelectedItems.Num() > 0)
			{
				OnSelectionChanged_Internal(SelectedItems[0], ESelectInfo::OnKeyPress);
				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}


	/** Play the pressed sound */
	void PlayPressedSound() const
	{
		FSlateApplication::Get().PlaySound(PressedSound);
	}

	/** Play the selection changed sound */
	void PlaySelectionChangeSound() const
	{
		FSlateApplication::Get().PlaySound(SelectionChangeSound);
	}

	/** The Sound to play when the button is pressed */
	FSlateSound PressedSound;

	/** The Sound to play when the selection is changed */
	FSlateSound SelectionChangeSound;

	/** The item style to use. */
	const FTableRowStyle* ItemStyle;

	/** The combo box style to use. */
	const FComboBoxStyle* ComboBoxStyle;

	/** The padding around each menu row */
	FMargin MenuRowPadding;

private:
	/** Delegate that is invoked when the selected item in the combo box changes */
	FOnSelectionChanged OnSelectionChanged;
	/** The item currently selected in the combo box */
	NullableOptionType SelectedItem;
	/** The ListView that we pop up; visualized the available options. */
	TSharedPtr< STileViewType > TileView;
	/** The Scrollbar used in the ListView. */
	TSharedPtr< SScrollBar > CustomScrollbar;
	/** Delegate to invoke before the combo box is opening. */
	FOnComboBoxOpening OnComboBoxOpening;
	/** Delegate to invoke when we need to visualize an option as a widget. */
	FOnGenerateWidget OnGenerateWidget;
	// Use activate button to toggle ListView when enabled
	bool EnableGamepadNavigationMode;
	// Holds a flag indicating whether a controller/keyboard is manipulating the combobox's value. 
	// When true, navigation away from the widget is prevented until a new value has been accepted or canceled. 
	bool bControllerInputCaptured;

	TSharedPtr<SBox> ComboBoxMenuContent;

	const TArray< OptionType >* OptionsSource;
};

//--------------------------------------------------------------------
// FAnimModEditor
//--------------------------------------------------------------------

FText scopedTransactionText = LOCTEXT("AnimModContext_ScopedTransaction", "Change Anim Mod Tool config...");

template<class T>
uint32 GetTypeHash_Internal(const TArray<T>& items)
{
	uint32 result = GetTypeHash(items.Num());

	for (const T& item : items)
	{
		result ^= GetTypeHash(item);
	}

	return result;
}

template<class T>
uint32 GetTypeHash_Internal(const TMap<FGuid, T>& map)
{
	uint32 result = GetTypeHash(map.Num());

	for (const TPair<FGuid, T>& entry : map)
	{
		result ^= GetTypeHash(entry.Key);
		result ^= GetTypeHash(entry.Value);
	}

	return result;
}

class AnimModContext
{
public:

	AnimModContext()
	{
		AnimModToolConfig = NewObject<UAnimModToolConfig>(GetTransientPackage());
		AnimModToolConfig->SetFlags(RF_Transactional);
		AnimModToolConfig->AddToRoot();

		Reset();
	}

	virtual ~AnimModContext()
	{
		AnimModToolConfig->RemoveFromRoot();
	}

	const FReferenceSkeleton& GetReferenceSkeleton() const { return AnimSequencesToMod[0]->GetSkeleton()->GetReferenceSkeleton(); }

	uint32 CalculateModificationHash() const
	{
		int32 i = 0;

		uint32 result = GetTypeHash((AnimModToolConfig->bReverse) << i); i++;
		result ^= (GetTypeHash(AnimModToolConfig->bReverseClearRootBoneZeroFrameOffset) << i); i++;
		
		result ^= (GetTypeHash(AnimModToolConfig->bMirror) << i); i++;
		
		result ^= (GetTypeHash(AnimModToolConfig->bRepeatFrames) << i); i++;
		if (AnimModToolConfig->bRepeatFrames)
		{
			result ^= (GetTypeHash_Internal(AnimModToolConfig->RepeatFramesValues) << i); i++;
			result ^= (GetTypeHash_Internal(AnimModToolConfig->RepeatFramesKeys) << i); i++;
		}

		result ^= (GetTypeHash(AnimModToolConfig->bMakeLoopedDynamic) << i); i++;
		if (AnimModToolConfig->bMakeLoopedDynamic)
		{
			result ^= (GetTypeHash(AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->MakeLoopedDynamicBalance) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->MakeLoopedDynamicBlendOption) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->MakeLoopedDynamicBoneName) << i); i++;
		}

		result ^= (GetTypeHash(AnimModToolConfig->bRemoveFrames) << i); i++;
		if (AnimModToolConfig->bRemoveFrames)
		{
			result ^= (GetTypeHash(AnimModToolConfig->RemoveFramesStartFrame) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->RemoveFramesFramesNum) << i); i++;
		}

		result ^= (GetTypeHash(AnimModToolConfig->bBlendWithOther) << i); i++;
		if (AnimModToolConfig->bBlendWithOther && AnimModToolConfig->Blend_Target.Get())
		{
			result ^= (GetTypeHash(AnimModToolConfig->Blend_StartFrame) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->Blend_Target_StartFrame) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->bUseStaticFrameFromTarget) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->Blend_Target_FramesNum) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->Blend_AFramesNum) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->Blend_BFramesNum) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->Blend_Target.Get()) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->Blend_Target_ABlendOption) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->Blend_Target_BBlendOption) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->bInsertAsNewFrames) << i); i++;
			result ^= (GetTypeHash_Internal(AnimModToolConfig->Blend_StartBones) << i); i++;
			result ^= (GetTypeHash_Internal(AnimModToolConfig->Blend_EndBones) << i); i++;
		}

		result ^= GetTypeHash(HasTimeManipulation()); i++;
		if (HasTimeManipulation())
		{
			result ^= (GetTypeHash(AnimModToolConfig->bCurveChanged) << i); i++;

			if (const FFloatCurve* floatCurve = GetTimeManipulationCurve())
			{
				TArray<float> times;
				TArray<float> values;
				floatCurve->GetKeys(times, values);

				result ^= (GetTypeHash_Internal(times) << i); i++;
				result ^= (GetTypeHash_Internal(values) << i); i++;
			}
		}

		result ^= (GetTypeHash(AnimModToolConfig->bHandPoses) << i); i++;
		if (AnimModToolConfig->bHandPoses && HandPoses_AnimSequence.Get() && HandPoses_AnimSequenceUE4.Get())
		{
			result ^= (GetTypeHash(AnimModToolConfig->HandPoses_UE4) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->HandPoses_PoseFrame) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->HandPoses_StartFrame) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->HandPoses_FramesNum) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->HandPoses_StartBlend_FramesNum) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->HandPoses_StartBlend_BlendOption) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->HandPoses_EndBlend_FramesNum) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->HandPoses_EndBlend_BlendOption) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->bHandPoses_Left) << i); i++;
			result ^= (GetTypeHash(AnimModToolConfig->bHandPoses_Right) << i); i++;
		}

		return result;
	}

	void ApplyModifiers(UAnimSequence* animSequenceToMod, const USkeleton* skeleton, int32 Num, const TMap<const USkeleton*, FBoneContainer>& BoneContainers, const TMap<const USkeleton*, UMirrorDataTable*>& MirrorDataTables, TArray<FCompactPose>& MirroredPoses, const int32 NumBones, const TMap<FName, int32>& nameIndexMapping, const DataModelType* Model, const FReferenceSkeleton RefSkeleton, const FName rootBoneName, const EModifierFilter modifierFilter) const
	{
		IAnimationDataController& Controller = animSequenceToMod->GetController();
		
		if (modifierFilter != EModifierFilter::DEFERRED)
		{
			// Mirror

			if (AnimModToolConfig->bMirror)
			{
				{
					FMemMark Mark(FMemStack::Get());

					for (int32 AnimKey = 0; AnimKey < Num + 1; AnimKey++)
					{
						FBlendedCurve Curve;
						Curve.InitFrom(BoneContainers[skeleton]);
						UE::Anim::FStackAttributeContainer AttributeContainer;

						UE::Anim::DataModel::FEvaluationContext EvaluationContext(FFrameTime(AnimKey), Model->GetFrameRate(), animSequenceToMod->GetRetargetTransformsSourceName(), animSequenceToMod->GetRetargetTransforms());

						FCompactPose& Pose = MirroredPoses[AnimKey];
						Pose.SetBoneContainer(&BoneContainers[skeleton]);

						FAnimationPoseData PoseData(Pose, Curve, AttributeContainer);
						Model->Evaluate(PoseData, EvaluationContext);

						TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> CompactPoseMirrorBones;
						TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ComponentSpaceRefRotations;
						MirrorDataTables[skeleton]->FillCompactPoseAndComponentRefRotations(BoneContainers[skeleton], CompactPoseMirrorBones, ComponentSpaceRefRotations);

						FAnimationRuntime::MirrorPose(Pose, EAxis::X, CompactPoseMirrorBones, ComponentSpaceRefRotations);
					}
				}

				SetFrames(Model, Controller, 0, Num + 1, MirroredPoses, nameIndexMapping);
			}

			// Reverse

			if (AnimModToolConfig->bReverse)
			{
				ReverseFrames(Model, Controller, 0, Num);

				if (AnimModToolConfig->bReverseClearRootBoneZeroFrameOffset)
				{
					OffsetFrames(Model, Controller, 0, Num, 0, rootBoneName);
				}
			}

			// Make Looped Dynamic

			if (AnimModToolConfig->bMakeLoopedDynamic && (AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum + AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum > 0))
			{
				TSet<FName> filteredBoneNames;

				if (AnimModToolConfig->MakeLoopedDynamicBoneName != NAME_None)
				{
					FillFilteredBoneNames(nameIndexMapping, RefSkeleton, filteredBoneNames, AnimModToolConfig->MakeLoopedDynamicBoneName, NAME_None);
				}

				const int32 aFrameIndex = Num - AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum;
				
				const int32 bFrameIndex = AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum;

				const float balance = AnimModToolConfig->MakeLoopedDynamicBalance;

				const EAlphaBlendOption alphaBlendOption = AnimModToolConfig->MakeLoopedDynamicBlendOption;

				const int32 firstBlendFramesNum = AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum;

				const int32 secondBlendFramesNum = AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum;

				LerpFramesDynamic(Model, Controller, aFrameIndex, bFrameIndex, balance, aFrameIndex, firstBlendFramesNum, 0, secondBlendFramesNum, alphaBlendOption, filteredBoneNames);
			}

			// Repeat Frames

			if (AnimModToolConfig->bRepeatFrames && AnimModToolConfig->RepeatFramesKeys.Num() > 0)
			{
				TArray<FRepeatFrame> repeatFrames;
				repeatFrames.SetNum(AnimModToolConfig->RepeatFramesKeys.Num());

				int32 index = 0;
				int32 total = 0;

				for (const TPair<FGuid, int32>& repeatFrame : AnimModToolConfig->RepeatFramesKeys)
				{
					repeatFrames[index].FrameNum = repeatFrame.Value;
					repeatFrames[index].RepeatNum = AnimModToolConfig->RepeatFramesValues[repeatFrame.Key];

					total += repeatFrames[index].RepeatNum;
					index++;
				}

				if (total > 0)
				{
					repeatFrames.Sort([](const FRepeatFrame& repeatFrameA, const FRepeatFrame& repeatFrameB) { return repeatFrameA.FrameNum >= repeatFrameB.FrameNum; });

					for (const FRepeatFrame& repeatFrame : repeatFrames)
					{
						if (repeatFrame.FrameNum != INDEX_NONE)
						{
							InsertFrames(Model, Controller, repeatFrame.FrameNum, repeatFrame.RepeatNum);
						}
					}
				}
			}

			// Remove frames

			if (AnimModToolConfig->bRemoveFrames)
			{
				RemoveFrames(Model, Controller, AnimModToolConfig->RemoveFramesStartFrame, AnimModToolConfig->RemoveFramesFramesNum);
			}

			// Blend

			if (AnimModToolConfig->bBlendWithOther && AnimModToolConfig->Blend_Target.Get())
			{
				if (AnimModToolConfig->bInsertAsNewFrames > 0)
				{
					InsertFrames(Model, Controller, AnimModToolConfig->Blend_StartFrame, AnimModToolConfig->Blend_Target_FramesNum);

					Num += AnimModToolConfig->Blend_Target_FramesNum;
				}

				TSet<FName> filteredBoneNames;

				for (const TPair<FGuid, FName>& startBoneEntry : AnimModToolConfig->Blend_StartBones)
				{
					FillFilteredBoneNames(nameIndexMapping, RefSkeleton, filteredBoneNames, startBoneEntry.Value, AnimModToolConfig->Blend_EndBones[startBoneEntry.Key]);
				}

				{
					if (AnimModToolConfig->bUseStaticFrameFromTarget)
					{
						SetFrames_HandPoses(Model, Controller, AnimModToolConfig->Blend_StartFrame, AnimModToolConfig->Blend_Target_FramesNum, AnimModToolConfig->Blend_Target->GetDataModel(), AnimModToolConfig->Blend_Target_StartFrame, filteredBoneNames);
					}
					else
					{
						SetFrames(Model, Controller, AnimModToolConfig->Blend_StartFrame, AnimModToolConfig->Blend_Target_FramesNum, AnimModToolConfig->Blend_Target->GetDataModel(), AnimModToolConfig->Blend_Target_StartFrame, filteredBoneNames);
					}
				}

				if (AnimModToolConfig->Blend_AFramesNum > 0)
				{
					const int32 startFrameIndex = FMath::Max(0, AnimModToolConfig->Blend_StartFrame - AnimModToolConfig->Blend_AFramesNum / 2);
					const int32 endFrameIndex = FMath::Min(Num, AnimModToolConfig->Blend_StartFrame + AnimModToolConfig->Blend_AFramesNum / 2);
					LerpFrames(Model, Controller, startFrameIndex, endFrameIndex - startFrameIndex, startFrameIndex, endFrameIndex, filteredBoneNames, AnimModToolConfig->Blend_Target_ABlendOption);
				}

				if (AnimModToolConfig->Blend_BFramesNum > 0)
				{
					const int32 startFrameIndex = FMath::Max(0, AnimModToolConfig->Blend_StartFrame + AnimModToolConfig->Blend_Target_FramesNum - AnimModToolConfig->Blend_BFramesNum / 2);
					const int32 endFrameIndex = FMath::Min(Num, AnimModToolConfig->Blend_StartFrame + AnimModToolConfig->Blend_Target_FramesNum + AnimModToolConfig->Blend_BFramesNum / 2);
					LerpFrames(Model, Controller, startFrameIndex, endFrameIndex - startFrameIndex, startFrameIndex, endFrameIndex, filteredBoneNames, AnimModToolConfig->Blend_Target_BBlendOption);
				}
			}

			// Hand Poses

			if (AnimModToolConfig->bHandPoses)
			{
				TSet<FName> filteredBoneNames;

				if (AnimModToolConfig->bHandPoses_Left)
				{
					filteredBoneNames.Add("wrist_outer_l");
					filteredBoneNames.Add("wrist_inner_l");
					filteredBoneNames.Add("weapon_l");
					filteredBoneNames.Add("thumb_03_l");
					filteredBoneNames.Add("thumb_02_l");
					filteredBoneNames.Add("thumb_01_l");
					filteredBoneNames.Add("ring_03_l");
					filteredBoneNames.Add("ring_02_l");
					filteredBoneNames.Add("ring_01_l");
					filteredBoneNames.Add("ring_metacarpal_l");
					filteredBoneNames.Add("pinky_03_l");
					filteredBoneNames.Add("pinky_02_l");
					filteredBoneNames.Add("pinky_01_l");
					filteredBoneNames.Add("pinky_metacarpal_l");
					filteredBoneNames.Add("middle_03_l");
					filteredBoneNames.Add("middle_02_l");
					filteredBoneNames.Add("middle_01_l");
					filteredBoneNames.Add("middle_metacarpal_l");
					filteredBoneNames.Add("index_03_l");
					filteredBoneNames.Add("index_02_l");
					filteredBoneNames.Add("index_01_l");
					filteredBoneNames.Add("index_metacarpal_l");
				}

				if (AnimModToolConfig->bHandPoses_Right)
				{
					filteredBoneNames.Add("wrist_outer_r");
					filteredBoneNames.Add("wrist_inner_r");
					filteredBoneNames.Add("weapon_r");
					filteredBoneNames.Add("thumb_03_r");
					filteredBoneNames.Add("thumb_02_r");
					filteredBoneNames.Add("thumb_01_r");
					filteredBoneNames.Add("ring_03_r");
					filteredBoneNames.Add("ring_02_r");
					filteredBoneNames.Add("ring_01_r");
					filteredBoneNames.Add("ring_metacarpal_r");
					filteredBoneNames.Add("pinky_03_r");
					filteredBoneNames.Add("pinky_02_r");
					filteredBoneNames.Add("pinky_01_r");
					filteredBoneNames.Add("pinky_metacarpal_r");
					filteredBoneNames.Add("middle_03_r");
					filteredBoneNames.Add("middle_02_r");
					filteredBoneNames.Add("middle_01_r");
					filteredBoneNames.Add("middle_metacarpal_r");
					filteredBoneNames.Add("index_03_r");
					filteredBoneNames.Add("index_02_r");
					filteredBoneNames.Add("index_01_r");
					filteredBoneNames.Add("index_metacarpal_r");
				}

				if (filteredBoneNames.Num() > 0)
				{
					{
						SetFrames_HandPoses(Model, Controller, AnimModToolConfig->HandPoses_StartFrame, AnimModToolConfig->HandPoses_FramesNum, AnimModToolConfig->HandPoses_UE4 ? HandPoses_AnimSequenceUE4->GetDataModel() : HandPoses_AnimSequence->GetDataModel(), AnimModToolConfig->HandPoses_PoseFrame, filteredBoneNames);
					}

					if (AnimModToolConfig->HandPoses_StartBlend_FramesNum > 0)
					{
						const int32 startFrameIndex = FMath::Max(0, AnimModToolConfig->HandPoses_StartFrame - AnimModToolConfig->HandPoses_StartBlend_FramesNum / 2);
						const int32 endFrameIndex = FMath::Min(Num, AnimModToolConfig->HandPoses_StartFrame + AnimModToolConfig->HandPoses_StartBlend_FramesNum / 2);
						LerpFrames(Model, Controller, startFrameIndex, endFrameIndex - startFrameIndex, startFrameIndex, endFrameIndex, filteredBoneNames, AnimModToolConfig->HandPoses_StartBlend_BlendOption);
					}

					if (AnimModToolConfig->HandPoses_EndBlend_FramesNum > 0)
					{
						const int32 startFrameIndex = FMath::Max(0, AnimModToolConfig->HandPoses_StartFrame + AnimModToolConfig->HandPoses_FramesNum - AnimModToolConfig->HandPoses_EndBlend_FramesNum / 2);
						const int32 endFrameIndex = FMath::Min(Num, AnimModToolConfig->HandPoses_StartFrame + AnimModToolConfig->HandPoses_FramesNum + AnimModToolConfig->HandPoses_EndBlend_FramesNum / 2);
						LerpFrames(Model, Controller, startFrameIndex, endFrameIndex - startFrameIndex, startFrameIndex, endFrameIndex, filteredBoneNames, AnimModToolConfig->HandPoses_EndBlend_BlendOption);
					}
				}
			}
		}

		if (modifierFilter != EModifierFilter::SIMPLE)
		{
			if (HasTimeManipulation())
			{
				TimeManipulation(Model, Controller, GetTimeManipulationCurve());
			}
		}
	}

	template<class TCancelPredicate, class TPreDelegate, class TPostDelegate>
	void ApplyModifiersToCollection(TArray<UAnimSequence*> animSequencesToMod, TCancelPredicate cancelPredicate, TPreDelegate preDelegate, TPostDelegate postDelegate, const EModifierFilter modifierFilter, const bool transaction) const
	{
		TMap<const USkeleton*, UMirrorDataTable*> MirrorDataTables;
		TMap<const USkeleton*, FBoneContainer> BoneContainers;

		for (UAnimSequence* animSequenceToMod : animSequencesToMod)
		{
			if (cancelPredicate()) break;

			preDelegate();

			USkeleton* nonConstSkeleton = animSequenceToMod->GetSkeleton();
			const USkeleton* skeleton = nonConstSkeleton;

			const FReferenceSkeleton RefSkeleton = skeleton->GetReferenceSkeleton();
			const int32 refSkeletonNum = RefSkeleton.GetNum();

			const FName rootBoneName = RefSkeleton.GetBoneName(0);

			if (rootBoneName == NAME_None) continue;

			const DataModelType* Model = animSequenceToMod->GetDataModel();

			int32 Num = Model->GetNumberOfFrames();

			if (Num == 0) continue;

			if (!BoneContainers.Contains(skeleton))
			{
				FBoneContainer& RequiredBones = BoneContainers.FindOrAdd(skeleton);
				RequiredBones.SetUseRAWData(true);

				TArray<FBoneIndexType> RequiredBoneIndexArray;
				RequiredBoneIndexArray.AddUninitialized(refSkeletonNum);
				for (int32 BoneIndex = 0; BoneIndex < refSkeletonNum; ++BoneIndex)
				{
					RequiredBoneIndexArray[BoneIndex] = BoneIndex;
				}
				RequiredBones.InitializeTo(RequiredBoneIndexArray, UE::Anim::FCurveFilterSettings(), *nonConstSkeleton);
			}

			const int32 NumBones = BoneContainers[skeleton].GetCompactPoseNumBones();

			if (NumBones == 0) continue;

			TMap<FName, int32> nameIndexMapping;

			for (FCompactPoseBoneIndex BoneIndex(0); BoneIndex < NumBones; ++BoneIndex)
			{
				FName boneName = RefSkeleton.GetBoneName(BoneIndex.GetInt());
				nameIndexMapping.FindOrAdd(boneName, BoneIndex.GetInt());
			}

			if (!MirrorDataTables.Contains(skeleton))
			{
				UMirrorDataTable* DataTable = NewObject<UMirrorDataTable>(GetTransientPackage());
				DataTable->RowStruct = const_cast<UScriptStruct*>(ToRawPtr(FMirrorTableRow::StaticStruct()));
				DataTable->Skeleton = nonConstSkeleton;
				DataTable->MirrorFindReplaceExpressions = NewObject<UMirrorTableFindReplaceExpressions>()->FindReplaceExpressions;
				if (DataTable->Skeleton)
				{
					DataTable->FindReplaceMirroredNames();
				}

				MirrorDataTables.Add(skeleton, DataTable);
			}

			TArray<FCompactPose> MirroredPoses;
			MirroredPoses.SetNum(Num + 1);

			if (transaction)
			{
				IAnimationDataController::FScopedBracket ScopedBracket(animSequenceToMod->GetController(), LOCTEXT("ScopedBracket_ApplyModifiers", "Apply Modifiers"));

				ApplyModifiers(animSequenceToMod, skeleton, Num, BoneContainers, MirrorDataTables, MirroredPoses, NumBones, nameIndexMapping, Model, RefSkeleton, rootBoneName, modifierFilter);
			}
			else
			{
				ApplyModifiers(animSequenceToMod, skeleton, Num, BoneContainers, MirrorDataTables, MirroredPoses, NumBones, nameIndexMapping, Model, RefSkeleton, rootBoneName, modifierFilter);
			}

			postDelegate();
		}
	}

	bool CanApplyModifiers() const
	{
		bool result = false;
		
		if (AnimModToolConfig->bReverse)
		{
			result = true;
		}

		if (AnimModToolConfig->bMirror)
		{
			result = true;
		}

		if (AnimModToolConfig->bRepeatFrames)
		{
			for (const TPair<FGuid, int32>& repeatFramesKeyEntry : AnimModToolConfig->RepeatFramesKeys)
			{
				if (repeatFramesKeyEntry.Value != INDEX_NONE)
				{
					result = true;
					break;
				}
			}
		}

		if (AnimModToolConfig->bMakeLoopedDynamic)
		{
			if (AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum > 0 || AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum > 0)
			{
				result = true;
			}
		}

		if (AnimModToolConfig->bRemoveFrames)
		{
			if (AnimModToolConfig->RemoveFramesFramesNum > 0)
			{
				result = true;
			}
		}

		if (AnimModToolConfig->bBlendWithOther)
		{
			if (HasBlendWithOtherTarget())
			{
				if (AnimModToolConfig->Blend_Target_FramesNum > 0)
				{
					result = true;
				}
			}
		}

		if (HasTimeManipulation())
		{
			result = true;
		}

		if (AnimModToolConfig->bHandPoses)
		{
			if (AnimModToolConfig->HandPoses_FramesNum > 0)
			{
				result = true;
			}
		}

		return result;
	}

	void ApplyModifiers()
	{
		FScopedSlowTask scopedSlowTask(100, LOCTEXT("ScopedSlowTaskMsg", "Modifying animation assets..."));
		scopedSlowTask.MakeDialog(true);  // We display the Cancel button here

		TArray<UAnimSequence*> animSequences;

		for (UAnimSequence* animSequence : AnimSequencesToMod)
		{
			if (animSequence->GetSkeleton() == AnimSequenceToMod->GetSkeleton())
			{
				animSequences.Add(animSequence);
			}
		}

		ApplyModifiersToCollection(animSequences
			, [&scopedSlowTask]() -> bool { return scopedSlowTask.ShouldCancel(); }
		, [&scopedSlowTask, this, animSequences]() { scopedSlowTask.EnterProgressFrame(100.f / animSequences.Num()); }
		, []() { FPlatformProcess::SleepNoStats(0.05f); }, EModifierFilter::ALL, true);

		SetFrames(AnimSequenceToMod, animSequences[0]);
		SetFrames(AnimSequenceToModDeferred, animSequences[0]);

		Reset();
	}

	void RefreshPreview()
	{
		const uint32 newModificationHash = CalculateModificationHash();

		if (ModificationHash != newModificationHash)
		{
			ModificationHash = newModificationHash;

			if (PersonaToolkit.IsValid())
			{
				{
					IAnimationDataController::FScopedBracket ScopedBracket(AnimSequenceToMod->GetController(), LOCTEXT("ScopedBracket_SetFrames", "Refresh Preview"));

					SetFrames(AnimSequenceToMod, AnimSequencesToMod[0]);

					TArray<UAnimSequence*> animSequences;
					animSequences.Add(AnimSequenceToMod);

					ApplyModifiersToCollection(animSequences
						, []() -> bool { return false; }
						, []() {}
						, []() {}
						, EModifierFilter::SIMPLE, false);
				}

				{
					IAnimationDataController::FScopedBracket ScopedBracket(AnimSequenceToModDeferred->GetController(), LOCTEXT("ScopedBracket_SetFrames", "Refresh Preview Deferred"));

					SetFrames(AnimSequenceToModDeferred, AnimSequenceToMod);

					TArray<UAnimSequence*> animSequences;
					animSequences.Add(AnimSequenceToModDeferred);

					ApplyModifiersToCollection(animSequences
						, []() -> bool { return false; }
						, []() {}
						, []() {}
						, EModifierFilter::DEFERRED, false);
				}

				PersonaToolkit->GetPreviewScene()->InvalidateViews();
			}
		}
	}

	virtual void RefreshTabs() {}

	void Reset()
	{
		LeftSuffix = FText::FromString("_l");
		RightSuffix = FText::FromString("_r");

		AnimModToolConfig->bReverse = 0;
		AnimModToolConfig->bReverseClearRootBoneZeroFrameOffset = 0;
		AnimModToolConfig->bMirror = 0;
		AnimModToolConfig->bRepeatFrames = 0;
		AnimModToolConfig->bMakeLoopedDynamic = 0;
		AnimModToolConfig->bRemoveFrames = 0;
		AnimModToolConfig->bBlendWithOther = 0;
		AnimModToolConfig->bUseStaticFrameFromTarget = 0;
		AnimModToolConfig->bInsertAsNewFrames = 0;
		AnimModToolConfig->bCurveChanged = 0;
		SetTimeManipulation_Internal(0);
		AnimModToolConfig->bHandPoses = 0;
		AnimModToolConfig->bHandPoses_Left = 1;
		AnimModToolConfig->bHandPoses_Right = 1;

		AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum = 0;
		AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum = 0;
		AnimModToolConfig->MakeLoopedDynamicBalance = 0.5f;
		AnimModToolConfig->MakeLoopedDynamicBlendOption = EAlphaBlendOption::Linear;
		AnimModToolConfig->MakeLoopedDynamicBoneName = NAME_None;

		AnimModToolConfig->RemoveFramesStartFrame = 0;
		AnimModToolConfig->RemoveFramesFramesNum = 1;

		AnimModToolConfig->Blend_StartFrame = 0;
		AnimModToolConfig->Blend_Target_StartFrame = 0;
		AnimModToolConfig->Blend_Target_FramesNum = 1;
		AnimModToolConfig->Blend_AFramesNum = 0;
		AnimModToolConfig->Blend_BFramesNum = 0;
		AnimModToolConfig->Blend_Target = nullptr;
		AnimModToolConfig->Blend_Target_ABlendOption = EAlphaBlendOption::Linear;
		AnimModToolConfig->Blend_Target_BBlendOption = EAlphaBlendOption::Linear;

		ModificationHash = CalculateModificationHash();

		if (AnimSequenceToMod)
		{
			if (FFloatCurve* timeManipulationCurve = GetTimeManipulationCurve())
			{
				TArray<FRichCurveKey> keys;
				keys.Add(FRichCurveKey(0, 1));

				timeManipulationCurve->FloatCurve.Keys = keys;
			}
		}

		AnimModToolConfig->HandPoses_StartFrame = 0;
		AnimModToolConfig->HandPoses_FramesNum = 1;
		AnimModToolConfig->HandPoses_StartBlend_FramesNum = 0;
		AnimModToolConfig->HandPoses_StartBlend_BlendOption = EAlphaBlendOption::Linear;
		AnimModToolConfig->HandPoses_EndBlend_FramesNum = 0;
		AnimModToolConfig->HandPoses_EndBlend_BlendOption = EAlphaBlendOption::Linear;
	}
	
	bool GetReverse() const { return AnimModToolConfig->bReverse; }

	void SetReverse(const bool value)
	{
		if (AnimModToolConfig->bReverse != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);

			AnimModToolConfig->Modify();

			AnimModToolConfig->bReverse = value;

			RefreshPreview();
		}
	}

	bool GetReverseClearRootBoneZeroFrameOffset() const { return AnimModToolConfig->bReverseClearRootBoneZeroFrameOffset; }

	void SetReverseClearRootBoneZeroFrameOffset(const bool value)
	{
		if (AnimModToolConfig->bReverseClearRootBoneZeroFrameOffset != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bReverseClearRootBoneZeroFrameOffset = value;

			RefreshPreview();
		}
	}

	bool GetEnableRootMotion() const { return AnimSequenceToModDeferred->bEnableRootMotion; }

	void SetEnableRootMotion(bool value) { AnimSequenceToModDeferred->bEnableRootMotion = value; }

	bool GetMirror() const { return AnimModToolConfig->bMirror; }

	void SetMirror(const bool value)
	{
		if (AnimModToolConfig->bMirror != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bMirror = value;

			RefreshPreview();
		}
	}

	bool GetRepeatFrames() const { return AnimModToolConfig->bRepeatFrames; }

	void SetRepeatFrames(const bool value)
	{
		if (AnimModToolConfig->bRepeatFrames != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bRepeatFrames = value;

			RefreshPreview();
		}
	}

	void AddRepeatFrameEntry(const FGuid& guid)
	{
		if (!AnimModToolConfig->RepeatFramesKeys.Contains(guid))
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->RepeatFramesKeys.Add(guid, 0);
			AnimModToolConfig->RepeatFramesValues.Add(guid, 1);

			RefreshPreview();
		}
	}

	void SetRepeatFrameKey(const FGuid& guid, const int32 value, bool refreshPreview)
	{
		if (AnimModToolConfig->RepeatFramesKeys.Contains(guid))
		{
			if (refreshPreview)
			{
				AnimModToolConfig->RepeatFramesKeys[guid] = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->RepeatFramesKeys[guid];

				Int32ValueOnSliderBeginMovement = INDEX_NONE;

				if (AnimModToolConfig->RepeatFramesKeys[guid] != value)
				{
					FScopedTransaction scopedTransaction(scopedTransactionText);

					AnimModToolConfig->Modify();

					AnimModToolConfig->RepeatFramesKeys[guid] = value;
				}
			}
			else
			{
				AnimModToolConfig->RepeatFramesKeys[guid] = value;
			}
		}

		if (refreshPreview) RefreshPreview();
	}

	void SetRepeatFrameValue(const FGuid& guid, const int32 value, bool refreshPreview)
	{
		if (AnimModToolConfig->RepeatFramesKeys.Contains(guid) && AnimModToolConfig->RepeatFramesKeys[guid] != INDEX_NONE)
		{
			if (refreshPreview)
			{
				AnimModToolConfig->RepeatFramesValues[guid] = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->RepeatFramesValues[guid];

				Int32ValueOnSliderBeginMovement = INDEX_NONE;

				if (AnimModToolConfig->RepeatFramesValues[guid] != value)
				{
					FScopedTransaction scopedTransaction(scopedTransactionText);

					AnimModToolConfig->Modify();

					AnimModToolConfig->RepeatFramesValues[guid] = value;
				}
			}
			else
			{
				AnimModToolConfig->RepeatFramesValues[guid] = value;
			}
		}

		if (refreshPreview) RefreshPreview();
	}

	void RemoveRepeatFrameEntry(const FGuid& guid)
	{
		if (AnimModToolConfig->RepeatFramesKeys.Contains(guid))
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->RepeatFramesKeys.Remove(guid);
			AnimModToolConfig->RepeatFramesValues.Remove(guid);

			RefreshPreview();
		}
	}

	bool GetMakeLoopedDynamic() const { return AnimModToolConfig->bMakeLoopedDynamic; }

	void SetMakeLoopedDynamic(const bool value)
	{
		if (AnimModToolConfig->bMakeLoopedDynamic != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);

			AnimModToolConfig->Modify();

			AnimModToolConfig->bMakeLoopedDynamic = value;

			RefreshPreview();
		}
	}

	int32 GetMakeLoopedDynamicStartBlendFramesNum() const { return AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum; }

	void SetMakeLoopedDynamicStartBlendFramesNum(const int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum = value;
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->MakeLoopedDynamicStartBlendFramesNum = value;
		}
	}

	int32 GetMakeLoopedDynamicEndBlendFramesNum() const { return AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum; }

	void SetMakeLoopedDynamicEndBlendFramesNum(const int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum = value;
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->MakeLoopedDynamicEndBlendFramesNum = value;
		}
	}

	float GetMakeLoopedDynamicBalance() const { return AnimModToolConfig->MakeLoopedDynamicBalance; }

	void SetMakeLoopedDynamicBalance(const float value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->MakeLoopedDynamicBalance = FloatValueOnSliderBeginMovement != INDEX_NONE ? FloatValueOnSliderBeginMovement : AnimModToolConfig->MakeLoopedDynamicBalance;

			FloatValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->MakeLoopedDynamicBalance != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->MakeLoopedDynamicBalance = value;
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->MakeLoopedDynamicBalance = value;
		}
	}

	EAlphaBlendOption GetMakeLoopedDynamicBlendOption() const { return AnimModToolConfig->MakeLoopedDynamicBlendOption; }

	void SetMakeLoopedDynamicBlendOption(const EAlphaBlendOption value)
	{
		if (AnimModToolConfig->MakeLoopedDynamicBlendOption != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);

			AnimModToolConfig->Modify();

			AnimModToolConfig->MakeLoopedDynamicBlendOption = value;

			RefreshPreview();
		}
	}

	FName GetMakeLoopedDynamicBoneName() const { return AnimModToolConfig->MakeLoopedDynamicBoneName; }

	void SetMakeLoopedDynamicBoneName(const FName value)
	{
		if (AnimModToolConfig->MakeLoopedDynamicBoneName != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);

			AnimModToolConfig->Modify();

			AnimModToolConfig->MakeLoopedDynamicBoneName = value;

			RefreshPreview();
		}
	}

	bool GetRemoveFrames() const { return AnimModToolConfig->bRemoveFrames; }

	void SetRemoveFrames(const bool value)
	{
		if (AnimModToolConfig->bRemoveFrames != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bRemoveFrames = value;

			RefreshPreview();
		}
	}

	int32 GetRemoveFramesStartFrame() const { return AnimModToolConfig->RemoveFramesStartFrame; }

	void SetRemoveFramesStartFrame(const int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->RemoveFramesStartFrame = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->RemoveFramesStartFrame;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->RemoveFramesStartFrame != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->RemoveFramesStartFrame = value;

				if (AnimModToolConfig->RemoveFramesStartFrame + AnimModToolConfig->RemoveFramesFramesNum > AnimSequencesToMod[0]->GetDataModel()->GetNumberOfFrames())
				{
					AnimModToolConfig->RemoveFramesFramesNum = AnimSequencesToMod[0]->GetDataModel()->GetNumberOfFrames() - AnimModToolConfig->RemoveFramesStartFrame;
				}
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->RemoveFramesStartFrame = value;

			if (AnimModToolConfig->RemoveFramesStartFrame + AnimModToolConfig->RemoveFramesFramesNum > AnimSequencesToMod[0]->GetDataModel()->GetNumberOfFrames())
			{
				AnimModToolConfig->RemoveFramesFramesNum = AnimSequencesToMod[0]->GetDataModel()->GetNumberOfFrames() - AnimModToolConfig->RemoveFramesStartFrame;
			}
		}
	}

	int32 GetRemoveFramesFramesNumMaxValue() const { return AnimSequencesToMod[0]->GetDataModel()->GetNumberOfFrames() - AnimModToolConfig->RemoveFramesStartFrame; }

	int32 GetRemoveFramesFramesNum() const { return AnimModToolConfig->RemoveFramesFramesNum; }

	void SetRemoveFramesFramesNum(const int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->RemoveFramesFramesNum = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->RemoveFramesFramesNum;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->RemoveFramesFramesNum != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->RemoveFramesFramesNum = value;
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->RemoveFramesFramesNum = value;
		}
	}

	bool GetBlendWithOther() const { return AnimModToolConfig->bBlendWithOther; }

	void SetBlendWithOther(const bool value)
	{
		if (AnimModToolConfig->bBlendWithOther != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bBlendWithOther = value;

			RefreshPreview();
		}
	}

	void RecalcBlendWithOtherFramesNum()
	{
		if (UAnimSequence* blendTarget = AnimModToolConfig->Blend_Target.Get())
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->Blend_Target_FramesNum = FMath::Min(AnimModToolConfig->Blend_Target_FramesNum, GetBlendWithOtherTargetFramesNumMax());
		}
	}

	int32 GetBlendWithOtherStartFrame() const { return AnimModToolConfig->Blend_StartFrame; }

	void SetBlendWithOtherStartFrame(int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->Blend_StartFrame = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->Blend_StartFrame;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->Blend_StartFrame != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->Blend_StartFrame = value;

				RecalcBlendWithOtherFramesNum();
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->Blend_StartFrame = value;

			RecalcBlendWithOtherFramesNum();
		}
	}
	
	bool HasBlendWithOtherTarget() const { return AnimModToolConfig->Blend_Target.operator bool(); }

	bool SetBlendWithOtherTarget(const FAssetData& assetData)
	{
		UAnimSequence* animSequence = Cast<UAnimSequence>(assetData.GetAsset());

		if (animSequence != AnimModToolConfig->Blend_Target.Get())
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->Blend_Target = animSequence;

			RecalcBlendWithOtherFramesNum();

			RefreshPreview();

			return true;
		}

		return false;
	}

	int32 GetBlendWithOtherTargetStartFrameMax() const
	{
		if (UAnimSequence* blendTarget = AnimModToolConfig->Blend_Target.Get())
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			return blendTarget->GetDataModel()->GetNumberOfFrames() - 1;
		}

		return 0;
	}

	int32 GetBlendWithOtherTargetStartFrame() const { return AnimModToolConfig->Blend_Target_StartFrame; }

	void SetBlendWithOtherTargetStartFrame(int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->Blend_Target_StartFrame = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->Blend_Target_StartFrame;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->Blend_Target_StartFrame != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->Blend_Target_StartFrame = value;

				RecalcBlendWithOtherFramesNum();
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->Blend_Target_StartFrame = value;

			RecalcBlendWithOtherFramesNum();
		}
	}

	bool GetUseStaticFrameFromTarget() const { return AnimModToolConfig->bUseStaticFrameFromTarget; }

	void SetUseStaticFrameFromTarget(bool value)
	{
		if (AnimModToolConfig->bUseStaticFrameFromTarget != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bUseStaticFrameFromTarget = value;

			RecalcBlendWithOtherFramesNum();

			RefreshPreview();
		}
	}

	int32 GetBlendWithOtherTargetFramesNumMax() const
	{
		if (UAnimSequence* blendTarget = AnimModToolConfig->Blend_Target.Get())
		{
			const int32 framesNum = AnimSequencesToMod[0]->GetDataModel()->GetNumberOfFrames();

			const int32 targetFramesNum = blendTarget->GetDataModel()->GetNumberOfFrames();

			if (AnimModToolConfig->bUseStaticFrameFromTarget)
			{
				return AnimModToolConfig->bInsertAsNewFrames ? framesNum : (framesNum - AnimModToolConfig->Blend_StartFrame);
			}
			else if (AnimModToolConfig->bInsertAsNewFrames)
			{
				return targetFramesNum - AnimModToolConfig->Blend_Target_StartFrame;
			}
			else
			{
				return FMath::Min(targetFramesNum - AnimModToolConfig->Blend_Target_StartFrame, framesNum - AnimModToolConfig->Blend_StartFrame);
			}
		}

		return 0;
	}

	int32 GetBlendWithOtherTargetFramesNum() const { return AnimModToolConfig->Blend_Target_FramesNum; }

	void SetBlendWithOtherTargetFramesNum(int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->Blend_Target_FramesNum = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->Blend_Target_FramesNum;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->Blend_Target_FramesNum != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->Blend_Target_FramesNum = value;
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->Blend_Target_FramesNum = value;
		}
	}

	int32 GetBlendWithOtherBlendFramesNumMax() const
	{
		int32 result = AnimSequencesToMod[0]->GetDataModel()->GetNumberOfFrames();

		if (UAnimSequence* blendTarget = AnimModToolConfig->Blend_Target.Get())
		{
			if (AnimModToolConfig->bInsertAsNewFrames)
			{
				result += AnimModToolConfig->Blend_Target_FramesNum;
			}
		}

		return result;
	}

	int32 GetBlend_Target_ABlendFramesNum() const { return AnimModToolConfig->Blend_AFramesNum; }

	void SetBlend_Target_ABlendFramesNum(int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->Blend_AFramesNum = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->Blend_AFramesNum;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->Blend_AFramesNum != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->Blend_AFramesNum = value;
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->Blend_AFramesNum = value;
		}
	}

	EAlphaBlendOption GetBlend_Target_ABlendOption() const { return AnimModToolConfig->Blend_Target_ABlendOption; }

	void SetBlend_Target_ABlendOption(const EAlphaBlendOption value)
	{
		if (AnimModToolConfig->Blend_Target_ABlendOption != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->Blend_Target_ABlendOption = value;

			RefreshPreview();
		}
	}

	int32 GetBlend_Target_BBlendFramesNum() const { return AnimModToolConfig->Blend_BFramesNum; }

	void SetBlend_Target_BBlendFramesNum(int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->Blend_BFramesNum = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->Blend_BFramesNum;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->Blend_BFramesNum != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->Blend_BFramesNum = value;
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->Blend_BFramesNum = value;
		}
	}

	EAlphaBlendOption GetBlend_Target_BBlendOption() const { return AnimModToolConfig->Blend_Target_BBlendOption; }

	void SetBlend_Target_BBlendOption(const EAlphaBlendOption value)
	{
		if (AnimModToolConfig->Blend_Target_BBlendOption != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->Blend_Target_BBlendOption = value;

			RefreshPreview();
		}
	}

	bool GetBlendWithOtherInsertAsNewFrames() const { return AnimModToolConfig->bInsertAsNewFrames; }

	void SetBlendWithOtherInsertAsNewFrames(bool value)
	{
		if (AnimModToolConfig->bInsertAsNewFrames != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bInsertAsNewFrames = value;

			RecalcBlendWithOtherFramesNum();

			RefreshPreview();
		}
	}

	bool GetTimeManipulation() const { return AnimModToolConfig->bTimeManipulation; }

	void SetTimeManipulation(const bool value)
	{
		if (SetTimeManipulation_Internal(value))
		{
			RefreshPreview();
		}
	}

	bool SetTimeManipulation_Internal(const bool value)
	{
		if (AnimModToolConfig->bTimeManipulation != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bTimeManipulation = value;

			RefreshTabs();

			return true;
		}

		return false;
	}

	const FFloatCurve* GetTimeManipulationCurve() const
	{
		return &AnimModToolConfig->TimeManipulationFloatCurve;
	}

	FFloatCurve* GetTimeManipulationCurve()
	{
		return &AnimModToolConfig->TimeManipulationFloatCurve;
	}

	bool HasTimeManipulation() const
	{
		if (AnimModToolConfig->bTimeManipulation)
		{
			if (const FFloatCurve* timeManipulationCurve = GetTimeManipulationCurve())
			{
				for (size_t i = 0; i < timeManipulationCurve->FloatCurve.GetNumKeys(); i++)
				{
					if (timeManipulationCurve->FloatCurve.Keys[i].Value != 1)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	bool GetHandPoses() const { return AnimModToolConfig->bHandPoses; }

	void SetHandPoses(const bool value)
	{
		if (AnimModToolConfig->bHandPoses != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bHandPoses = value;

			RefreshPreview();
		}
	}

	bool GetHandPosesUE4() const { return AnimModToolConfig->HandPoses_UE4; }

	void SetHandPosesUE4(bool ue4)
	{
		if (AnimModToolConfig->HandPoses_UE4 != ue4)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);

			AnimModToolConfig->Modify();

			AnimModToolConfig->HandPoses_UE4 = ue4;

			RefreshPreview();
		}
	}

	int32 GetHandPosesPoseFrame() const { return AnimModToolConfig->HandPoses_PoseFrame; }

	void SetHandPosesPoseFrame(int32 value, bool refreshPreview)
	{
		if (AnimModToolConfig->HandPoses_PoseFrame != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->HandPoses_PoseFrame = value;
		}

		if (refreshPreview) RefreshPreview();
	}

	int32 GetHandPosesStartFrame() const { return AnimModToolConfig->HandPoses_StartFrame; }

	void SetHandPosesStartFrame(int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->HandPoses_StartFrame = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->HandPoses_StartFrame;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->HandPoses_StartFrame != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->HandPoses_StartFrame = value;

				RecalcHandPosesFramesNum();
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->HandPoses_StartFrame = value;

			RecalcHandPosesFramesNum();
		}
	}

	void RecalcHandPosesFramesNum()
	{
		FScopedTransaction scopedTransaction(scopedTransactionText);
		
		AnimModToolConfig->Modify();

		AnimModToolConfig->HandPoses_FramesNum = FMath::Min(AnimModToolConfig->HandPoses_FramesNum, GetHandPosesFramesNumMax());
	}

	int32 GetHandPosesFramesNumMax() const
	{
		return AnimSequenceToMod->GetDataModel()->GetNumberOfFrames() - AnimModToolConfig->HandPoses_StartFrame;
	}

	int32 GetHandPosesFramesNum() const { return AnimModToolConfig->HandPoses_FramesNum; }

	void SetHandPosesFramesNum(int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->HandPoses_FramesNum = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->HandPoses_FramesNum;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->HandPoses_FramesNum != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->HandPoses_FramesNum = value;
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->HandPoses_FramesNum = value;
		}
	}

	int32 GetHandPosesStartBlendFramesNumMax() const
	{
		return FMath::Min(GetHandPosesFramesNum(), 2 * GetHandPosesStartFrame());
	}

	int32 GetHandPoses_StartBlend_FramesNum() const { return AnimModToolConfig->HandPoses_StartBlend_FramesNum; }

	void SetHandPoses_StartBlend_FramesNum(int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->HandPoses_StartBlend_FramesNum = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->HandPoses_StartBlend_FramesNum;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->HandPoses_StartBlend_FramesNum != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->HandPoses_StartBlend_FramesNum = value;
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->HandPoses_StartBlend_FramesNum = value;
		}
	}

	EAlphaBlendOption GetHandPoses_StartBlend_BlendOption() const { return AnimModToolConfig->HandPoses_StartBlend_BlendOption; }

	void SetHandPoses_StartBlend_BlendOption(const EAlphaBlendOption value)
	{
		if (AnimModToolConfig->HandPoses_StartBlend_BlendOption != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->HandPoses_StartBlend_BlendOption = value;

			RefreshPreview();
		}
	}

	int32 GetHandPosesEndBlendFramesNumMax() const
	{
		return FMath::Min(GetHandPosesFramesNum(), (AnimSequenceToMod->GetDataModel()->GetNumberOfFrames() - (GetHandPosesStartFrame() + GetHandPosesFramesNum())) * 2);
	}

	int32 GetHandPoses_EndBlend_FramesNum() const { return AnimModToolConfig->HandPoses_EndBlend_FramesNum; }

	void SetHandPoses_EndBlend_FramesNum(int32 value, bool refreshPreview)
	{
		if (refreshPreview)
		{
			AnimModToolConfig->HandPoses_EndBlend_FramesNum = Int32ValueOnSliderBeginMovement != INDEX_NONE ? Int32ValueOnSliderBeginMovement : AnimModToolConfig->HandPoses_EndBlend_FramesNum;

			Int32ValueOnSliderBeginMovement = INDEX_NONE;

			if (AnimModToolConfig->HandPoses_EndBlend_FramesNum != value)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText);

				AnimModToolConfig->Modify();

				AnimModToolConfig->HandPoses_EndBlend_FramesNum = value;
			}

			RefreshPreview();
		}
		else
		{
			AnimModToolConfig->HandPoses_EndBlend_FramesNum = value;
		}
	}

	EAlphaBlendOption GetHandPoses_EndBlend_BlendOption() const { return AnimModToolConfig->HandPoses_EndBlend_BlendOption; }

	void SetHandPoses_EndBlend_BlendOption(const EAlphaBlendOption value)
	{
		if (AnimModToolConfig->HandPoses_EndBlend_BlendOption != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->HandPoses_EndBlend_BlendOption = value;

			RefreshPreview();
		}
	}

	bool GetHandPoses_Left() const { return AnimModToolConfig->bHandPoses_Left; }

	void SetHandPoses_Left(const bool value)
	{
		if (AnimModToolConfig->bHandPoses_Left != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bHandPoses_Left = value;

			RefreshPreview();
		}
	}

	bool GetHandPoses_Right() const { return AnimModToolConfig->bHandPoses_Right; }

	void SetHandPoses_Right(const bool value)
	{
		if (AnimModToolConfig->bHandPoses_Right != value)
		{
			FScopedTransaction scopedTransaction(scopedTransactionText);
			
			AnimModToolConfig->Modify();

			AnimModToolConfig->bHandPoses_Right = value;

			RefreshPreview();
		}
	}

protected:
	TArray<UAnimSequence*> AnimSequencesToMod;
	TObjectPtr<UAnimSequenceCustom> AnimSequenceToMod;
	TObjectPtr<UAnimSequenceCustom> AnimSequenceToModDeferred;

	TSharedPtr<class IPersonaToolkit> PersonaToolkit;

	FText LeftSuffix;
	FText RightSuffix;

	TObjectPtr<UAnimSequence> HandPoses_AnimSequence;

	TObjectPtr<UAnimSequence> HandPoses_AnimSequenceUE4;

	TObjectPtr<UAnimModToolConfig> AnimModToolConfig;

	uint32 ModificationHash = 0;

	int32 Int32ValueOnSliderBeginMovement = INDEX_NONE;

	float FloatValueOnSliderBeginMovement = INDEX_NONE;
};

//--------------------------------------------------------------------
// FAnimModEditor
//--------------------------------------------------------------------

const FName AnimModEditorAppIdentifier = FName(TEXT("AnimModEditorAppIdentifier"));

namespace AnimModEditorModes
{
	// Mode identifiers
	const FName SingleAssetEditorMode(TEXT("SingleAssetEditorMode"));
}

namespace AnimModEditorTabs
{
	// Tab identifiers
	const FName ViewportTab(TEXT("Viewport"));
	const FName ModifiersTab(TEXT("Modifiers"));
	const FName PlayerTab(TEXT("Player"));
	const FName EditCurvesTab(TEXT("EditCurvesTab"));
	const FName TimeManipulationTab(TEXT("TimeManipulation"));
}

//--------------------------------------------------------------------
// FApplicationMode_AnimModEditor_SingleAssetEditorMode
//--------------------------------------------------------------------

class FApplicationMode_AnimModEditor_SingleAssetEditorMode : public FApplicationMode
{
public:
	FApplicationMode_AnimModEditor_SingleAssetEditorMode(TSharedRef<class FWorkflowCentricApplication> InHostingApp, const TSharedRef<IPersonaPreviewScene>& previewScene);

	/** FApplicationMode interface */
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

protected:
	virtual void AddTabFactory(FCreateWorkflowTabFactory FactoryCreator) override;
	virtual void RemoveTabFactory(FName TabFactoryID) override;
protected:
	/** The hosting app */
	TWeakPtr<class FWorkflowCentricApplication> HostingAppPtr;

	/** The tab factories we support */
	FWorkflowAllowedTabSet TabFactories;
};

//--------------------------------------------------------------------
// FAnimModEditor
//--------------------------------------------------------------------

class FAnimModEditor : public IAnimationEditor, public FGCObject, public FTickableEditorObject, public AnimModContext, public FEditorUndoClient
{
public:
	FAnimModEditor();
	virtual ~FAnimModEditor();
	void InitAnimModEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, TArray<UAnimSequence*>& animSequencesToMod);

	/** IToolkit interface */
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override { FAssetEditorToolkit::UnregisterTabSpawners(InTabManager); }
	virtual FName GetToolkitFName() const override { return FName("Anim Mod Tools"); }
	virtual FText GetBaseToolkitName() const override { return LOCTEXT("FAnimModEditor_BaseToolkitName", "Anim Mod Tools"); }
	virtual FString GetWorldCentricTabPrefix() const override { return LOCTEXT("FAnimModEditor_WorldCentricTabPrefix", "AnimModTools ").ToString(); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f); }

	virtual FText GetToolkitName() const override { return FPersonaAssetEditorToolkit::GetToolkitName(); }

	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	/** FTickableEditorObject Interface */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FAnimModEditor, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		for (UAnimSequence* animSequenceToMod : AnimSequencesToMod)
		{
			Collector.AddReferencedObject(animSequenceToMod);
		}
	}
	virtual FString GetReferencerName() const override { return TEXT("FAnimModEditor"); }

	UObject* HandleGetAsset() { return GetEditingObject(); }

	TSharedRef<SWidget> GenerateModifiersWidget();

	TSharedRef<SWidget> GenerateTimeManipulationWidget(const FWorkflowTabSpawnInfo& Info)
	{
		TSharedRef<SAnimSequenceCurveEditor_COPY> NewCurveEditor = SNew(SAnimSequenceCurveEditor_COPY, GetPersonaToolkit()->GetPreviewScene(), AnimSequenceToMod)
			.ExternalTimeSliderController(SequenceEditorPtr->GetExternalTimeSliderController())
			.TabManager(TabManager);

		FSmartName smartName;
		TUniquePtr<FRichCurveEditorModelNamed_COPY> NewCurveModel = MakeUnique<FRichCurveEditorModelNamed_COPY>(smartName, ERawCurveTrackTypes::RCT_Float, 0, AnimModToolConfig);
		NewCurveModel->SetColor(FLinearColor::Green, false);
		NewCurveModel->SetIsKeyDrawEnabled(true);
		NewCurveModel->OnCurveModified().AddLambda([this]() { HandleCurveChanged(); });
		NewCurveEditor->CurveEditor->AddCurve(MoveTemp(NewCurveModel));

		FCurveEditorTreeItem* TreeItem = NewCurveEditor->CurveEditor->AddTreeItem(FCurveEditorTreeItemID());
		TreeItem->SetStrongItem(MakeShared<FAnimSequenceCurveEditorItem>(smartName, ERawCurveTrackTypes::RCT_Float, 0, AnimSequenceToMod, LOCTEXT("TimeManipulationCurve_DisplayName", "Time Manipulation"), FLinearColor::Green, FSimpleDelegate::CreateLambda([this]() {}), TreeItem->GetID()));

		// Update selection
		TArray<FCurveEditorTreeItemID> NewSelection;
		NewSelection.Add(TreeItem->GetID());
		NewCurveEditor->CurveEditor->SetTreeSelection(MoveTemp(NewSelection));

		return NewCurveEditor;
	}

	TSharedRef<SWidget> GeneratePlayerWidget(const FWorkflowTabSpawnInfo& Info) { return SequenceEditorPtr.ToSharedRef(); }

	virtual void SetAnimationAsset(UAnimationAsset* AnimAsset) override {}

	virtual IAnimationSequenceBrowser* GetAssetBrowser() const override { return nullptr; }

	virtual TSharedRef<class IPersonaToolkit> GetPersonaToolkit() const override { return PersonaToolkit.ToSharedRef(); }

	virtual void EditCurves(UAnimSequenceBase* InAnimSequence, const TArray<FCurveEditInfo>& InCurveInfo, const TSharedPtr<ITimeSliderController>& InExternalTimeSliderController) override
	{
		FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");

		if (!AnimCurveDocumentTab.IsValid())
		{
			TSharedRef<IAnimSequenceCurveEditor> NewCurveEditor = PersonaModule.CreateCurveWidgetForAnimDocument(SharedThis(this), GetPersonaToolkit()->GetPreviewScene(), InAnimSequence, InExternalTimeSliderController, TabManager);
			CurveEditor = NewCurveEditor;

			TSharedPtr<SDockTab> CurveTab = SNew(SDockTab)
				.Label(LOCTEXT("CurveEditorTabTitle", "Curve Editor"))
				.TabRole(ETabRole::DocumentTab)
				.TabColorScale(GetTabColorScale())
				[
					NewCurveEditor
				];

			AnimCurveDocumentTab = CurveTab;

			TabManager->InsertNewDocumentTab(AnimModEditorTabs::EditCurvesTab, FTabManager::ESearchPreference::RequireClosedTab, CurveTab.ToSharedRef());
		}
		else
		{
			TabManager->DrawAttention(AnimCurveDocumentTab.Pin().ToSharedRef());
		}

		check(CurveEditor.IsValid());

		for (const FCurveEditInfo& CurveInfo : InCurveInfo)
		{
			CurveEditor.Pin()->AddCurve(CurveInfo.CurveDisplayName, CurveInfo.CurveColor, CurveInfo.Name, CurveInfo.Type, CurveInfo.CurveIndex, CurveInfo.OnCurveModified);
		}

		CurveEditor.Pin()->ZoomToFit();
	}

	virtual void StopEditingCurves(const TArray<FCurveEditInfo>& InCurveInfo) override {}

private:

	void OnReimportAnimation();

	void ConditionalRefreshEditor(UObject* InObject);

	void HandlePostReimport(UObject* InObject, bool bSuccess);

	void HandlePostImport(class UFactory* InFactory, UObject* InObject) { ConditionalRefreshEditor(InObject); }

	void HandleCurveChanged()
	{		
		AnimModToolConfig->Modify();

		AnimModToolConfig->bCurveChanged = 1 - AnimModToolConfig->bCurveChanged;

		RefreshPreview();
	}

	virtual void RefreshTabs() override
	{
		if (AnimModToolConfig->bTimeManipulation)
		{
			TabManager->TryInvokeTab(AnimModEditorTabs::TimeManipulationTab);
		}
		else
		{
			if (TSharedPtr<SDockTab> timeManipulationTab = TabManager->FindExistingLiveTab(AnimModEditorTabs::TimeManipulationTab))
			{
				timeManipulationTab->RequestCloseTab();
			}
		}

		RefreshComboBoxes();

		RebuildRepeatFramesEntries();

		RebuildBlendEntries();

		RefreshPreview();
	}

private:

	void HandleOnPreviewSceneSettingsCustomized(IDetailLayoutBuilder& DetailBuilder) const { /* DetailBuilder.HideCategory("Animation Blueprint"); */ }

	void RebuildRepeatFramesEntry(const FGuid& guid)
	{
		const int32 Num = AnimSequencesToMod[0]->GetDataModel()->GetNumberOfFrames();

		RepeatFramesVerticalBoxPtr->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox).Tag(FName(guid.ToString()))

					+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
					[
						SNew(SNumericEntryBox<int32>)
							.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
							.AllowSpin(true)
							.MaxValue(Num)
							.MinValue(0)
							.MaxSliderValue(Num)
							.MinSliderValue(0)
							.OnBeginSliderMovement_Lambda([this, guid]() { Int32ValueOnSliderBeginMovement = AnimModToolConfig->RepeatFramesValues[guid]; })
							.Value_Lambda([this, guid]() { return AnimModToolConfig->RepeatFramesKeys[guid]; })
							.OnValueCommitted_Lambda([this, guid](int32 Value, ETextCommit::Type CommitType) { SetRepeatFrameKey(guid, Value, true); })
							.OnValueChanged_Lambda([this, guid](int32 Value) { SetRepeatFrameKey(guid, Value, false); })
					]
					+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
					[
						SNew(SNumericEntryBox<int32>)
							.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
							.AllowSpin(true)
							.MaxValue(Num)
							.MinValue(1)
							.MaxSliderValue(Num)
							.MinSliderValue(1)
							.OnBeginSliderMovement_Lambda([this, guid]() { Int32ValueOnSliderBeginMovement = AnimModToolConfig->RepeatFramesValues[guid]; })
							.Value_Lambda([this, guid]() { return AnimModToolConfig->RepeatFramesValues[guid]; })
							.OnValueCommitted_Lambda([this, guid](int32 Value, ETextCommit::Type CommitType) { SetRepeatFrameValue(guid, Value, true); })
							.OnValueChanged_Lambda([this, guid](int32 Value) { SetRepeatFrameValue(guid, Value, false); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding)
					[
						SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(this, &FAnimModEditor::OnRemoveRepeatFramesEntryClicked, guid)
							.ContentPadding(FMargin(1, 0))
							[
								SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
							]
					]
			];
	}

	void RefreshComboBoxes()
	{
		MakeLoopedDynamicBlendComboBoxPtr->SetButtonContent(SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetMakeLoopedDynamicBlendOption())));

		BlendWithOtherStartBlendComboBoxPtr->SetButtonContent(SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetBlend_Target_ABlendOption())));

		BlendWithOtherEndBlendComboBoxPtr->SetButtonContent(SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetBlend_Target_BBlendOption())));

		HandPosesStartBlendComboBoxPtr->SetButtonContent(SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetHandPoses_StartBlend_BlendOption())));

		HandPosesEndBlendComboBoxPtr->SetButtonContent(SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetHandPoses_EndBlend_BlendOption())));

		for (const TSharedPtr<int32>& handPosesOption : HandPosesOptions)
		{
			if (GetHandPosesPoseFrame() == *handPosesOption)
			{
				HandPosesComboBoxPtr->SetSelectedItem(handPosesOption);
			}
		}

	}

	void RebuildRepeatFramesEntries()
	{
		TArray<FGuid> repeatFramesGuids;
		AnimModToolConfig->RepeatFramesKeys.GetKeys(repeatFramesGuids);

		RepeatFramesVerticalBoxPtr->ClearChildren();

		for (const FGuid& repeatFramesGuid : repeatFramesGuids)
		{
			RebuildRepeatFramesEntry(repeatFramesGuid);
		}
	}

	FReply OnAddRepeatFramesEntryClicked()
	{
		AddRepeatFrameEntry(FGuid::NewGuid());

		RebuildRepeatFramesEntries();

		return FReply::Handled();
	}

	FReply OnRemoveRepeatFramesEntryClicked(FGuid guid)
	{
		RemoveRepeatFrameEntry(guid);

		RebuildRepeatFramesEntries();

		return FReply::Handled();
	}

	void OnGetAllowedClasses(TArray<const UClass*>& outAllowedClasses)
	{
		outAllowedClasses.Add(UAnimSequence::StaticClass());
	}

	void RebuildBlendEntry(const FGuid& guid)
	{
		BlendWithOtherVerticalBoxPtr->AddSlot()
			[
				SNew(SHorizontalBox).Tag(FName(guid.ToString()))

					+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SBoneSelectionWidget)
							.OnBoneSelectionChanged(FOnBoneSelectionChanged::CreateLambda([this, guid](FName InName) { FScopedTransaction scopedTransaction(scopedTransactionText); AnimModToolConfig->Modify(); AnimModToolConfig->Blend_StartBones[guid] = InName; RefreshPreview(); }))
							.OnGetSelectedBone(FGetSelectedBone::CreateLambda([this, guid](bool& bMultipleValues) { bMultipleValues = false; return AnimModToolConfig->Blend_StartBones[guid]; }))
							.OnGetReferenceSkeleton(this, &FAnimModEditor::GetReferenceSkeleton)
					]
					+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SBoneSelectionWidget)
							.OnBoneSelectionChanged(FOnBoneSelectionChanged::CreateLambda([this, guid](FName InName) { FScopedTransaction scopedTransaction(scopedTransactionText); AnimModToolConfig->Modify(); AnimModToolConfig->Blend_EndBones[guid] = InName; RefreshPreview(); }))
							.OnGetSelectedBone(FGetSelectedBone::CreateLambda([this, guid](bool& bMultipleValues) { bMultipleValues = false; return AnimModToolConfig->Blend_EndBones[guid]; }))
							.OnGetReferenceSkeleton(this, &FAnimModEditor::GetReferenceSkeleton)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(this, &FAnimModEditor::OnRemoveBlendEntryClicked, guid)
							.ContentPadding(FMargin(1, 0))
							[
								SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
							]
					]
			];
	}

	void RebuildBlendEntries()
	{
		TArray<FGuid> blendGuids;
		AnimModToolConfig->Blend_StartBones.GetKeys(blendGuids);

		BlendWithOtherVerticalBoxPtr->ClearChildren();

		for (const FGuid& blendGuid : blendGuids)
		{
			RebuildBlendEntry(blendGuid);
		}
	}

	FReply OnAddBlendEntryClicked() ////// TODO Maube extract as function
	{
		FGuid guid = FGuid::NewGuid();

		FScopedTransaction scopedTransaction(scopedTransactionText);

		AnimModToolConfig->Modify();
		AnimModToolConfig->Blend_StartBones.Add(guid, NAME_None);
		AnimModToolConfig->Blend_EndBones.Add(guid, NAME_None);

		RebuildBlendEntries();

		return FReply::Handled();
	}

	FReply OnRemoveBlendEntryClicked(FGuid guid) ////// TODO Maube extract as function
	{
		FScopedTransaction scopedTransaction(scopedTransactionText);
		
		AnimModToolConfig->Modify();
		AnimModToolConfig->Blend_StartBones.Remove(guid);
		AnimModToolConfig->Blend_EndBones.Remove(guid);

		RebuildBlendEntries();

		RefreshPreview();

		return FReply::Handled();
	}

	FReply OnApplyButtonPressed()
	{
		ApplyModifiers();

		return FReply::Handled();
	}

	bool HandleShouldFilterAsset(const FAssetData& InAssetData) const
	{
		if (AnimSequencesToMod.Num() > 0)
		{
			if (AnimSequencesToMod[0])
			{
				if (USkeleton* skeleton = AnimSequencesToMod[0]->GetSkeleton())
				{
					if (skeleton->IsCompatibleForEditor(InAssetData))
					{
						return false;
					}

					return true;
				}

				return true;
			}

			return true;
		}

		return true;
	}

	void HandleMeshChanged(const FAssetData& InAssetData)
	{
		if (USkeletalMesh* NewPreviewMesh = Cast<USkeletalMesh>(InAssetData.GetAsset()))
		{
			PreviewMesh = NewPreviewMesh->GetPathName();
			PersonaToolkit.Get()->SetPreviewMesh(NewPreviewMesh, false);
		}
	}

private:

	TSharedPtr<SVerticalBox> RepeatFramesVerticalBoxPtr;

	TSharedPtr<SComboButtonCustom> MakeLoopedDynamicBlendComboBoxPtr;

	TSharedPtr<SVerticalBox> BlendWithOtherVerticalBoxPtr;

	TSharedPtr<SComboButtonCustom> BlendWithOtherStartBlendComboBoxPtr;

	TSharedPtr<SComboButtonCustom> BlendWithOtherEndBlendComboBoxPtr;

	TArray<TSharedPtr<int32>> HandPosesOptions;

	TSharedPtr<SComboBox_COPY<TSharedPtr<int32>>> HandPosesComboBoxPtr;

	TSharedPtr<SComboButtonCustom> HandPosesStartBlendComboBoxPtr;

	TSharedPtr<SComboButtonCustom> HandPosesEndBlendComboBoxPtr;

	TArray<FText> HandPosesLabels;

	TSharedPtr<SVerticalBox> HandPosesVerticalBoxPtr;

	TSharedPtr<ISlateStyle> StyleSet;

	TSharedPtr<IEditableSkeleton> EditableSkeleton;

	FSimpleMulticastDelegate OnSectionsChanged;

	FSimpleDelegate OnCurveChanged;

	TWeakPtr<IAnimSequenceCurveEditor> CurveEditor;

	TWeakPtr<SDockTab> AnimCurveDocumentTab;

	TSharedPtr<SSequenceEditor_COPY> SequenceEditorPtr;

	FString PreviewMesh;
};

FAnimModEditor::FAnimModEditor()
{
	GEditor->RegisterForUndo(this);
}

FAnimModEditor::~FAnimModEditor()
{
	if (PersonaToolkit.IsValid())
	{
		constexpr bool bSetPreviewMeshInAsset = false;
		PersonaToolkit->SetPreviewMesh(nullptr, bSetPreviewMeshInAsset);
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);

	HandPoses_AnimSequenceUE4->RemoveFromRoot();

	HandPoses_AnimSequence->RemoveFromRoot();

	GEditor->UnregisterForUndo(this);
}

void FAnimModEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_AnimationEditor", "Animation Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FAnimModEditor::InitAnimModEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, TArray<UAnimSequence*>& animSequencesToMod)
{
	HandPoses_AnimSequence = LoadObject<UAnimSequence>(nullptr, TEXT("/AnimModTool/Presets/HandPoses.HandPoses"));
	HandPoses_AnimSequence->AddToRoot();

	HandPoses_AnimSequenceUE4 = LoadObject<UAnimSequence>(nullptr, TEXT("/AnimModTool/Presets/HandPoses_UE4.HandPoses_UE4"));
	HandPoses_AnimSequenceUE4->AddToRoot();

	AnimSequencesToMod = animSequencesToMod;

	PreviewMesh = AnimSequencesToMod[0]->GetPreviewMesh()->GetPathName();

	AnimSequenceToMod = NewObject<UAnimSequenceCustom>(GetTransientPackage());
	AnimSequenceToMod->SetSkeleton(AnimSequencesToMod[0]->GetSkeleton());
	AnimSequenceToMod->GetController().InitializeModel();
	AnimSequenceToMod->GetController().SetNumberOfFrames(1);
	AnimSequenceToMod->SetPreviewMesh(AnimSequencesToMod[0]->GetPreviewMesh());
	AnimSequenceToMod->GetController().NotifyPopulated();

	TArray<FName> boneTrackNames;
	AnimSequencesToMod[0]->GetDataModel()->GetBoneTrackNames(boneTrackNames);

	for (const FName boneTrackName : boneTrackNames)
	{
		AnimSequenceToMod->GetController().AddBoneCurve(boneTrackName, false);
	}

	SetFrames(AnimSequenceToMod, AnimSequencesToMod[0]);

	AnimSequenceToModDeferred = NewObject<UAnimSequenceCustom>(GetTransientPackage());
	AnimSequenceToModDeferred->SetSkeleton(AnimSequencesToMod[0]->GetSkeleton());
	AnimSequenceToModDeferred->GetController().InitializeModel();
	AnimSequenceToModDeferred->GetController().SetNumberOfFrames(1);
	AnimSequenceToModDeferred->SetPreviewMesh(AnimSequencesToMod[0]->GetPreviewMesh());
	AnimSequenceToModDeferred->GetController().NotifyPopulated();
	
	for (const FName boneTrackName : boneTrackNames)
	{
		AnimSequenceToModDeferred->GetController().AddBoneCurve(boneTrackName, false);
	}

	SetFrames(AnimSequenceToModDeferred, AnimSequencesToMod[0]);

	// Register post import callback to catch animation imports when we have the asset open (we need to reinit)
	FReimportManager::Instance()->OnPostReimport().AddRaw(this, &FAnimModEditor::HandlePostReimport);
	GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.AddRaw(this, &FAnimModEditor::HandlePostImport);

	FPersonaToolkitArgs PersonaToolkitArgs;
	PersonaToolkitArgs.OnPreviewSceneSettingsCustomized = FOnPreviewSceneSettingsCustomized::FDelegate::CreateSP(this, &FAnimModEditor::HandleOnPreviewSceneSettingsCustomized);

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(AnimSequenceToModDeferred, PersonaToolkitArgs);

	PersonaToolkit->GetPreviewScene()->SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode::Animation);

	const bool bCreateDefaultStandaloneMenu = false;
	const bool bCreateDefaultToolbar = false;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, AnimModEditorAppIdentifier, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, AnimSequencesToMod[0]);

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	EditableSkeleton = SkeletonEditorModule.CreateEditableSkeleton(AnimSequenceToMod->GetSkeleton());

	FAnimDocumentArgs Args(PersonaToolkit->GetPreviewScene(), PersonaToolkit.ToSharedRef(), EditableSkeleton.ToSharedRef(), OnSectionsChanged);

	SAssignNew(SequenceEditorPtr, SSequenceEditor_COPY, Args.PreviewScene.Pin().ToSharedRef(), Args.EditableSkeleton.Pin().ToSharedRef(), GetToolkitCommands())
		.Sequence(AnimSequenceToMod)
		.OnObjectsSelected(Args.OnDespatchObjectsSelected)
		.OnInvokeTab(Args.OnDespatchInvokeTab)
		.OnEditCurves_Raw(this, &FAnimModEditor::EditCurves);

	AddApplicationMode(AnimModEditorModes::SingleAssetEditorMode, MakeShareable(new FApplicationMode_AnimModEditor_SingleAssetEditorMode(SharedThis(this), PersonaToolkit->GetPreviewScene())));

	SetCurrentMode(AnimModEditorModes::SingleAssetEditorMode);

	RegenerateMenusAndToolbars();

	PersonaToolkit->GetPreviewScene()->SetAllowMeshHitProxies(false);

	RefreshTabs();
}

void FAnimModEditor::PostUndo(bool bSuccess)
{
	RefreshTabs();
}

void FAnimModEditor::PostRedo(bool bSuccess)
{
	RefreshTabs();
}

void FAnimModEditor::Tick(float DeltaTime)
{
	if (PersonaToolkit.IsValid())
	{
		if (PersonaToolkit->GetMesh() && PersonaToolkit->GetMesh()->IsCompiling())
		{
			return;
		}
		PersonaToolkit->GetPreviewScene()->InvalidateViews();
	}
}

TSharedRef<SWidget> FAnimModEditor::GenerateModifiersWidget()
{
	const FAnimModToolEditorModule& animModToolEditorModule = FModuleManager::GetModuleChecked<FAnimModToolEditorModule>("AnimModToolEditor");

	const ISlateStyle& styleSet = animModToolEditorModule.GetStyleSet();

	Reset();

	const int32 Num = AnimSequenceToMod->GetDataModel()->GetNumberOfFrames();

	HandPosesOptions.SetNum(HAND_POSES_NUM);

	for (size_t i = 0; i < HAND_POSES_NUM; i++)
	{
		HandPosesOptions[i] = MakeShareable(new int32(i));
	}

	HandPosesLabels.SetNum(HAND_POSES_NUM);

	for (size_t i = 0; i < HAND_POSES_NUM; i++)
	{
		HandPosesLabels[i] = FText::FromString("Hand pose name");
	}

	TSet<EMovieSceneBuiltInEasing> FilterExclude;
	FilterExclude.Add(EMovieSceneBuiltInEasing::SinIn);
	FilterExclude.Add(EMovieSceneBuiltInEasing::SinOut);
	FilterExclude.Add(EMovieSceneBuiltInEasing::QuadIn);
	FilterExclude.Add(EMovieSceneBuiltInEasing::QuadOut);
	FilterExclude.Add(EMovieSceneBuiltInEasing::CubicIn);
	FilterExclude.Add(EMovieSceneBuiltInEasing::CubicOut);
	FilterExclude.Add(EMovieSceneBuiltInEasing::QuartIn);
	FilterExclude.Add(EMovieSceneBuiltInEasing::QuartOut);
	FilterExclude.Add(EMovieSceneBuiltInEasing::QuintIn);
	FilterExclude.Add(EMovieSceneBuiltInEasing::QuintOut);
	FilterExclude.Add(EMovieSceneBuiltInEasing::Custom);

	TSharedRef<SBorder> result = SNew(SBorder)
		.Padding(0)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		[
			SNew(SVerticalBox)

				+ SVerticalBox::Slot().FillHeight(1)
				[
					SNew(SScrollBox)

						+ SScrollBox::Slot()
						[
							SNew(SGridPanel)
								.FillColumn(0, 1).FillColumn(1, 0)
								.FillRow(0, 0).FillRow(1, 0).FillRow(2, 0).FillRow(3, 0).FillRow(4, 0).FillRow(5, 0).FillRow(6, 0)
								.FillRow(7, 0).FillRow(8, 0).FillRow(9, 0).FillRow(10, 0).FillRow(11, 0).FillRow(12, 0).FillRow(13, 0).FillRow(14, 0).FillRow(15, 0).FillRow(16, 0)

								+ SGridPanel::Slot(0, 0).ColumnSpan(2).Padding(slotPadding)
								[
									SNew(SObjectPropertyEntryBox)
										.AllowedClass(USkeletalMesh::StaticClass())
										.OnShouldFilterAsset(this, &FAnimModEditor::HandleShouldFilterAsset)
										.OnObjectChanged(this, &FAnimModEditor::HandleMeshChanged)
										.ObjectPath_Lambda([this]() { return PreviewMesh; })
								]

								+ SGridPanel::Slot(0, 1).ColumnSpan(2).Padding(slotPadding, internalPadding)
								[
									SNew(SSeparator)
										.Orientation(EOrientation::Orient_Horizontal)
										.Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
								]

								+ SGridPanel::Slot(1, 2)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Top)
								.Padding(slotPadding)
								[
									SNew(SImage).Image(styleSet.GetBrush(FAnimModToolEditorModule::GetReverseIconName())).IsEnabled_Lambda([this]() { return GetReverse(); })
								]

								+ SGridPanel::Slot(0, 2)
								.Padding(slotPadding)
								.VAlign(VAlign_Top)
								[
									SNew(SVerticalBox)

										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SCheckBox)
												.IsChecked_Lambda([this]() { return GetReverse() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
												.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetReverse(checkBoxState == ECheckBoxState::Checked); })
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_Reverse", "Reverse animation"))
														.Justification(ETextJustify::Center)
														.TextStyle(FAppStyle::Get(), "NormalText")
												]
										]

										+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
										[
											SNew(SBox).Padding(internalPadding)
												[
													SNew(SCheckBox).IsEnabled_Lambda([this]() { return GetReverse(); })
														.Visibility_Lambda([this]() { return GetReverse() ? EVisibility::Visible : EVisibility::Collapsed; })
														.IsChecked_Lambda([this]() { return GetReverseClearRootBoneZeroFrameOffset() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
														.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetReverseClearRootBoneZeroFrameOffset(checkBoxState == ECheckBoxState::Checked); })
														[
															SNew(STextBlock)
																.Text(LOCTEXT("FAnimModEditor_ClearRootBoneZeroFrameOffset", "Clear Root Bone zero frame offset"))
																.Justification(ETextJustify::Center)
																.TextStyle(FAppStyle::Get(), "SmallText")
														]
												]
										]
								]

								+ SGridPanel::Slot(0, 3).ColumnSpan(2).Padding(slotPadding, internalPadding)
								[
									SNew(SSeparator)
										.Orientation(EOrientation::Orient_Horizontal)
										.Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
								]

								+ SGridPanel::Slot(1, 4)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Top)
								.Padding(slotPadding)
								[
									SNew(SImage).Image(styleSet.GetBrush(FAnimModToolEditorModule::GetMirrorIconName())).IsEnabled_Lambda([this]() { return GetMirror(); })
								]

								+ SGridPanel::Slot(0, 4)
								.Padding(slotPadding)
								.VAlign(VAlign_Top)
								[
									SNew(SVerticalBox)

										+ SVerticalBox::Slot()
										.AutoHeight()
										[
											SNew(SCheckBox)
												.IsChecked_Lambda([this]() { return GetMirror() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
												.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetMirror(checkBoxState == ECheckBoxState::Checked); })
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_Mirror", "Mirror animation"))
														.Justification(ETextJustify::Center)
														.TextStyle(FAppStyle::Get(), "NormalText")
												]
										]
								]

								+ SGridPanel::Slot(0, 5).ColumnSpan(2).Padding(slotPadding, internalPadding)
								[
									SNew(SSeparator)
										.Orientation(EOrientation::Orient_Horizontal)
										.Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
								]

								+ SGridPanel::Slot(0, 6).ColumnSpan(2).Padding(slotPadding).VAlign(VAlign_Top)
								[
									SNew(SVerticalBox)

										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SCheckBox)
												.IsChecked_Lambda([this]() { return GetRepeatFrames() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
												.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetRepeatFrames(checkBoxState == ECheckBoxState::Checked); })
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_RepeatFrames", "Repeat frames"))
														.Justification(ETextJustify::Center)
														.TextStyle(FAppStyle::Get(), "NormalText")
												]
										]

										+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
										[
											SNew(SVerticalBox).Visibility_Lambda([this]() { return GetRepeatFrames() ? EVisibility::Visible : EVisibility::Collapsed; })

												+ SVerticalBox::Slot().AutoHeight()
												[
													SNew(SVerticalBox)

														+ SVerticalBox::Slot().AutoHeight()
														[
															SNew(SHorizontalBox)

																+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
																[
																	SNew(STextBlock)
																		.Text(LOCTEXT("FAnimModEditor_RepeatFrames_FrameNum", "Frame Num"))
																		.Justification(ETextJustify::Center)
																		.TextStyle(FAppStyle::Get(), "SmallText")
																]
																+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
																[
																	SNew(STextBlock)
																		.Text(LOCTEXT("FAnimModEditor_RepeatFrames_RepeatNum", "Repeat Num"))
																		.Justification(ETextJustify::Center)
																		.TextStyle(FAppStyle::Get(), "SmallText")
																]
																+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding)
																[
																	SNew(SButton).Visibility(EVisibility::Hidden)
																		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
																		.ContentPadding(FMargin(1, 0))
																		[
																			SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
																		]
																]
														]
												]

												+ SVerticalBox::Slot().AutoHeight()
												[
													SAssignNew(RepeatFramesVerticalBoxPtr, SVerticalBox)
												]

												+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
												[
													SNew(SButton)
														.ButtonStyle(FAppStyle::Get(), "SimpleButton")
														.OnClicked(this, &FAnimModEditor::OnAddRepeatFramesEntryClicked)
														.ContentPadding(FMargin(1, 0))
														[
															SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle")).ColorAndOpacity(FSlateColor::UseForeground())
														]
												]
										]
								]

								+ SGridPanel::Slot(0, 7).ColumnSpan(2).Padding(slotPadding, internalPadding)
								[
									SNew(SSeparator)
										.Orientation(EOrientation::Orient_Horizontal)
										.Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
								]

								+ SGridPanel::Slot(0, 8).ColumnSpan(2).Padding(slotPadding).VAlign(VAlign_Top)
								[
									SNew(SVerticalBox)

										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SCheckBox)
												.IsChecked_Lambda([this]() { return GetMakeLoopedDynamic() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
												.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetMakeLoopedDynamic(checkBoxState == ECheckBoxState::Checked); })
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_MakeLoopedDynamic", "Make Looped"))
														.Justification(ETextJustify::Center)
														.TextStyle(FAppStyle::Get(), "NormalText")
												]
										]

										+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
										[
											SNew(SGridPanel).Visibility_Lambda([this]() { return GetMakeLoopedDynamic() ? EVisibility::Visible : EVisibility::Collapsed; })
												.FillColumn(0, 0).FillColumn(1, 1)
												.FillRow(0, 0).FillRow(1, 0).FillRow(2, 0).FillRow(3, 0).FillRow(4, 0)

												+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_MakeLoopedDynamicStartBlendFramesNum", "Start Blend Frames Num"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 0).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this, Num]() { return Num / 2; })
														.MinValue(0)
														.MaxSliderValue_Lambda([this, Num]() { return Num / 2; })
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetMakeLoopedDynamicStartBlendFramesNum(); })
														.Value_Lambda([this]() { return GetMakeLoopedDynamicStartBlendFramesNum(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetMakeLoopedDynamicStartBlendFramesNum(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetMakeLoopedDynamicStartBlendFramesNum(Value, false); })
												]

												+ SGridPanel::Slot(0, 1).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_MakeLoopedDynamicEndBlendFramesNum", "End Blend Frames Num"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 1).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this, Num]() { return Num / 2; })
														.MinValue(0)
														.MaxSliderValue_Lambda([this, Num]() { return Num / 2; })
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetMakeLoopedDynamicEndBlendFramesNum(); })
														.Value_Lambda([this]() { return GetMakeLoopedDynamicEndBlendFramesNum(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetMakeLoopedDynamicEndBlendFramesNum(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetMakeLoopedDynamicEndBlendFramesNum(Value, false); })
												]

												+ SGridPanel::Slot(0, 2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_MakeLoopedDynamicBalance", "Balance"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SHorizontalBox)

														+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(internalPadding)
														[
															SNew(STextBlock)
																.Text(LOCTEXT("FAnimModEditor_MakeLoopedDynamicBalance", "Start"))
																.TextStyle(FAppStyle::Get(), "SmallText")
														]

														+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(internalPadding)
														[
															SNew(SSlider)
																.MinValue(0)
																.MaxValue(1)
																.Value_Lambda([this]() { return GetMakeLoopedDynamicBalance(); })
																.OnValueChanged_Lambda([this](float Value) { SetMakeLoopedDynamicBalance(Value, false); })
																.OnMouseCaptureBegin_Lambda([this]() { FloatValueOnSliderBeginMovement = GetMakeLoopedDynamicBalance(); })
																.OnMouseCaptureEnd_Lambda([this]() { SetMakeLoopedDynamicBalance(GetMakeLoopedDynamicBalance(), true); })
														]

														+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(internalPadding)
														[
															SNew(STextBlock)
																.Text(LOCTEXT("FAnimModEditor_MakeLoopedDynamicBalance", "End"))
																.TextStyle(FAppStyle::Get(), "SmallText")
														]
												]

												+ SGridPanel::Slot(0, 3).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_MakeLoopedDynamicBlendOption", "Blend Option"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 3).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SAssignNew(MakeLoopedDynamicBlendComboBoxPtr, SComboButtonCustom)
														.ButtonContent()
														[
															SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetMakeLoopedDynamicBlendOption()))
														]
														.MenuContent()
														[
															SNew(SEasingFunctionGridWidgetCustom)
																.FilterExclude(FilterExclude)
																.OnTypeChanged_Lambda([this](EMovieSceneBuiltInEasing NewType)
																	{
																		SetMakeLoopedDynamicBlendOption(Convert(NewType));
																		MakeLoopedDynamicBlendComboBoxPtr->SetButtonContent(SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetMakeLoopedDynamicBlendOption())));
																	})
														]
												]

												+ SGridPanel::Slot(0, 4).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_MakeLoopedDynamicBoneFilter", "Bone Filter"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 4).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SBoneSelectionWidget)
														.OnBoneSelectionChanged(FOnBoneSelectionChanged::CreateLambda([this](FName InName) { SetMakeLoopedDynamicBoneName(InName); }))
														.OnGetSelectedBone(FGetSelectedBone::CreateLambda([this](bool& bMultipleValues) { bMultipleValues = false; return GetMakeLoopedDynamicBoneName(); }))
														.OnGetReferenceSkeleton(this, &FAnimModEditor::GetReferenceSkeleton)
												]
										]
								]

								+ SGridPanel::Slot(0, 9).ColumnSpan(2).Padding(slotPadding, internalPadding)
								[
									SNew(SSeparator)
										.Orientation(EOrientation::Orient_Horizontal)
										.Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
								]

								+ SGridPanel::Slot(0, 10).ColumnSpan(2).Padding(slotPadding).VAlign(VAlign_Top)
								[
									SNew(SVerticalBox)

										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SCheckBox)
												.IsChecked_Lambda([this]() { return GetRemoveFrames() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
												.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetRemoveFrames(checkBoxState == ECheckBoxState::Checked); })
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_RemoveFrames", "Remove frames"))
														.Justification(ETextJustify::Center)
														.TextStyle(FAppStyle::Get(), "NormalText")
												]
										]

										+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
										[
											SNew(SGridPanel).Visibility_Lambda([this]() { return GetRemoveFrames() ? EVisibility::Visible : EVisibility::Collapsed; })
												.FillColumn(0, 0).FillColumn(1, 1)
												.FillRow(0, 0).FillRow(1, 0)

												+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_RemoveFramesStartFrame", "Start Frame"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 0).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue(Num - 1)
														.MinValue(0)
														.MaxSliderValue(Num - 1)
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetRemoveFramesStartFrame(); })
														.Value_Lambda([this]() { return GetRemoveFramesStartFrame(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetRemoveFramesStartFrame(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetRemoveFramesStartFrame(Value, false); })
												]

												+ SGridPanel::Slot(0, 1).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_RemoveFramesFramesNum", "Frames Num"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 1).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this]() { return GetRemoveFramesFramesNumMaxValue(); })
														.MinValue(1)
														.MaxSliderValue_Lambda([this]() { return GetRemoveFramesFramesNumMaxValue(); })
														.MinSliderValue(1)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetRemoveFramesFramesNum(); })
														.Value_Lambda([this]() { return GetRemoveFramesFramesNum(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetRemoveFramesFramesNum(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetRemoveFramesFramesNum(Value, false); })
												]
										]
								]

								+ SGridPanel::Slot(0, 11).ColumnSpan(2).Padding(slotPadding, internalPadding)
								[
									SNew(SSeparator)
										.Orientation(EOrientation::Orient_Horizontal)
										.Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
								]

								+ SGridPanel::Slot(0, 12).ColumnSpan(2).Padding(slotPadding).VAlign(VAlign_Top)
								[
									SNew(SVerticalBox)

										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SCheckBox)
												.IsChecked_Lambda([this]() { return GetBlendWithOther() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
												.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetBlendWithOther(checkBoxState == ECheckBoxState::Checked); })
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_BlendWithOther", "Blend with other"))
														.Justification(ETextJustify::Center)
														.TextStyle(FAppStyle::Get(), "NormalText")
												]
										]

										+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
										[
											SNew(SGridPanel).Visibility_Lambda([this]() { return GetBlendWithOther() ? EVisibility::Visible : EVisibility::Collapsed; })
												.FillColumn(0, 0).FillColumn(1, 1).FillColumn(2, 0)
												.FillRow(0, 0).FillRow(1, 0).FillRow(2, 0).FillRow(3, 0).FillRow(4, 0).FillRow(5, 0).FillRow(6, 0).FillRow(7, 0).FillRow(8, 0).FillRow(9, 0).FillRow(10, 0)

												+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_BlendWithOther_TargetAsset", "Target Asset"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 0).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text_Lambda([this]() { return AnimModToolConfig->Blend_Target ? FText::FromString(AnimModToolConfig->Blend_Target->GetName()) : FText::FromName(NAME_None); })
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(2, 0).VAlign(VAlign_Center).Padding(internalPadding)
												[
													PropertyCustomizationHelpers::MakeAssetPickerAnchorButton(
														FOnGetAllowedClasses::CreateSP(this, &FAnimModEditor::OnGetAllowedClasses),
														FOnAssetSelected::CreateLambda([this](const FAssetData& assetData) { SetBlendWithOtherTarget(assetData); })
													)
												]

												+ SGridPanel::Slot(0, 1).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.Text(LOCTEXT("FAnimModEditor_BlendWithOther_StartFrame", "Start Frame"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 1).ColumnSpan(2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue(Num)
														.MinValue(0)
														.MaxSliderValue(Num)
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetBlendWithOtherStartFrame(); })
														.Value_Lambda([this]() { return GetBlendWithOtherStartFrame(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetBlendWithOtherStartFrame(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetBlendWithOtherStartFrame(Value, false); })
												]

												+ SGridPanel::Slot(0, 2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.Text(LOCTEXT("FAnimModEditor_BlendWithOther_TargetStartFrame", "Target Start Frame"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 2).ColumnSpan(2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this]() { return GetBlendWithOtherTargetStartFrameMax(); })
														.MinValue(0)
														.MaxSliderValue_Lambda([this]() { return GetBlendWithOtherTargetStartFrameMax(); })
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetBlendWithOtherTargetStartFrame(); })
														.Value_Lambda([this]() { return GetBlendWithOtherTargetStartFrame(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetBlendWithOtherTargetStartFrame(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetBlendWithOtherTargetStartFrame(Value, false); })
												]

												+ SGridPanel::Slot(0, 3).ColumnSpan(3).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SCheckBox)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.IsChecked_Lambda([this]() { return GetUseStaticFrameFromTarget() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
														.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetUseStaticFrameFromTarget(checkBoxState == ECheckBoxState::Checked); })
														[
															SNew(STextBlock)
																.Text(LOCTEXT("FAnimModEditor_BlendWithOther_UseStaticFrameFromTarget", "Use static frame from Target"))
																.Justification(ETextJustify::Center)
																.TextStyle(FAppStyle::Get(), "SmallText")
														]
												]

												+ SGridPanel::Slot(0, 4).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.Text(LOCTEXT("FAnimModEditor_BlendWithOther_TargetFramesNum", "Target Frames Num"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 4).ColumnSpan(2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this]() { return GetBlendWithOtherTargetFramesNumMax(); })
														.MinValue(0)
														.MaxSliderValue_Lambda([this]() { return GetBlendWithOtherTargetFramesNumMax(); })
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetBlendWithOtherTargetFramesNum(); })
														.Value_Lambda([this]() { return GetBlendWithOtherTargetFramesNum(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetBlendWithOtherTargetFramesNum(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetBlendWithOtherTargetFramesNum(Value, false); })
												]

												+ SGridPanel::Slot(0, 5).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.Text(LOCTEXT("FAnimModEditor_BlendWithOther_StartBlendFramesNum", "Start Blend Frames Num"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 5).ColumnSpan(2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this]() { return GetBlendWithOtherBlendFramesNumMax(); })
														.MinValue(0)
														.MaxSliderValue_Lambda([this]() { return GetBlendWithOtherBlendFramesNumMax(); })
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetBlend_Target_ABlendFramesNum(); })
														.Value_Lambda([this]() { return GetBlend_Target_ABlendFramesNum(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetBlend_Target_ABlendFramesNum(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetBlend_Target_ABlendFramesNum(Value, false); })
												]

												+ SGridPanel::Slot(0, 6).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.Text(LOCTEXT("FAnimModEditor_BlendWithOther_StartBlendOption", "Start Blend Option"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 6).ColumnSpan(2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SAssignNew(BlendWithOtherStartBlendComboBoxPtr, SComboButtonCustom)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.ButtonContent()
														[
															SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetBlend_Target_ABlendOption()))
														]
														.MenuContent()
														[
															SNew(SEasingFunctionGridWidgetCustom)
																.FilterExclude(FilterExclude)
																.OnTypeChanged_Lambda([this](EMovieSceneBuiltInEasing NewType)
																	{
																		SetBlend_Target_ABlendOption(Convert(NewType));
																		BlendWithOtherStartBlendComboBoxPtr->SetButtonContent(SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetBlend_Target_ABlendOption())));
																	})
														]
												]

												+ SGridPanel::Slot(0, 7).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.Text(LOCTEXT("FAnimModEditor_BlendWithOther_EndBlendFramesNum", "End Blend Frames Num"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 7).ColumnSpan(2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this]() { return GetBlendWithOtherBlendFramesNumMax(); })
														.MinValue(0)
														.MaxSliderValue_Lambda([this]() { return GetBlendWithOtherBlendFramesNumMax(); })
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetBlend_Target_BBlendFramesNum(); })
														.Value_Lambda([this]() { return GetBlend_Target_BBlendFramesNum(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetBlend_Target_BBlendFramesNum(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetBlend_Target_BBlendFramesNum(Value, false); })
												]

												+ SGridPanel::Slot(0, 8).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.Text(LOCTEXT("FAnimModEditor_BlendWithOther_EndBlendOption", "End Blend Option"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 8).ColumnSpan(2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SAssignNew(BlendWithOtherEndBlendComboBoxPtr, SComboButtonCustom)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.ButtonContent()
														[
															SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetBlend_Target_BBlendOption()))
														]
														.MenuContent()
														[
															SNew(SEasingFunctionGridWidgetCustom)
																.FilterExclude(FilterExclude)
																.OnTypeChanged_Lambda([this](EMovieSceneBuiltInEasing NewType)
																	{
																		SetBlend_Target_BBlendOption(Convert(NewType));
																		BlendWithOtherEndBlendComboBoxPtr->SetButtonContent(SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetBlend_Target_BBlendOption())));
																	})
														]
												]

												+ SGridPanel::Slot(0, 9).ColumnSpan(3).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SCheckBox)
														.IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })
														.IsChecked_Lambda([this]() { return GetBlendWithOtherInsertAsNewFrames() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
														.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetBlendWithOtherInsertAsNewFrames(checkBoxState == ECheckBoxState::Checked); })
														[
															SNew(STextBlock)
																.Text(LOCTEXT("FAnimModEditor_BlendWithOther_InsertAsNewFrames", "Insert As New Frames"))
																.Justification(ETextJustify::Center)
																.TextStyle(FAppStyle::Get(), "SmallText")
														]
												]

												+ SGridPanel::Slot(0, 10).ColumnSpan(3).VAlign(VAlign_Fill).Padding(internalPadding)
												[
													SNew(SVerticalBox).IsEnabled_Lambda([this]() { return HasBlendWithOtherTarget(); })

														+ SVerticalBox::Slot().AutoHeight()
														[
															SNew(STextBlock)
																.Text(LOCTEXT("FAnimModEditor_Blend_BoneChains", "Bone Chains"))
																.Justification(ETextJustify::Left)
																.TextStyle(FAppStyle::Get(), "SmallText")
														]

														+ SVerticalBox::Slot().AutoHeight()
														[
															SNew(SVerticalBox)

																+ SVerticalBox::Slot().AutoHeight()
																[
																	SNew(SHorizontalBox)

																		+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
																		[
																			SNew(STextBlock)
																				.Text(LOCTEXT("FAnimModEditor_Blend_StartBone", "Start Bone"))
																				.Justification(ETextJustify::Center)
																				.TextStyle(FAppStyle::Get(), "SmallText")
																		]
																		+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
																		[
																			SNew(STextBlock)
																				.Text(LOCTEXT("FAnimModEditor_Blend_EndBone", "End Bone"))
																				.Justification(ETextJustify::Center)
																				.TextStyle(FAppStyle::Get(), "SmallText")
																		]
																		+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding)
																		[
																			SNew(SButton).Visibility(EVisibility::Hidden)
																				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
																				.ContentPadding(FMargin(1, 0))
																				[
																					SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
																				]
																		]
																]
														]

														+ SVerticalBox::Slot().AutoHeight()
														[
															SAssignNew(BlendWithOtherVerticalBoxPtr, SVerticalBox)
														]

														+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
														[
															SNew(SButton)
																.ButtonStyle(FAppStyle::Get(), "SimpleButton")
																.OnClicked(this, &FAnimModEditor::OnAddBlendEntryClicked)
																.ContentPadding(FMargin(1, 0))
																[
																	SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle")).ColorAndOpacity(FSlateColor::UseForeground())
																]
														]
												]
										]
								]

								+ SGridPanel::Slot(0, 13).ColumnSpan(2).Padding(slotPadding, internalPadding)
								[
									SNew(SSeparator)
										.Orientation(EOrientation::Orient_Horizontal)
										.Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
								]

								+ SGridPanel::Slot(0, 14).ColumnSpan(2).Padding(slotPadding).VAlign(VAlign_Top)
								[
									SNew(SCheckBox)
										.IsChecked_Lambda([this]() { return GetTimeManipulation() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
										.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetTimeManipulation(checkBoxState == ECheckBoxState::Checked); })
										[
											SNew(STextBlock)
												.Text(LOCTEXT("FAnimModEditor_TimeManipulation", "Time Manipulation"))
												.Justification(ETextJustify::Center)
												.TextStyle(FAppStyle::Get(), "NormalText")
										]
								]

								+ SGridPanel::Slot(0, 15).ColumnSpan(2).Padding(slotPadding, internalPadding)
								[
									SNew(SSeparator)
										.Orientation(EOrientation::Orient_Horizontal)
										.Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
								]

								+ SGridPanel::Slot(0, 16).ColumnSpan(2).Padding(slotPadding).VAlign(VAlign_Top)
								[
									SNew(SVerticalBox)

										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SCheckBox)
												.IsChecked_Lambda([this]() { return GetHandPoses() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
												.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetHandPoses(checkBoxState == ECheckBoxState::Checked); })
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_HandPoses", "Add Hand Poses"))
														.Justification(ETextJustify::Center)
														.TextStyle(FAppStyle::Get(), "NormalText")
												]
										]

										+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
										[
											SNew(SGridPanel).Visibility_Lambda([this]() { return GetHandPoses() ? EVisibility::Visible : EVisibility::Collapsed; })
												.FillColumn(0, 0).FillColumn(1, 1)
												.FillRow(0, 0).FillRow(1, 0).FillRow(2, 0).FillRow(3, 0).FillRow(4, 0).FillRow(5, 0).FillRow(6, 0).FillRow(7, 0).FillRow(8, 0).FillRow(9, 0)

												+ SGridPanel::Slot(0, 0).ColumnSpan(2).VAlign(VAlign_Fill).Padding(internalPadding)
												[
													SNew(SCheckBox)
														.IsChecked_Lambda([this]() { return GetHandPosesUE4() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
														.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetHandPosesUE4(checkBoxState == ECheckBoxState::Checked); })
														[
															SNew(STextBlock)
																.Text(LOCTEXT("FAnimModEditor_UE4", "UE4"))
																.Justification(ETextJustify::Center)
																.TextStyle(FAppStyle::Get(), "SmallText")
														]
												]

												+ SGridPanel::Slot(0, 2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_HandPoses_HandPose", "Pose"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 2).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SOverlay)

														+ SOverlay::Slot()
														[
															SAssignNew(HandPosesComboBoxPtr, SComboBox_COPY<TSharedPtr<int32>>)
																.OptionsSource(&HandPosesOptions)
																.OnSelectionChanged_Lambda([this](TSharedPtr<int32> InSelectedItem, ESelectInfo::Type SelectInfo) { SetHandPosesPoseFrame(*InSelectedItem, true); })
																.OnGenerateWidget_Lambda([this, &styleSet](TSharedPtr<int32> InSelectedItem)
																	{
																		auto brush = styleSet.GetBrush(FName("HandPose", *InSelectedItem));

																		return SNew(SOverlay)

																			+ SOverlay::Slot()
																			[
																				SNew(SImage).ColorAndOpacity(FLinearColor::Black).Visibility(EVisibility::HitTestInvisible)
																			]

																			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
																			[
																				SNew(SBox).WidthOverride(TileSizeX).HeightOverride(TileSizeY).Visibility(EVisibility::HitTestInvisible)
																					[
																						SNew(SImage).Image(brush)
																					]
																			];
																	})
																.InitiallySelectedItem(HandPosesOptions[0])
																[
																	SNew(SOverlay)
																]
														]

														+ SOverlay::Slot().Padding(FMargin(1.5, 1.5, 36, 1.5))
														[
															SNew(SImage).ColorAndOpacity(FLinearColor::Black).Visibility(EVisibility::HitTestInvisible)
														]

														+ SOverlay::Slot().Padding(FMargin(1.5, 1.5, 36, 1.5)).HAlign(HAlign_Center)
														[
															SNew(SBox).WidthOverride(TileSizeX).HeightOverride(TileSizeY).Visibility(EVisibility::HitTestInvisible)
																[
																	SNew(SImage).Image_Lambda([this, &styleSet]() { return styleSet.GetBrush(FName("HandPose", GetHandPosesPoseFrame())); })
																]
														]
												]

												+ SGridPanel::Slot(0, 3).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_HandPoses_StartFrame", "Start Frame"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 3).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this]() { return AnimSequenceToMod->GetDataModel()->GetNumberOfFrames() - 1; })
														.MinValue(0)
														.MaxSliderValue_Lambda([this]() { return AnimSequenceToMod->GetDataModel()->GetNumberOfFrames() - 1; })
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetHandPosesStartFrame(); })
														.Value_Lambda([this]() { return GetHandPosesStartFrame(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetHandPosesStartFrame(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetHandPosesStartFrame(Value, false); })
												]

												+ SGridPanel::Slot(0, 4).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_HandPoses_FramesNum", "Frames Num"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 4).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this]() { return GetHandPosesFramesNumMax(); })
														.MinValue(1)
														.MaxSliderValue_Lambda([this]() { return GetHandPosesFramesNumMax(); })
														.MinSliderValue(1)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetHandPosesFramesNum(); })
														.Value_Lambda([this]() { return GetHandPosesFramesNum(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetHandPosesFramesNum(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetHandPosesFramesNum(Value, false); })
												]

												+ SGridPanel::Slot(0, 5).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_HandPoses_StartBlendFramesNum", "Start Blend Frames Num"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 5).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this]() { return GetHandPosesStartBlendFramesNumMax(); })
														.MinValue(0)
														.MaxSliderValue_Lambda([this]() { return GetHandPosesStartBlendFramesNumMax(); })
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetHandPoses_StartBlend_FramesNum(); })
														.Value_Lambda([this]() { return GetHandPoses_StartBlend_FramesNum(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetHandPoses_StartBlend_FramesNum(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetHandPoses_StartBlend_FramesNum(Value, false); })
												]

												+ SGridPanel::Slot(0, 6).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_HandPoses_StartBlendOption", "Start Blend Option"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 6).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SAssignNew(HandPosesStartBlendComboBoxPtr, SComboButtonCustom)
														.ButtonContent()
														[
															SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetHandPoses_StartBlend_BlendOption()))
														]
														.MenuContent()
														[
															SNew(SEasingFunctionGridWidgetCustom)
																.FilterExclude(FilterExclude)
																.OnTypeChanged_Lambda([this](EMovieSceneBuiltInEasing NewType)
																	{
																		SetHandPoses_StartBlend_BlendOption(Convert(NewType));
																		HandPosesStartBlendComboBoxPtr->SetButtonContent(SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetHandPoses_StartBlend_BlendOption())));
																	})
														]
												]

												+ SGridPanel::Slot(0, 7).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_HandPoses_EndBlendFramesNum", "End Blend Frames Num"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 7).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(SNumericEntryBox<int32>)
														.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
														.AllowSpin(true)
														.MaxValue_Lambda([this]() { return GetHandPosesEndBlendFramesNumMax(); })
														.MinValue(0)
														.MaxSliderValue_Lambda([this]() { return GetHandPosesEndBlendFramesNumMax(); })
														.MinSliderValue(0)
														.OnBeginSliderMovement_Lambda([this]() { Int32ValueOnSliderBeginMovement = GetHandPoses_EndBlend_FramesNum(); })
														.Value_Lambda([this]() { return GetHandPoses_EndBlend_FramesNum(); })
														.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType) { SetHandPoses_EndBlend_FramesNum(Value, true); })
														.OnValueChanged_Lambda([this](int32 Value) { SetHandPoses_EndBlend_FramesNum(Value, false); })
												]

												+ SGridPanel::Slot(0, 8).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("FAnimModEditor_HandPoses_EndBlendOption", "End Blend Option"))
														.TextStyle(FAppStyle::Get(), "SmallText")
												]
												+ SGridPanel::Slot(1, 8).VAlign(VAlign_Center).Padding(internalPadding)
												[
													SAssignNew(HandPosesEndBlendComboBoxPtr, SComboButtonCustom)
														.ButtonContent()
														[
															SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetHandPoses_EndBlend_BlendOption()))
														]
														.MenuContent()
														[
															SNew(SEasingFunctionGridWidgetCustom)
																.FilterExclude(FilterExclude)
																.OnTypeChanged_Lambda([this](EMovieSceneBuiltInEasing NewType)
																	{
																		SetHandPoses_EndBlend_BlendOption(Convert(NewType));
																		HandPosesEndBlendComboBoxPtr->SetButtonContent(SNew(SBuiltInFunctionVisualizerWithTextCustom, Convert(GetHandPoses_EndBlend_BlendOption())));
																	})
														]
												]

												+ SGridPanel::Slot(0, 9).ColumnSpan(2).VAlign(VAlign_Fill).Padding(internalPadding)
												[
													SNew(SHorizontalBox)

														+ SHorizontalBox::Slot().FillWidth(1)
														[
															SNew(SCheckBox)
																.IsChecked_Lambda([this]() { return GetHandPoses_Left() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
																.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetHandPoses_Left(checkBoxState == ECheckBoxState::Checked); })
																[
																	SNew(STextBlock)
																		.Text(LOCTEXT("FAnimModEditor_HandPoses_L", "Left Hand"))
																		.Justification(ETextJustify::Center)
																		.TextStyle(FAppStyle::Get(), "SmallText")
																]
														]

														+ SHorizontalBox::Slot().FillWidth(1)
														[
															SNew(SCheckBox)
																.IsChecked_Lambda([this]() { return GetHandPoses_Right() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
																.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetHandPoses_Right(checkBoxState == ECheckBoxState::Checked); })
																[
																	SNew(STextBlock)
																		.Text(LOCTEXT("FAnimModEditor_HandPoses_R", "Right Hand"))
																		.Justification(ETextJustify::Center)
																		.TextStyle(FAppStyle::Get(), "SmallText")
																]
														]
												]
										]
								]
						]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(slotPadding)
				[
					SNew(SButton)
						.Visibility_Lambda([this]() { return CanApplyModifiers() ? EVisibility::Visible : EVisibility::Collapsed; })
						.TextStyle(FAppStyle::Get(), "DialogButtonText")
						.Text(LOCTEXT("FAnimModEditor_Apply", "Apply"))
						.HAlign(HAlign_Center)
						.OnClicked(this, &FAnimModEditor::OnApplyButtonPressed)
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(slotPadding)
				[
					SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return GetEnableRootMotion() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { SetEnableRootMotion(checkBoxState == ECheckBoxState::Checked); })
						[
							SNew(STextBlock).Text(LOCTEXT("FAnimModEditor_EnableRootMotion", "EnableRootMotion")).TextStyle(FAppStyle::Get(), "NormalText")
						]
				]
		];

	return result;
}

void FAnimModEditor::OnReimportAnimation()
{
	for (UAnimSequence* animSequenceToMod : AnimSequencesToMod)
	{
		if (animSequenceToMod)
		{
			FReimportManager::Instance()->ReimportAsync(animSequenceToMod, true);
		}
	}
}

void FAnimModEditor::ConditionalRefreshEditor(UObject* InObject)
{
	if (PersonaToolkit.IsValid())
	{
		bool bInterestingAsset = true;

		if (InObject != PersonaToolkit->GetSkeleton() && (PersonaToolkit->GetSkeleton() && InObject != PersonaToolkit->GetSkeleton()->GetPreviewMesh()) && AnimSequencesToMod.Contains(InObject))
		{
			bInterestingAsset = false;
		}

		if (PersonaToolkit->GetSkeleton() == nullptr)
		{
			bInterestingAsset = false;
		}

		if (bInterestingAsset)
		{
			PersonaToolkit->GetPreviewScene()->InvalidateViews();
		}
	}
}

void FAnimModEditor::HandlePostReimport(UObject* InObject, bool bSuccess)
{
	if (bSuccess)
	{
		ConditionalRefreshEditor(InObject);
	}
}

//--------------------------------------------------------------------
// FModifiersTabSummoner
//--------------------------------------------------------------------

struct FModifiersTabSummoner : public FWorkflowTabFactory
{
public:
	FModifiersTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TArray<UObject*>& InObjects);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

private:
	TArray<TWeakObjectPtr<UObject>> Objects;
};

FModifiersTabSummoner::FModifiersTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TArray<UObject*>& InObjects)
	: FWorkflowTabFactory(AnimModEditorTabs::ModifiersTab, InHostingApp)
	, Objects(InObjects)
{
	TabLabel = LOCTEXT("FModifiersTabSummoner_TabLabel", "Modifiers");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.Tabs.ControlRigMappingWindow");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("FModifiersTabSummoner_MenuDescription", "Modifiers");
	ViewMenuTooltip = LOCTEXT("FModifiersTabSummoner_MenuTooltip", "Setup modifiers");
}

TSharedRef<SWidget> FModifiersTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return StaticCastSharedPtr<FAnimModEditor>(HostingApp.Pin())->GenerateModifiersWidget();
}

//--------------------------------------------------------------------
// FPlayerTabSummoner
//--------------------------------------------------------------------

struct FPlayerTabSummoner : public FWorkflowTabFactory
{
public:
	FPlayerTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TArray<UObject*>& InObjects);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

private:
	TArray<TWeakObjectPtr<UObject>> Objects;
};

FPlayerTabSummoner::FPlayerTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TArray<UObject*>& InObjects)
	: FWorkflowTabFactory(AnimModEditorTabs::PlayerTab, InHostingApp)
	, Objects(InObjects)
{
	TabLabel = LOCTEXT("FPlayerTabSummoner_TabLabel", "Player");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.Tabs.ControlRigMappingWindow");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("FPlayerTabSummoner_MenuDescription", "Player");
	ViewMenuTooltip = LOCTEXT("FPlayerTabSummoner_MenuTooltip", "Player");
}

TSharedRef<SWidget> FPlayerTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return StaticCastSharedPtr<FAnimModEditor>(HostingApp.Pin())->GeneratePlayerWidget(Info);
}

//--------------------------------------------------------------------
// FTimeManipulationTabSummoner
//--------------------------------------------------------------------

struct FTimeManipulationTabSummoner : public FWorkflowTabFactory
{
public:
	FTimeManipulationTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TArray<UObject*>& InObjects);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

private:
	TArray<TWeakObjectPtr<UObject>> Objects;
};

FTimeManipulationTabSummoner::FTimeManipulationTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TArray<UObject*>& InObjects)
	: FWorkflowTabFactory(AnimModEditorTabs::TimeManipulationTab, InHostingApp)
	, Objects(InObjects)
{
	TabLabel = LOCTEXT("FTimeManipulationTabSummoner_TabLabel", "Time Manipulation");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.Tabs.ControlRigMappingWindow");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("FTimeManipulationTabSummoner_MenuDescription", "Time Manipulation");
	ViewMenuTooltip = LOCTEXT("FTimeManipulationTabSummoner_MenuTooltip", "Player");
}

TSharedRef<SWidget> FTimeManipulationTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return StaticCastSharedPtr<FAnimModEditor>(HostingApp.Pin())->GenerateTimeManipulationWidget(Info);
}

//--------------------------------------------------------------------
// FApplicationMode_AnimModEditor_SingleAssetEditorMode
//--------------------------------------------------------------------

FApplicationMode_AnimModEditor_SingleAssetEditorMode::FApplicationMode_AnimModEditor_SingleAssetEditorMode(TSharedRef<FWorkflowCentricApplication> InHostingApp, const TSharedRef<IPersonaPreviewScene>& previewScene)
	: FApplicationMode(AnimModEditorModes::SingleAssetEditorMode)
{
	HostingAppPtr = InHostingApp;

	TSharedRef<FAnimModEditor> AnimationEditor = StaticCastSharedRef<FAnimModEditor>(InHostingApp);

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");

	FPersonaViewportArgs ViewportArgs(previewScene);
	ViewportArgs.ContextName = TEXT("AnimationEditor.Viewport");
	ViewportArgs.bAlwaysShowTransformToolbar = false;
	ViewportArgs.bShowFloorOptions = false;
	ViewportArgs.bShowLODMenu = false;
	ViewportArgs.bShowPhysicsMenu = false;
	ViewportArgs.bShowPlaySpeedMenu = false;
	ViewportArgs.bShowShowMenu = false;
	ViewportArgs.bShowStats = false;
	ViewportArgs.bShowTimeline = false;
	ViewportArgs.bShowTurnTable = false;

	PersonaModule.RegisterPersonaViewportTabFactories(TabFactories, InHostingApp, ViewportArgs);

	bool isFirst = true;
	for (auto It = TabFactories.CreateIterator(); It; ++It)
	{
		if (isFirst)
		{
			isFirst = false;
		}
		else
		{
			It.RemoveCurrent();
		}
	}

	TabFactories.RegisterFactory(MakeShareable(new FModifiersTabSummoner(InHostingApp, *AnimationEditor->GetObjectsCurrentlyBeingEdited())));
	TabFactories.RegisterFactory(MakeShareable(new FTimeManipulationTabSummoner(InHostingApp, *AnimationEditor->GetObjectsCurrentlyBeingEdited())));
	TabFactories.RegisterFactory(MakeShareable(new FPlayerTabSummoner(InHostingApp, *AnimationEditor->GetObjectsCurrentlyBeingEdited())));

	TabLayout = FTabManager::NewLayout("FApplicationMode_AnimModEditor_SingleAssetEditorMode_v2.3.4")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.9f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->SetHideTabWell(true)
					->AddTab(AnimModEditorTabs::ModifiersTab, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.8f)
						->SetHideTabWell(true)
						->AddTab(AnimModEditorTabs::ViewportTab, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.2f)
						->AddTab(AnimModEditorTabs::TimeManipulationTab, ETabState::OpenedTab)
						->AddTab(AnimModEditorTabs::PlayerTab, ETabState::OpenedTab)
						->AddTab(AnimModEditorTabs::EditCurvesTab, ETabState::ClosedTab)
					)
				)
			)
		);

	PersonaModule.OnRegisterTabs().Broadcast(TabFactories, InHostingApp);
	LayoutExtender = MakeShared<FLayoutExtender>();
	PersonaModule.OnRegisterLayoutExtensions().Broadcast(*LayoutExtender.Get());
	TabLayout->ProcessExtensions(*LayoutExtender.Get());
}

void FApplicationMode_AnimModEditor_SingleAssetEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FWorkflowCentricApplication> HostingApp = HostingAppPtr.Pin();
	HostingApp->RegisterTabSpawners(InTabManager.ToSharedRef());
	HostingApp->PushTabFactories(TabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FApplicationMode_AnimModEditor_SingleAssetEditorMode::AddTabFactory(FCreateWorkflowTabFactory FactoryCreator)
{
	if (FactoryCreator.IsBound())
	{
		TabFactories.RegisterFactory(FactoryCreator.Execute(HostingAppPtr.Pin()));
	}
}

void FApplicationMode_AnimModEditor_SingleAssetEditorMode::RemoveTabFactory(FName TabFactoryID)
{
	TabFactories.UnregisterFactory(TabFactoryID);
}

//--------------------------------------------------------------------
// FContentBrowserSelectionMenuExtender
//--------------------------------------------------------------------

template<class T>
class FContentBrowserSelectionMenuExtender : public IContentBrowserSelectionMenuExtender, public TSharedFromThis<FContentBrowserSelectionMenuExtender<T>>
{
public:
	FContentBrowserSelectionMenuExtender(const FText& label, const FText& toolTip, const FName styleSetName, const FName iconName)
		: Label(label), ToolTip(toolTip), StyleSetName(styleSetName), IconName(iconName)
	{}

	virtual ~FContentBrowserSelectionMenuExtender() = default;

	virtual void Extend() override
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(FContentBrowserMenuExtender_SelectedAssets::CreateSP(this, &FContentBrowserSelectionMenuExtender::CreateExtender));
	}

protected:
	virtual void Execute(TArray<T*> SelectedAssets) const = 0;

private:
	TSharedRef<FExtender> CreateExtender(const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();

		Extender->AddMenuExtension(
			"GetAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateSP(this, &FContentBrowserSelectionMenuExtender::AddMenuExtension, SelectedAssets)
		);

		return Extender;
	}

	void AddMenuExtension(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
	{
		TArray<T*> typedSelectedAssets;

		for (const FAssetData& SelectedAsset : SelectedAssets)
		{
			if (SelectedAsset.GetClass() != T::StaticClass()) return;

			typedSelectedAssets.Add(static_cast<T*>(SelectedAsset.GetAsset()));
		}

		if (typedSelectedAssets.Num() == 0) return;

		MenuBuilder.AddMenuEntry(
			Label,
			ToolTip,
			FSlateIcon(StyleSetName, IconName),
			FUIAction(FExecuteAction::CreateSP(this, &FContentBrowserSelectionMenuExtender::Execute, typedSelectedAssets), FCanExecuteAction())
		);
	}

protected:

	const FText Label;
	const FText ToolTip;
	const FName StyleSetName;
	const FName IconName;
};

//--------------------------------------------------------------------
// FContentBrowserSelectionMenuExtender_AnimSequence
//--------------------------------------------------------------------

class FContentBrowserSelectionMenuExtender_AnimSequence : public FContentBrowserSelectionMenuExtender<UAnimSequence>
{
public:
	FContentBrowserSelectionMenuExtender_AnimSequence(const FText& label, const FText& toolTip, const FName styleSetName, const FName iconName)
		: FContentBrowserSelectionMenuExtender(label, toolTip, styleSetName, iconName)
	{}

protected:
	virtual void Execute(TArray<UAnimSequence*> SelectedAssets) const override
	{
		TSharedRef<FAnimModEditor> animModeEditor = MakeShareable(new FAnimModEditor);
		animModeEditor->InitAnimModEditor(EToolkitMode::Standalone, nullptr, SelectedAssets);
	}
};

//--------------------------------------------------------------------
// FAnimModToolEditorModule
//--------------------------------------------------------------------

void FAnimModToolEditorModule::StartupModule()
{
	StartupStyle();

	FAnimSequenceTimelineCommands_COPY::Register();

	ContentBrowserSelectionMenuExtenders.Add(MakeShareable(new FContentBrowserSelectionMenuExtender_AnimSequence(
		LOCTEXT("FContentBrowserSelectionMenuExtender_AnimSequence_Label", "Anim Mod Tool"),
		LOCTEXT("FContentBrowserSelectionMenuExtender_AnimSequence_ToolTip", "Modify selected assets with Anim Mod Tool"),
		GetStyleSetName(),
		GetContextMenuIconName()
	)));

	for (const TSharedPtr<IContentBrowserSelectionMenuExtender>& extender : ContentBrowserSelectionMenuExtenders)
	{
		if (extender.IsValid())
		{
			extender->Extend();
		}
	}
}

void FAnimModToolEditorModule::ShutdownModule()
{
	ContentBrowserSelectionMenuExtenders.Empty();

	FAnimSequenceTimelineCommands_COPY::Unregister();

	ShutdownStyle();
}

FName FAnimModToolEditorModule::GetStyleSetName()
{
	static FName styleSetName("FAnimModToolEditorModule_StyleSet_Name");
	return styleSetName;
}

FName FAnimModToolEditorModule::GetContextMenuIconName()
{
	static FName iconName("FAnimModToolEditorModule_ContextMenu_Icon");
	return iconName;
}

FName FAnimModToolEditorModule::GetReverseIconName()
{
	static FName iconName("FAnimModToolEditorModule_Reverse_Icon");
	return iconName;
}

FName FAnimModToolEditorModule::GetMirrorIconName()
{
	static FName iconName("FAnimModToolEditorModule_Mirror_Icon");
	return iconName;
}

void FAnimModToolEditorModule::StartupStyle()
{
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon160x137(160.0f, 137.0f);

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	StyleSet->SetContentRoot(IPluginManager::Get().FindPlugin("AnimModTool")->GetBaseDir() / TEXT("Resources"));

	StyleSet->Set(GetContextMenuIconName(), new FSlateVectorImageBrush(StyleSet->RootToContentDir(TEXT("PlaceholderButtonIcon"), TEXT(".svg")), Icon20x20));

	StyleSet->Set(GetReverseIconName(), new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("ReverseIcon"), TEXT(".png")), Icon160x137));

	StyleSet->Set(GetMirrorIconName(), new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("MirrorIcon"), TEXT(".png")), Icon160x137));

	for (size_t i = 0; i < HAND_POSES_NUM; i++)
	{
		StyleSet->Set(FName("HandPose", i), new FSlateImageBrush(StyleSet->RootToContentDir(FString::FromInt(i), TEXT(".jpg")), Icon20x20));
	}

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

void FAnimModToolEditorModule::ShutdownStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	ensure(StyleSet.IsUnique());

	StyleSet.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAnimModToolEditorModule, AnimModToolEditor)