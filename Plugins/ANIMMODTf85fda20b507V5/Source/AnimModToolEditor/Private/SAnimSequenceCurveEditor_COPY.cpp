// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "SAnimSequenceCurveEditor_COPY.h"
#include "CurveEditor.h"
#include "RichCurveEditorModel.h"
#include "Animation/AnimSequenceBase.h"
#include "SCurveEditorPanel.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "SAnimTimelineTransportControls_COPY.h"
#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "ToolMenus.h"
#include "AnimModToolEditor_private.h"
#include "AnimatedRange.h"

#define LOCTEXT_NAMESPACE "SAnimSequenceCurveEditor_COPY"

FText scopedTransactionText2 = LOCTEXT("FRichCurveEditorModelNamed_COPY_ScopedTransaction", "Change Anim Mod Tool config...");

void RefineCurvePoints_COPY(const FRichCurve& RichCurve, double TimeThreshold, float ValueThreshold, TArray<TTuple<double, double>>& InOutPoints)
{
	const float InterpTimes[] = { 0.25f, 0.5f, 0.6f };

	for (int32 Index = 0; Index < InOutPoints.Num() - 1; ++Index)
	{
		TTuple<double, double> Lower = InOutPoints[Index];
		TTuple<double, double> Upper = InOutPoints[Index + 1];

		if ((Upper.Get<0>() - Lower.Get<0>()) >= TimeThreshold)
		{
			bool bSegmentIsLinear = true;

			TTuple<double, double> Evaluated[UE_ARRAY_COUNT(InterpTimes)] = { TTuple<double, double>(0, 0) };

			for (int32 InterpIndex = 0; InterpIndex < UE_ARRAY_COUNT(InterpTimes); ++InterpIndex)
			{
				double& EvalTime = Evaluated[InterpIndex].Get<0>();

				EvalTime = FMath::Lerp(Lower.Get<0>(), Upper.Get<0>(), InterpTimes[InterpIndex]);

				float Value = RichCurve.Eval(EvalTime);

				const float LinearValue = FMath::Lerp(Lower.Get<1>(), Upper.Get<1>(), InterpTimes[InterpIndex]);
				if (bSegmentIsLinear)
				{
					bSegmentIsLinear = FMath::IsNearlyEqual(Value, LinearValue, ValueThreshold);
				}

				Evaluated[InterpIndex].Get<1>() = Value;
			}

			if (!bSegmentIsLinear)
			{
				// Add the point
				InOutPoints.Insert(Evaluated, UE_ARRAY_COUNT(Evaluated), Index + 1);
				--Index;
			}
		}
	}
}

class FRichBufferedCurveModel_COPY : public IBufferedCurveModel
{
public:
	FRichBufferedCurveModel_COPY(const FRichCurve& InRichCurve, TArray<FKeyPosition>&& InKeyPositions, TArray<FKeyAttributes>&& InKeyAttributes,
		const FString& InLongDisplayName, const double InValueMin, const double InValueMax)
		: IBufferedCurveModel(MoveTemp(InKeyPositions), MoveTemp(InKeyAttributes), InLongDisplayName, InValueMin, InValueMax)
		, RichCurve(InRichCurve)
	{}

	virtual void DrawCurve(const FCurveEditor& CurveEditor, const FCurveEditorScreenSpace& ScreenSpace, TArray<TTuple<double, double>>& InterpolatingPoints) const override
	{
		const double StartTimeSeconds = ScreenSpace.GetInputMin();
		const double EndTimeSeconds = ScreenSpace.GetInputMax();
		const double TimeThreshold = FMath::Max(0.0001, 1.0 / ScreenSpace.PixelsPerInput());
		const double ValueThreshold = FMath::Max(0.0001, 1.0 / ScreenSpace.PixelsPerOutput());

		InterpolatingPoints.Add(MakeTuple(StartTimeSeconds, double(RichCurve.Eval(StartTimeSeconds))));

		for (const FRichCurveKey& Key : RichCurve.GetConstRefOfKeys())
		{
			if (Key.Time > StartTimeSeconds && Key.Time < EndTimeSeconds)
			{
				InterpolatingPoints.Add(MakeTuple(double(Key.Time), double(Key.Value)));
			}
		}

		InterpolatingPoints.Add(MakeTuple(EndTimeSeconds, double(RichCurve.Eval(EndTimeSeconds))));

		int32 OldSize = InterpolatingPoints.Num();
		do
		{
			OldSize = InterpolatingPoints.Num();
			RefineCurvePoints_COPY(RichCurve, TimeThreshold, ValueThreshold, InterpolatingPoints);
		}
		while (OldSize != InterpolatingPoints.Num());
	}

