using UnrealBuildTool;

public class BlueprintSerializer : ModuleRules
{
	public BlueprintSerializer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AssetRegistry",
				"BlueprintGraph",
				"DeveloperSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"AnimGraph",
				"AnimGraphRuntime",
				"EditorStyle",
				"EditorWidgets",
				"ToolMenus",
				"Slate",
				"SlateCore",
				"Json",
				"JsonUtilities",
				"KismetCompiler",
				"PropertyEditor",
				"WorkspaceMenuStructure",
				"ContentBrowser",
				"AssetTools",
				"GraphEditor",
				"Kismet",
				"Projects",
				"Settings",
				"EditorSubsystem",
				"DesktopPlatform",
				"ApplicationCore",
				"ControlRig",
				"ControlRigDeveloper",
				"RigVM",
				"RigVMDeveloper",
				"UMG",
				"UMGEditor",
				"MovieScene",
				// Task 33-50: node handlers requiring external module headers
				"GameplayAbilitiesEditor",   // K2Node_LatentAbilityCall
				"GameplayTasksEditor",       // K2Node_LatentGameplayTaskCall
				"InputBlueprintNodes",       // K2Node_EnhancedInputAction
				"EnhancedInput",             // UInputAction definition (used by K2Node_EnhancedInputAction)
				"InputCore",                 // FKey::GetFName / FKey::GetDisplayName (K2Node_InputKey)
			}
		);

		// Add runtime dependencies for content
		if (Target.bBuildEditor)
		{
			RuntimeDependencies.Add("$(PluginDir)/Content/...", StagedFileType.UFS);
		}
	}
}
