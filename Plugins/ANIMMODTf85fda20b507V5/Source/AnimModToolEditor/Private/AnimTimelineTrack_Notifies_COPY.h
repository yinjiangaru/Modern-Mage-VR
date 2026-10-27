// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "AnimTimelineTrack_COPY.h"
#include "SAnimTimingPanel_COPY.h"

class FAnimTimelineTrack_NotifiesPanel_COPY;

class FAnimTimelineTrack_Notifies_COPY : public FAnimTimelineTrack_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_Notifies_COPY, FAnimTimelineTrack_COPY);

public:
	FAnimTimelineTrack_Notifies_COPY(const TSharedRef<FAnimModel_COPY>& InModel);

	/** FAnimTimelineTrack interface */
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow) override;

	/** Get a new, unused track name using the specified anim sequence */
	static FName GetNewTrackName(UAnimSequenceBase* InAnimSequenceBase);

	void SetAnimNotifyPanel(const TSharedRef<FAnimTimelineTrack_NotifiesPanel_COPY>& InNotifiesPanel) { NotifiesPanel = InNotifiesPanel; }

	/** Controls timing visibility for notify tracks */
	EVisibility OnGetTimingNodeVisibility(ETimingElementType::Type InType) const;

private:
	/** Populate the track menu */
	TSharedRef<SWidget> BuildNotifiesSubMenu();

	/** Add a new track */
	void AddTrack();
	
	/** The legacy notifies panel we are linked to */
	TWeakPtr<FAnimTimelineTrack_NotifiesPanel_COPY> NotifiesPanel;
};
