// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "AnimTimelineTrack_Attribute_COPY.h"
#include "SAnimOutlinerItem_COPY.h"
#include "Animation/AnimData/IAnimationDataModel.h"

#define LOCTEXT_NAMESPACE "FAnimTimelineTrack_CustomBoneAttributes"

ANIMTIMELINE_IMPLEMENT_TRACK(FAnimTimelineTrack_Attribute_COPY);

FAnimTimelineTrack_Attribute_COPY::FAnimTimelineTrack_Attribute_COPY(const FAnimatedBoneAttribute& InAttribute, const TSharedRef<FAnimModel_COPY>& InModel)
	: FAnimTimelineTrack_COPY(LOCTEXT("AttributeTrackLabel", "Attribute"), LOCTEXT("AttributeToolTip", "Attribute contained in this asset"), InModel), Attribute(InAttribute)
{
}

TSharedRef<SWidget> FAnimTimelineTrack_Attribute_COPY::GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow)
{
	TSharedPtr<SBorder> OuterBorder;
	TSharedPtr<SHorizontalBox> InnerHorizontalBox;
	OutlinerWidget = GenerateStandardOutlinerWidget(InRow, false, OuterBorder, InnerHorizontalBox);
	
	InnerHorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(2.0f, 1.0f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(this, &FAnimTimelineTrack_Attribute_COPY::GetLabel)
			.HighlightText(InRow->GetHighlightText())
		];

	return OutlinerWidget.ToSharedRef();
}

FText FAnimTimelineTrack_Attribute_COPY::GetLabel() const
{
	return FText::FromName(Attribute.Identifier.GetName());
}

FText FAnimTimelineTrack_Attribute_COPY::GetToolTipText() const
{
	return FText::Format(LOCTEXT("AttributeTooltipFormat", "Attribute: {0}\nType: {1}\nNumber of Keys: {2}"), FText::FromName(Attribute.Identifier.GetName()), FText::FromName(Attribute.Identifier.GetType()->GetFName()), FText::AsNumber(Attribute.Curve.GetNumKeys()));
}

#undef LOCTEXT_NAMESPACE
