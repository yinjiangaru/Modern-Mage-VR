// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "AnimTimelineTrack_COPY.h"
#include "Animation/Skeleton.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

class FAnimTimelineTrack_Curves_COPY : public FAnimTimelineTrack_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_Curves_COPY, FAnimTimelineTrack_COPY);

public:
	FAnimTimelineTrack_Curves_COPY(const TSharedRef<FAnimModel_COPY>& InModel);

	/** FAnimTimelineTrack interface */
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow) override;

private:
	TSharedRef<SWidget> BuildCurvesSubMenu();
	void FillMetadataEntryMenu(FMenuBuilder& Builder);
	void FillVariableCurveMenu(FMenuBuilder& Builder);
	void AddMetadataEntry(USkeleton::AnimCurveUID Uid);
	void CreateNewMetadataEntryClicked();
	void CreateNewMetadataEntry(const FText& CommittedText, ETextCommit::Type CommitType);
	void CreateNewCurveClicked();
	void CreateTrack(const FText& ComittedText, ETextCommit::Type CommitInfo);
	void AddVariableCurve(USkeleton::AnimCurveUID CurveUid);
	void DeleteAllCurves();

	/** Handlers for showing curve points */
	void HandleShowCurvePoints();
	bool IsShowCurvePointsEnabled() const;

	/** Curve Picker Callbacks */
	void OnMetadataCurveNamePicked(const FSmartName & InCurveSmartName);
	void OnVariableCurveNamePicked(const FSmartName & InCurveSmartName);
	bool IsCurveMarkedForExclusion(const FSmartName & InCurveSmartName);
	
private:
	TSharedPtr<SWidget>	OutlinerWidget;
};
