// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"

/**
 * Handles importing .lingotion pack files into the project.
 * Manages extraction, file movement, and asset import operations.
 */
class FEditorPackImporter
{
public:
	FEditorPackImporter();
	~FEditorPackImporter();

	/**
	 * Opens a file dialog to select and import a .lingotion pack file.
	 * Extracts the pack, moves .onnx files to Content/RuntimeData, and moves other files to RuntimeData.
	 * @return true if import was successful, false if user cancelled or import failed
	 */
	static bool ImportPack();

private:
	/**
	 * Extracts a .lingotion zip file to a temporary directory using Python.
	 * @param PackFilePath - Full path to the .lingotion file
	 * @param TempDir - Directory to extract files to
	 * @return true if extraction succeeded
	 */
	static bool ExtractPackToTemp(const FString& PackFilePath, const FString& TempDir);

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
