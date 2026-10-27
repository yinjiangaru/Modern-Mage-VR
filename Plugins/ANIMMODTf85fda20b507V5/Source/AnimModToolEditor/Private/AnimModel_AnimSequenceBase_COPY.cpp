// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "AnimModel_AnimSequenceBase_COPY.h"
#include "Animation/AnimSequence.h"
#include "AnimTimelineTrack_COPY.h"
#include "AnimTimelineTrack_Notifies_COPY.h"
#include "AnimTimelineTrack_NotifiesPanel_COPY.h"
#include "AnimTimelineTrack_Curves_COPY.h"
#include "AnimTimelineTrack_Curve_COPY.h"
#include "AnimTimelineTrack_CompoundCurve_COPY.h"
#include "AnimTimelineTrack_FloatCurve_COPY.h"
#include "AnimTimelineTrack_VectorCurve_COPY.h"
#include "AnimTimelineTrack_TransformCurve_COPY.h"
#include "AnimTimelineTrack_Attributes_COPY.h"
#include "AnimTimelineTrack_PerBoneAttributes_COPY.h"
#include "AnimTimelineTrack_Attribute_COPY.h"
#include "AnimSequenceTimelineCommands_COPY.h"
#include "Framework/Commands/UICommandList.h"
#include "IAnimationEditor.h"
#include "Preferences/PersonaOptions.h"
#include "FrameNumberDisplayFormat.h"
#include "Framework/Commands/GenericCommands.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "AnimTimelineClipboard_COPY.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ScopedTransaction.h"
#include "Animation/AnimCurveTypes.h"

#define LOCTEXT_NAMESPACE "FAnimModel_AnimSequence"

FAnimModel_AnimSequenceBase_COPY::FAnimModel_AnimSequenceBase_COPY(const TSharedRef<IPersonaPreviewScene>& InPreviewScene, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, const TSharedRef<FUICommandList>& InCommandList, UAnimSequenceBase* InAnimSequenceBase)
	: FAnimModel_COPY(InPreviewScene, InEditableSkeleton, InCommandList)
	, AnimSequenceBase(InAnimSequenceBase)
{
	SnapTypes.Add(FAnimModel_COPY::FSnapType::Frames.Type, FAnimModel_COPY::FSnapType::Frames);
	SnapTypes.Add(FAnimModel_COPY::FSnapType::Notifies.Type, FAnimModel_COPY::FSnapType::Notifies);

	UpdateRange();

	// Clear display flags
	for(bool& bElementNodeDisplayFlag : NotifiesTimingElementNodeDisplayFlags)
	{
		bElementNodeDisplayFlag = false;
	}

	AnimSequenceBase->RegisterOnNotifyChanged(UAnimSequenceBase::FOnNotifyChanged::CreateRaw(this, &FAnimModel_AnimSequenceBase_COPY::RefreshSnapTimes));	

	AnimSequenceBase->GetDataModel()->GetModifiedEvent().AddRaw(this, &FAnimModel_AnimSequenceBase_COPY::OnDataModelChanged);
}

FAnimModel_AnimSequenceBase_COPY::~FAnimModel_AnimSequenceBase_COPY()
{
	AnimSequenceBase->UnregisterOnNotifyChanged(this);
	AnimSequenceBase->GetDataModel()->GetModifiedEvent().RemoveAll(this);
}

