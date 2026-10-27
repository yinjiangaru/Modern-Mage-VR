// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "AnimTimelineTrack_Curves_COPY.h"
#include "PersonaUtils.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "AnimSequenceTimelineCommands_COPY.h"
#include "SAnimCurvePicker_COPY.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Animation/AnimSequenceBase.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Framework/Application/SlateApplication.h"
#include "ScopedTransaction.h"
#include "Animation/AnimMontage.h"
#include "SAnimOutlinerItem_COPY.h"
#include "Preferences/PersonaOptions.h"
#include "SListViewSelectorDropdownMenu.h"
#include "Animation/AnimData/IAnimationDataModel.h"

#define LOCTEXT_NAMESPACE "FAnimTimelineTrack_Curves_COPY"

ANIMTIMELINE_IMPLEMENT_TRACK(FAnimTimelineTrack_Curves_COPY);

FAnimTimelineTrack_Curves_COPY::FAnimTimelineTrack_Curves_COPY(const TSharedRef<FAnimModel_COPY>& InModel)
	: FAnimTimelineTrack_COPY(LOCTEXT("CurvesRootTrackLabel", "Curves"), LOCTEXT("CurvesRootTrackToolTip", "Curve data contained in this asset"), InModel)
{
}

TSharedRef<SWidget> FAnimTimelineTrack_Curves_COPY::GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem_COPY>& InRow)
{
	TSharedPtr<SBorder> OuterBorder;
	TSharedPtr<SHorizontalBox> InnerHorizontalBox;
	OutlinerWidget = GenerateStandardOutlinerWidget(InRow, false, OuterBorder, InnerHorizontalBox);

	OuterBorder->SetBorderBackgroundColor(FAppStyle::GetColor("AnimTimeline.Outliner.HeaderColor"));

	InnerHorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(2.0f, 1.0f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("AnimTimeline.Outliner.Label"))
			.Text(this, &FAnimTimelineTrack_Curves_COPY::GetLabel)
			.HighlightText(InRow->GetHighlightText())
		];

	InnerHorizontalBox->AddSlot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5.0f, 1.0f)
		[
			SNew(STextBlock)
			.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("TinyText"))
			.Text_Lambda([this]()
			{ 
				UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();
				return FText::Format(LOCTEXT("CurveCountFormat", "({0})"), FText::AsNumber(AnimSequenceBase->GetDataModel()->GetNumberOfFloatCurves())); 
			})
		];

	UAnimMontage* AnimMontage = Cast<UAnimMontage>(GetModel()->GetAnimSequenceBase());
	if(!(AnimMontage && AnimMontage->HasParentAsset()))
	{
		InnerHorizontalBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(OutlinerRightPadding, 1.0f)
			[
				PersonaUtils::MakeTrackButton(LOCTEXT("EditCurvesButtonText", "Curves"), FOnGetContent::CreateSP(this, &FAnimTimelineTrack_Curves_COPY::BuildCurvesSubMenu), MakeAttributeSP(this, &FAnimTimelineTrack_Curves_COPY::IsHovered))
			];
	}

	return OutlinerWidget.ToSharedRef();
}

void FAnimTimelineTrack_Curves_COPY::DeleteAllCurves()
{
	UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();
	IAnimationDataController& Controller = AnimSequenceBase->GetController();
	Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float);
}

