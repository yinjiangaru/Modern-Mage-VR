// Copyright BMK Games LLC. All Rights Reserved.

#include "SurfaceNavEditor.h"
#include "SurfaceNavPawnMovementViz.h"
#include "SurfaceNavMovementComponent.h"
#include "UnrealEd.h"

#define LOCTEXT_NAMESPACE "FSurfaceNavEditorModule"

void FSurfaceNavEditorModule::StartupModule()
{
    if (GUnrealEd)
    {
        // Make a new instance of the visualizer
        TSharedPtr<FSurfaceNavPawnMovementViz> PawnMoveViz = MakeShareable(new FSurfaceNavPawnMovementViz());

        // Register it to our specific component class
        GUnrealEd->RegisterComponentVisualizer(USurfaceNavMovementComponent::StaticClass()->GetFName(), PawnMoveViz);
        PawnMoveViz->OnRegister();
    }
}

void FSurfaceNavEditorModule::ShutdownModule()
{
    if (GUnrealEd)
    {
        // Unregister when the module shuts down
        GUnrealEd->UnregisterComponentVisualizer(USurfaceNavMovementComponent::StaticClass()->GetFName());
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_GAME_MODULE(FSurfaceNavEditorModule, SurfaceNavEditor);
