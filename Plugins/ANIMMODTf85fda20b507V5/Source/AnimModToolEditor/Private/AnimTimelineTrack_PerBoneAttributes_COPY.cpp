// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "AnimTimelineTrack_PerBoneAttributes_COPY.h"
#include "Framework/Application/SlateApplication.h"
#include "SAnimOutlinerItem_COPY.h"

#define LOCTEXT_NAMESPACE "FAnimTimelineTrack_PerBoneAttributes"

ANIMTIMELINE_IMPLEMENT_TRACK(FAnimTimelineTrack_PerBoneAttributes_COPY);

FAnimTimelineTrack_PerBoneAttributes_COPY::FAnimTimelineTrack_PerBoneAttributes_COPY(const FName& InBoneName, const TSharedRef<FAnimModel_COPY>& InModel)
	: FAnimTimelineTrack_COPY(LOCTEXT("BoneAttributesTrackLabel", "Bone Attributes"), LOCTEXT("BoneAttributesToolTip", "Contained Attributes for specific Bone"), InModel), BoneName(InBoneName)
{
}

TSharedRef<SWidget> FAnimTimelineTrack_PerBoneAttributes_COPY::GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow)
{
	TSharedPtr<SBorder> OuterBorder;
	TSharedPtr<SHorizontalBox> InnerHorizontalBox;
	OutlinerWidget = GenerateStandardOutlinerWidget(InRow, false, OuterBorder, InnerHorizontalBox);

	OuterBorder->SetBorderBackgroundColor(FAppStyle::GetColor("AnimTimeline.Outliner.HeaderColor"));

	InnerHorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(2.0f, 1.0f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("AnimTimeline.Outliner.Label"))
			.Text(this, &FAnimTimelineTrack_PerBoneAttributes_COPY::GetLabel)
			.HighlightText(InRow->GetHighlightText())
		];

	return OutlinerWidget.ToSharedRef();
}

FText FAnimTimelineTrack_PerBoneAttributes_COPY::GetLabel() const
{
	return FText::FromName(BoneName);
}

FText FAnimTimelineTrack_PerBoneAttributes_COPY::GetToolTipText() const
{
	return FText::Format(LOCTEXT("BoneAttributesTooltipFormat", "Attributes for Bone: {0}\nNumber of Attributes: {1}"), FText::FromName(BoneName), FText::AsNumber(Children.Num()));
}

#undef LOCTEXT_NAMESPACE
