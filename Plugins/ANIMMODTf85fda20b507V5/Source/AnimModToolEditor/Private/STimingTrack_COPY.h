// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "HAL/Platform.h"
#include "Misc/Attribute.h"
#include "STrack_COPY.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FArrangedChildren;
struct FGeometry;

//////////////////////////////////////////////////////////////////////////
// Specialised anim track which arranges overlapping nodes into groups
class STimingTrack_COPY : public STrack
{
public:

	SLATE_BEGIN_ARGS(STimingTrack_COPY)
	{}

	SLATE_ATTRIBUTE(float, ViewInputMin)
	SLATE_ATTRIBUTE(float, ViewInputMax)
	SLATE_ATTRIBUTE(float, TrackMaxValue)
	SLATE_ATTRIBUTE(float, TrackMinValue)
	SLATE_ATTRIBUTE(int32, TrackNumDiscreteValues)

	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;

protected:
};
