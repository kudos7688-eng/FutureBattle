// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FutureBattle : ModuleRules
{
	public FutureBattle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "NetCore", "RenderCore", "RHI", "MassCore", "MassEntity" });
		// Two-bone IK of the body animation (BattleBodyAnim.cpp: hands raised over the cover for an over-the-top blind fire).
		PrivateDependencyModuleNames.Add("AnimationCore");
		// Battle effects (BattleEffects.cpp, 2026-11-22): explosions are Niagara systems.
		PrivateDependencyModuleNames.Add("Niagara");
		// T5 test-only view visibility observer; no renderer behavior changes.
		PrivateDependencyModuleNames.Add("Renderer");
		PrivateIncludePaths.Add(System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Private"));
		PrivateIncludePaths.Add(System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Internal"));
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AnimToTexture", "AnimToTextureEditor", "MeshDescription", "StaticMeshDescription", "AssetRegistry" });
		}

		PublicIncludePaths.AddRange(new string[] {
			"FutureBattle",
			"FutureBattle/Variant_Platforming",
			"FutureBattle/Variant_Platforming/Animation",
			"FutureBattle/Variant_Combat",
			"FutureBattle/Variant_Combat/AI",
			"FutureBattle/Variant_Combat/Animation",
			"FutureBattle/Variant_Combat/Gameplay",
			"FutureBattle/Variant_Combat/Interfaces",
			"FutureBattle/Variant_Combat/UI",
			"FutureBattle/Variant_SideScrolling",
			"FutureBattle/Variant_SideScrolling/AI",
			"FutureBattle/Variant_SideScrolling/Gameplay",
			"FutureBattle/Variant_SideScrolling/Interfaces",
			"FutureBattle/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
