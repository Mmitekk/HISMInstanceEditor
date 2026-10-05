// Copyright Olkon Games / HISM Instance Editor. All Rights Reserved.

using UnrealBuildTool;

public class HISMInstanceEditor : ModuleRules
{
    public HISMInstanceEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "CoreUObject",
            "Engine",
            "PhysicsCore",
            "UnrealEd",
            "ToolMenus",
            "Slate",
            "SlateCore",
            "InputCore",
            "LevelEditor",
            "EditorScriptingUtilities"
        });
    }
}
