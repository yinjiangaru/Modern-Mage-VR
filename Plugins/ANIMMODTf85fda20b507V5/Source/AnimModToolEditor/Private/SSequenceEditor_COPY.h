// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "IPersonaPreviewScene.h"
#include "SAnimationScrubPanel_COPY.h"
#include "SAnimEditorBase_COPY.h"
#include "EditorUndoClient.h"
#include "Animation/AnimSequence.h"

class SAnimNotifyPanel;
class FAnimModel_AnimSequenceBase_COPY;

//////////////////////////////////////////////////////////////////////////
// SSequenceEditor_COPY

/** Overall animation sequence editing widget */
class SSequenceEditor_COPY : public SAnimEditorBase_COPY, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS( SSequenceEditor_COPY )
		: _Sequence(NULL)
		{}

		SLATE_ARGUMENT(UAnimSequenceBase*, Sequence)
		SLATE_EVENT(FOnObjectsSelected, OnObjectsSelected)
		SLATE_EVENT(FOnInvokeTab, OnInvokeTab)
		SLATE_EVENT(FOnEditCurves, OnEditCurves)

	SLATE_END_ARGS()

private:
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;
	TSharedPtr<FAnimModel_AnimSequenceBase_COPY> AnimModel;
public:
	void Construct(const FArguments& InArgs, TSharedRef<class IPersonaPreviewScene> InPreviewScene, TSharedRef<class IEditableSkeleton> InEditableSkeleton, const TSharedRef<FUICommandList>& InCommandList);

	~SSequenceEditor_COPY();

	virtual UAnimationAsset* GetEditorObject() const override { return SequenceObj; }

	TSharedPtr<ITimeSliderController> GetExternalTimeSliderController() const;

private:
	/** Pointer to the animation sequence being edited */
	UAnimSequenceBase* SequenceObj;

	/** FEditorUndoClient interface */
	virtual void PostUndo( bool bSuccess ) override;
	virtual void PostRedo( bool bSuccess ) override;

	/** Post undo **/
	void PostUndoRedo();
};
