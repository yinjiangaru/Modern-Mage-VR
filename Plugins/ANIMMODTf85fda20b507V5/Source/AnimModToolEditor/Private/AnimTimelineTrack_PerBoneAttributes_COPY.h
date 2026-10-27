// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "AnimTimelineTrack_COPY.h"

/** Animation timeline track inserted for each unique bone containing animated attributes (inserted as child of FAnimTimelineTrack_Attributes) */
class FAnimTimelineTrack_PerBoneAttributes_COPY : public FAnimTimelineTrack_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_PerBoneAttributes_COPY, FAnimTimelineTrack_COPY);

public:
	FAnimTimelineTrack_PerBoneAttributes_COPY(const FName& InBoneName, const TSharedRef<FAnimModel_COPY>& InModel);

	/** FAnimTimelineTrack interface */
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow) override;
	virtual FText GetLabel() const override;
	virtual FText GetToolTipText() const override;
	   	   
private:
	FName BoneName;
	TSharedPtr<SWidget> OutlinerWidget;
};
