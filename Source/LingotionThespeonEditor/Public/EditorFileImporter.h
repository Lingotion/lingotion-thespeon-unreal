// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/**
 * Handles importing .lingotion files into the project.
 * Manages extraction, validation, file movement, and asset import operations.
 */
class FEditorFileImporter
{
  public:
	FEditorFileImporter();
	~FEditorFileImporter();

	/**
	 * Opens a file dialog to select and import a .lingotion file.
	 * Extracts the file, validates every module config in it, imports the .onnx files to Content/RuntimeData,
	 * and moves the remaining module files and the configs to RuntimeData. Nothing is installed if a config is invalid.
	 * @return true if import was successful, false if user cancelled or import failed
	 */
	static bool Import();

  private:
	/**
	 * Validates, imports and installs the contents of an extracted .lingotion file.
	 * @param TempDir - Directory containing extracted files
	 * @return true if every module in the archive was installed
	 */
	static bool InstallExtractedArchive(const FString& TempDir);

	/**
	 * Extracts a .lingotion zip file to a temporary directory.
	 * @param FilePath - Full path to the .lingotion file
	 * @param TempDir - Directory to extract files to
	 * @return true if extraction succeeded
	 */
	static bool ExtractFileToTemp(const FString& FilePath, const FString& TempDir);

	/**
	 * Checks that a module config is well formed and that every file it lists was extracted.
	 * @param ConfigPath - Path to the extracted config file, used for logging
	 * @param Contents - Parsed config
	 * @param ExtractedFiles - Extracted files, keyed by clean filename
	 * @param OutRequiredFiles - Receives the files the module needs, keyed by clean filename
	 * @return true if the module can be installed
	 */
	static bool ValidateModuleConfig(
	    const FString& ConfigPath,
	    const TSharedPtr<FJsonObject>& Contents,
	    const TMap<FString, FString>& ExtractedFiles,
	    TMap<FString, FString>& OutRequiredFiles
	);

	/**
	 * Creates UNNEModelData assets in the Content/RuntimeData folder from .onnx files and saves them in a single batch.
	 * Files whose asset already exists are skipped, since assets are named by the MD5 of their content.
	 * Models that store their weights in external files are not supported.
	 * @param OnnxFiles - Absolute paths of the .onnx files to import
	 * @param OutImported - Number of files imported
	 * @param OutSkipped - Number of files skipped because they were already imported
	 * @return true if every file is available as an asset afterwards
	 */
	static bool ImportOnnxFiles(const TArray<FString>& OnnxFiles, int32& OutImported, int32& OutSkipped);

	/**
	 * Moves files to the RuntimeData directory, replacing existing files.
	 * @param Files - Absolute paths of the files to move
	 * @return true if every file was moved
	 */
	static bool MoveFilesToRuntimeData(const TArray<FString>& Files);
};