void FAnimModel_AnimSequenceBase_COPY::Initialize()
{
	TSharedRef<FUICommandList> CommandList = WeakCommandList.Pin().ToSharedRef();

	const FAnimSequenceTimelineCommands_COPY& Commands = FAnimSequenceTimelineCommands_COPY::Get();

	CommandList->MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateLambda([this]
		{
			SelectedTracks.Array()[0]->RequestRename();
		}),
		FCanExecuteAction::CreateLambda([this]
		{
			return (SelectedTracks.Num() > 0) && (SelectedTracks.Array()[0]->CanRename());
		})
	);

	CommandList->MapAction(
		Commands.EditSelectedCurves,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::EditSelectedCurves),
		FCanExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::CanEditSelectedCurves));

	CommandList->MapAction(
		Commands.CopySelectedCurveNames,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::CopySelectedCurveNamesToClipboard));
	
	CommandList->MapAction(
		Commands.DisplayFrames,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::SetDisplayFormat, EFrameNumberDisplayFormats::Frames),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsDisplayFormatChecked, EFrameNumberDisplayFormats::Frames));

	CommandList->MapAction(
		Commands.DisplaySeconds,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::SetDisplayFormat, EFrameNumberDisplayFormats::Seconds),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsDisplayFormatChecked, EFrameNumberDisplayFormats::Seconds));

	CommandList->MapAction(
		Commands.DisplayPercentage,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::ToggleDisplayPercentage),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsDisplayPercentageChecked));

	CommandList->MapAction(
		Commands.DisplaySecondaryFormat,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::ToggleDisplaySecondary),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsDisplaySecondaryChecked));

	CommandList->MapAction(
		Commands.SnapToFrames,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::ToggleSnap, FAnimModel_COPY::FSnapType::Frames.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsSnapChecked, FAnimModel_COPY::FSnapType::Frames.Type), 
		FIsActionButtonVisible::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsSnapAvailable, FAnimModel_COPY::FSnapType::Frames.Type));

	CommandList->MapAction(
		Commands.SnapToNotifies,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::ToggleSnap, FAnimModel_COPY::FSnapType::Notifies.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsSnapChecked, FAnimModel_COPY::FSnapType::Notifies.Type), 
		FIsActionButtonVisible::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsSnapAvailable, FAnimModel_COPY::FSnapType::Notifies.Type));

	CommandList->MapAction(
		Commands.SnapToCompositeSegments,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::ToggleSnap, FAnimModel_COPY::FSnapType::CompositeSegment.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsSnapChecked, FAnimModel_COPY::FSnapType::CompositeSegment.Type),
		FIsActionButtonVisible::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsSnapAvailable, FAnimModel_COPY::FSnapType::CompositeSegment.Type));

	CommandList->MapAction(
		Commands.SnapToMontageSections,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::ToggleSnap, FAnimModel_COPY::FSnapType::MontageSection.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsSnapChecked, FAnimModel_COPY::FSnapType::MontageSection.Type),
		FIsActionButtonVisible::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::IsSnapAvailable, FAnimModel_COPY::FSnapType::MontageSection.Type));

	CommandList->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::CopyToClipboard),
		FCanExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::CanCopyToClipboard));

	CommandList->MapAction(
		Commands.PasteDataIntoCurve,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::PasteDataFromClipboardToSelectedCurve),
		FCanExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::CanPasteDataFromClipboardToSelectedCurve));

	CommandList->MapAction(
		FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::PasteFromClipboard),
		FCanExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::CanPasteFromClipboard));
	
	CommandList->MapAction(
		FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::CutToClipboard),
		FCanExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::CanCutToClipboard));

	CommandList->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::RemoveSelectedCurves),
		FCanExecuteAction::CreateSP(this, &FAnimModel_AnimSequenceBase_COPY::AreAnyCurvesSelected));
}


void FAnimModel_AnimSequenceBase_COPY::RefreshTracks()
{
	ClearTrackSelection();

	// Clear all tracks
	ClearRootTracks();

	// Add notifies
	RefreshNotifyTracks();

	// Add curves
	RefreshCurveTracks();

	// Add attributes
	RefreshAttributeTracks();

	// Snaps
	RefreshSnapTimes();

	// Tell the UI to refresh
	OnTracksChangedDelegate.Broadcast();

	UpdateRange();
}

UAnimSequenceBase* FAnimModel_AnimSequenceBase_COPY::GetAnimSequenceBase() const 
{
	return AnimSequenceBase;
}

void FAnimModel_AnimSequenceBase_COPY::RefreshNotifyTracks()
{
	if(!NotifyRoot.IsValid())
	{
		// Add a root track for notifies & then the main 'panel' legacy track
		NotifyRoot = MakeShared<FAnimTimelineTrack_Notifies_COPY>(SharedThis(this));
	}

	NotifyRoot->ClearChildren();
	AddRootTrack(NotifyRoot.ToSharedRef());

	if(!NotifyPanel.IsValid())
	{
		NotifyPanel = MakeShared<FAnimTimelineTrack_NotifiesPanel_COPY>(SharedThis(this));
		NotifyRoot->SetAnimNotifyPanel(NotifyPanel.ToSharedRef());
	}

	NotifyRoot->AddChild(NotifyPanel.ToSharedRef());
}

