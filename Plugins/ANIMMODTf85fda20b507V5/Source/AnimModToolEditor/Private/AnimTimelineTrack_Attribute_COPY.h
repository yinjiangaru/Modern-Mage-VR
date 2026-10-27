// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "AnimTimelineTrack_COPY.h"

struct FAnimatedBoneAttribute;

/** Animation timeline track inserted for each animated attribute (inserted as child of FAnimTimelineTrack_PerBoneAttributes) */
class FAnimTimelineTrack_Attribute_COPY : public FAnimTimelineTrack_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_Attribute_COPY, FAnimTimelineTrack_COPY);

public:
	FAnimTimelineTrack_Attribute_COPY(const FAnimatedBoneAttribute& InAttribute, const TSharedRef<FAnimModel_COPY>& InModel);

	/** FAnimTimelineTrack interface */
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow) override;
	virtual FText GetLabel() const override;
	virtual FText GetToolTipText() const override;
	   	   
private:
	const FAnimatedBoneAttribute& Attribute;
	TSharedPtr<SWidget> OutlinerWidget;
};
