// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "AnimModel_COPY.h"
#include "PersonaDelegates.h"
#include "SAnimTimingPanel_COPY.h"
#include "EditorUndoClient.h"
#include "Animation/AnimSequenceHelpers.h"
#include "Animation/AnimData/AnimDataModelNotifyCollector.h"

class UAnimSequenceBase;
class FAnimTimelineTrack_Notifies_COPY;
class FAnimTimelineTrack_Curves_COPY;
class FAnimTimelineTrack_COPY;
class FAnimTimelineTrack_NotifiesPanel_COPY;
class FAnimTimelineTrack_Attributes_COPY;
enum class EFrameNumberDisplayFormats : uint8;

/** Anim model for an anim sequence base */
class FAnimModel_AnimSequenceBase_COPY : public FAnimModel_COPY
{
public:
	FAnimModel_AnimSequenceBase_COPY(const TSharedRef<IPersonaPreviewScene>& InPreviewScene, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, const TSharedRef<FUICommandList>& InCommandList, UAnimSequenceBase* InAnimSequenceBase);

	~FAnimModel_AnimSequenceBase_COPY();

	/** FAnimModel interface */
	virtual void RefreshTracks() override;
	virtual UAnimSequenceBase* GetAnimSequenceBase() const override;
	virtual void Initialize() override;
	virtual void UpdateRange() override;

	const TSharedPtr<FAnimTimelineTrack_Notifies_COPY>& GetNotifyRoot() const { return NotifyRoot; }

	/** Delegate used to edit curves */
	FOnEditCurves OnEditCurves;

	/** Notify track timing options */
	bool IsNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType) const;
	void ToggleNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType);

	/** 
	 * Clamps the sequence to the specified length 
	 * @return		Whether clamping was/is necessary
	 */
	virtual bool ClampToEndTime(float NewEndTime);
	
	/** Refresh any simple snap times */
	virtual void RefreshSnapTimes();

protected:
	/** Refresh notify tracks */
	void RefreshNotifyTracks();

	/** Refresh curve tracks */
	void RefreshCurveTracks();

	/** Refresh attribute tracks */
	void RefreshAttributeTracks();

	/** Callback for any change made to the IAnimationDataModel embedded in the AnimSequenceBase instance this represents */
	virtual void OnDataModelChanged(const EAnimDataModelNotifyType& NotifyType, IAnimationDataModel* Model, const FAnimDataModelNotifPayload& PayLoad);

private:
	/** UI handlers */
	void EditSelectedCurves();
	bool CanEditSelectedCurves() const;
	void RemoveSelectedCurves();
	void CopySelectedCurveNamesToClipboard();
	void SetDisplayFormat(EFrameNumberDisplayFormats InFormat);
	bool IsDisplayFormatChecked(EFrameNumberDisplayFormats InFormat) const;
	void ToggleDisplayPercentage();
	bool IsDisplayPercentageChecked() const;
	void ToggleDisplaySecondary();
	bool IsDisplaySecondaryChecked() const;
	bool AreAnyCurvesSelected() const;
	
	/** Copy selected curves to clipboard */
	void CopyToClipboard() const;
	bool CanCopyToClipboard();

	/** Paste curve data into selected curve. Only modifies curves, does not add any new curves. */
	void PasteDataFromClipboardToSelectedCurve();
	bool CanPasteDataFromClipboardToSelectedCurve();

	/** Paste curves from clipboard. Adds or overwrites curves (if identifiers collide) */
	void PasteFromClipboard();
	bool CanPasteFromClipboard();

	/** Cut selected curves to clipboard */
	void CutToClipboard();
	bool CanCutToClipboard();

private:
	/** The anim sequence base we wrap */
	UAnimSequenceBase* AnimSequenceBase;

	/** Root track for notifies */
	TSharedPtr<FAnimTimelineTrack_Notifies_COPY> NotifyRoot;

	/** Legacy notify panel track */
	TSharedPtr<FAnimTimelineTrack_NotifiesPanel_COPY> NotifyPanel;

	/** Root track for curves */
	TSharedPtr<FAnimTimelineTrack_Curves_COPY> CurveRoot;

	/** Root track for additive layers */
	TSharedPtr<FAnimTimelineTrack_COPY> AdditiveRoot;

	/** Root track for custom attributes */
	TSharedPtr<FAnimTimelineTrack_Attributes_COPY> AttributesRoot;

	/** Display flags for notifies track */
	bool NotifiesTimingElementNodeDisplayFlags[ETimingElementType::Max];
protected:
	UE::Anim::FAnimDataModelNotifyCollector NotifyCollector;
};
