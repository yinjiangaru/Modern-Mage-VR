// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "AnimTimelineTrack_COPY.h"
#include "AnimModel_COPY.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Styling/AppStyle.h"
#include "Widgets/SOverlay.h"
#include "Preferences/PersonaOptions.h"
#include "Animation/AnimSequenceBase.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/STableViewBase.h"
#include "SAnimOutlinerItem_COPY.h"
 
#define LOCTEXT_NAMESPACE "FAnimTimelineTrack_COPY"

const float FAnimTimelineTrack_COPY::OutlinerRightPadding = 8.0f;

ANIMTIMELINE_IMPLEMENT_TRACK(FAnimTimelineTrack_COPY);

FText FAnimTimelineTrack_COPY::GetLabel() const
{
	return DisplayName;
}

FText FAnimTimelineTrack_COPY::GetToolTipText() const
{
	return ToolTipText;
}

bool FAnimTimelineTrack_COPY::Traverse_ChildFirst(const TFunctionRef<bool(FAnimTimelineTrack_COPY&)>& InPredicate, bool bIncludeThisTrack)
{
	for (TSharedRef<FAnimTimelineTrack_COPY>& Child : Children)
	{
		if (!Child->Traverse_ChildFirst(InPredicate, true))
		{
			return false;
		}
	}

	return bIncludeThisTrack ? InPredicate(*this) : true;
}

bool FAnimTimelineTrack_COPY::Traverse_ParentFirst(const TFunctionRef<bool(FAnimTimelineTrack_COPY&)>& InPredicate, bool bIncludeThisTrack)
{
	if (bIncludeThisTrack && !InPredicate(*this))
	{
		return false;
	}

	for (TSharedRef<FAnimTimelineTrack_COPY>& Child : Children)
	{
		if (!Child->Traverse_ParentFirst(InPredicate, true))
		{
			return false;
		}
	}

	return true;
}

bool FAnimTimelineTrack_COPY::TraverseVisible_ChildFirst(const TFunctionRef<bool(FAnimTimelineTrack_COPY&)>& InPredicate, bool bIncludeThisTrack)
{
	// If the item is not expanded, its children ain't visible
	if (IsExpanded())
	{
		for (TSharedRef<FAnimTimelineTrack_COPY>& Child : Children)
		{
			if (Child->IsVisible() && !Child->TraverseVisible_ChildFirst(InPredicate, true))
			{
				return false;
			}
		}
	}

	if (bIncludeThisTrack && IsVisible())
	{
		return InPredicate(*this);
	}

	// Continue iterating regardless of visibility
	return true;
}

bool FAnimTimelineTrack_COPY::TraverseVisible_ParentFirst(const TFunctionRef<bool(FAnimTimelineTrack_COPY&)>& InPredicate, bool bIncludeThisTrack)
{
	if (bIncludeThisTrack && IsVisible() && !InPredicate(*this))
	{
		return false;
	}

	// If the item is not expanded, its children ain't visible
	if (IsExpanded())
	{
		for (TSharedRef<FAnimTimelineTrack_COPY>& Child : Children)
		{
			if (Child->IsVisible() && !Child->TraverseVisible_ParentFirst(InPredicate, true))
			{
				return false;
			}
		}
	}

	return true;
}

TSharedRef<SWidget> FAnimTimelineTrack_COPY::GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow)
{
	TSharedPtr<SBorder> OuterBorder;
	TSharedPtr<SHorizontalBox> InnerHorizontalBox;
	TSharedRef<SWidget> Widget = GenerateStandardOutlinerWidget(InRow, true, OuterBorder, InnerHorizontalBox);

	if(bIsHeaderTrack)
	{
		OuterBorder->SetBorderBackgroundColor(FAppStyle::GetColor("AnimTimeline.Outliner.HeaderColor"));
	}

	return Widget;
}

TSharedRef<SWidget> FAnimTimelineTrack_COPY::GenerateStandardOutlinerWidget(const TSharedRef<SAnimOutlinerItem_COPY>& InRow, bool bWithLabelText, TSharedPtr<SBorder>& OutOuterBorder, TSharedPtr<SHorizontalBox>& OutInnerHorizontalBox)
{
	TSharedRef<SWidget> Widget =
		SAssignNew(OutOuterBorder, SBorder)
		.ToolTipText(this, &FAnimTimelineTrack_COPY::GetToolTipText)
		.BorderImage(FAppStyle::GetBrush("Sequencer.Section.BackgroundTint"))
		.BorderBackgroundColor(FAppStyle::GetColor("AnimTimeline.Outliner.ItemColor"))
		[
			SAssignNew(OutInnerHorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(4.0f, 1.0f)
			[
				SNew(SExpanderArrow, InRow)
			]
		];

	if(bWithLabelText)
	{
		OutInnerHorizontalBox->AddSlot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(2.0f, 1.0f)
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("AnimTimeline.Outliner.Label"))
				.Text(this, &FAnimTimelineTrack_COPY::GetLabel)
				.HighlightText(InRow->GetHighlightText())
			];
	}

	return Widget;
}

TSharedRef<SWidget> FAnimTimelineTrack_COPY::GenerateContainerWidgetForTimeline()
{
	return SNullWidget::NullWidget;
}

void FAnimTimelineTrack_COPY::AddToContextMenu(FMenuBuilder& InMenuBuilder, TSet<FName>& InOutExistingMenuTypes) const
{

}

float FAnimTimelineTrack_COPY::GetMaxInput() const
{
	return GetModel()->GetAnimSequenceBase()->GetPlayLength(); 
}

float FAnimTimelineTrack_COPY::GetViewMinInput() const
{
	return static_cast<float>(GetModel()->GetViewRange().GetLowerBoundValue());
}

float FAnimTimelineTrack_COPY::GetViewMaxInput() const
{
	return static_cast<float>(GetModel()->GetViewRange().GetUpperBoundValue());
}

float FAnimTimelineTrack_COPY::GetScrubValue() const
{
	const int64 Resolution = FMath::RoundToInt(static_cast<double>(GetDefault<UPersonaOptions>()->TimelineScrubSnapValue) * GetModel()->GetFrameRate());
	return static_cast<float>(static_cast<double>(GetModel()->GetScrubPosition().Value) / static_cast<double>(Resolution));
}

void FAnimTimelineTrack_COPY::SelectObjects(const TArray<UObject*>& SelectedItems)
{
	GetModel()->SelectObjects(SelectedItems);
}

void FAnimTimelineTrack_COPY::OnSetInputViewRange(float ViewMin, float ViewMax)
{
	GetModel()->SetViewRange(TRange<double>(ViewMin, ViewMax));
}

void FAnimTimelineTrack_COPY::AddChild(const TSharedRef<FAnimTimelineTrack_COPY>& InChild)
{
	if (GetMutableDefault<UPersonaOptions>()->GetAllowedAnimationEditorTracks().PassesFilter(InChild->GetTypeName()))
	{
		Children.Add(InChild); 
	}
}

#undef LOCTEXT_NAMESPACE
