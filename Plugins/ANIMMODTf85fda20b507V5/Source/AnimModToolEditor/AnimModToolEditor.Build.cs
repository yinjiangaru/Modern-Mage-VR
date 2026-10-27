// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

using UnrealBuildTool;

public class AnimModToolEditor : ModuleRules
{
	public AnimModToolEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"SlateCore",
				"Slate",
				"Projects",
				"ContentBrowser",
				"AssetTools",
				"Engine",
                "AnimationEditor",
				"UnrealEd",
				"Persona",
				"PropertyEditor",
				"ToolMenus",
				"EditorWidgets",
				"InputCore",
				"AnimGraph",
				"KismetWidgets",
				"SequencerWidgets",
				"CurveEditor",
				"AppFramework",
                "AdvancedPreviewScene",
				"AnimGraphRuntime",
				"StatusBar",
				"ApplicationCore",
				"BlueprintGraph",
				"TimeManagement",
				"SequencerWidgets",
				"MovieScene",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}