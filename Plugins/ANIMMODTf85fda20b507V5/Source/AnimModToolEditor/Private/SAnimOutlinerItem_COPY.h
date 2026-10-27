// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "AnimTimelineTrack_COPY.h"

class FAnimObject;
class SAnimTrack_COPY;

class SAnimOutlinerItem_COPY : public SMultiColumnTableRow<TSharedRef<FAnimTimelineTrack_COPY>>
{
public:
	/** Delegate to invoke to create a new column for this row */
	DECLARE_DELEGATE_RetVal_ThreeParams(TSharedRef<SWidget>, FOnGenerateWidgetForColumn, const TSharedRef<FAnimTimelineTrack_COPY>&, const FName&, const TSharedRef<SAnimOutlinerItem_COPY>&);

	SLATE_BEGIN_ARGS(SAnimOutlinerItem_COPY) {}

	/** Delegate to invoke to create a new column for this row */
	SLATE_EVENT(FOnGenerateWidgetForColumn, OnGenerateWidgetForColumn)

	/** Text to highlight when searching */
	SLATE_ATTRIBUTE(FText, HighlightText)

	SLATE_END_ARGS()

	~SAnimOutlinerItem_COPY();

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedRef<FAnimTimelineTrack_COPY>& InTrack);

	/** SWidget interface */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	/** SMultiColumnTableRow interface */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override;

	/** Get the track this item represents */
	TSharedRef<FAnimTimelineTrack_COPY> GetTrack() const { return Track.Pin().ToSharedRef(); }

	/** Add a reference to the specified track, keeping it alive until this row is destroyed */
	void AddTrackAreaReference(const TSharedPtr<SAnimTrack_COPY>& InTrackWidget);

	/** Get the text to highlight when searching */
	TAttribute<FText> GetHighlightText() const { return HighlightText; }

private:
	/** Get the label text for this item */
	FText GetLabelText() const;

	/** Get the hover state for this item */
	bool ShouldAppearHovered() const;

private:
	/** The track that we represent */
	TWeakPtr<FAnimTimelineTrack_COPY> Track;

	/** Cached reference to a track lane that we relate to. This keeps the track lane alive (it's a weak widget) as long as we are in view. */
	TSharedPtr<SAnimTrack_COPY> TrackWidget;

	/** Delegate to invoke to create a new column for this row */
	FOnGenerateWidgetForColumn OnGenerateWidgetForColumn;

	/** Text to highlight when searching */
	TAttribute<FText> HighlightText;
	
	/** Keep an internal IsHovered flag*/
	bool bHovered;
};
