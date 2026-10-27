// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "SAnimOutliner_COPY.h"
#include "AnimModel_COPY.h"
#include "AnimTimelineTrack_COPY.h"
#include "SAnimOutlinerItem_COPY.h"
#include "SAnimTrackArea_COPY.h"
#include "Widgets/Input/SButton.h"
#include "SAnimTrack_COPY.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/TextFilterExpressionEvaluator.h"

#define LOCTEXT_NAMESPACE "SAnimOutliner_COPY"

SAnimOutliner_COPY::~SAnimOutliner_COPY()
{
	if(AnimModel.IsValid())
	{
		AnimModel.Pin()->OnTracksChanged().Remove(TracksChangedDelegateHandle);
	}
}

void SAnimOutliner_COPY::Construct(const FArguments& InArgs, const TSharedRef<FAnimModel_COPY>& InAnimModel, const TSharedRef<SAnimTrackArea_COPY>& InTrackArea)
{	
	AnimModel = InAnimModel;
	TrackArea = InTrackArea;
	FilterText = InArgs._FilterText;
	bPhysicalTracksNeedUpdate = false;

	TracksChangedDelegateHandle = InAnimModel->OnTracksChanged().AddSP(this, &SAnimOutliner_COPY::HandleTracksChanged);

	TextFilter = MakeShareable(new FTextFilterExpressionEvaluator(ETextFilterExpressionEvaluatorMode::BasicString));

	HeaderRow = SNew(SHeaderRow)
		.Visibility(EVisibility::Collapsed);

	HeaderRow->AddColumn(
		SHeaderRow::Column(TEXT("Outliner"))
		.FillWidth(1.0f)
	);

	STreeView::Construct
	(
		STreeView::FArguments()
		.TreeItemsSource(&InAnimModel->GetAllRootTracks())
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SAnimOutliner_COPY::HandleGenerateRow) 
		.OnGetChildren(this, &SAnimOutliner_COPY::HandleGetChildren)
		.HeaderRow(HeaderRow)
		.ExternalScrollbar(InArgs._ExternalScrollbar)
		.OnExpansionChanged(this, &SAnimOutliner_COPY::HandleExpansionChanged)
		.AllowOverscroll(EAllowOverscroll::No)
		.OnContextMenuOpening(this, &SAnimOutliner_COPY::HandleContextMenuOpening)
	);

	// expand all
	InAnimModel->ForEachRootTrack([this](FAnimTimelineTrack_COPY& InRootTrack)
	{
		InRootTrack.Traverse_ParentFirst([this](FAnimTimelineTrack_COPY& InTrack){ SetItemExpansion(InTrack.AsShared(), InTrack.IsExpanded()); return true; });
	});
}

void SAnimOutliner_COPY::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	STreeView::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// These are updated in both tick and paint since both calls can cause changes to the cached rows and the data needs
	// to be kept synchronized so that external measuring calls get correct and reliable results.
	if (bPhysicalTracksNeedUpdate)
	{
		PhysicalTracks.Reset();
		CachedTrackGeometry.GenerateValueArray(PhysicalTracks);

		PhysicalTracks.Sort([](const FCachedGeometry& A, const FCachedGeometry& B)
		{
			return A.Top < B.Top;
		});

		bPhysicalTracksNeedUpdate = false;
	}
}

int32 SAnimOutliner_COPY::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = STreeView::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// These are updated in both tick and paint since both calls can cause changes to the cached rows and the data needs
	// to be kept synchronized so that external measuring calls get correct and reliable results.
	if (bPhysicalTracksNeedUpdate)
	{
		PhysicalTracks.Reset();
		CachedTrackGeometry.GenerateValueArray(PhysicalTracks);

		PhysicalTracks.Sort([](const FCachedGeometry& A, const FCachedGeometry& B) 
		{
			return A.Top < B.Top;
		});

		bPhysicalTracksNeedUpdate = false;
	}

	return LayerId + 1;
}

