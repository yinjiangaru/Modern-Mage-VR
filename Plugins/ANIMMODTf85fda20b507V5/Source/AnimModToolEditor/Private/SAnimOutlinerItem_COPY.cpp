// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "SAnimOutlinerItem_COPY.h"
#include "AnimTimelineTrack_COPY.h"
#include "Widgets/Text/STextBlock.h"
#include "SAnimOutliner_COPY.h"
#include "Widgets/SOverlay.h"
#include "SAnimTrackResizeArea_COPY.h"

SAnimOutlinerItem_COPY::~SAnimOutlinerItem_COPY()
{
	TSharedPtr<SAnimOutliner_COPY> Outliner = StaticCastSharedPtr<SAnimOutliner_COPY>(OwnerTablePtr.Pin());
	TSharedPtr<FAnimTimelineTrack_COPY> PinnedTrack = Track.Pin();
	if (Outliner.IsValid() && PinnedTrack.IsValid())
	{
		Outliner->OnChildRowRemoved(PinnedTrack.ToSharedRef());
	}
}

void SAnimOutlinerItem_COPY::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedRef<FAnimTimelineTrack_COPY>& InTrack)
{
	Track = InTrack;
	OnGenerateWidgetForColumn = InArgs._OnGenerateWidgetForColumn;
	HighlightText = InArgs._HighlightText;

	bHovered = false;
	SetHover(TAttribute<bool>::CreateSP(this, &SAnimOutlinerItem_COPY::ShouldAppearHovered));

	SMultiColumnTableRow::Construct(
		SMultiColumnTableRow::FArguments()
			.ShowSelection(true),
		InOwnerTableView);
}

TSharedRef<SWidget> SAnimOutlinerItem_COPY::GenerateWidgetForColumn(const FName& ColumnId)
{
	TSharedPtr<FAnimTimelineTrack_COPY> PinnedTrack = Track.Pin();
	if (PinnedTrack.IsValid())
	{
		TSharedPtr<SWidget> ColumnWidget = SNullWidget::NullWidget;
		if(OnGenerateWidgetForColumn.IsBound())
		{
			ColumnWidget = OnGenerateWidgetForColumn.Execute(PinnedTrack.ToSharedRef(), ColumnId, SharedThis(this));
		}

		return SNew(SOverlay)
		+SOverlay::Slot()
		[
			ColumnWidget.ToSharedRef()
		];
	}

	return SNullWidget::NullWidget;
}

void SAnimOutlinerItem_COPY::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	StaticCastSharedPtr<SAnimOutliner_COPY>(OwnerTablePtr.Pin())->ReportChildRowGeometry(Track.Pin().ToSharedRef(), AllottedGeometry);
}

FVector2D SAnimOutlinerItem_COPY::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	TSharedPtr<FAnimTimelineTrack_COPY> PinnedTrack = Track.Pin();
	if(PinnedTrack.IsValid())
	{
		return FVector2D(100.0f, PinnedTrack->GetHeight() + PinnedTrack->GetPadding().Combined());
	}

	return FVector2D(100.0f, 16.0f);
}

void SAnimOutlinerItem_COPY::AddTrackAreaReference(const TSharedPtr<SAnimTrack_COPY>& InTrackWidget)
{
	TrackWidget = InTrackWidget;
}

void SAnimOutlinerItem_COPY::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bHovered = true;
	SMultiColumnTableRow<TSharedRef<FAnimTimelineTrack_COPY>>::OnMouseEnter(MyGeometry, MouseEvent);

	TSharedPtr<FAnimTimelineTrack_COPY> PinnedTrack = Track.Pin();
	if(PinnedTrack.IsValid())
	{
		PinnedTrack->SetHovered(true);
	}
}

void SAnimOutlinerItem_COPY::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bHovered = false;
	SMultiColumnTableRow<TSharedRef<FAnimTimelineTrack_COPY>>::OnMouseLeave(MouseEvent);

	TSharedPtr<FAnimTimelineTrack_COPY> PinnedTrack = Track.Pin();
	if(PinnedTrack.IsValid())
	{
		PinnedTrack->SetHovered(false);
	}
}

bool SAnimOutlinerItem_COPY::ShouldAppearHovered() const
{
	if(TSharedPtr<FAnimTimelineTrack_COPY> PinnedTrack = Track.Pin())
	{
		return bHovered || PinnedTrack->IsHovered();
	}

	return bHovered;
}
