// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "AnimTimelineTrack_Curve_COPY.h"

struct FVectorCurve;
class SBorder;
class FCurveEditor;

class FAnimTimelineTrack_VectorCurve_COPY : public FAnimTimelineTrack_Curve_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_VectorCurve_COPY, FAnimTimelineTrack_Curve_COPY);

public:
	FAnimTimelineTrack_VectorCurve_COPY(const FVectorCurve* InCurve, const FSmartName& InName, int32 InCurveIndex, ERawCurveTrackTypes InType, const FText& InCurveName, const FText& InFullCurveName, const FLinearColor& InColor, const TSharedRef<FAnimModel_COPY>& InModel);

	/** FAnimTimelineTrack_Curve_COPY interface */
	virtual FLinearColor GetCurveColor(int32 InCurveIndex) const override;
	virtual FText GetFullCurveName(int32 InCurveIndex) const override;
	virtual bool CanEditCurve(int32 InCurveIndex) const override { return true; }
	virtual void GetCurveEditInfo(int32 InCurveIndex, FSmartName& OutName, ERawCurveTrackTypes& OutType, int32& OutCurveIndex) const override;

	/** Access the curve we are editing */
	const FVectorCurve& GetVectorCurve() { return *VectorCurve; }

private:
	/** The curve we are editing */
	const FVectorCurve* VectorCurve;
};
