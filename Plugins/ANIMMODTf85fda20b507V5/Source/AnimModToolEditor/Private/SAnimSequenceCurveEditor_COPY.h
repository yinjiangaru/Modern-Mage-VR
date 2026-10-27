// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "IAnimSequenceCurveEditor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EditorUndoClient.h"
#include "RichCurveEditorModel.h"
#include "Animation/SmartName.h"
#include "Animation/AnimCurveTypes.h"
#include "CurveEditorTypes.h"
#include "Animation/AnimSequenceBase.h"
#include "CurveEditorKeyProxy.h"
#include "Tree/ICurveEditorTreeItem.h"
#include "SAnimSequenceCurveEditor_COPY.generated.h"

class FCurveEditor;
class ITimeSliderController;
class IPersonaPreviewScene;
class SCurveEditorPanel;
class FTabManager;

// Model that references a named curve, rather than a raw pointer, so we avoid issues with 
// reallocation of the curves arrays under the UI
class FRichCurveEditorModelNamed_COPY : public FCurveModel
{
public:
	FRichCurveEditorModelNamed_COPY(const FSmartName& InName, ERawCurveTrackTypes InType, int32 InCurveIndex, class UAnimModToolConfig* animModToolConfig);
	virtual ~FRichCurveEditorModelNamed_COPY() {}

	virtual const void* GetCurve() const override;

	virtual void Modify() override;

	virtual void DrawCurve(const FCurveEditor& CurveEditor, const FCurveEditorScreenSpace& ScreenSpace, TArray<TTuple<double, double>>& InterpolatingPoints) const override;
	virtual void GetKeys(const FCurveEditor& CurveEditor, double MinTime, double MaxTime, double MinValue, double MaxValue, TArray<FKeyHandle>& OutKeyHandles) const override;
	virtual void GetKeyDrawInfo(ECurvePointType PointType, const FKeyHandle InKeyHandle, FKeyDrawInfo& OutDrawInfo) const override;

	virtual void GetKeys(double MinTime, double MaxTime, double MinValue, double MaxValue, TArray<FKeyHandle>& OutKeyHandles) const override;

	virtual void GetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyPosition> OutKeyPositions) const override;
	virtual void SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType) override;

	virtual void GetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyAttributes> OutAttributes) const override;
	virtual void SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType = EPropertyChangeType::Unspecified) override;

	virtual void GetCurveAttributes(FCurveAttributes& OutCurveAttributes) const override;
	virtual void SetCurveAttributes(const FCurveAttributes& InCurveAttributes) override;
	virtual void GetTimeRange(double& MinTime, double& MaxTime) const override;
	virtual void GetValueRange(double& MinValue, double& MaxValue) const override;
	virtual int32 GetNumKeys() const override;
	virtual void GetNeighboringKeys(const FKeyHandle InKeyHandle, TOptional<FKeyHandle>& OutPreviousKeyHandle, TOptional<FKeyHandle>& OutNextKeyHandle) const override;
	virtual bool Evaluate(double ProspectiveTime, double& OutValue) const override;
	virtual void AddKeys(TArrayView<const FKeyPosition> InKeyPositions, TArrayView<const FKeyAttributes> InAttributes, TArrayView<TOptional<FKeyHandle>>* OutKeyHandles) override;
	virtual void RemoveKeys(TArrayView<const FKeyHandle> InKeys) override;
	virtual void RemoveKeys(TArrayView<const FKeyHandle> InKeys, double InCurrentTime) override { RemoveKeys(InKeys); }

	virtual void CreateKeyProxies(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<UObject*> OutObjects) override;

	virtual TUniquePtr<IBufferedCurveModel> CreateBufferedCurveCopy() const override;

	// Set a range to clamp key input values
	void SetClampInputRange(TAttribute<TRange<double>> InClampInputRange) { ClampInputRange = InClampInputRange; }

	// Check for whether this rich curve is valid. This is required mostly in the case of undo/redo, where 
	// curves can potentially become invalid underneath this model before owners get their undo/redo callbacks
	virtual bool IsValid() const;

	// Get the rich curve we are operating on
	virtual FRichCurve& GetRichCurve();
	virtual const FRichCurve& GetReadOnlyRichCurve() const;

	FSmartName Name;
	int32 CurveIndex;
	ERawCurveTrackTypes Type;

	FAnimationCurveIdentifier CurveId;
	UE::Anim::FAnimDataModelNotifyCollector NotifyCollector;
	bool bCurveRemoved;

private:

	TWeakObjectPtr<> WeakOwner;

	TAttribute<TRange<double>> ClampInputRange;

	FFloatCurve* TimeManipulationFloatCurve;
};

UCLASS()
class URichCurveKeyProxy_COPY : public UObject, public ICurveEditorKeyProxy
{
public:
	GENERATED_BODY()

	/**
	 * Initialize this key proxy object by caching the underlying key object, and retrieving the time/value each tick
	 */
	void Initialize(FKeyHandle InKeyHandle, FRichCurveEditorModelNamed_COPY* InModel, TWeakObjectPtr<UObject> InWeakOwner)
	{
		KeyHandle = InKeyHandle;
		WeakOwner = InWeakOwner;
		Model = InModel;

		if (UObject* Owner = WeakOwner.Get())
		{
			if (Model->IsValid())
			{
				const FRichCurve& RichCurve = Model->GetReadOnlyRichCurve();
				if (RichCurve.IsKeyHandleValid(KeyHandle))
				{
					Value = RichCurve.GetKey(KeyHandle);
				}
			}
		}
	}

private:

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);

		if (UObject* Owner = WeakOwner.Get())
		{
			if (Model->IsValid())
			{
				FRichCurve& RichCurve = Model->GetRichCurve();
				if (RichCurve.IsKeyHandleValid(KeyHandle))
				{
					Owner->Modify();

					FRichCurveKey& ActualKey = RichCurve.GetKey(KeyHandle);
					const FKeyPosition KeyPosition(Value.Time, Value.Value);
					Model->SetKeyPositions({ KeyHandle }, { KeyPosition }, PropertyChangedEvent.ChangeType);

					Owner->PostEditChangeProperty(PropertyChangedEvent);
					Model->OnCurveModified().Broadcast();
				}
			}
		}
	}

	virtual void UpdateValuesFromRawData() override
	{
		if (UObject* Owner = WeakOwner.Get())
		{
			if (Model->IsValid())
			{
				const FRichCurve& RichCurve = Model->GetReadOnlyRichCurve();
				if (RichCurve.IsKeyHandleValid(KeyHandle))
				{
					Value = RichCurve.GetKey(KeyHandle);
				}
			}
		}
	}

