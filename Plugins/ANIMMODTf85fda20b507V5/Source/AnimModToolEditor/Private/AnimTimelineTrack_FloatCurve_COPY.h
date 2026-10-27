// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "AnimTimelineTrack_Curve_COPY.h"

struct FFloatCurve;
class SBorder;
class FCurveEditor;

class FAnimTimelineTrack_FloatCurve_COPY : public FAnimTimelineTrack_Curve_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_FloatCurve_COPY, FAnimTimelineTrack_Curve_COPY);

public:
	FAnimTimelineTrack_FloatCurve_COPY(const FFloatCurve* InCurve, const TSharedRef<FAnimModel_COPY>& InModel);

	/** FAnimTimelineTrack_Curve_COPY interface */
	virtual TSharedRef<SWidget> MakeTimelineWidgetContainer() override;
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow) override;
	virtual TSharedRef<SWidget> BuildCurveTrackMenu() override;
	virtual FText GetLabel() const override;
	virtual bool CanEditCurve(int32 InCurveIndex) const override;
	virtual bool CanRename() const override { return true; }
	virtual void RequestRename() override;
	virtual void AddCurveTrackButton(TSharedPtr<SHorizontalBox> InnerHorizontalBox) override;
	virtual FLinearColor GetCurveColor(int32 InCurveIndex) const override;
	virtual void GetCurveEditInfo(int32 InCurveIndex, FSmartName& OutName, ERawCurveTrackTypes& OutType, int32& OutCurveIndex) const override;
	virtual bool SupportsCopy() const override { return true; }
	virtual void Copy(UAnimTimelineClipboardContent_COPY* InOutClipboard) const override;
	
	/** Access the curve we are editing */
	const FFloatCurve* GetFloatCurve() { return FloatCurve; }

	/** Helper function used to get a smart name for a curve */
	static FText GetFloatCurveName(const TSharedRef<FAnimModel_COPY>& InModel, const FSmartName& InSmartName);

	/** Get this curves name */
	FSmartName GetName() const { return CurveName; }

private:
	void ConvertCurveToMetaData();
	void ConvertMetaDataToCurve();
	void RemoveCurve();
	void OnCommitCurveName(const FText& InText, ETextCommit::Type CommitInfo);

private:
	/** The curve we are editing */
	const FFloatCurve* FloatCurve;

	/** The curve name and identifier */
	FSmartName CurveName;
	FAnimationCurveIdentifier CurveId;

	/** Label we can edit */
	TSharedPtr<SInlineEditableTextBlock> EditableTextLabel;
};
