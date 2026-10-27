// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "AnimTimelineTrack_COPY.h"
#include "SAnimTimingPanel_COPY.h"
#include "StatusBarSubsystem.h"

class SAnimNotifyPanel_COPY;
class SVerticalBox;
class SInlineEditableTextBlock;

/** A timeline track that re-uses the legacy panel widget to display notifies */
class FAnimTimelineTrack_NotifiesPanel_COPY : public FAnimTimelineTrack_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_NotifiesPanel_COPY, FAnimTimelineTrack_COPY);

public:
	static const float NotificationTrackHeight;
	static const FName AnimationEditorStatusBarName;

	FAnimTimelineTrack_NotifiesPanel_COPY(const TSharedRef<FAnimModel_COPY>& InModel);

	/** FAnimTimelineTrack interface */
	virtual TSharedRef<SWidget> GenerateContainerWidgetForTimeline() override;
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow) override;
	virtual bool SupportsFiltering() const override { return false; }

	TSharedRef<SAnimNotifyPanel_COPY> GetAnimNotifyPanel();
	
	void Update();

	/** Request a rename next update */
	void RequestTrackRename(int32 InTrackIndex) { PendingRenameTrackIndex = InTrackIndex; }

private:
	TSharedRef<SWidget> BuildNotifiesPanelSubMenu(int32 InTrackIndex);
	void InsertTrack(int32 InTrackIndexToInsert);
	void RemoveTrack(int32 InTrackIndexToRemove);
	void RefreshOutlinerWidget();
	void OnCommitTrackName(const FText& InText, ETextCommit::Type CommitInfo, int32 TrackIndexToName);
	EVisibility OnGetTimingNodeVisibility(ETimingElementType::Type ElementType) const;
	EActiveTimerReturnType HandlePendingRenameTimer(double InCurrentTime, float InDeltaTime, TWeakPtr<SInlineEditableTextBlock> InInlineEditableTextBlock);
	void HandleNotifyChanged();

	/** The legacy notify panel */
	TSharedPtr<SAnimNotifyPanel_COPY> AnimNotifyPanel;

	/** The outliner widget to allow for dynamic refresh */
	TSharedPtr<SVerticalBox> OutlinerWidget;

	/** Track index we want to trigger a rename for */
	int32 PendingRenameTrackIndex;

	/** Handle to status bar message */
	FStatusBarMessageHandle StatusBarMessageHandle;
};
