// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "AnimModel_COPY.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "Preferences/PersonaOptions.h"
#include "Animation/EditorAnimBaseObj.h"
#include "AnimTimelineTrack_COPY.h"
#include "Animation/AnimSequence.h"

#define LOCTEXT_NAMESPACE "FAnimModel_COPY"

const FAnimModel_COPY::FSnapType FAnimModel_COPY::FSnapType::Frames("Frames", LOCTEXT("FramesSnapName", "Frames"), [](const FAnimModel_COPY& InModel, double InTime)
{
	// Round to nearest frame
	double FrameRate = InModel.GetFrameRate();
	if(FrameRate > 0)
	{
		return FMath::RoundToDouble(InTime * FrameRate) / FrameRate;
	}
	
	return InTime;
});

const FAnimModel_COPY::FSnapType FAnimModel_COPY::FSnapType::Notifies("Notifies", LOCTEXT("NotifiesSnapName", "Notifies"));

const FAnimModel_COPY::FSnapType FAnimModel_COPY::FSnapType::CompositeSegment("CompositeSegment", LOCTEXT("CompositeSegmentSnapName", "Composite Segments"));

const FAnimModel_COPY::FSnapType FAnimModel_COPY::FSnapType::MontageSection("MontageSection", LOCTEXT("MontageSectionSnapName", "Montage Sections"));

FAnimModel_COPY::FAnimModel_COPY(const TSharedRef<IPersonaPreviewScene>& InPreviewScene, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, const TSharedRef<FUICommandList>& InCommandList)
	: WeakPreviewScene(InPreviewScene)
	, WeakEditableSkeleton(InEditableSkeleton)
	, WeakCommandList(InCommandList)
	, bIsSelecting(false)
{
}

void FAnimModel_COPY::Initialize()
{

}

FAnimatedRange FAnimModel_COPY::GetViewRange() const
{
	return ViewRange;
}

FAnimatedRange FAnimModel_COPY::GetWorkingRange() const
{
	return WorkingRange;
}

double FAnimModel_COPY::GetFrameRate() const
{
	if(UAnimSequence* AnimSequence = Cast<UAnimSequence>(GetAnimSequenceBase()))
	{
		return AnimSequence->GetSamplingFrameRate().AsDecimal();
	}
	else
	{
		return 30.0;
	}
}

int32 FAnimModel_COPY::GetTickResolution() const
{
	return FMath::RoundToInt32((double)GetDefault<UPersonaOptions>()->TimelineScrubSnapValue * GetFrameRate());
}

TRange<FFrameNumber> FAnimModel_COPY::GetPlaybackRange() const
{
	const int32 Resolution = GetTickResolution();
	return TRange<FFrameNumber>(FFrameNumber(FMath::RoundToInt32(PlaybackRange.GetLowerBoundValue() * (double)Resolution)), FFrameNumber(FMath::RoundToInt32(PlaybackRange.GetUpperBoundValue() * (double)Resolution)));
}

FFrameNumber FAnimModel_COPY::GetScrubPosition() const
{
	if(WeakPreviewScene.IsValid())
	{
		UDebugSkelMeshComponent* PreviewMeshComponent = WeakPreviewScene.Pin()->GetPreviewMeshComponent();
		if(PreviewMeshComponent && PreviewMeshComponent->IsPreviewOn())
		{
			return FFrameNumber(FMath::RoundToInt32(PreviewMeshComponent->PreviewInstance->GetCurrentTime() * (double)GetTickResolution()));
		}
	}

	return FFrameNumber(0);
}

float FAnimModel_COPY::GetScrubTime() const
{
	if(WeakPreviewScene.IsValid())
	{
		UDebugSkelMeshComponent* PreviewMeshComponent = WeakPreviewScene.Pin()->GetPreviewMeshComponent();
		if(PreviewMeshComponent && PreviewMeshComponent->IsPreviewOn())
		{
			return PreviewMeshComponent->PreviewInstance->GetCurrentTime();
		}
	}

	return 0.0f;
}

