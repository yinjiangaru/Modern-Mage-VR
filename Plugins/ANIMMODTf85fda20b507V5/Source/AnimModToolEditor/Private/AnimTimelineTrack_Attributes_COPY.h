// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "AnimTimelineTrack_COPY.h"

/** Root-level animation timeline track under which per-bone animated attributes are inserted */
class FAnimTimelineTrack_Attributes_COPY : public FAnimTimelineTrack_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_Attributes_COPY, FAnimTimelineTrack_COPY);

public:
	FAnimTimelineTrack_Attributes_COPY(const TSharedRef<FAnimModel_COPY>& InModel);

	/** FAnimTimelineTrack interface */
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow) override;

private:
	TSharedPtr<SWidget> OutlinerWidget;
};
