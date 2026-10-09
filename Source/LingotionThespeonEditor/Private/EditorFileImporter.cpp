// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "EditorFileImporter.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "Core/ManifestHandler.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Core/LingotionLogger.h"
#include "FileUtilities/ZipArchiveReader.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "FileHelpers.h"
#include "NNEModelData.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UEditorFileWatcher.h"

#define LOCTEXT_NAMESPACE "LingotionEditorFileImporter"

FEditorFileImporter::FEditorFileImporter() {}

FEditorFileImporter::~FEditorFileImporter() {}

bool FEditorFileImporter::Import()
{
	using Thespeon::Core::IO::RuntimeFileLoader;

	const void* ParentWindowPtr = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get DesktopPlatform module"));
		return false;
	}

	TArray<FString> OutFilePaths;
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

	// Extract into the project's Intermediate folder, since the plugin folder may be read-only when installed in the engine.
	// Start from an empty folder so leftovers from an earlier failed import cannot be mistaken for this archive's contents.
	const FString TempDir =
	    FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("LingotionThespeon"), TEXT("ImportTemp")));
	IFileManager& FM = IFileManager::Get();
	if (FM.DirectoryExists(*TempDir) && !FM.DeleteDirectory(*TempDir, false, true))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to empty import temp directory: %s. Cancelling import."), *TempDir);
		return false;
	}
	if (!FM.MakeDirectory(*TempDir, true))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to create import temp directory: %s. Cancelling import."), *TempDir);
		return false;
	}

	// Progress weights roughly follow measured time: extraction, then model creation and saving, then the manifest rebuild
	FScopedSlowTask SlowTask(
	    10.f, FText::Format(LOCTEXT("ImportingFile", "Importing {0}"), FText::FromString(FPaths::GetCleanFilename(OutFilePaths[0])))
	);
	SlowTask.MakeDialog();

	SlowTask.EnterProgressFrame(2.f, LOCTEXT("Extracting", "Extracting files"));
	bool bSuccess = ExtractFileToTemp(OutFilePaths[0], TempDir);
	if (!bSuccess)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT(
		        "Failed to extract file: %s. Cancelling import. Try re-downloading and reimporting the file or contact support if the issue persists."
		    ),
		    *OutFilePaths[0]
		);
	}
	else
	{
		SlowTask.EnterProgressFrame(7.f, LOCTEXT("Installing", "Installing modules"));
		bSuccess = InstallExtractedArchive(TempDir);
	}

	// Clean up temp directory
	FM.DeleteDirectory(*TempDir, false, true);

	if (!bSuccess)
	{
		return false;
	}

	// Rebuild the manifest even if the set of config filenames is unchanged, so a re-imported module with new contents is picked up.
	SlowTask.EnterProgressFrame(1.f, LOCTEXT("UpdatingModuleList", "Updating module list"));
	if (UEditorFileWatcher* Watcher = UEditorFileWatcher::Get())
	{
		Watcher->UpdateMappingsInfo(/*bForce=*/true);
	}

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("File %s was successfully imported!"), *FPaths::GetCleanFilename(OutFilePaths[0]));

	return true;
}

bool FEditorFileImporter::InstallExtractedArchive(const FString& TempDir)
{
	IFileManager& FM = IFileManager::Get();

	TArray<FString> ExtractedPaths;
	FM.FindFilesRecursive(ExtractedPaths, *TempDir, TEXT("*.*"), /*Files=*/true, /*Directories=*/false);

	TMap<FString, FString> ExtractedFiles;
	for (const FString& AbsPath : ExtractedPaths)
	{
		ExtractedFiles.Add(FPaths::GetCleanFilename(AbsPath), AbsPath);
	}

	// Find the module configs and validate all of them before anything is installed, so a bad archive leaves the project untouched.
	TArray<FString> ConfigFiles;
	TMap<FString, FString> RequiredFiles;
	bool bAllValid = true;
	for (const FString& AbsPath : ExtractedPaths)
	{
		if (FPaths::GetExtension(AbsPath) != TEXT("json"))
		{
			continue;
		}

		const FString JsonStr = Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsString(AbsPath);
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
		TSharedPtr<FJsonObject> Contents;
		FString Type;
		if (!FJsonSerializer::Deserialize(Reader, Contents) || !Contents.IsValid() || !Contents->TryGetStringField(TEXT("type"), Type))
		{
			// Data files referenced by a module (such as phonemizer data) are JSON too, but have no type
			continue;
		}
		if (Type != TEXT("lara") && Type != TEXT("phonemizer"))
		{
			continue;
		}

		ConfigFiles.Add(AbsPath);
		bAllValid &= ValidateModuleConfig(AbsPath, Contents, ExtractedFiles, RequiredFiles);
	}

	if (ConfigFiles.Num() == 0)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("No module configs found in the imported file. Try re-downloading and reimporting the file or contact support if the issue persists."
		    )
		);
		return false;
	}

	if (!bAllValid)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Import cancelled because the file contains invalid modules. Nothing was installed."));
		return false;
	}

	TArray<FString> OnnxFiles;
	TArray<FString> OtherFiles;
	for (const TPair<FString, FString>& Pair : RequiredFiles)
	{
		if (FPaths::GetExtension(Pair.Value) == TEXT("onnx"))
		{
			OnnxFiles.Add(Pair.Value);
		}
		else
		{
			OtherFiles.Add(Pair.Value);
		}
	}

	int32 OnnxFilesImported = 0;
	int32 OnnxFilesSkipped = 0;
	if (!ImportOnnxFiles(OnnxFiles, OnnxFilesImported, OnnxFilesSkipped))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Import cancelled because one or more models failed to import. No modules were added."));
		return false;
	}

	if (!MoveFilesToRuntimeData(OtherFiles))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Import cancelled because one or more module files could not be installed. No modules were added."));
		return false;
	}

	// Configs go last: they are what makes a module visible, so they must only appear once all of its files are in place
	if (!MoveFilesToRuntimeData(ConfigFiles))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("One or more module configs could not be installed."));
		return false;
	}

	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug,
	    TEXT("Installed %d modules (%d ONNX files imported, %d already imported, %d other files)"),
	    ConfigFiles.Num(),
	    OnnxFilesImported,
	    OnnxFilesSkipped,
	    OtherFiles.Num()
	);

	return true;
}

