// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "SAnimTrackPanel_COPY.h"
#include "STrack_COPY.h"

class SBorder;
class UAnimMontage;
class UAnimSequenceBase;
class FAnimModel;
class FAnimModel_AnimMontage;

//////////////////////////////////////////////////////////////////////////
// FTimingRelevantElement - data object holding timing data

namespace ETimingElementType
{
	enum Type
	{
		QueuedNotify,
		BranchPointNotify,
		NotifyStateBegin,
		NotifyStateEnd,
		Section,
		Max,
	};
};

struct FTimingRelevantElementBase
{
	virtual ~FTimingRelevantElementBase()
	{

	}

	virtual FName GetTypeName()
	{
		return FName(TEXT("BASE"));
	}

	virtual float GetElementTime() const
	{
		return -1.0f;
	}

	virtual int32 GetElementSortPriority() const
	{
		return 0;
	}

	virtual ETimingElementType::Type GetType()
	{
		return ETimingElementType::Max;
	}

	// Get a list of descriptions key/values to describe the element.
	// Intended for UI/Tooltip use
	virtual void GetDescriptionItems(TMap<FString, FText>& Items)
	{

	}

	// Comparison for sorting lists of elements
	virtual bool Compare(const FTimingRelevantElementBase& Other)
	{
		if(FMath::IsNearlyEqual(GetElementTime(), Other.GetElementTime(), SMALL_NUMBER))
		{
			return GetElementSortPriority() < Other.GetElementSortPriority();
		}

		return GetElementTime() < Other.GetElementTime();
	}

	// Where in the order for the sequence this element will trigger
	int32 TriggerIdx;
};

// Small helper to store information about timing relevant elements (notifies, branch points, sections etc.)
struct FTimingRelevantElement_Notify : public FTimingRelevantElementBase
{
	UAnimSequenceBase* Sequence;	// The sequence the notify exists within
	int32 NotifyIndex;				// The index of the notify in the sequence

	virtual FName GetTypeName() override;
	virtual float GetElementTime() const override;
	virtual int32 GetElementSortPriority() const override;
	virtual ETimingElementType::Type GetType() override;
	virtual void GetDescriptionItems(TMap<FString, FText>& Items) override;
};

// Small helper to store information about timing relevant elements (notifies, branch points, sections etc.)
struct FTimingRelevantElement_NotifyStateEnd : public FTimingRelevantElement_Notify
{
	virtual FName GetTypeName() override;
	virtual float GetElementTime() const override;
	virtual ETimingElementType::Type GetType() override;
};

struct FTimingRelevantElement_Section : public FTimingRelevantElementBase
{
	UAnimMontage* Montage;	// The montage the section exists within
	int32 SectionIdx;		// The index of the section in the montage

	virtual FName GetTypeName() override;
	virtual float GetElementTime() const override;
	virtual ETimingElementType::Type GetType() override;
	virtual void GetDescriptionItems(TMap<FString, FText>& Items) override;
};

// Delegate to get the visibility of a type of timing node on an external panel (not the timing track)
DECLARE_DELEGATE_RetVal_OneParam(EVisibility, FOnGetTimingNodeVisibility, ETimingElementType::Type)

//////////////////////////////////////////////////////////////////////////
// The content of SAnimTimingTrackNode, separated to be used in non STrack widgets
//////////////////////////////////////////////////////////////////////////
class SAnimTimingNode : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAnimTimingNode)
		: _InElement()
		, _bUseTooltip(true)
	{}

	SLATE_ARGUMENT(TSharedPtr<FTimingRelevantElementBase>, InElement)
	SLATE_ARGUMENT(bool, bUseTooltip)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override;

protected:

	// The observed element
	TSharedPtr<FTimingRelevantElementBase> Element;
};

//////////////////////////////////////////////////////////////////////////
// Track node containing an identifier for trigger order of a timing element
//////////////////////////////////////////////////////////////////////////
class SAnimTimingTrackNode : public STrackNode
{
public:
	SLATE_BEGIN_ARGS(SAnimTimingTrackNode)
		: _bUseTooltip(true)
	{}

	SLATE_ATTRIBUTE(float, ViewInputMin)
	SLATE_ATTRIBUTE(float, ViewInputMax)
	SLATE_ATTRIBUTE(float, DataStartPos)
	SLATE_ATTRIBUTE(FString, NodeName)
	SLATE_ATTRIBUTE(FLinearColor, NodeColor)
	SLATE_ARGUMENT(TSharedPtr<FTimingRelevantElementBase>, Element)
	SLATE_ARGUMENT(bool, bUseTooltip)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

};

namespace SAnimTimingPanel_COPY
{
	void GetTimingRelevantElements(UAnimSequenceBase* Sequence, TArray<TSharedPtr<FTimingRelevantElementBase>>& Elements);
}