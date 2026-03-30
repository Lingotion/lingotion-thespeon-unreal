// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LingotionThespeon : ModuleRules
{
	public LingotionThespeon(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = false;

        // Add protobuf include path directly
		string ProtobufSrcPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", "Protobuf", "protobuf", "src");
        PublicSystemIncludePaths.Add(ProtobufSrcPath);

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
				"CoreUObject",
				"Engine",
				"NNE",
				"NNERuntimeORT",
				"AudioMixer",
				"Projects",
				"DeveloperSettings",
				"InputCore",
				"Protobuf",
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", "CoreUObject", "Engine", "NNE", "Json", "JsonUtilities", "Projects"
				// ... add private dependencies that you statically link with here ...	
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		// Make sure the manifest as well as imported .json files are included in a build
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "RuntimeData", "..."), StagedFileType.NonUFS);
	}
}