TSharedRef<SWidget> FAnimTimelineTrack_Curves_COPY::BuildCurvesSubMenu()
{
	FMenuBuilder MenuBuilder(true, GetModel()->GetCommandList());

	MenuBuilder.BeginSection("Curves", LOCTEXT("CurvesMenuSection", "Curves"));
	{
		MenuBuilder.AddSubMenu(
			FAnimSequenceTimelineCommands_COPY::Get().AddCurve->GetLabel(),
			FAnimSequenceTimelineCommands_COPY::Get().AddCurve->GetDescription(),
			FNewMenuDelegate::CreateSP(this, &FAnimTimelineTrack_Curves_COPY::FillVariableCurveMenu)
		);

		MenuBuilder.AddSubMenu(
			FAnimSequenceTimelineCommands_COPY::Get().AddMetadata->GetLabel(),
			FAnimSequenceTimelineCommands_COPY::Get().AddMetadata->GetDescription(),
			FNewMenuDelegate::CreateSP(this, &FAnimTimelineTrack_Curves_COPY::FillMetadataEntryMenu)
		);

		UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();
		if(AnimSequenceBase->GetDataModel()->GetNumberOfFloatCurves() > 0)
		{
			MenuBuilder.AddMenuEntry(
				FAnimSequenceTimelineCommands_COPY::Get().RemoveAllCurves->GetLabel(),
				FAnimSequenceTimelineCommands_COPY::Get().RemoveAllCurves->GetDescription(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FAnimTimelineTrack_Curves_COPY::DeleteAllCurves))
			);
		}
	}

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Options", LOCTEXT("OptionsMenuSection", "Options"));
	{
		MenuBuilder.AddMenuEntry(
			FAnimSequenceTimelineCommands_COPY::Get().ShowCurveKeys->GetLabel(),
			FAnimSequenceTimelineCommands_COPY::Get().ShowCurveKeys->GetDescription(),
			FAnimSequenceTimelineCommands_COPY::Get().ShowCurveKeys->GetIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAnimTimelineTrack_Curves_COPY::HandleShowCurvePoints),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FAnimTimelineTrack_Curves_COPY::IsShowCurvePointsEnabled)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FAnimTimelineTrack_Curves_COPY::FillMetadataEntryMenu(FMenuBuilder& Builder)
{
	// Add new metadata entry menu button
	{
		const FText Description = LOCTEXT("NewMetadataCreateNew_ToolTip", "Create a new metadata entry");
		const FText Label = LOCTEXT("NewMetadataCreateNew_Label","Create Metadata Entry");
		FUIAction UIAction;
		UIAction.ExecuteAction.BindRaw(this, &FAnimTimelineTrack_Curves_COPY::CreateNewMetadataEntryClicked);

		Builder.AddMenuEntry(Label, Description, FSlateIcon(), UIAction);
	}
	
	Builder.BeginSection(NAME_None, LOCTEXT("MetadataMenu_ListHeading", "Available Names"));

	// Add existing curve to timeline using curve picker
	{
		const TSharedRef<SWidget> CurvePickerWidget = SNew(SAnimCurvePicker_COPY, GetModel()->GetEditableSkeleton())
		.OnCurveNamePicked(this, &FAnimTimelineTrack_Curves_COPY::OnMetadataCurveNamePicked)
		.IsCurveMarkedForExclusion(this, &FAnimTimelineTrack_Curves_COPY::IsCurveMarkedForExclusion);
		Builder.AddWidget(CurvePickerWidget, FText::GetEmpty(), true);
	}
	
	Builder.EndSection();
}

void FAnimTimelineTrack_Curves_COPY::FillVariableCurveMenu(FMenuBuilder& Builder)
{
	// Menu entry to create a new curve
	{
		FText Description = LOCTEXT("NewVariableCurveCreateNew_ToolTip", "Create a new variable curve");
		FText Label = LOCTEXT("NewVariableCurveCreateNew_Label", "Create Curve");
		FUIAction UIAction;
		UIAction.ExecuteAction.BindRaw(this, &FAnimTimelineTrack_Curves_COPY::CreateNewCurveClicked);
		Builder.AddMenuEntry(Label, Description, FSlateIcon(), UIAction);
	}
	
	Builder.BeginSection(NAME_None, LOCTEXT("VariableMenu_ListHeading", "Available Names"));

	// Add existing curve to timeline using curve picker
	{
		const TSharedRef<SWidget> CurvePickerWidget = SNew(SAnimCurvePicker_COPY, GetModel()->GetEditableSkeleton())
		.OnCurveNamePicked(this, &FAnimTimelineTrack_Curves_COPY::OnVariableCurveNamePicked)
		.IsCurveMarkedForExclusion(this, &FAnimTimelineTrack_Curves_COPY::IsCurveMarkedForExclusion);
		
		Builder.AddWidget(CurvePickerWidget, FText::GetEmpty(), true);
	}
	
	Builder.EndSection();
}

void FAnimTimelineTrack_Curves_COPY::AddMetadataEntry(USkeleton::AnimCurveUID Uid)
{
	FSmartName NewName;
	UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();
	ensureAlways(AnimSequenceBase->GetSkeleton()->GetSmartNameByUID(USkeleton::AnimCurveMappingName, Uid, NewName));

	IAnimationDataController& Controller = AnimSequenceBase->GetController();
	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("AddCurveMetadata", "Add Curve Metadata"));

	const FAnimationCurveIdentifier MetadataCurveId(NewName, ERawCurveTrackTypes::RCT_Float);
	Controller.AddCurve(MetadataCurveId, AACF_Metadata);
	Controller.SetCurveKeys(MetadataCurveId, { FRichCurveKey(0.f, 1.f) });	
}

