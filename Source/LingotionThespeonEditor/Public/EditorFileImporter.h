// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"

/**
 * Handles importing .lingotion files into the project.
 * Manages extraction, file movement, and asset import operations.
 */
class FEditorFileImporter
{
  public:
	FEditorFileImporter();
	~FEditorFileImporter();

	/**
	 * Opens a file dialog to select and import a .lingotion file.
	 * Extracts the file, moves .onnx files to Content/RuntimeData, and moves other files to RuntimeData.
	 * @return true if import was successful, false if user cancelled or import failed
	 */
	static bool Import();

  private:
	/**
	 * Extracts a .lingotion zip file to a temporary directory using Python.
	 * @param FilePath - Full path to the .lingotion file
	 * @param TempDir - Directory to extract files to
	 * @return true if extraction succeeded
	 */
	static bool ExtractFileToTemp(const FString& FilePath, const FString& TempDir);

	/**
	 * Imports .onnx files from temp directory to the Content/RuntimeData folder as assets.
	 * @param TempDir - Directory containing extracted files
	 * @return Number of files imported
	 */
	static int32 ImportOnnxFiles(const FString& TempDir);

	/**
	 * Moves non-.onnx files (like JSON) from temp to RuntimeData directory.
	 * @param TempDir - Directory containing extracted files
	 * @return Number of files moved
	 */
	static int32 MoveOtherFiles(const FString& TempDir);
};