TSharedRef<ITableRow> SAnimOutliner_COPY::HandleGenerateRow(TSharedRef<FAnimTimelineTrack_COPY> InTrack, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SAnimOutlinerItem_COPY> Row =
		SNew(SAnimOutlinerItem_COPY, OwnerTable, InTrack)
		.OnGenerateWidgetForColumn(this, &SAnimOutliner_COPY::GenerateWidgetForColumn)
		.HighlightText(FilterText);

	// Ensure the track area is kept up to date with the virtualized scroll of the tree view
	TSharedPtr<SAnimTrack_COPY> TrackWidget = TrackArea->FindTrackSlot(InTrack);

	if (!TrackWidget.IsValid())
	{
		// Add a track slot for the row
		TrackWidget = SNew(SAnimTrack_COPY, InTrack, SharedThis(this))
			.ViewRange(AnimModel.Pin().Get(), &FAnimModel_COPY::GetViewRange)
		[
			 InTrack->GenerateContainerWidgetForTimeline()
		];

		TrackArea->AddTrackSlot(InTrack, TrackWidget);
	}

	if (ensure(TrackWidget.IsValid()))
	{
		Row->AddTrackAreaReference(TrackWidget);
	}

	return Row;
}

TSharedRef<SWidget> SAnimOutliner_COPY::GenerateWidgetForColumn(const TSharedRef<FAnimTimelineTrack_COPY>& InTrack, const FName& ColumnId, const TSharedRef<SAnimOutlinerItem_COPY>& Row) const
{
	return InTrack->GenerateContainerWidgetForOutliner(Row);
}

void SAnimOutliner_COPY::HandleGetChildren(TSharedRef<FAnimTimelineTrack_COPY> Item, TArray<TSharedRef<FAnimTimelineTrack_COPY>>& OutChildren)
{
	if(!FilterText.Get().IsEmpty())
	{
		for(const TSharedRef<FAnimTimelineTrack_COPY>& Child : Item->GetChildren())
		{
			if(!Child->SupportsFiltering() || TextFilter->TestTextFilter(FBasicStringFilterExpressionContext(Child->GetLabel().ToString())))
			{
				OutChildren.Add(Child);
			}
		}
	}
	else
	{
		OutChildren.Append(Item->GetChildren());
	}
}

void SAnimOutliner_COPY::HandleExpansionChanged(TSharedRef<FAnimTimelineTrack_COPY> InTrack, bool bIsExpanded)
{
	InTrack->SetExpanded(bIsExpanded);
	
	// Expand any children that are also expanded
	for (const TSharedRef<FAnimTimelineTrack_COPY>& Child : InTrack->GetChildren())
	{
		if (Child->IsExpanded())
		{
			SetItemExpansion(Child, true);
		}
	}
}

TSharedPtr<SWidget> SAnimOutliner_COPY::HandleContextMenuOpening()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, AnimModel.Pin()->GetCommandList());

	AnimModel.Pin()->BuildContextMenu(MenuBuilder);

	// > 1 because the search widget is always added
	return MenuBuilder.GetMultiBox()->GetBlocks().Num() > 1 ? MenuBuilder.MakeWidget() : TSharedPtr<SWidget>();
}

void SAnimOutliner_COPY::HandleTracksChanged()
{
	RequestTreeRefresh();
}

void SAnimOutliner_COPY::ReportChildRowGeometry(const TSharedRef<FAnimTimelineTrack_COPY>& InTrack, const FGeometry& InGeometry)
{
	const float ChildOffset = static_cast<float>(TransformPoint(
		Concatenate(
			InGeometry.GetAccumulatedLayoutTransform(),
			GetCachedGeometry().GetAccumulatedLayoutTransform().Inverse()
		),
		FVector2D(0,0)
	).Y);

	const FCachedGeometry* ExistingGeometry = CachedTrackGeometry.Find(InTrack);
	if(ExistingGeometry == nullptr || (ExistingGeometry->Top != ChildOffset || ExistingGeometry->Height != InGeometry.Size.Y))
	{
		CachedTrackGeometry.Add(InTrack, FCachedGeometry(InTrack, ChildOffset, static_cast<float>(InGeometry.Size.Y)));
		bPhysicalTracksNeedUpdate = true;
	}
}

