// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "AnimTimelineTrack_Attributes_COPY.h"
#include "PersonaUtils.h"
#include "Widgets/SBoxPanel.h"
#include "AnimSequenceTimelineCommands_COPY.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Animation/AnimSequence.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Framework/Application/SlateApplication.h"
#include "ScopedTransaction.h"
#include "SAnimOutlinerItem_COPY.h"

#define LOCTEXT_NAMESPACE "FAnimTimelineTrack_Attributes_COPY"

ANIMTIMELINE_IMPLEMENT_TRACK(FAnimTimelineTrack_Attributes_COPY);

FAnimTimelineTrack_Attributes_COPY::FAnimTimelineTrack_Attributes_COPY(const TSharedRef<FAnimModel_COPY>& InModel)
	: FAnimTimelineTrack_COPY(LOCTEXT("AttributesRootTrackLabel", "Attributes"), LOCTEXT("AttributesRootToolTip", "Animated (per bone) Attribute data contained in this asset"), InModel)
{
}

TSharedRef<SWidget> FAnimTimelineTrack_Attributes_COPY::GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow)
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
			.Text(this, &FAnimTimelineTrack_Attributes_COPY::GetLabel)
			.HighlightText(InRow->GetHighlightText())
		];

	return OutlinerWidget.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE
