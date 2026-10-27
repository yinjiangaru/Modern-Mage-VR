// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "EditorObjectsTracker_COPY.h"

#include "HAL/PlatformCrt.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

void FEditorObjectTracker_COPY::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObjects(EditorObjMap);
	Collector.AddReferencedObjects(EditorObjectArray);
}

UObject* FEditorObjectTracker_COPY::GetEditorObjectForClass( UClass* EdClass )
{
	UObject *Obj = (bAllowOnePerClass && EditorObjMap.Contains(EdClass) ? *EditorObjMap.Find(EdClass) : nullptr);

	if (Obj == NULL)
	{
		FString ObjName = MakeUniqueObjectName(GetTransientPackage(), EdClass).ToString();
		ObjName += "_EdObj";
		Obj = NewObject<UObject>(GetTransientPackage(), EdClass, FName(*ObjName), RF_Public | RF_Standalone | RF_Transient);
		if (bAllowOnePerClass)
		{
			EditorObjMap.Add(EdClass, Obj);
		}
		else
		{
			EditorObjectArray.Add(Obj);
		}
	}
	return Obj;
}