void SAnimOutliner_COPY::OnChildRowRemoved(const TSharedRef<FAnimTimelineTrack_COPY>& InTrack)
{
	CachedTrackGeometry.Remove(InTrack);
	bPhysicalTracksNeedUpdate = true;
}

TOptional<SAnimOutliner_COPY::FCachedGeometry> SAnimOutliner_COPY::GetCachedGeometryForTrack(const TSharedRef<FAnimTimelineTrack_COPY>& InTrack) const
{
	if (const FCachedGeometry* FoundGeometry = CachedTrackGeometry.Find(InTrack))
	{
		return *FoundGeometry;
	}

	return TOptional<FCachedGeometry>();
}

TOptional<float> SAnimOutliner_COPY::ComputeTrackPosition(const TSharedRef<FAnimTimelineTrack_COPY>& InTrack) const
{
	// Positioning strategy:
	// Attempt to root out any visible track in the specified track's sub-hierarchy, and compute the track's offset from that
	float NegativeOffset = 0.f;
	TOptional<float> Top;
	
	// Iterate parent first until we find a tree view row we can use for the offset height
	auto Iter = [&](FAnimTimelineTrack_COPY& InTrack)
	{		
		TOptional<FCachedGeometry> ChildRowGeometry = GetCachedGeometryForTrack(InTrack.AsShared());
		if (ChildRowGeometry.IsSet())
		{
			Top = ChildRowGeometry->Top;
			// Stop iterating
			return false;
		}

		NegativeOffset -= InTrack.GetHeight() + InTrack.GetPadding().Combined();
		return true;
	};

	InTrack->TraverseVisible_ParentFirst(Iter);

	if (Top.IsSet())
	{
		return NegativeOffset + Top.GetValue();
	}

	return Top;
}

void SAnimOutliner_COPY::ScrollByDelta(float DeltaInSlateUnits)
{
	ScrollBy(GetCachedGeometry(), DeltaInSlateUnits, EAllowOverscroll::No);
}

void SAnimOutliner_COPY::Private_SetItemSelection( TSharedRef<FAnimTimelineTrack_COPY> TheItem, bool bShouldBeSelected, bool bWasUserDirected )
{
	if(TheItem->SupportsSelection())
	{
		AnimModel.Pin()->SetTrackSelected(TheItem, bShouldBeSelected);

		STreeView::Private_SetItemSelection(TheItem, bShouldBeSelected, bWasUserDirected);
	}
}

void SAnimOutliner_COPY::Private_ClearSelection()
{
	AnimModel.Pin()->ClearTrackSelection();

	STreeView::Private_ClearSelection();
}

void SAnimOutliner_COPY::Private_SelectRangeFromCurrentTo( TSharedRef<FAnimTimelineTrack_COPY> InRangeSelectionEnd )
{
	STreeView::Private_SelectRangeFromCurrentTo(InRangeSelectionEnd);

	for(TSet<TSharedRef<FAnimTimelineTrack_COPY>>::TIterator Iter = SelectedItems.CreateIterator(); Iter; ++Iter)
	{
		if(!(*Iter)->SupportsSelection())
		{
			Iter.RemoveCurrent();
		}
	}

	for(const TSharedRef<FAnimTimelineTrack_COPY>& SelectedItem : SelectedItems)
	{
		AnimModel.Pin()->SetTrackSelected(SelectedItem, true);
	}
}

void SAnimOutliner_COPY::RefreshFilter()
{
	TextFilter->SetFilterText(FilterText.Get());

	RequestTreeRefresh();
}

#undef LOCTEXT_NAMESPACE