void FAnimModel_AnimSequenceBase_COPY::RefreshCurveTracks()
{
	if(!CurveRoot.IsValid())
	{
		// Add a root track for curves
		CurveRoot = MakeShared<FAnimTimelineTrack_Curves_COPY>(SharedThis(this));
	}

	CurveRoot->ClearChildren();
	AddRootTrack(CurveRoot.ToSharedRef());

	// Next add a track for each float curve
	const FAnimationCurveData& AnimationModelCurveData = AnimSequenceBase->GetDataModel()->GetCurveData();
	const UPersonaOptions* PersonaOptions = GetDefault<UPersonaOptions>();
	if (!PersonaOptions || !PersonaOptions->bUseTreeViewForAnimationCurves)
	{
		for (const FFloatCurve& FloatCurve : AnimationModelCurveData.FloatCurves)
		{
			CurveRoot->AddChild(MakeShared<FAnimTimelineTrack_FloatCurve_COPY>(&FloatCurve, SharedThis(this)));
		}
	}
	else
	{
		FAnimTimelineTrack_CompoundCurve_COPY::AddGroupedCurveTracks(AnimationModelCurveData.FloatCurves, *CurveRoot, SharedThis(this), PersonaOptions->AnimationCurveGroupingDelimiters);
	}

	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimSequenceBase);
	if (AnimSequence)
	{
		if(!AdditiveRoot.IsValid())
		{
			// Add a root track for additive layers
			AdditiveRoot = MakeShared<FAnimTimelineTrack_COPY>(LOCTEXT("AdditiveLayerTrackList_Title", "Additive Layer Tracks"), LOCTEXT("AdditiveLayerTrackList_Tooltip", "Additive modifications to bone transforms"), SharedThis(this), true);
		}

		AdditiveRoot->ClearChildren();
		AddRootTrack(AdditiveRoot.ToSharedRef());

		// Next add a track for each transform curve
		for(const FTransformCurve& TransformCurve : AnimationModelCurveData.TransformCurves)
		{
			TSharedRef<FAnimTimelineTrack_TransformCurve_COPY> TransformCurveTrack = MakeShared<FAnimTimelineTrack_TransformCurve_COPY>(&TransformCurve, SharedThis(this));
			TransformCurveTrack->SetExpanded(false);
			AdditiveRoot->AddChild(TransformCurveTrack);

			FText TransformName = FAnimTimelineTrack_TransformCurve_COPY::GetTransformCurveName(AsShared(), TransformCurve.Name_DEPRECATED);
			FLinearColor TransformColor = TransformCurve.GetColor();
			FLinearColor XColor = FLinearColor::Red;
			FLinearColor YColor = FLinearColor::Green;
			FLinearColor ZColor = FLinearColor::Blue;
			FText XName = LOCTEXT("VectorXTrackName", "X");
			FText YName = LOCTEXT("VectorYTrackName", "Y");
			FText ZName = LOCTEXT("VectorZTrackName", "Z");
			
			FText VectorFormat = LOCTEXT("TransformVectorFormat", "{0}.{1}");
			FText TranslationName = LOCTEXT("TransformTranslationTrackName", "Translation");
			TSharedRef<FAnimTimelineTrack_VectorCurve_COPY> TranslationCurveTrack = MakeShared<FAnimTimelineTrack_VectorCurve_COPY>(&TransformCurve.TranslationCurve, TransformCurve.Name_DEPRECATED, 0, ERawCurveTrackTypes::RCT_Transform, TranslationName, FText::Format(VectorFormat, TransformName, TranslationName), TransformColor, SharedThis(this));
			TranslationCurveTrack->SetExpanded(false);
			TransformCurveTrack->AddChild(TranslationCurveTrack);			

			FText ComponentFormat = LOCTEXT("TransformComponentFormat", "{0}.{1}.{2}");
			TranslationCurveTrack->AddChild(MakeShared<FAnimTimelineTrack_Curve_COPY>(&TransformCurve.TranslationCurve.FloatCurves[0], TransformCurve.Name_DEPRECATED, 0, ERawCurveTrackTypes::RCT_Transform, XName, FText::Format(ComponentFormat, TransformName, TranslationName, XName), XColor, XColor, SharedThis(this)));
			TranslationCurveTrack->AddChild(MakeShared<FAnimTimelineTrack_Curve_COPY>(&TransformCurve.TranslationCurve.FloatCurves[1], TransformCurve.Name_DEPRECATED, 1, ERawCurveTrackTypes::RCT_Transform, YName, FText::Format(ComponentFormat, TransformName, TranslationName, YName), YColor, YColor, SharedThis(this)));
			TranslationCurveTrack->AddChild(MakeShared<FAnimTimelineTrack_Curve_COPY>(&TransformCurve.TranslationCurve.FloatCurves[2], TransformCurve.Name_DEPRECATED, 2, ERawCurveTrackTypes::RCT_Transform, ZName, FText::Format(ComponentFormat, TransformName, TranslationName, ZName), ZColor, ZColor, SharedThis(this)));

			FText RollName = LOCTEXT("RotationRollTrackName", "Roll");
			FText PitchName = LOCTEXT("RotationPitchTrackName", "Pitch");
			FText YawName = LOCTEXT("RotationYawTrackName", "Yaw");
			FText RotationName = LOCTEXT("TransformRotationTrackName", "Rotation");
			TSharedRef<FAnimTimelineTrack_VectorCurve_COPY> RotationCurveTrack = MakeShared<FAnimTimelineTrack_VectorCurve_COPY>(&TransformCurve.RotationCurve, TransformCurve.Name_DEPRECATED, 3, ERawCurveTrackTypes::RCT_Transform, RotationName, FText::Format(VectorFormat, TransformName, RotationName), TransformColor, SharedThis(this));
			RotationCurveTrack->SetExpanded(false);
			TransformCurveTrack->AddChild(RotationCurveTrack);
			RotationCurveTrack->AddChild(MakeShared<FAnimTimelineTrack_Curve_COPY>(&TransformCurve.RotationCurve.FloatCurves[0], TransformCurve.Name_DEPRECATED, 3, ERawCurveTrackTypes::RCT_Transform, RollName, FText::Format(ComponentFormat, TransformName, RotationName, RollName), XColor, XColor, SharedThis(this)));
			RotationCurveTrack->AddChild(MakeShared<FAnimTimelineTrack_Curve_COPY>(&TransformCurve.RotationCurve.FloatCurves[1], TransformCurve.Name_DEPRECATED, 4, ERawCurveTrackTypes::RCT_Transform, PitchName, FText::Format(ComponentFormat, TransformName, RotationName, PitchName), YColor, YColor, SharedThis(this)));
			RotationCurveTrack->AddChild(MakeShared<FAnimTimelineTrack_Curve_COPY>(&TransformCurve.RotationCurve.FloatCurves[2], TransformCurve.Name_DEPRECATED, 5, ERawCurveTrackTypes::RCT_Transform, YawName, FText::Format(ComponentFormat, TransformName, RotationName, YawName), ZColor, ZColor, SharedThis(this)));

			FText ScaleName = LOCTEXT("TransformScaleTrackName", "Scale");
			TSharedRef<FAnimTimelineTrack_VectorCurve_COPY> ScaleCurveTrack = MakeShared<FAnimTimelineTrack_VectorCurve_COPY>(&TransformCurve.ScaleCurve, TransformCurve.Name_DEPRECATED, 6, ERawCurveTrackTypes::RCT_Transform, ScaleName, FText::Format(VectorFormat, TransformName, ScaleName), TransformColor, SharedThis(this));
			ScaleCurveTrack->SetExpanded(false);
			TransformCurveTrack->AddChild(ScaleCurveTrack);
			ScaleCurveTrack->AddChild(MakeShared<FAnimTimelineTrack_Curve_COPY>(&TransformCurve.ScaleCurve.FloatCurves[0], TransformCurve.Name_DEPRECATED, 6, ERawCurveTrackTypes::RCT_Transform, XName, FText::Format(ComponentFormat, TransformName, ScaleName, XName), XColor, XColor, SharedThis(this)));
			ScaleCurveTrack->AddChild(MakeShared<FAnimTimelineTrack_Curve_COPY>(&TransformCurve.ScaleCurve.FloatCurves[1], TransformCurve.Name_DEPRECATED, 7, ERawCurveTrackTypes::RCT_Transform, YName, FText::Format(ComponentFormat, TransformName, ScaleName, YName), YColor, YColor, SharedThis(this)));
			ScaleCurveTrack->AddChild(MakeShared<FAnimTimelineTrack_Curve_COPY>(&TransformCurve.ScaleCurve.FloatCurves[2], TransformCurve.Name_DEPRECATED, 8, ERawCurveTrackTypes::RCT_Transform, ZName, FText::Format(ComponentFormat, TransformName, ScaleName, ZName), ZColor, ZColor, SharedThis(this)));
		}		
	}
}