	virtual bool Evaluate(double InTime, double& OutValue) const override { return true; }

private:
	FRichCurve RichCurve;
};

FRichCurveEditorModelNamed_COPY::FRichCurveEditorModelNamed_COPY(const FSmartName& InName, ERawCurveTrackTypes InType, int32 InCurveIndex, UAnimModToolConfig* animModToolConfig)
	: Name(InName)
	, CurveIndex(InCurveIndex)
	, Type(InType)
	, CurveId(FAnimationCurveIdentifier(Name, Type))
	, bCurveRemoved(false)
	, WeakOwner(animModToolConfig)
	, ClampInputRange(TRange<double>(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max()))
	, TimeManipulationFloatCurve(&animModToolConfig->TimeManipulationFloatCurve)
{
	if (Type == ERawCurveTrackTypes::RCT_Transform)
	{
		UAnimationCurveIdentifierExtensions::GetTransformChildCurveIdentifier(CurveId, (ETransformCurveChannel)(CurveIndex / 3), (EVectorCurveChannel)(CurveIndex % 3));
	}
}

const void* FRichCurveEditorModelNamed_COPY::GetCurve() const
{
	if (IsValid())
	{
		return &GetReadOnlyRichCurve();
	}
	return nullptr;
}

void FRichCurveEditorModelNamed_COPY::Modify()
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			Owner->SetFlags(RF_Transactional);
			Owner->Modify();
		}
	}
}

void FRichCurveEditorModelNamed_COPY::AddKeys(TArrayView<const FKeyPosition> InKeyPositions, TArrayView<const FKeyAttributes> InKeyAttributes, TArrayView<TOptional<FKeyHandle>>* OutKeyHandles)
{
	check(InKeyPositions.Num() == InKeyAttributes.Num() && (!OutKeyHandles || OutKeyHandles->Num() == InKeyPositions.Num()));

	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			if (InKeyPositions.Num() > 0)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText2);

				Owner->Modify();

				TArray<FKeyHandle> NewKeyHandles;
				NewKeyHandles.SetNumUninitialized(InKeyPositions.Num());

				FRichCurve& RichCurve = GetRichCurve();

				for (int32 Index = 0; Index < InKeyPositions.Num(); ++Index)
				{
					FKeyPosition   Position = InKeyPositions[Index];
					FKeyAttributes Attributes = InKeyAttributes[Index];

					const TRange<double> InputRange = ClampInputRange.Get();
					FKeyHandle     NewHandle = RichCurve.UpdateOrAddKey(FMath::Clamp(Position.InputValue, InputRange.GetLowerBoundValue(), InputRange.GetUpperBoundValue()), Position.OutputValue >= 0 ? Position.OutputValue : 0);
					if (NewHandle != FKeyHandle::Invalid())
					{
						NewKeyHandles[Index] = NewHandle;
						if (OutKeyHandles)
						{
							(*OutKeyHandles)[Index] = NewHandle;
						}
					}
				}

				// We reuse SetKeyAttributes here as there is complex logic determining which parts of the attributes are valid to pass on.
				// For now we need to duplicate the new key handle array due to API mismatch. This will auto-calculate tangents if required.
				SetKeyAttributes(NewKeyHandles, InKeyAttributes);

				CurveModifiedDelegate.Broadcast();
			}
		}
	}
}

bool FRichCurveEditorModelNamed_COPY::Evaluate(double Time, double& OutValue) const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			OutValue = GetReadOnlyRichCurve().Eval(Time);
			return true;
		}
	}

	return false;
}