void FAnimModel_COPY::SetScrubPosition(FFrameTime NewScrubPostion) const
{
	if(WeakPreviewScene.IsValid())
	{
		UDebugSkelMeshComponent* PreviewMeshComponent = WeakPreviewScene.Pin()->GetPreviewMeshComponent();
		if(PreviewMeshComponent && PreviewMeshComponent->IsPreviewOn())
		{
			if(PreviewMeshComponent->PreviewInstance->IsPlaying())
			{
				PreviewMeshComponent->PreviewInstance->SetPlaying(false);
			}
			
			PreviewMeshComponent->PreviewInstance->SetPosition(static_cast<float>(NewScrubPostion.AsDecimal() / static_cast<double>(GetTickResolution())));
		}
	}
}

void FAnimModel_COPY::HandleViewRangeChanged(TRange<double> InRange, EViewRangeInterpolation InInterpolation)
{
	SetViewRange(InRange);
}

void FAnimModel_COPY::SetViewRange(TRange<double> InRange)
{
	ViewRange = InRange;

	if(WorkingRange.HasLowerBound() && WorkingRange.HasUpperBound())
	{
		WorkingRange = TRange<double>::Hull(WorkingRange, ViewRange);
	}
	else
	{
		WorkingRange = ViewRange;
	}
}

void FAnimModel_COPY::HandleWorkingRangeChanged(TRange<double> InRange)
{
	WorkingRange = InRange;
}

bool FAnimModel_COPY::IsTrackSelected(const TSharedRef<FAnimTimelineTrack_COPY>& InTrack) const
{ 
	return SelectedTracks.Find(InTrack) != nullptr;
}

void FAnimModel_COPY::ClearTrackSelection()
{
	SelectedTracks.Empty();
}

void FAnimModel_COPY::SetTrackSelected(const TSharedRef<FAnimTimelineTrack_COPY>& InTrack, bool bIsSelected)
{
	if(bIsSelected)
	{
		SelectedTracks.Add(InTrack);
	}
	else
	{
		SelectedTracks.Remove(InTrack);
	}
}

void FAnimModel_COPY::AddReferencedObjects(FReferenceCollector& Collector)
{
	EditorObjectTracker.AddReferencedObjects(Collector);
}

void FAnimModel_COPY::SelectObjects(const TArray<UObject*>& Objects)
{
	if(!bIsSelecting)
	{
		TGuardValue<bool> GuardValue(bIsSelecting, true);
		OnSelectObjects.ExecuteIfBound(Objects);

		OnHandleObjectsSelectedDelegate.Broadcast(Objects);
	}
}

UObject* FAnimModel_COPY::ShowInDetailsView(UClass* EdClass)
{
	UObject* Obj = EditorObjectTracker.GetEditorObjectForClass(EdClass);
	if(Obj != nullptr)
	{
		if(Obj->IsA(UEditorAnimBaseObj::StaticClass()))
		{
			if(!bIsSelecting)
			{
				TGuardValue<bool> GuardValue(bIsSelecting, true);

				ClearTrackSelection();

				UEditorAnimBaseObj *EdObj = Cast<UEditorAnimBaseObj>(Obj);
				InitDetailsViewEditorObject(EdObj);

				TArray<UObject*> Objects;
				Objects.Add(EdObj);
				OnSelectObjects.ExecuteIfBound(Objects);

				OnHandleObjectsSelectedDelegate.Broadcast(Objects);
			}
		}
	}
	return Obj;
}

void FAnimModel_COPY::ClearDetailsView()
{
	if(!bIsSelecting)
	{
		TGuardValue<bool> GuardValue(bIsSelecting, true);

		TArray<UObject*> Objects;
		OnSelectObjects.ExecuteIfBound(Objects);
		OnHandleObjectsSelectedDelegate.Broadcast(Objects);
	}
}

float FAnimModel_COPY::CalculateSequenceLengthOfEditorObject() const
{
	if(UAnimSequenceBase* AnimSequenceBase = GetAnimSequenceBase())
	{
		return AnimSequenceBase->GetPlayLength();
	}

	return 0.0f;
}

void FAnimModel_COPY::RecalculateSequenceLength()
{
	if(UAnimSequenceBase* AnimSequenceBase = GetAnimSequenceBase())
	{
		AnimSequenceBase->ClampNotifiesAtEndOfSequence();
	}
}

void FAnimModel_COPY::SetEditableTime(int32 TimeIndex, double Time, bool bIsDragging)
{
	EditableTimes[TimeIndex] = FMath::Clamp(Time, 0.0, (double)CalculateSequenceLengthOfEditorObject());

	OnSetEditableTime(TimeIndex, EditableTimes[TimeIndex], bIsDragging);
}

