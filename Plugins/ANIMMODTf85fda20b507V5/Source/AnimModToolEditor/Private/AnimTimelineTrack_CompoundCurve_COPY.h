// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once
#include "AnimTimelineTrack_Curve_COPY.h"
#include "AnimTimelineTrack_FloatCurve_COPY.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"

struct FFloatCurve;

//! \brief Tree view of a group of float curves, use AddGroupedCurveTracks(FloatCurves, ...) to create a tree view for curves
class FAnimTimelineTrack_CompoundCurve_COPY : public FAnimTimelineTrack_Curve_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_CompoundCurve_COPY, FAnimTimelineTrack_Curve_COPY);

public:
	FAnimTimelineTrack_CompoundCurve_COPY(TArray<const FFloatCurve*> InCurves, const FText& InCurveName, const FText& InFullCurveName, const FLinearColor& InBackgroundColor, const TSharedRef<FAnimModel_COPY>& InModel);

	/** FAnimTimelineTrack_Curve_COPY interface */
	virtual FLinearColor GetCurveColor(int32 InCurveIndex) const override;
	virtual FText GetFullCurveName(int32 InCurveIndex) const override;
	virtual bool CanEditCurve(int32 InCurveIndex) const override;
	virtual void GetCurveEditInfo(int32 InCurveIndex, FSmartName& OutName, ERawCurveTrackTypes& OutType, int32& OutCurveIndex) const override;
	/** End FAnimTimelineTrack_Curve_COPY interface */

	static constexpr auto DefaultDelimiters = TEXT("._/\\|");
	//! \brief Add grouped view curve tracks for a list of curves named like 'A.B.C'
	//! \param[in] FloatCurves The curves to display
	//! \param[inout] ParentTrack The parent track to add the curves, typically a FAnimTimelineTrack_Curve_COPY
	//! \param[in] InModel FAnimModel to display the track
	static void AddGroupedCurveTracks(TArrayView<const FFloatCurve> FloatCurves, FAnimTimelineTrack_COPY& ParentTrack, const TSharedRef<FAnimModel_COPY>& InModel, FStringView Delimiters = DefaultDelimiters);

	const TArray<FSmartName>& GetCurveNames() const { return CurveNames; }
private:
	TArray<const FFloatCurve*> Curves;
	TArray<FSmartName> CurveNames; // Curve smart names used when removing curves (in case the FFloatCurve was removed)

	static TArray<const FRichCurve*> ToRichCurves(TArrayView<FFloatCurve const* const> InCurves);
	struct FCurveGroup;
};

// FloatCurve with customizable display name, e.g. showing "C" for curve "A.B.C"
class FAnimTimelineTrack_FloatCurveWithDisplayName_COPY : public FAnimTimelineTrack_FloatCurve_COPY
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_FloatCurveWithDisplayName_COPY, FAnimTimelineTrack_FloatCurve_COPY);

public:
	FAnimTimelineTrack_FloatCurveWithDisplayName_COPY(const FFloatCurve* InCurve, FText InDisplayName, const TSharedRef<FAnimModel_COPY>& InModel);
	virtual FText GetLabel() const override;
	virtual bool ShowCurves() const override;
};