bool FEditorFileImporter::ExtractFileToTemp(const FString& FilePath, const FString& TempDir)
{
	// The reader takes ownership of the file handle, even if it is null
	FZipArchiveReader Reader(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*FPaths::ConvertRelativePathToFull(FilePath)));
	if (!Reader.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Could not open %s as a zip archive."), *FilePath);
		return false;
	}

	FString TempDirWithSlash = TempDir;
	FPaths::NormalizeDirectoryName(TempDirWithSlash);
	TempDirWithSlash += TEXT("/");

	const TArray<FString> EntryNames = Reader.GetFileNames();
	FScopedSlowTask SlowTask(EntryNames.Num());

	int32 NumExtracted = 0;
	TArray<uint8> Data;
	for (int32 Index = 0; Index < EntryNames.Num(); ++Index)
	{
		const FString& EntryName = EntryNames[Index];
		SlowTask.EnterProgressFrame(
		    1.f,
		    FText::Format(LOCTEXT("ExtractingEntry", "Extracting file {0} of {1}"), FText::AsNumber(Index + 1), FText::AsNumber(EntryNames.Num()))
		);

		// Directory entries
		if (EntryName.EndsWith(TEXT("/")))
		{
			continue;
		}

		// Reject entries such as "../x" that would be written outside the temp directory
		FString DestPath = FPaths::Combine(TempDir, EntryName);
		FPaths::NormalizeFilename(DestPath);
		if (!FPaths::CollapseRelativeDirectories(DestPath) || !DestPath.StartsWith(TempDirWithSlash))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Archive entry %s has an invalid path."), *EntryName);
			return false;
		}

		if (!Reader.TryReadFile(EntryName, Data))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to read %s from the archive."), *EntryName);
			return false;
		}
		if (!FFileHelper::SaveArrayToFile(Data, *DestPath))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to write %s."), *DestPath);
			return false;
		}
		++NumExtracted;
	}

	return NumExtracted > 0;
}

bool FEditorFileImporter::ValidateModuleConfig(
    const FString& ConfigPath,
    const TSharedPtr<FJsonObject>& Contents,
    const TMap<FString, FString>& ExtractedFiles,
    TMap<FString, FString>& OutRequiredFiles
)
{
	const FString ConfigName = FPaths::GetCleanFilename(ConfigPath);

	UManifestHandler* ManifestHandler = UManifestHandler::Get();
	if (!ManifestHandler || !ManifestHandler->ReadVersionObject(Contents).IsValid())
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT(
		        "Lingotion import rejected %s: module config has no valid 'version' field. Expected an object with integer 'major', 'minor', and 'patch' fields. Re-download the module from the Lingotion portal."
		    ),
		    *ConfigName
		);
		return false;
	}

	// The manifest keys character modules by source_id and language modules by base_module_id
	const FString Type = Contents->GetStringField(TEXT("type"));
	const TCHAR* IdField = Type == TEXT("lara") ? TEXT("source_id") : TEXT("base_module_id");
	FString Identifier;
	FString Name;
	if (!Contents->TryGetStringField(IdField, Identifier) || Identifier.IsEmpty() || !Contents->TryGetStringField(TEXT("name"), Name))
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Lingotion import rejected %s: module config is missing '%s' or 'name'. Re-download the module from the Lingotion portal."),
		    *ConfigName,
		    IdField
		);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ModuleFilesPtr = nullptr;
	if (!Contents->TryGetArrayField(TEXT("files"), ModuleFilesPtr) || !ModuleFilesPtr)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Lingotion import rejected %s: module config has no 'files' list. Re-download the module from the Lingotion portal."),
		    *ConfigName
		);
		return false;
	}

	bool bAllFilesPresent = true;
	for (const TSharedPtr<FJsonValue>& ModuleFileEntry : *ModuleFilesPtr)
	{
		const TSharedPtr<FJsonObject> FileObj = ModuleFileEntry->AsObject();
		FString MD5;
		FString Extension;
		if (!FileObj.IsValid() || !FileObj->TryGetStringField(TEXT("md5"), MD5) || MD5.IsEmpty() ||
		    !FileObj->TryGetStringField(TEXT("extension"), Extension) || Extension.IsEmpty())
		{
			LINGO_LOG(
			    EVerbosityLevel::Error,
			    TEXT("Lingotion import rejected %s: module config has a malformed file entry. Re-download the module from the Lingotion portal."),
			    *ConfigName
			);
			return false;
		}

		// Module files are stored under the MD5 of their content
		const FString FileName = MD5 + TEXT(".") + Extension;
		if (const FString* AbsPath = ExtractedFiles.Find(FileName))
		{
			OutRequiredFiles.Add(FileName, *AbsPath);
		}
		else
		{
			LINGO_LOG(
			    EVerbosityLevel::Error,
			    TEXT(
			        "Lingotion import rejected %s: required file %s is missing from the imported file. Re-download the module from the Lingotion portal."
			    ),
			    *ConfigName,
			    *FileName
			);
			bAllFilesPresent = false;
		}
	}

	return bAllFilesPresent;
}

