// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "AnimTimelineTrack_Curve_COPY.h"

struct FTransformCurve;
class SBorder;
class FCurveEditor;

class FAnimTimelineTrack_TransformCurve_COPY : public FAnimTimelineTrack_Curve_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_TransformCurve_COPY, FAnimTimelineTrack_Curve_COPY);

public:
	FAnimTimelineTrack_TransformCurve_COPY(const FTransformCurve* InCurve, const TSharedRef<FAnimModel_COPY>& InModel);

	/** FAnimTimelineTrack_Curve_COPY interface */
	virtual FLinearColor GetCurveColor(int32 InCurveIndex) const override;
	virtual FText GetFullCurveName(int32 InCurveIndex) const override;
	virtual TSharedRef<SWidget> BuildCurveTrackMenu() override;
	virtual bool CanEditCurve(int32 InCurveIndex) const override { return true; }
	virtual void GetCurveEditInfo(int32 InCurveIndex, FSmartName& OutName, ERawCurveTrackTypes& OutType, int32& OutCurveIndex) const override;
	virtual bool SupportsCopy() const override { return true; }
	virtual void Copy(UAnimTimelineClipboardContent_COPY* InOutClipboard) const override;
	
	/** Access the curve we are editing */
	const FTransformCurve& GetTransformCurve() { return *TransformCurve; }

	/** Helper function used to get a smart name for a curve */
	static FText GetTransformCurveName(const TSharedRef<FAnimModel_COPY>& InModel, const FSmartName& InSmartName);

	/** Get this curves name */
	FSmartName GetName() const { return CurveName; }

private:
	/** Delete this track via the track menu */
	void DeleteTrack();

	/** Show enabled state in the menu */
	bool IsEnabled() const;

	/** Toggle enabled state via the menu */
	void ToggleEnabled();

private:
	/** The curve we are editing */
	const FTransformCurve* TransformCurve;

	/** The curve name and identifier */
	FSmartName CurveName;
	FAnimationCurveIdentifier CurveId;
};
