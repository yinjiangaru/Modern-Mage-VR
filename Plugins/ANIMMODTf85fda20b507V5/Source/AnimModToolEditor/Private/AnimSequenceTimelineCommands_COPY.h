// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "CoreTypes.h"
#include "Framework/Commands/Commands.h"
#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class FUICommandInfo;

/**
 * Defines commands for the anim sequence timeline editor
 */
class FAnimSequenceTimelineCommands_COPY : public TCommands<FAnimSequenceTimelineCommands_COPY>
{
public:
	FAnimSequenceTimelineCommands_COPY()
		: TCommands<FAnimSequenceTimelineCommands_COPY>
		(
			TEXT("AnimSequenceCurveEditor (AnimModTools)"),
			NSLOCTEXT("Contexts", "AnimSequenceTimelineEditor_AnimModTools", "Anim Sequence Timeline Editor (AnimModTools)"),
			NAME_None,
			FAppStyle::GetAppStyleSetName()
		)
	{
	}

	TSharedPtr<FUICommandInfo> EditSelectedCurves;
	
	TSharedPtr<FUICommandInfo> AddNotifyTrack;
	
	TSharedPtr<FUICommandInfo> PasteDataIntoCurve;

	TSharedPtr<FUICommandInfo> InsertNotifyTrack;

	TSharedPtr<FUICommandInfo> RemoveNotifyTrack;

	TSharedPtr<FUICommandInfo> AddCurve;

	TSharedPtr<FUICommandInfo> EditCurve;

	TSharedPtr<FUICommandInfo> ShowCurveKeys;

	TSharedPtr<FUICommandInfo> AddMetadata;

	TSharedPtr<FUICommandInfo> ConvertCurveToMetaData;

	TSharedPtr<FUICommandInfo> ConvertMetaDataToCurve;

	TSharedPtr<FUICommandInfo> RemoveCurve;

	TSharedPtr<FUICommandInfo> RemoveAllCurves;

	TSharedPtr<FUICommandInfo> CopySelectedCurveNames;
	
	TSharedPtr<FUICommandInfo> DisplaySeconds;

	TSharedPtr<FUICommandInfo> DisplayFrames;

	TSharedPtr<FUICommandInfo> DisplayPercentage;

	TSharedPtr<FUICommandInfo> DisplaySecondaryFormat;

	TSharedPtr<FUICommandInfo> SnapToFrames;

	TSharedPtr<FUICommandInfo> SnapToNotifies;

	TSharedPtr<FUICommandInfo> SnapToMontageSections;

	TSharedPtr<FUICommandInfo> SnapToCompositeSegments;
public:
	virtual void RegisterCommands() override;
};