void FRichCurveEditorModelNamed_COPY::RemoveKeys(TArrayView<const FKeyHandle> InKeys)
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			if (InKeys.Num() > 0)
			{
				FScopedTransaction scopedTransaction(scopedTransactionText2);

				Owner->Modify();
				FRichCurve& RichCurve = GetRichCurve();
				for (FKeyHandle Handle : InKeys)
				{
					RichCurve.DeleteKey(Handle);
				}

				CurveModifiedDelegate.Broadcast();
			}
		}
	}
}

void FRichCurveEditorModelNamed_COPY::DrawCurve(const FCurveEditor& CurveEditor, const FCurveEditorScreenSpace& ScreenSpace, TArray<TTuple<double, double>>& InOutPoints) const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			const double StartTimeSeconds = ScreenSpace.GetInputMin();
			const double EndTimeSeconds = ScreenSpace.GetInputMax();
			const double TimeThreshold = FMath::Max(0.0001, 1.0 / ScreenSpace.PixelsPerInput());
			const double ValueThreshold = FMath::Max(0.0001, 1.0 / ScreenSpace.PixelsPerOutput());

			const FRichCurve& RichCurve = GetReadOnlyRichCurve();

			InOutPoints.Add(MakeTuple(StartTimeSeconds, double(RichCurve.Eval(StartTimeSeconds))));

			for (const FRichCurveKey& Key : RichCurve.GetConstRefOfKeys())
			{
				if (Key.Time > StartTimeSeconds && Key.Time < EndTimeSeconds)
				{
					InOutPoints.Add(MakeTuple(double(Key.Time), double(Key.Value)));
				}
			}

			InOutPoints.Add(MakeTuple(EndTimeSeconds, double(RichCurve.Eval(EndTimeSeconds))));

			int32 OldSize = InOutPoints.Num();
			do
			{
				OldSize = InOutPoints.Num();
				RefineCurvePoints_COPY(RichCurve, TimeThreshold, ValueThreshold, InOutPoints);
			}
			while (OldSize != InOutPoints.Num());
		}
	}
}

void FRichCurveEditorModelNamed_COPY::GetKeys(const FCurveEditor& CurveEditor, double MinTime, double MaxTime, double MinValue, double MaxValue, TArray<FKeyHandle>& OutKeyHandles) const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			const FRichCurve& RichCurve = GetReadOnlyRichCurve();
			for (auto It = RichCurve.GetKeyHandleIterator(); It; ++It)
			{
				if (RichCurve.IsKeyHandleValid(*It))
				{
					const FRichCurveKey& Key = RichCurve.GetKeyRef(*It);
					if (Key.Time >= MinTime && Key.Time <= MaxTime && Key.Value >= MinValue && Key.Value <= MaxValue)
					{
						OutKeyHandles.Add(*It);
					}
				}
			}
		}
	}
}

void FRichCurveEditorModelNamed_COPY::GetKeyDrawInfo(ECurvePointType PointType, const FKeyHandle InKeyHandle, FKeyDrawInfo& OutDrawInfo) const
{
	if (PointType == ECurvePointType::ArriveTangent || PointType == ECurvePointType::LeaveTangent)
	{
		OutDrawInfo.Brush = FAppStyle::GetBrush("GenericCurveEditor.TangentHandle");
		OutDrawInfo.ScreenSize = FVector2D(9, 9);
	}
	else
	{
		// All keys are the same size by default
		OutDrawInfo.ScreenSize = FVector2D(11, 11);

		ERichCurveInterpMode KeyType = (IsValid() && GetReadOnlyRichCurve().IsKeyHandleValid(InKeyHandle)) ? GetReadOnlyRichCurve().GetKeyRef(InKeyHandle).InterpMode.GetValue() : RCIM_None;
		switch (KeyType)
		{
		case ERichCurveInterpMode::RCIM_Constant:
			OutDrawInfo.Brush = FAppStyle::GetBrush("GenericCurveEditor.ConstantKey");
			break;
		case ERichCurveInterpMode::RCIM_Linear:
			OutDrawInfo.Brush = FAppStyle::GetBrush("GenericCurveEditor.LinearKey");
			break;
		case ERichCurveInterpMode::RCIM_Cubic:
			OutDrawInfo.Brush = FAppStyle::GetBrush("GenericCurveEditor.CubicKey");
			break;
		default:
			OutDrawInfo.Brush = FAppStyle::GetBrush("GenericCurveEditor.Key");
			break;
		}
	}
}

