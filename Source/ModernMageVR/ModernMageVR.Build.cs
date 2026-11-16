// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ModernMageVR : ModuleRules
{
	public ModernMageVR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",

			"MassCrowd",
			"MassEntity",
			"MassActors",
			"MassCommon",
			"MassLOD",

			"MassMovement",
			"MassNavigation",
			"MassZoneGraphNavigation",
			"MassReplication",

			"MassSignals",
			"MassSimulation",
			"MassSpawner",
			"MassRepresentation",

			"MassAIReplication",
			"MassAIBehavior",
			
			"NavigationSystem",
			"MassNavMeshNavigation",
			"NavCorridor",
			"AnimToTexture",

			"AIModule",
			"GameplayTags",
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
