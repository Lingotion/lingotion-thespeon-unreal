// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "EditorFileImporter.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Core/LingotionLogger.h"
#include "IPythonScriptPlugin.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "AssetToolsModule.h"
#include "AssetImportTask.h"

FEditorFileImporter::FEditorFileImporter() {}

FEditorFileImporter::~FEditorFileImporter() {}

bool FEditorFileImporter::Import()
{
	using Thespeon::Core::IO::RuntimeFileLoader;

	void* ParentWindowPtr = FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle();
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get DesktopPlatform module"));
		return false;
	}

	TArray<FString> OutFilePaths;
	const FString TempDir = FPaths::Combine(RuntimeFileLoader::GetPluginDir(), TEXT("temp"));
	IFileManager& FM = IFileManager::Get();

	if (!FM.DirectoryExists(*TempDir))
	{
		FM.MakeDirectory(*TempDir, true);
	}

	uint32 SelectionFlag = 0; // A value of 0 represents single file selection while a value of 1 represents multiple file selection

	// Open file dialog
	DesktopPlatform->OpenFileDialog(
	    ParentWindowPtr,
	    TEXT("Select a file to import..."),
	    RuntimeFileLoader::GetPluginDir(),
	    FString(""),
	    TEXT("Lingotion Files|*.lingotion|All Files|*.*"),
	    SelectionFlag,
	    OutFilePaths
	);

	if (OutFilePaths.Num() == 0)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("File import cancelled by user"));
		return false;
	}

	// Extract file
	if (!ExtractFileToTemp(OutFilePaths[0], TempDir))
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT(
		        "Failed to extract file: %s. Cancelling import. Try re-downloading and reimporting the file or contact support if the issue persists."
		    ),
		    *OutFilePaths[0]
		);
		return false;
	}

	// Import .onnx files to Content
	int32 OnnxFilesImported = ImportOnnxFiles(TempDir);

	// Move other files to RuntimeData
	int32 OtherFilesMoved = MoveOtherFiles(TempDir);

	// Clean up temp directory
	FM.DeleteDirectory(*TempDir, true, true);

	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug,
	    TEXT("File %s was successfully imported! (%d ONNX files, %d other files)"),
	    *FPaths::GetCleanFilename(OutFilePaths[0]),
	    OnnxFilesImported,
	    OtherFilesMoved
	);

	return true;
}

bool FEditorFileImporter::ExtractFileToTemp(const FString& FilePath, const FString& TempDir)
{
	using Thespeon::Core::IO::RuntimeFileLoader;

	// Unzip files with Python, quoted names to handle paths with spaces
	FString Command = FString::Printf(
	    TEXT("\"%s\" \"%s\" \"%s\""),
	    *FPaths::Combine(RuntimeFileLoader::GetPluginDir(), TEXT("Source"), TEXT("Python"), TEXT("extract_zip.py")),
	    *FPaths::ConvertRelativePathToFull(FilePath),
	    *TempDir
	);

	IPythonScriptPlugin::Get()->ExecPythonCommand(*Command);

	// Verify extraction succeeded by checking if temp directory has files
	TArray<FString> ExtractedFiles;
	IFileManager::Get().FindFilesRecursive(ExtractedFiles, *TempDir, TEXT("*.*"), true, false);

	return ExtractedFiles.Num() > 0;
}

int32 FEditorFileImporter::ImportOnnxFiles(const FString& TempDir)
{
	using Thespeon::Core::IO::RuntimeFileLoader;

	const FString& OnnxDest = RuntimeFileLoader::GetRuntimeModelDir();
	IFileManager& FM = IFileManager::Get();

	// Find all *.onnx files recursively
	TArray<FString> OnnxFiles;
	FM.FindFilesRecursive(OnnxFiles, *TempDir, TEXT("*.onnx"), /*Files=*/true, /*Directories=*/false);

	if (OnnxFiles.Num() == 0)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("No .onnx files found under: %s"), *TempDir);
		return 0;
	}

	for (const FString& AbsPath : OnnxFiles)
	{
		// Import file
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = AbsPath;
		Task->DestinationPath = OnnxDest;
		Task->bAutomated = true;
		Task->bSave = true;            // auto-save
		Task->bReplaceExisting = true; // overwrite if already exists

		// Run the task
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		AssetToolsModule.Get().ImportAssetTasks({Task});
	}

	return OnnxFiles.Num();
}

int32 FEditorFileImporter::MoveOtherFiles(const FString& TempDir)
{
	using Thespeon::Core::IO::RuntimeFileLoader;

	IFileManager& FM = IFileManager::Get();

	// Move rest of the files to a corresponding folder in RuntimeData
	TArray<FString> JsonFiles;
	TArray<FString> MetaGraphFiles;
	FM.FindFilesRecursive(JsonFiles, *TempDir, TEXT("*.json"), /*Files=*/true, /*Directories=*/false);
	FM.FindFilesRecursive(MetaGraphFiles, *TempDir, TEXT("*.metagraph"), /*Files=*/true, /*Directories=*/false);

	TArray<FString> OtherFiles = MoveTemp(JsonFiles);
	OtherFiles.Append(MetaGraphFiles);

	for (const FString& AbsPath : OtherFiles)
	{
		FString NewPath = FPaths::Combine(RuntimeFileLoader::GetRuntimeFileDir(), FPaths::GetCleanFilename(AbsPath));
		FM.Move(*NewPath, *AbsPath);
	}

	return OtherFiles.Num();
}