void FRichCurveEditorModelNamed_COPY::GetKeys(double MinTime, double MaxTime, double MinValue, double MaxValue, TArray<FKeyHandle>& OutKeyHandles) const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			const FRichCurve& RichCurve = GetReadOnlyRichCurve();
			for (auto It = RichCurve.GetKeyHandleIterator(); It; ++It)
			{
				if (RichCurve.IsKeyHandleValid(*It))
				{
					const FRichCurveKey& Key = RichCurve.GetKeyRef(*It);
					if (Key.Time >= MinTime && Key.Time <= MaxTime && Key.Value >= MinValue && Key.Value <= MaxValue)
					{
						OutKeyHandles.Add(*It);
					}
				}
			}
		}
	}
}

void FRichCurveEditorModelNamed_COPY::GetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyPosition> OutKeyPositions) const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			const FRichCurve& RichCurve = GetReadOnlyRichCurve();
			for (int32 Index = 0; Index < InKeys.Num(); ++Index)
			{
				if (RichCurve.IsKeyHandleValid(InKeys[Index]))
				{
					const FRichCurveKey& Key = RichCurve.GetKeyRef(InKeys[Index]);

					OutKeyPositions[Index].InputValue = Key.Time;
					OutKeyPositions[Index].OutputValue = Key.Value;
				}
			}
		}
	}
}

void FRichCurveEditorModelNamed_COPY::SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType)
{
	const bool bInteractiveChange = ChangeType == EPropertyChangeType::Interactive;

	if (!IsReadOnly())
	{
		if (UObject* Owner = WeakOwner.Get())
		{
			if (IsValid())
			{
				if (InKeys.Num())
				{
					FScopedTransaction scopedTransaction(scopedTransactionText2);

					Owner->Modify();

					FRichCurve& RichCurve = GetRichCurve();
					for (int32 Index = 0; Index < InKeys.Num(); ++Index)
					{
						FKeyHandle Handle = InKeys[Index];
						if (RichCurve.IsKeyHandleValid(Handle))
						{
							// Set key time last so we don't have to worry about the key handle changing
							RichCurve.GetKey(Handle).Value = InKeyPositions[Index].OutputValue >= 0 ? InKeyPositions[Index].OutputValue : 0;
							const TRange<double> InputRange = ClampInputRange.Get();
							RichCurve.SetKeyTime(Handle, FMath::Clamp(InKeyPositions[Index].InputValue, InputRange.GetLowerBoundValue(), InputRange.GetUpperBoundValue()));
						}
					}
					RichCurve.AutoSetTangents();
					FPropertyChangedEvent PropertyChangeStruct(nullptr, ChangeType);
					Owner->PostEditChangeProperty(PropertyChangeStruct);

					if (ChangeType == EPropertyChangeType::ValueSet)
					{
						CurveModifiedDelegate.Broadcast();
					}
				}
			}
		}
	}
}