void FAnimModel_AnimSequenceBase_COPY::RefreshAttributeTracks()
{
	if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimSequenceBase))
	{
		if (!AttributesRoot.IsValid())
		{
			// Add a root track for attributes
			AttributesRoot = MakeShared<FAnimTimelineTrack_Attributes_COPY>(SharedThis(this));
		}

		AttributesRoot->ClearChildren();
		AddRootTrack(AttributesRoot.ToSharedRef());
			   
		TMap<FName, TSharedPtr<FAnimTimelineTrack_PerBoneAttributes_COPY>> BoneTracks;


		// Next add a track for each attribute curve
		for (const FAnimatedBoneAttribute& BoneAttribute : AnimSequence->GetDataModel()->GetAttributes())
		{
			TSharedPtr<FAnimTimelineTrack_PerBoneAttributes_COPY> BoneTrack = nullptr;

			if (BoneTracks.Contains(BoneAttribute.Identifier.GetBoneName()))
			{
				BoneTrack = BoneTracks.FindChecked(BoneAttribute.Identifier.GetBoneName());
			}
			else
			{
				TSharedRef<FAnimTimelineTrack_PerBoneAttributes_COPY> BoneTrackRef = MakeShared<FAnimTimelineTrack_PerBoneAttributes_COPY>(BoneAttribute.Identifier.GetBoneName(), SharedThis(this));
				AttributesRoot->AddChild(BoneTrackRef);

				BoneTracks.Add(BoneAttribute.Identifier.GetBoneName(), BoneTrackRef);

				BoneTrack = BoneTrackRef;

				BoneTrackRef->SetExpanded(false);
			}

			TSharedRef<FAnimTimelineTrack_Attribute_COPY> AttributeTrack = MakeShared<FAnimTimelineTrack_Attribute_COPY>(BoneAttribute, SharedThis(this));
			BoneTrack->AddChild(AttributeTrack);
		}		
	}
}