bool FAnimModel_COPY::Snap(float& InOutTime, float InSnapMargin, TArrayView<const FName> InSkippedSnapTypes) const
{
	double DoubleTime = InOutTime;
	bool bResult = Snap(DoubleTime, (double)InSnapMargin, InSkippedSnapTypes);
	InOutTime = static_cast<float>(DoubleTime);
	return bResult;
}

bool FAnimModel_COPY::Snap(double& InOutTime, double InSnapMargin, TArrayView<const FName> InSkippedSnapTypes) const
{
	InSnapMargin = FMath::Max(InSnapMargin, (double)KINDA_SMALL_NUMBER);

	double ClosestDelta = DBL_MAX;
	double ClosestSnapTime = DBL_MAX;

	// Check for enabled snap functions first
	for(const TPair<FName, FSnapType>& SnapTypePair : SnapTypes)
	{
		if(SnapTypePair.Value.SnapFunction != nullptr)
		{
			if(IsSnapChecked(SnapTypePair.Value.Type))
			{
				if(!InSkippedSnapTypes.Contains(SnapTypePair.Value.Type))
				{
					double SnappedTime = SnapTypePair.Value.SnapFunction(*this, InOutTime);
					if(SnappedTime != InOutTime)
					{
						double Delta = FMath::Abs(SnappedTime - InOutTime);
						if(Delta < InSnapMargin && Delta < ClosestDelta)
						{
							ClosestDelta = Delta;
							ClosestSnapTime = SnappedTime;
						}
					}
				}
			}
		}
	}

	// Find the closest in-range enabled snap time
	for(const FSnapTime& SnapTime : SnapTimes)
	{
		double Delta = FMath::Abs(SnapTime.Time - InOutTime);
		if(Delta < InSnapMargin && Delta < ClosestDelta)
		{
			if(!InSkippedSnapTypes.Contains(SnapTime.Type))
			{
				if(const FSnapType* SnapType = SnapTypes.Find(SnapTime.Type))
				{
					if(IsSnapChecked(SnapTime.Type))
					{
						ClosestDelta = Delta;
						ClosestSnapTime = SnapTime.Time;
					}
				}
			}
		}
	}

	if(ClosestDelta != DBL_MAX)
	{
		InOutTime = ClosestSnapTime;
		return true;
	}

	return false;
}

void FAnimModel_COPY::ToggleSnap(FName InSnapName)
{
	if(IsSnapChecked(InSnapName))
	{
		GetMutableDefault<UPersonaOptions>()->TimelineEnabledSnaps.Remove(InSnapName);
	}
	else
	{
		GetMutableDefault<UPersonaOptions>()->TimelineEnabledSnaps.AddUnique(InSnapName);
	}
}

bool FAnimModel_COPY::IsSnapChecked(FName InSnapName) const
{
	return GetDefault<UPersonaOptions>()->TimelineEnabledSnaps.Contains(InSnapName);
}

bool FAnimModel_COPY::IsSnapAvailable(FName InSnapName) const
{
	return SnapTypes.Find(InSnapName) != nullptr;
}

void FAnimModel_COPY::BuildContextMenu(FMenuBuilder& InMenuBuilder)
{
	// Let each selected item contribute to the context menu
	TSet<FName> ExistingMenuTypes;
	for(const TSharedRef<FAnimTimelineTrack_COPY>& SelectedItem : SelectedTracks)
	{
		SelectedItem->AddToContextMenu(InMenuBuilder, ExistingMenuTypes);
	}
}

void FAnimModel_COPY::AddRootTrack(TSharedRef<FAnimTimelineTrack_COPY> InTrack)
{
	if (GetMutableDefault<UPersonaOptions>()->GetAllowedAnimationEditorTracks().PassesFilter(InTrack->GetTypeName()))
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		RootTracks.Add(InTrack);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

void FAnimModel_COPY::ClearRootTracks()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RootTracks.Empty();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FAnimModel_COPY::ForEachRootTrack(TFunctionRef<void(FAnimTimelineTrack_COPY&)> InFunction)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	for (TSharedRef<FAnimTimelineTrack_COPY>& Track : RootTracks)
	{
		InFunction(Track.Get());
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#undef LOCTEXT_NAMESPACE
