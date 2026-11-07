// Copyright BMK Games LLC. All Rights Reserved.

using UnrealBuildTool;

public class SurfaceNavEditor : ModuleRules
{
	public SurfaceNavEditor(ReadOnlyTargetRules Target) : base(Target)
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
			
		
		// SurfaceNavPlugin needed for the objects we’re visualizing
		PublicDependencyModuleNames.AddRange(new string[] {  
			"Core", 
			"Engine", 
			"CoreUObject", 
			"SurfaceNavPlugin" });

		// Needed for our editor logic
		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });

			DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
