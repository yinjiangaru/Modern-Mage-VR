// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "SSequenceEditor_COPY.h"
#include "Animation/AnimSequence.h"

#include "AnimPreviewInstance.h"
#include "Editor.h"
#include "AnimModel_AnimSequenceBase_COPY.h"
#include "SAnimTimeline_COPY.h"

#define LOCTEXT_NAMESPACE "AnimSequenceEditor"

//////////////////////////////////////////////////////////////////////////
// SSequenceEditor_COPY

void SSequenceEditor_COPY::Construct(const FArguments& InArgs, TSharedRef<class IPersonaPreviewScene> InPreviewScene, TSharedRef<class IEditableSkeleton> InEditableSkeleton, const TSharedRef<FUICommandList>& InCommandList)
{
	SequenceObj = InArgs._Sequence;
	check(SequenceObj);
	PreviewScenePtr = InPreviewScene;

	AnimModel = MakeShared<FAnimModel_AnimSequenceBase_COPY>(InPreviewScene, InEditableSkeleton, InCommandList, SequenceObj);

	AnimModel->OnEditCurves = FOnEditCurves::CreateLambda([this, InOnEditCurves = InArgs._OnEditCurves](UAnimSequenceBase* InAnimSequence, const TArray<IAnimationEditor::FCurveEditInfo>& InCurveInfo, const TSharedPtr<ITimeSliderController>& InExternalTimeSliderController)
	{
		InOnEditCurves.ExecuteIfBound(InAnimSequence, InCurveInfo, TimelineWidget->GetTimeSliderController());
	});

	AnimModel->OnSelectObjects = FOnObjectsSelected::CreateSP(this, &SAnimEditorBase_COPY::OnSelectionChanged);
	AnimModel->OnInvokeTab = InArgs._OnInvokeTab;
	AnimModel->Initialize();

	SAnimEditorBase_COPY::Construct( SAnimEditorBase_COPY::FArguments()
		.OnObjectsSelected(InArgs._OnObjectsSelected)
		.AnimModel(AnimModel), 
		InPreviewScene);

	if(GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

SSequenceEditor_COPY::~SSequenceEditor_COPY()
{
	if(GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

TSharedPtr<ITimeSliderController> SSequenceEditor_COPY::GetExternalTimeSliderController() const
{
	return TimelineWidget->GetTimeSliderController();
}

void SSequenceEditor_COPY::PostUndo( bool bSuccess )
{
	PostUndoRedo();
}

void SSequenceEditor_COPY::PostRedo( bool bSuccess )
{
	PostUndoRedo();
}

void SSequenceEditor_COPY::PostUndoRedo()
{
	GetPreviewScene()->SetPreviewAnimationAsset(SequenceObj);

	AnimModel->RefreshTracks();
}

#undef LOCTEXT_NAMESPACE
