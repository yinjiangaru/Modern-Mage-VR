// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "UObject/GCObject.h"

class FReferenceCollector;
class UClass;
class UObject;

//////////////////////////////////////////////////////////////////////////
// FEditorObjectTracker_COPY

class FEditorObjectTracker_COPY : public FGCObject
{
public:
	FEditorObjectTracker_COPY(bool bInAllowOnePerClass = true)
		: bAllowOnePerClass(bInAllowOnePerClass)
	{}

	// FGCObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FEditorObjectTracker_COPY");
	}
	// End of FGCObject interface

	/** Returns an existing editor object for the specified class or creates one
	    if none exist */
	UObject* GetEditorObjectForClass( UClass* EdClass );

	void SetAllowOnePerClass(bool bInAllowOnePerClass)
	{
		bAllowOnePerClass = bInAllowOnePerClass;
	}
private:

	/** If true, it uses TMap, otherwise, it just uses TArray */
	bool bAllowOnePerClass;

	/** Tracks editor objects created for details panel */
	TMap< UClass*, UObject* >	EditorObjMap;

	/** Tracks editor objects created for detail panel */
	TArray<UObject*> EditorObjectArray;
};
