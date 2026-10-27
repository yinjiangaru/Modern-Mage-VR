// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "SAnimTrackResizeArea_COPY.h"
#include "AnimTimelineTrack_COPY.h"
#include "Widgets/Layout/SBox.h"

void SAnimTrackResizeArea_COPY::Construct(const FArguments& InArgs, TWeakPtr<FAnimTimelineTrack_COPY> InTrack)
{
	Track = InTrack;

	ChildSlot
	[
		SNew(SBox)
		.HeightOverride(5.f)
	];
}

FReply SAnimTrackResizeArea_COPY::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const TSharedPtr<FAnimTimelineTrack_COPY> ResizeTarget = Track.Pin();
	if (ResizeTarget.IsValid() && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		DragParameters = FDragParameters(ResizeTarget->GetHeight(), static_cast<float>(MouseEvent.GetScreenSpacePosition().Y));
		return FReply::Handled().CaptureMouse(AsShared());
	}
	return FReply::Handled();
}

FReply SAnimTrackResizeArea_COPY::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	DragParameters.Reset();
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SAnimTrackResizeArea_COPY::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DragParameters.IsSet() && HasMouseCapture())
	{
		const float NewHeight = DragParameters->OriginalHeight + (static_cast<float>(MouseEvent.GetScreenSpacePosition().Y) - DragParameters->DragStartY);

		const TSharedPtr<FAnimTimelineTrack_COPY> ResizeTarget = Track.Pin();
		if (ResizeTarget.IsValid() && FMath::RoundToInt(NewHeight) != FMath::RoundToInt(DragParameters->OriginalHeight))
		{
			ResizeTarget->SetHeight(NewHeight);
		}
	}

	return FReply::Handled();
}

FCursorReply SAnimTrackResizeArea_COPY::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
}