void FRichCurveEditorModelNamed_COPY::GetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyAttributes> OutAttributes) const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			const FRichCurve& RichCurve = GetReadOnlyRichCurve();
			const TArray<FRichCurveKey>& AllKeys = RichCurve.GetConstRefOfKeys();
			if (AllKeys.Num() == 0)
			{
				return;
			}

			const FRichCurveKey* FirstKey = &AllKeys[0];
			const FRichCurveKey* LastKey = &AllKeys.Last();

			for (int32 Index = 0; Index < InKeys.Num(); ++Index)
			{
				if (RichCurve.IsKeyHandleValid(InKeys[Index]))
				{
					const FRichCurveKey& ThisKey = RichCurve.GetKeyRef(InKeys[Index]);
					FKeyAttributes& Attributes = OutAttributes[Index];

					Attributes.SetInterpMode(ThisKey.InterpMode);

					if (ThisKey.InterpMode != RCIM_Constant && ThisKey.InterpMode != RCIM_Linear)
					{
						Attributes.SetTangentMode(ThisKey.TangentMode);
						if (&ThisKey != FirstKey)
						{
							Attributes.SetArriveTangent(ThisKey.ArriveTangent);
						}

						if (&ThisKey != LastKey)
						{
							Attributes.SetLeaveTangent(ThisKey.LeaveTangent);
						}
						if (ThisKey.InterpMode == RCIM_Cubic)
						{
							Attributes.SetTangentWeightMode(ThisKey.TangentWeightMode);
							if (ThisKey.TangentWeightMode != RCTWM_WeightedNone)
							{
								Attributes.SetArriveTangentWeight(ThisKey.ArriveTangentWeight);
								Attributes.SetLeaveTangentWeight(ThisKey.LeaveTangentWeight);
							}
						}
					}
				}
			}
		}
	}
}

void FRichCurveEditorModelNamed_COPY::SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType)
{
	const bool bInteractiveChange = ChangeType == EPropertyChangeType::Interactive;

	if (!IsReadOnly())
	{
		if (UObject* Owner = WeakOwner.Get())
		{
			if (IsValid())
			{
				FRichCurve& RichCurve = GetRichCurve();
				const TArray<FRichCurveKey>& AllKeys = RichCurve.GetConstRefOfKeys();
				if (AllKeys.Num() == 0)
				{
					return;
				}

				Owner->Modify();

				const FRichCurveKey* FirstKey = &AllKeys[0];
				const FRichCurveKey* LastKey = &AllKeys.Last();

				bool bAutoSetTangents = false;

				for (int32 Index = 0; Index < InKeys.Num(); ++Index)
				{
					FKeyHandle KeyHandle = InKeys[Index];
					if (RichCurve.IsKeyHandleValid(KeyHandle))
					{
						FRichCurveKey* ThisKey = &RichCurve.GetKey(KeyHandle);
						const FKeyAttributes& Attributes = InAttributes[Index];

						if (Attributes.HasInterpMode()) { ThisKey->InterpMode = Attributes.GetInterpMode();  bAutoSetTangents = true; }
						if (Attributes.HasTangentMode())
						{
							ThisKey->TangentMode = Attributes.GetTangentMode();
							if (ThisKey->TangentMode == RCTM_Auto)
							{
								ThisKey->TangentWeightMode = RCTWM_WeightedNone;
							}
							bAutoSetTangents = true;
						}
						if (Attributes.HasTangentWeightMode())
						{
							if (ThisKey->TangentWeightMode == RCTWM_WeightedNone) //set tangent weights to default use
							{
								const float OneThird = 1.0f / 3.0f;

								//calculate a tangent weight based upon tangent and time difference
								//calculate arrive tangent weight
								if (ThisKey != FirstKey)
								{
									const float X = ThisKey->Time - RichCurve.GetKey(RichCurve.GetPreviousKey(KeyHandle)).Time;
									const float Y = ThisKey->ArriveTangent * X;
									ThisKey->ArriveTangentWeight = FMath::Sqrt(X * X + Y * Y) * OneThird;
								}
								//calculate leave weight
								if (ThisKey != LastKey)
								{
									const float X = RichCurve.GetKey(RichCurve.GetNextKey(KeyHandle)).Time - ThisKey->Time;
									const float Y = ThisKey->LeaveTangent * X;
									ThisKey->LeaveTangentWeight = FMath::Sqrt(X * X + Y * Y) * OneThird;
								}
							}
							ThisKey->TangentWeightMode = Attributes.GetTangentWeightMode();

							if (ThisKey->TangentWeightMode != RCTWM_WeightedNone)
							{
								if (ThisKey->TangentMode != RCTM_User && ThisKey->TangentMode != RCTM_Break)
								{
									ThisKey->TangentMode = RCTM_User;
								}
							}
						}

						if (Attributes.HasArriveTangent())
						{
							if (ThisKey->TangentMode == RCTM_Auto)
							{
								ThisKey->TangentMode = RCTM_User;
								ThisKey->TangentWeightMode = RCTWM_WeightedNone;
							}

							ThisKey->ArriveTangent = Attributes.GetArriveTangent();
							if (ThisKey->InterpMode == RCIM_Cubic && ThisKey->TangentMode != RCTM_Break)
							{
								ThisKey->LeaveTangent = ThisKey->ArriveTangent;
							}
						}

						if (Attributes.HasLeaveTangent())
						{
							if (ThisKey->TangentMode == RCTM_Auto)
							{
								ThisKey->TangentMode = RCTM_User;
								ThisKey->TangentWeightMode = RCTWM_WeightedNone;
							}

							ThisKey->LeaveTangent = Attributes.GetLeaveTangent();
							if (ThisKey->InterpMode == RCIM_Cubic && ThisKey->TangentMode != RCTM_Break)
							{
								ThisKey->ArriveTangent = ThisKey->LeaveTangent;
							}
						}

						if (Attributes.HasArriveTangentWeight())
						{
							if (ThisKey->TangentMode == RCTM_Auto)
							{
								ThisKey->TangentMode = RCTM_User;
								ThisKey->TangentWeightMode = RCTWM_WeightedNone;
							}

							ThisKey->ArriveTangentWeight = Attributes.GetArriveTangentWeight();
							if (ThisKey->InterpMode == RCIM_Cubic && ThisKey->TangentMode != RCTM_Break)
							{
								ThisKey->LeaveTangentWeight = ThisKey->ArriveTangentWeight;
							}
						}

						if (Attributes.HasLeaveTangentWeight())
						{

							if (ThisKey->TangentMode == RCTM_Auto)
							{
								ThisKey->TangentMode = RCTM_User;
								ThisKey->TangentWeightMode = RCTWM_WeightedNone;
							}

							ThisKey->LeaveTangentWeight = Attributes.GetLeaveTangentWeight();
							if (ThisKey->InterpMode == RCIM_Cubic && ThisKey->TangentMode != RCTM_Break)
							{
								ThisKey->ArriveTangentWeight = ThisKey->LeaveTangentWeight;
							}
						}
					}
				}

				if (bAutoSetTangents)
				{
					RichCurve.AutoSetTangents();
				}

				FPropertyChangedEvent PropertyChangeStruct(nullptr, ChangeType);
				Owner->PostEditChangeProperty(PropertyChangeStruct);
				CurveModifiedDelegate.Broadcast();
			}
		}
	}
}