void FAnimModel_AnimSequenceBase_COPY::OnDataModelChanged(const EAnimDataModelNotifyType& NotifyType, IAnimationDataModel* Model, const FAnimDataModelNotifPayload& PayLoad)
{
	NotifyCollector.Handle(NotifyType);

	switch(NotifyType)
	{ 
		case EAnimDataModelNotifyType::CurveAdded:
		case EAnimDataModelNotifyType::CurveChanged:
		case EAnimDataModelNotifyType::CurveRemoved:
		case EAnimDataModelNotifyType::TrackAdded:
		case EAnimDataModelNotifyType::TrackChanged:
		case EAnimDataModelNotifyType::TrackRemoved:
		case EAnimDataModelNotifyType::AttributeAdded:
		case EAnimDataModelNotifyType::AttributeChanged:
		case EAnimDataModelNotifyType::AttributeRemoved:
		case EAnimDataModelNotifyType::SequenceLengthChanged:
		case EAnimDataModelNotifyType::FrameRateChanged:
		{
			if (NotifyCollector.IsNotWithinBracket())
			{
				RefreshTracks();
			}
			break;
		}
		case EAnimDataModelNotifyType::BracketClosed:
		{
			if (NotifyCollector.IsNotWithinBracket())
			{
				RefreshTracks();
			}
			break;
		}
	}
}

