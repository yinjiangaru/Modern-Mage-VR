// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "SAnimTrackPanel_COPY.h"

#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "SlotBase.h"
#include "Types/SlateEnums.h"
#include "Types/SlateStructs.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "AnimTrackPanel"

//////////////////////////////////////////////////////////////////////////
// S2ColumnWidget_COPY

void S2ColumnWidget_COPY::Construct(const FArguments& InArgs)
{
	this->ChildSlot
		[
			SNew( SBorder )
			.Padding( FMargin(2.f, 2.f) )
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(1)
				[
					SAssignNew(LeftColumn, SVerticalBox)
				]

				+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					[
						SNew( SBox )
						.WidthOverride(InArgs._WidgetWidth)
						.HAlign(HAlign_Center)
						[
							SAssignNew(RightColumn,SVerticalBox)
						]
					]
			]
		];
}

//////////////////////////////////////////////////////////////////////////
// SAnimTrackPanel_COPY

void SAnimTrackPanel_COPY::Construct(const FArguments& InArgs)
{
	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;
	InputMin = InArgs._InputMin;
	InputMax = InArgs._InputMax;
	OnSetInputViewRange = InArgs._OnSetInputViewRange;

	WidgetWidth = InArgs._WidgetWidth;
}

TSharedRef<class S2ColumnWidget_COPY> SAnimTrackPanel_COPY::Create2ColumnWidget( TSharedRef<SVerticalBox> Parent )
{
	TSharedPtr<S2ColumnWidget_COPY> NewTrack;
	Parent->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			SAssignNew (NewTrack, S2ColumnWidget_COPY)
			.WidgetWidth(WidgetWidth)
		];

	return NewTrack.ToSharedRef();
}

void SAnimTrackPanel_COPY::PanInputViewRange(int32 ScreenDelta, FVector2D ScreenViewSize)
{
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(),  ViewInputMax.Get(), 0.f, 0.f, ScreenViewSize);

	float InputDeltaX = ScaleInfo.PixelsPerInput > 0.0f ? ScreenDelta/ScaleInfo.PixelsPerInput : 0.0f;

	float NewViewInputMin = ViewInputMin.Get() + InputDeltaX;
	float NewViewInputMax = ViewInputMax.Get() + InputDeltaX;

	InputViewRangeChanged(NewViewInputMin, NewViewInputMax);
}

void SAnimTrackPanel_COPY::InputViewRangeChanged(float ViewMin, float ViewMax)
{
	OnSetInputViewRange.ExecuteIfBound(ViewMin, ViewMax);
}

#undef LOCTEXT_NAMESPACE
