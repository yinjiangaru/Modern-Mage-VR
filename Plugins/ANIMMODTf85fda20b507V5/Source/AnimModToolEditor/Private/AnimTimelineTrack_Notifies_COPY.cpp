// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "AnimTimelineTrack_Notifies_COPY.h"
#include "PersonaUtils.h"
#include "Animation/AnimSequenceBase.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AnimSequenceTimelineCommands_COPY.h"
#include "AnimTimelineTrack_NotifiesPanel_COPY.h"
#include "Widgets/Layout/SBorder.h"
#include "ScopedTransaction.h"
#include "Animation/AnimMontage.h"
#include "AnimModel_AnimSequenceBase_COPY.h"

#define LOCTEXT_NAMESPACE "FAnimTimelineTrack_Notifies_COPY"

ANIMTIMELINE_IMPLEMENT_TRACK(FAnimTimelineTrack_Notifies_COPY);

FAnimTimelineTrack_Notifies_COPY::FAnimTimelineTrack_Notifies_COPY(const TSharedRef<FAnimModel_COPY>& InModel)
	: FAnimTimelineTrack_COPY(LOCTEXT("NotifiesRootTrackLabel", "Notifies"), LOCTEXT("NotifiesRootTrackToolTip", "Notifies and sync markers"), InModel)
{
}

TSharedRef<SWidget> FAnimTimelineTrack_Notifies_COPY::GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow)
{
	TSharedPtr<SBorder> OuterBorder;
	TSharedPtr<SHorizontalBox> InnerHorizontalBox;
	TSharedRef<SWidget> OutlinerWidget = GenerateStandardOutlinerWidget(InRow, true, OuterBorder, InnerHorizontalBox);

	OuterBorder->SetBorderBackgroundColor(FAppStyle::GetColor("AnimTimeline.Outliner.HeaderColor"));

	UAnimMontage* AnimMontage = Cast<UAnimMontage>(GetModel()->GetAnimSequenceBase());
	if(!(AnimMontage && AnimMontage->HasParentAsset()))
	{
		InnerHorizontalBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(OutlinerRightPadding, 1.0f)
			[
				PersonaUtils::MakeTrackButton(LOCTEXT("AddTrackButtonText", "Track"), FOnGetContent::CreateSP(this, &FAnimTimelineTrack_Notifies_COPY::BuildNotifiesSubMenu), MakeAttributeSP(this, &FAnimTimelineTrack_Notifies_COPY::IsHovered))
			];
	}

	return OutlinerWidget;
}

TSharedRef<SWidget> FAnimTimelineTrack_Notifies_COPY::BuildNotifiesSubMenu()
{
	FMenuBuilder MenuBuilder(true, GetModel()->GetCommandList());

	MenuBuilder.BeginSection("Notifies", LOCTEXT("NotifiesMenuSection", "Notifies"));
	{
		MenuBuilder.AddMenuEntry(
			FAnimSequenceTimelineCommands_COPY::Get().AddNotifyTrack->GetLabel(),
			FAnimSequenceTimelineCommands_COPY::Get().AddNotifyTrack->GetDescription(),
			FAnimSequenceTimelineCommands_COPY::Get().AddNotifyTrack->GetIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAnimTimelineTrack_Notifies_COPY::AddTrack)
			)
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("TimingPanelOptions", LOCTEXT("TimingPanelOptionsHeader", "Options"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleTimingNodes_Notifies", "Show Notify Timing Nodes"),
			LOCTEXT("ShowNotifyTimingNodes", "Show or hide the timing display for notifies in the notify panel"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(&StaticCastSharedRef<FAnimModel_AnimSequenceBase_COPY>(GetModel()).Get(), &FAnimModel_AnimSequenceBase_COPY::ToggleNotifiesTimingElementDisplayEnabled, ETimingElementType::QueuedNotify),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(&StaticCastSharedRef<FAnimModel_AnimSequenceBase_COPY>(GetModel()).Get(), &FAnimModel_AnimSequenceBase_COPY::IsNotifiesTimingElementDisplayEnabled, ETimingElementType::QueuedNotify)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FAnimTimelineTrack_Notifies_COPY::AddTrack()
{
	UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();

	FScopedTransaction Transaction(LOCTEXT("AddNotifyTrack", "Add Notify Track"));
	AnimSequenceBase->Modify();

	FAnimNotifyTrack NewItem;
	NewItem.TrackName = GetNewTrackName(AnimSequenceBase);
	NewItem.TrackColor = FLinearColor::White;

	AnimSequenceBase->AnimNotifyTracks.Add(NewItem);

	NotifiesPanel.Pin()->RequestTrackRename(AnimSequenceBase->AnimNotifyTracks.Num() - 1);

	NotifiesPanel.Pin()->Update();
}

FName FAnimTimelineTrack_Notifies_COPY::GetNewTrackName(UAnimSequenceBase* InAnimSequenceBase)
{
	TArray<FName> TrackNames;
	TrackNames.Reserve(50);

	for (const FAnimNotifyTrack& Track : InAnimSequenceBase->AnimNotifyTracks)
	{
		TrackNames.Add(Track.TrackName);
	}

	FName NameToTest;
	int32 TrackIndex = 1;
	
	do 
	{
		NameToTest = *FString::FromInt(TrackIndex++);
	} while (TrackNames.Contains(NameToTest));

	return NameToTest;
}

#undef LOCTEXT_NAMESPACE