void FAnimModel_AnimSequenceBase_COPY::EditSelectedCurves()
{
	TArray<IAnimationEditor::FCurveEditInfo> EditCurveInfo;
	for(TSharedRef<FAnimTimelineTrack_COPY>& SelectedTrack : SelectedTracks)
	{
		if(SelectedTrack->IsA<FAnimTimelineTrack_Curve_COPY>())
		{
			TSharedRef<FAnimTimelineTrack_Curve_COPY> CurveTrack = StaticCastSharedRef<FAnimTimelineTrack_Curve_COPY>(SelectedTrack);
			const TArray<const FRichCurve*> Curves = CurveTrack->GetCurves();
			int32 NumCurves = Curves.Num();
			for(int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
			{
				if(CurveTrack->CanEditCurve(CurveIndex))
				{
					FText FullName = CurveTrack->GetFullCurveName(CurveIndex);
					FLinearColor Color = CurveTrack->GetCurveColor(CurveIndex);
					FSmartName Name;
					ERawCurveTrackTypes Type;
					int32 EditCurveIndex;
					CurveTrack->GetCurveEditInfo(CurveIndex, Name, Type, EditCurveIndex);
					FSimpleDelegate OnCurveChanged = FSimpleDelegate::CreateSP(&CurveTrack.Get(), &FAnimTimelineTrack_Curve_COPY::HandleCurveChanged);
					EditCurveInfo.AddUnique(IAnimationEditor::FCurveEditInfo(FullName, Color, Name, Type, EditCurveIndex, OnCurveChanged));
				}
			}
		}
	}

	if(EditCurveInfo.Num() > 0)
	{
		OnEditCurves.ExecuteIfBound(AnimSequenceBase, EditCurveInfo, nullptr);
	}
}

bool FAnimModel_AnimSequenceBase_COPY::CanEditSelectedCurves() const
{
	for(const TSharedRef<FAnimTimelineTrack_COPY>& SelectedTrack : SelectedTracks)
	{
		if(SelectedTrack->IsA<FAnimTimelineTrack_Curve_COPY>())
		{
			TSharedRef<FAnimTimelineTrack_Curve_COPY> CurveTrack = StaticCastSharedRef<FAnimTimelineTrack_Curve_COPY>(SelectedTrack);
			const TArray<const FRichCurve*>& Curves = CurveTrack->GetCurves();
			for(int32 CurveIndex = 0; CurveIndex < Curves.Num(); ++CurveIndex)
			{
				if(CurveTrack->CanEditCurve(CurveIndex))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FAnimModel_AnimSequenceBase_COPY::RemoveSelectedCurves()
{
	IAnimationDataController& Controller = AnimSequenceBase->GetController();
	Controller.OpenBracket(LOCTEXT("CurvePanel_RemoveCurves", "Remove Curves"));

	bool bDeletedCurve = false;

	for(TSharedRef<FAnimTimelineTrack_COPY>& SelectedTrack : SelectedTracks)
	{
		if(SelectedTrack->IsA<FAnimTimelineTrack_FloatCurve_COPY>())
		{
			TSharedRef<FAnimTimelineTrack_FloatCurve_COPY> FloatCurveTrack = StaticCastSharedRef<FAnimTimelineTrack_FloatCurve_COPY>(SelectedTrack);

			FSmartName CurveName = FloatCurveTrack->GetName();
			const FAnimationCurveIdentifier FloatCurveId(CurveName, ERawCurveTrackTypes::RCT_Float);
			if (AnimSequenceBase->GetDataModel()->FindCurve(FloatCurveId))
			{
				FSmartName TrackName;
				if (AnimSequenceBase->GetSkeleton()->GetSmartNameByUID(USkeleton::AnimCurveMappingName, CurveName.UID, TrackName))
				{
					Controller.RemoveCurve(FloatCurveId);
					bDeletedCurve = true;
				}
			}
		}
		else if(SelectedTrack->IsA<FAnimTimelineTrack_TransformCurve_COPY>())
		{
			TSharedRef<FAnimTimelineTrack_TransformCurve_COPY> TransformCurveTrack = StaticCastSharedRef<FAnimTimelineTrack_TransformCurve_COPY>(SelectedTrack);

			const FTransformCurve& TransformCurve = TransformCurveTrack->GetTransformCurve();
			FSmartName CurveName = TransformCurveTrack->GetName();

			const FAnimationCurveIdentifier TransformCurveId(CurveName, ERawCurveTrackTypes::RCT_Transform);

			if (AnimSequenceBase->GetDataModel()->FindTransformCurve(TransformCurveId))
			{
				FSmartName CurveToDelete;
				if (AnimSequenceBase->GetSkeleton()->GetSmartNameByUID(USkeleton::AnimTrackCurveMappingName, CurveName.UID, CurveToDelete))
				{
					Controller.RemoveCurve(TransformCurveId);
					bDeletedCurve = true;
				}
			}	
		}
		else if (SelectedTrack->IsA<FAnimTimelineTrack_CompoundCurve_COPY>())
		{
			// Lowest priority for base class delete, ERawCurveTrackTypes::RCT_Transform must take priority
			TSharedRef<FAnimTimelineTrack_CompoundCurve_COPY> CurveTrack = StaticCastSharedRef<FAnimTimelineTrack_CompoundCurve_COPY>(SelectedTrack);

			// Find all editable curves in this track
			for (const FSmartName CurveName : CurveTrack->GetCurveNames())
			{
				const FAnimationCurveIdentifier CurveId(CurveName, ERawCurveTrackTypes::RCT_Float);
				if (AnimSequenceBase->GetDataModel()->FindCurve(CurveId))
				{
					FSmartName TrackName;
					if (AnimSequenceBase->GetSkeleton()->GetSmartNameByUID(USkeleton::AnimCurveMappingName, CurveName.UID, TrackName))
					{
						Controller.RemoveCurve(CurveId);
						bDeletedCurve = true;
					}
				}
			}
		}
	}

	Controller.CloseBracket();

	if(bDeletedCurve)
	{
		AnimSequenceBase->PostEditChange();

		if (GetPreviewScene()->GetPreviewMeshComponent()->PreviewInstance != nullptr)
		{
			GetPreviewScene()->GetPreviewMeshComponent()->PreviewInstance->RefreshCurveBoneControllers();
		}
	}
}


void FAnimModel_AnimSequenceBase_COPY::CopySelectedCurveNamesToClipboard()
{
	TArray<FString> TrackNames; 
	for(TSharedRef<FAnimTimelineTrack_COPY>& SelectedTrack : SelectedTracks)
	{
		if(SelectedTrack->IsA<FAnimTimelineTrack_FloatCurve_COPY>())
		{
			TSharedRef<FAnimTimelineTrack_FloatCurve_COPY> FloatCurveTrack = StaticCastSharedRef<FAnimTimelineTrack_FloatCurve_COPY>(SelectedTrack);
			TrackNames.Add(FloatCurveTrack->GetName().DisplayName.ToString());
		}
		else if(SelectedTrack->IsA<FAnimTimelineTrack_TransformCurve_COPY>())
		{
			TSharedRef<FAnimTimelineTrack_TransformCurve_COPY> TransformCurveTrack = StaticCastSharedRef<FAnimTimelineTrack_TransformCurve_COPY>(SelectedTrack);
			TrackNames.Add(TransformCurveTrack->GetName().DisplayName.ToString());
		}

	}
	if (!TrackNames.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*FString::Join(TrackNames, TEXT("\n")));
	}
}


void FAnimModel_AnimSequenceBase_COPY::SetDisplayFormat(EFrameNumberDisplayFormats InFormat)
{
	GetMutableDefault<UPersonaOptions>()->TimelineDisplayFormat = InFormat;
}

bool FAnimModel_AnimSequenceBase_COPY::IsDisplayFormatChecked(EFrameNumberDisplayFormats InFormat) const
{
	return GetDefault<UPersonaOptions>()->TimelineDisplayFormat == InFormat;
}

void FAnimModel_AnimSequenceBase_COPY::ToggleDisplayPercentage()
{
	GetMutableDefault<UPersonaOptions>()->bTimelineDisplayPercentage = !GetDefault<UPersonaOptions>()->bTimelineDisplayPercentage;
}

bool FAnimModel_AnimSequenceBase_COPY::IsDisplayPercentageChecked() const
{
	return GetDefault<UPersonaOptions>()->bTimelineDisplayPercentage;
}

void FAnimModel_AnimSequenceBase_COPY::ToggleDisplaySecondary()
{
	GetMutableDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary = !GetDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary;
}

bool FAnimModel_AnimSequenceBase_COPY::IsDisplaySecondaryChecked() const
{
	return GetDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary;
}

bool FAnimModel_AnimSequenceBase_COPY::AreAnyCurvesSelected() const
{
	if (!SelectedTracks.IsEmpty())
	{
		for (const TSharedRef<FAnimTimelineTrack_COPY>& SelectedTrack : SelectedTracks)
		{
			if (SelectedTrack->IsA<FAnimTimelineTrack_Curve_COPY>())
			{
				return true;
			}
		}
	}

	return false;
}

void FAnimModel_AnimSequenceBase_COPY::CopyToClipboard() const
{
	if (!SelectedTracks.IsEmpty())
	{
		if (UAnimTimelineClipboardContent_COPY* ClipboardContent = UAnimTimelineClipboardContent_COPY::Create())
		{
			FAnimTimelineClipboardUtilities_COPY::CopySelectedTracksToClipboard(SelectedTracks, ClipboardContent);
			FAnimTimelineClipboardUtilities_COPY::CopyContentToClipboard(ClipboardContent);
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("Failed to get create valid clipboard object for Animation Timeline while attempting to copy data"));
		}
	}
}

bool FAnimModel_AnimSequenceBase_COPY::CanCopyToClipboard()
{
	if (!SelectedTracks.IsEmpty())
	{
		for (const TSharedRef<FAnimTimelineTrack_COPY> & Track : SelectedTracks)
		{
			if (!Track->SupportsCopy())
			{
				return false;
			}
		}
	}
	else
	{
		return false;
	}
	
	return true;
}

void FAnimModel_AnimSequenceBase_COPY::PasteDataFromClipboardToSelectedCurve()
{
	if (const UAnimTimelineClipboardContent_COPY* ClipboardContent = FAnimTimelineClipboardUtilities_COPY::GetContentFromClipboard())
	{
		const FScopedTransaction Transaction(LOCTEXT("AnimSequenceBase_PasteCurveData", "Paste Data To Selected Curve"));

		// Paste data
		FAnimTimelineClipboardUtilities_COPY::OverwriteSelectedCurveDataFromClipboard(ClipboardContent, SelectedTracks, AnimSequenceBase);
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("Failed to get valid clipboard for Animation Timeline while attempting to paste data"));
	}
}

bool FAnimModel_AnimSequenceBase_COPY::CanPasteDataFromClipboardToSelectedCurve()
{
	return FAnimTimelineClipboardUtilities_COPY::CanOverwriteSelectedCurveDataFromClipboard(SelectedTracks);
}

void FAnimModel_AnimSequenceBase_COPY::PasteFromClipboard()
{
	if (const UAnimTimelineClipboardContent_COPY* ClipboardContent = FAnimTimelineClipboardUtilities_COPY::GetContentFromClipboard())
	{
		const FScopedTransaction Transaction(LOCTEXT("AnimSequenceBase_PasteCurves", "Paste"));
		// Paste curves from clipboard
		FAnimTimelineClipboardUtilities_COPY::OverwriteOrAddCurvesFromClipboardContent(ClipboardContent, AnimSequenceBase);
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("Failed to get valid clipboard for Animation Timeline while attempting to paste data"));
	}
}

bool FAnimModel_AnimSequenceBase_COPY::CanPasteFromClipboard()
{
	const UAnimTimelineClipboardContent_COPY* ClipboardContent = FAnimTimelineClipboardUtilities_COPY::GetContentFromClipboard();
	return ClipboardContent && !ClipboardContent->IsEmpty();
}

void FAnimModel_AnimSequenceBase_COPY::CutToClipboard()
{
	const FScopedTransaction Transaction(LOCTEXT("AnimSequenceBase_CutCurveSelection", "Cut Selection"));
	
	CopyToClipboard();
	RemoveSelectedCurves();
}

bool FAnimModel_AnimSequenceBase_COPY::CanCutToClipboard()
{
	return CanCopyToClipboard();
}

void FAnimModel_AnimSequenceBase_COPY::UpdateRange()
{
	FAnimatedRange OldPlaybackRange = PlaybackRange;

	// update playback range
	PlaybackRange = FAnimatedRange(0.0, (double)AnimSequenceBase->GetPlayLength());

	if (OldPlaybackRange != PlaybackRange)
	{
		// Update view/range if playback range changed
		SetViewRange(PlaybackRange);
	}
}

bool FAnimModel_AnimSequenceBase_COPY::IsNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType) const
{
	return NotifiesTimingElementNodeDisplayFlags[ElementType];
}

void FAnimModel_AnimSequenceBase_COPY::ToggleNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType)
{
	NotifiesTimingElementNodeDisplayFlags[ElementType] = !NotifiesTimingElementNodeDisplayFlags[ElementType];
}

bool FAnimModel_AnimSequenceBase_COPY::ClampToEndTime(float NewEndTime)
{
	float SequenceLength = AnimSequenceBase->GetPlayLength();

	//if we had a valid sequence length before and our new end time is shorter
	//then we need to clamp.
	return (SequenceLength > 0.f && NewEndTime < SequenceLength);
}

void FAnimModel_AnimSequenceBase_COPY::RefreshSnapTimes()
{
	SnapTimes.Reset();
	for(const FAnimNotifyEvent& Notify : AnimSequenceBase->Notifies)
	{
		SnapTimes.Emplace(FSnapType::Notifies.Type, (double)Notify.GetTime());
		if(Notify.NotifyStateClass != nullptr)
		{
			SnapTimes.Emplace(FSnapType::Notifies.Type, (double)(Notify.GetTime() + Notify.GetDuration()));
		}
	}
}

#undef LOCTEXT_NAMESPACE