void FRichCurveEditorModelNamed_COPY::GetCurveAttributes(FCurveAttributes& OutCurveAttributes) const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			const FRichCurve& RichCurve = GetReadOnlyRichCurve();
			OutCurveAttributes.SetPreExtrapolation(RichCurve.PreInfinityExtrap);
			OutCurveAttributes.SetPostExtrapolation(RichCurve.PostInfinityExtrap);
		}
	}
}

void FRichCurveEditorModelNamed_COPY::SetCurveAttributes(const FCurveAttributes& InCurveAttributes)
{
	Modify();
}

void FRichCurveEditorModelNamed_COPY::CreateKeyProxies(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<UObject*> OutObjects)
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			for (int32 Index = 0; Index < InKeyHandles.Num(); ++Index)
			{
				URichCurveKeyProxy_COPY* NewProxy = NewObject<URichCurveKeyProxy_COPY>(GetTransientPackage(), NAME_None);

				NewProxy->Initialize(InKeyHandles[Index], this, WeakOwner);
				OutObjects[Index] = NewProxy;
			}
		}
	}
}

TUniquePtr<IBufferedCurveModel> FRichCurveEditorModelNamed_COPY::CreateBufferedCurveCopy() const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			const FRichCurve& RichCurve = GetReadOnlyRichCurve();

			TArray<FKeyHandle> TargetKeyHandles;
			for (auto It = RichCurve.GetKeyHandleIterator(); It; ++It)
			{
				if (RichCurve.IsKeyHandleValid(*It))
				{
					TargetKeyHandles.Add(*It);
				}
			}

			TArray<FKeyPosition> KeyPositions;
			KeyPositions.SetNumUninitialized(TargetKeyHandles.Num());
			TArray<FKeyAttributes> KeyAttributes;
			KeyAttributes.SetNumUninitialized(TargetKeyHandles.Num());
			GetKeyPositions(TargetKeyHandles, KeyPositions);
			GetKeyAttributes(TargetKeyHandles, KeyAttributes);

			double ValueMin = 0.f, ValueMax = 1.f;
			GetValueRange(ValueMin, ValueMax);

			return MakeUnique<FRichBufferedCurveModel_COPY>(RichCurve, MoveTemp(KeyPositions), MoveTemp(KeyAttributes), GetLongDisplayName().ToString(), ValueMin, ValueMax);
		}
	}

	return nullptr;
}