void FAnimTimelineTrack_Curves_COPY::CreateNewMetadataEntryClicked()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewMetadataCurveEntryLabal", "Metadata Name"))
		.OnTextCommitted(this, &FAnimTimelineTrack_Curves_COPY::CreateNewMetadataEntry);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.PushMenu(
		OutlinerWidget.ToSharedRef(),
		FWidgetPath(),
		TextEntry,
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}

void FAnimTimelineTrack_Curves_COPY::CreateNewMetadataEntry(const FText& CommittedText, ETextCommit::Type CommitType)
{
	FSlateApplication::Get().DismissAllMenus();
	if(CommitType == ETextCommit::OnEnter)
	{
		// Add the name to the skeleton and then add the new curve to the sequence
		UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();
		USkeleton* Skeleton = AnimSequenceBase->GetSkeleton();
		if(Skeleton && !CommittedText.IsEmpty())
		{
			FSmartName CurveName;

			if(Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, FName(*CommittedText.ToString()), CurveName))
			{
				AddMetadataEntry(CurveName.UID);
			}
		}
	}
}

void FAnimTimelineTrack_Curves_COPY::CreateNewCurveClicked()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewCurveEntryLabal", "Curve Name"))
		.OnTextCommitted(this, &FAnimTimelineTrack_Curves_COPY::CreateTrack);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.PushMenu(
		OutlinerWidget.ToSharedRef(),
		FWidgetPath(),
		TextEntry,
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}

void FAnimTimelineTrack_Curves_COPY::CreateTrack(const FText& ComittedText, ETextCommit::Type CommitInfo)
{
	if ( CommitInfo == ETextCommit::OnEnter )
	{
		UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();
		USkeleton* Skeleton = AnimSequenceBase->GetSkeleton();
		if(Skeleton && !ComittedText.IsEmpty())
		{
			const FScopedTransaction Transaction(LOCTEXT("AnimCurve_AddTrack", "Add New Curve"));
			FSmartName NewTrackName;

			Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, FName(*ComittedText.ToString()), NewTrackName);
			if ( NewTrackName.IsValid() )
			{
				AddVariableCurve(NewTrackName.UID);
			}
		}

		FSlateApplication::Get().DismissAllMenus();
	}
}

void FAnimTimelineTrack_Curves_COPY::AddVariableCurve(USkeleton::AnimCurveUID CurveUid)
{
	FScopedTransaction Transaction(LOCTEXT("AddCurve", "Add Curve"));

	UAnimSequenceBase* AnimSequenceBase = GetModel()->GetAnimSequenceBase();
	AnimSequenceBase->Modify();
	
	USkeleton* Skeleton = AnimSequenceBase->GetSkeleton();
	FSmartName NewName;
	ensureAlways(Skeleton->GetSmartNameByUID(USkeleton::AnimCurveMappingName, CurveUid, NewName));

	IAnimationDataController& Controller = AnimSequenceBase->GetController();
	const FAnimationCurveIdentifier FloatCurveId(NewName, ERawCurveTrackTypes::RCT_Float);
	Controller.AddCurve(FloatCurveId);
}

void FAnimTimelineTrack_Curves_COPY::HandleShowCurvePoints()
{
	GetMutableDefault<UPersonaOptions>()->bTimelineDisplayCurveKeys = !GetDefault<UPersonaOptions>()->bTimelineDisplayCurveKeys;
}

bool FAnimTimelineTrack_Curves_COPY::IsShowCurvePointsEnabled() const
{
	return GetDefault<UPersonaOptions>()->bTimelineDisplayCurveKeys;
}

void FAnimTimelineTrack_Curves_COPY::OnMetadataCurveNamePicked(const FSmartName& InCurveSmartName)
{
	FSlateApplication::Get().DismissAllMenus();

	if (InCurveSmartName.IsValid())
	{
		AddMetadataEntry(InCurveSmartName.UID);
	}
}

void FAnimTimelineTrack_Curves_COPY::OnVariableCurveNamePicked(const FSmartName& InCurveSmartName)
{
	FSlateApplication::Get().DismissAllMenus();

	if (InCurveSmartName.IsValid())
	{
		AddVariableCurve(InCurveSmartName.UID);
	}
}

bool FAnimTimelineTrack_Curves_COPY::IsCurveMarkedForExclusion(const FSmartName& InCurveSmartName)
{
	return GetModel()->GetAnimSequenceBase()->GetDataModel()->FindFloatCurve(FAnimationCurveIdentifier(InCurveSmartName, ERawCurveTrackTypes::RCT_Float)) != nullptr;
}
#undef LOCTEXT_NAMESPACE
