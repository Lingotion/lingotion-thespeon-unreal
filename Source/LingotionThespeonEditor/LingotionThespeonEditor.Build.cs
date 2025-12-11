// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LingotionThespeonEditor : ModuleRules
{
	public LingotionThespeonEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;

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
				"DeveloperSettings",
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorFramework",
				"EditorStyle",
				"UnrealEd",
				"ToolMenus",
				"WorkspaceMenuStructure",
				"LevelEditor",
				"InteractiveToolsFramework",
				"EditorInteractiveToolsFramework",
				"LingotionThespeon",
				"Core",
				"CoreUObject",
				"Engine",
				"NNE",
				"Json",
				"JsonUtilities",
				"EditorSubsystem",
				"DirectoryWatcher",
				"Projects",
				"PythonScriptPlugin",
				"AssetTools",
				"ApplicationCore",
				"HTTP",
				
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