bool FRichCurveEditorModelNamed_COPY::IsValid() const
{
	return true;
}

FRichCurve& FRichCurveEditorModelNamed_COPY::GetRichCurve()
{
	return TimeManipulationFloatCurve->FloatCurve;
}

const FRichCurve& FRichCurveEditorModelNamed_COPY::GetReadOnlyRichCurve() const
{
	return TimeManipulationFloatCurve->FloatCurve;
}

void FRichCurveEditorModelNamed_COPY::GetTimeRange(double& MinTime, double& MaxTime) const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			float MinTimeFloat = 0.f, MaxTimeFloat = 0.f;
			const FRichCurve& RichCurve = GetReadOnlyRichCurve();
			RichCurve.GetTimeRange(MinTimeFloat, MaxTimeFloat);

			MinTime = MinTimeFloat;
			MaxTime = MaxTimeFloat;
		}
	}
}

void FRichCurveEditorModelNamed_COPY::GetValueRange(double& MinValue, double& MaxValue) const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			float MinValueFloat = 0.f, MaxValueFloat = 0.f;
			const FRichCurve& RichCurve = GetReadOnlyRichCurve();
			RichCurve.GetValueRange(MinValueFloat, MaxValueFloat);

			MinValue = MinValueFloat;
			MaxValue = MaxValueFloat;
		}
	}
}

int32 FRichCurveEditorModelNamed_COPY::GetNumKeys() const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			return GetReadOnlyRichCurve().GetNumKeys();
		}
	}
	return 0;
}

void FRichCurveEditorModelNamed_COPY::GetNeighboringKeys(const FKeyHandle InKeyHandle, TOptional<FKeyHandle>& OutPreviousKeyHandle, TOptional<FKeyHandle>& OutNextKeyHandle) const
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (IsValid())
		{
			const FRichCurve& RichCurve = GetReadOnlyRichCurve();
			if (RichCurve.IsKeyHandleValid(InKeyHandle))
			{
				FKeyHandle NextKeyHandle = RichCurve.GetNextKey(InKeyHandle);

				if (RichCurve.IsKeyHandleValid(NextKeyHandle))
				{
					OutNextKeyHandle = NextKeyHandle;
				}

				FKeyHandle PreviousKeyHandle = RichCurve.GetPreviousKey(InKeyHandle);

				if (RichCurve.IsKeyHandleValid(PreviousKeyHandle))
				{
					OutPreviousKeyHandle = PreviousKeyHandle;
				}
			}
		}
	}
}

class FAnimSequenceCurveEditorBounds : public ICurveEditorBounds
{
public:
	FAnimSequenceCurveEditorBounds(TSharedPtr<ITimeSliderController> InExternalTimeSliderController)
		: ExternalTimeSliderController(InExternalTimeSliderController)
	{}

	virtual void GetInputBounds(double& OutMin, double& OutMax) const override
	{
		FAnimatedRange ViewRange = ExternalTimeSliderController.Pin()->GetViewRange();
		OutMin = ViewRange.GetLowerBoundValue();
		OutMax = ViewRange.GetUpperBoundValue();
	}

	virtual void SetInputBounds(double InMin, double InMax) override
	{
		ExternalTimeSliderController.Pin()->SetViewRange(InMin, InMax, EViewRangeInterpolation::Immediate);
	}

	TWeakPtr<ITimeSliderController> ExternalTimeSliderController;
};

