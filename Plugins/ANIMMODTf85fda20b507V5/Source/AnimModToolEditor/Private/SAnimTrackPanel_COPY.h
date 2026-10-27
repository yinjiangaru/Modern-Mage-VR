// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "SCurveEditor.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

//////////////////////////////////////////////////////////////////////////
// S2ColumnWidget
//
// Widget for drawing any number of widgets in 2 columns, useful for track layout
class S2ColumnWidget_COPY : public SCompoundWidget
{
public:
	enum EDefaultColumnWidth
	{
		DEFAULT_RIGHT_COLUMN_WIDTH = 176
	};
	TSharedPtr<SVerticalBox> LeftColumn;
	TSharedPtr<SVerticalBox> RightColumn;

	SLATE_BEGIN_ARGS(S2ColumnWidget_COPY)
	{}
	SLATE_ARGUMENT( float, WidgetWidth )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

//////////////////////////////////////////////////////////////////////////
// SAnimTrackPanel

class SAnimTrackPanel_COPY: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAnimTrackPanel_COPY)
		: _ViewInputMin()
		, _ViewInputMax()
		, _InputMin()
		, _InputMax()
		, _OnSetInputViewRange()
	{}

	SLATE_ARGUMENT( float, WidgetWidth )
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_ATTRIBUTE( float, InputMin )
	SLATE_ATTRIBUTE( float, InputMax )
	SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void PanInputViewRange(int32 ScreenDelta, FVector2D ScreenViewSize);

	virtual float GetSequenceLength() const {return 0.0f;}

protected:

	// Calls out to notify of a range change, can be overridden by derived classes to respond
	// but they must call this version too after processing range changes
	virtual void InputViewRangeChanged(float ViewMin, float ViewMax);

	/** Create a 2 column widget */
	TSharedRef<class S2ColumnWidget_COPY> Create2ColumnWidget( TSharedRef<SVerticalBox> Parent );

	TAttribute<float> ViewInputMin;
	TAttribute<float> ViewInputMax;
	TAttribute<float> InputMin;
	TAttribute<float> InputMax;
	FOnSetInputViewRange OnSetInputViewRange;

	/** Controls the width of the tracks column */
	float WidgetWidth;
};