private:

	/** User-facing value of the key, applied to the actual key on PostEditChange, and updated every tick */
	UPROPERTY(EditAnywhere, Category = "Key", meta = (ShowOnlyInnerProperties))
	FRichCurveKey Value;

private:

	/** Cached key handle that this key proxy relates to */
	FKeyHandle KeyHandle;
	/** Cached model that created this proxy */
	FRichCurveEditorModelNamed_COPY* Model;
	/** Cached owner in which the raw curve resides */
	TWeakObjectPtr<UObject> WeakOwner;
};

class FAnimSequenceCurveEditorItem : public ICurveEditorTreeItem
{
public:
	FAnimSequenceCurveEditorItem(const FSmartName& InName, ERawCurveTrackTypes InType, int32 InCurveIndex, UAnimSequenceBase* InAnimSequence, const FText& InCurveDisplayName, const FLinearColor& InCurveColor, FSimpleDelegate InOnCurveModified, FCurveEditorTreeItemID InTreeId)
		: Name(InName)
		, Type(InType)
		, CurveIndex(InCurveIndex)
		, AnimSequence(InAnimSequence)
		, CurveDisplayName(InCurveDisplayName)
		, CurveColor(InCurveColor)
		, OnCurveModified(InOnCurveModified)
		, TreeId(InTreeId)
	{}

	virtual TSharedPtr<SWidget> GenerateCurveEditorTreeWidget(const FName& InColumnName, TWeakPtr<FCurveEditor> InCurveEditor, FCurveEditorTreeItemID InTreeItemID, const TSharedRef<ITableRow>& InTableRow) override
	{
		return nullptr;
	}

	virtual void CreateCurveModels(TArray<TUniquePtr<FCurveModel>>& OutCurveModels) override
	{
		//TUniquePtr<FRichCurveEditorModelNamed_COPY> NewCurveModel = MakeUnique<FRichCurveEditorModelNamed_COPY>(Name, Type, CurveIndex, AnimSequence.Get(), TreeId);
		//NewCurveModel->SetShortDisplayName(CurveDisplayName);
		//NewCurveModel->SetLongDisplayName(CurveDisplayName);
		//NewCurveModel->SetColor(CurveColor);
		//NewCurveModel->OnCurveModified().Add(OnCurveModified);

		//OutCurveModels.Add(MoveTemp(NewCurveModel));
	}

	virtual bool PassesFilter(const FCurveEditorTreeFilter* InFilter) const override
	{
		return true;
	}

	FSmartName Name;
	ERawCurveTrackTypes Type;
	int32 CurveIndex;
	TWeakObjectPtr<UAnimSequenceBase> AnimSequence;
	FText CurveDisplayName;
	FLinearColor CurveColor;
	FSimpleDelegate OnCurveModified;
	FCurveEditorTreeItemID TreeId;
};

class SAnimSequenceCurveEditor_COPY : public IAnimSequenceCurveEditor
{
	SLATE_BEGIN_ARGS(SAnimSequenceCurveEditor_COPY) {}

	SLATE_ARGUMENT(TSharedPtr<ITimeSliderController>, ExternalTimeSliderController)

	SLATE_ARGUMENT(TSharedPtr<FTabManager>, TabManager)

	SLATE_END_ARGS()

	~SAnimSequenceCurveEditor_COPY();

	void Construct(const FArguments& InArgs, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, UAnimSequenceBase* InAnimSequence);

	/** IAnimSequenceCurveEditor interface */
	virtual void ResetCurves() override {}
	virtual void AddCurve(const FText& InCurveDisplayName, const FLinearColor& InCurveColor, const FSmartName& InName, ERawCurveTrackTypes InType, int32 InCurveIndex, FSimpleDelegate InOnCurveModified) override {}
	virtual void RemoveCurve(const FSmartName& InName, ERawCurveTrackTypes InType, int32 InCurveIndex) override {}
	virtual void AddCurve(const FText& InCurveDisplayName, const FLinearColor& InCurveColor, const FName& InName, ERawCurveTrackTypes InType, int32 InCurveIndex, FSimpleDelegate InOnCurveModified) override {}
	virtual void RemoveCurve(const FName& InName, ERawCurveTrackTypes InType, int32 InCurveIndex) override {}
	virtual void ZoomToFit() override;
	
	void OnModelHasChanged(const EAnimDataModelNotifyType& NotifyType, IAnimationDataModel* Model, const FAnimDataModelNotifPayload& Payload);
private:
	// Build the toolbar for this curve editor
	TSharedRef<SWidget> MakeToolbar(TSharedRef<SCurveEditorPanel> InEditorPanel);

public:
	/** The actual curve editor */
	TSharedPtr<FCurveEditor> CurveEditor;

private:
	/** The anim sequence we are editing */
	UAnimSequenceBase* AnimSequence;
};