SAnimSequenceCurveEditor_COPY::~SAnimSequenceCurveEditor_COPY()
{
	if(AnimSequence)
	{
		AnimSequence->GetDataModel()->GetModifiedEvent().RemoveAll(this);
	}
}

void SAnimSequenceCurveEditor_COPY::Construct(const FArguments& InArgs, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, UAnimSequenceBase* InAnimSequence)
{
	CurveEditor = MakeShared<FCurveEditor>();
	CurveEditor->GridLineLabelFormatXAttribute = LOCTEXT("GridXLabelFormat", "{0}s");
	CurveEditor->SetBounds(MakeUnique<FAnimSequenceCurveEditorBounds>(InArgs._ExternalTimeSliderController));

	FCurveEditorInitParams CurveEditorInitParams;
	CurveEditor->InitCurveEditor(CurveEditorInitParams);
	CurveEditor->InputSnapRateAttribute = InAnimSequence->GetSamplingFrameRate();

	AnimSequence = InAnimSequence;

	AnimSequence->GetDataModel()->GetModifiedEvent().AddRaw(this, &SAnimSequenceCurveEditor_COPY::OnModelHasChanged);

	TSharedRef<SCurveEditorPanel> CurveEditorPanel = SNew(SCurveEditorPanel, CurveEditor.ToSharedRef())
		.GridLineTint(FLinearColor(0.f, 0.f, 0.f, 0.3f))
		.ExternalTimeSliderController(InArgs._ExternalTimeSliderController)
		.TabManager(InArgs._TabManager);

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 3.0f)
		[
			MakeToolbar(CurveEditorPanel)
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			CurveEditorPanel
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SAnimTimelineTransportControls_COPY, InPreviewScene, InAnimSequence)
		]
	];
}

void SAnimSequenceCurveEditor_COPY::OnModelHasChanged(const EAnimDataModelNotifyType& NotifyType, IAnimationDataModel* Model, const FAnimDataModelNotifPayload& Payload)
{
	auto StopEditingCurve = [this, NotifyType, &Payload, Model]()
	{
		const FCurvePayload& TypedPayload = Payload.GetPayload<FCurvePayload>();
		const FAnimationCurveIdentifier& CurveId = TypedPayload.Identifier;
		const int32 ChannelIndices = CurveId.CurveType == ERawCurveTrackTypes::RCT_Transform ? 9 : 1;
		for (int32 ChannelIndex = 0; ChannelIndex < ChannelIndices; ++ChannelIndex)
		{
			RemoveCurve(CurveId.InternalName_DEPRECATED, CurveId.CurveType, ChannelIndex);
		}
	};
	
	switch(NotifyType)
	{
	case EAnimDataModelNotifyType::CurveRemoved:
	case EAnimDataModelNotifyType::CurveRenamed:
		{
			StopEditingCurve();
			break;
		}			
	case EAnimDataModelNotifyType::CurveFlagsChanged:
		{
			const FCurveFlagsChangedPayload& TypedPayload = Payload.GetPayload<FCurveFlagsChangedPayload>();
			////// TODO
			//////if(Model->FindCurve(TypedPayload.Identifier)->GetCurveTypeFlag(AACF_Metadata))
			//////{
			//////	StopEditingCurve();
			//////}
			break;
		}
	}
	
}

TSharedRef<SWidget> SAnimSequenceCurveEditor_COPY::MakeToolbar(TSharedRef<SCurveEditorPanel> InEditorPanel)
{
	FToolBarBuilder ToolBarBuilder(InEditorPanel->GetCommands(), FMultiBoxCustomization::None, InEditorPanel->GetToolbarExtender(), true);
	ToolBarBuilder.BeginSection("Asset");
	ToolBarBuilder.EndSection();
	// We just use all of the extenders as our toolbar, we don't have a need to create a separate toolbar.
	return ToolBarBuilder.MakeWidget();
}

void SAnimSequenceCurveEditor_COPY::ZoomToFit()
{
	CurveEditor->ZoomToFit(EAxisList::Y);
}

#undef LOCTEXT_NAMESPACE