bool FEditorFileImporter::ImportOnnxFiles(const TArray<FString>& OnnxFiles, int32& OutImported, int32& OutSkipped)
{
	using Thespeon::Core::IO::RuntimeFileLoader;

	OutImported = 0;
	OutSkipped = 0;

	// The assets are created directly instead of through the engine's ONNX import factory. The factory also parses every model
	// to pick up weights stored in external files, which Lingotion models never use, and that parse is most of its import time.
	TArray<FString> FilesToImport;
	for (const FString& AbsPath : OnnxFiles)
	{
		// Assets are named by the MD5 of the model, so an existing asset already holds identical content
		if (UEditorAssetLibrary::DoesAssetExist(RuntimeFileLoader::GetRuntimeModelPath(FPaths::GetBaseFilename(AbsPath))))
		{
			++OutSkipped;
		}
		else
		{
			FilesToImport.Add(AbsPath);
		}
	}

	if (FilesToImport.Num() == 0)
	{
		return true;
	}

	// Saving takes about four times as long as creating the assets
	const float SaveWorkPerModel = 4.f;
	FScopedSlowTask SlowTask(FilesToImport.Num() * (1.f + SaveWorkPerModel));

	TArray<UPackage*> Packages;
	TArray64<uint8> OnnxData;
	for (int32 Index = 0; Index < FilesToImport.Num(); ++Index)
	{
		const FString& AbsPath = FilesToImport[Index];
		SlowTask.EnterProgressFrame(
		    1.f,
		    FText::Format(LOCTEXT("CreatingModel", "Creating model {0} of {1}"), FText::AsNumber(Index + 1), FText::AsNumber(FilesToImport.Num()))
		);

		const FString AssetName = FPaths::GetBaseFilename(AbsPath);
		const FString PackagePath = RuntimeFileLoader::GetRuntimeModelPath(AssetName);
		if (!FFileHelper::LoadFileToArray(OnnxData, *AbsPath) || OnnxData.IsEmpty())
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to read model: %s"), *FPaths::GetCleanFilename(AbsPath));
			return false;
		}

		UPackage* Package = CreatePackage(*PackagePath);
		UNNEModelData* ModelData = FindObject<UNNEModelData>(Package, *AssetName);
		const bool bIsNewAsset = ModelData == nullptr;
		if (bIsNewAsset)
		{
			ModelData = NewObject<UNNEModelData>(Package, *AssetName, RF_Public | RF_Standalone);
		}
		ModelData->Init(TEXT("onnx"), OnnxData);
		if (bIsNewAsset)
		{
			FAssetRegistryModule::AssetCreated(ModelData);
		}
		Package->MarkPackageDirty();
		Packages.Add(Package);
	}

	// Save all new assets in one batch
	SlowTask.EnterProgressFrame(FilesToImport.Num() * SaveWorkPerModel, LOCTEXT("SavingModels", "Saving models"));
	if (!UEditorLoadingAndSavingUtils::SavePackages(Packages, /*bOnlyDirty=*/false))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to save one or more imported models to %s"), *RuntimeFileLoader::GetRuntimeModelDir());
		return false;
	}

	OutImported = Packages.Num();
	return true;
}

bool FEditorFileImporter::MoveFilesToRuntimeData(const TArray<FString>& Files)
{
	using Thespeon::Core::IO::RuntimeFileLoader;

	IFileManager& FM = IFileManager::Get();

	bool bAllMoved = true;
	for (const FString& AbsPath : Files)
	{
		FString NewPath = FPaths::Combine(RuntimeFileLoader::GetRuntimeFileDir(), FPaths::GetCleanFilename(AbsPath));
		if (!FM.Move(*NewPath, *AbsPath))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to move %s to %s"), *AbsPath, *NewPath);
			bAllMoved = false;
		}
	}

	return bAllMoved;
}

#undef LOCTEXT_NAMESPACE
