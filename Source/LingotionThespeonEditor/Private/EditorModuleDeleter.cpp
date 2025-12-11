// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "EditorModuleDeleter.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "HAL/FileManager.h"
#include "Core/LingotionLogger.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FEditorModuleDeleter::FEditorModuleDeleter()
{
}

FEditorModuleDeleter::~FEditorModuleDeleter()
{
}

void FEditorModuleDeleter::DeleteActorModule(const FString& ModuleID, const FString& PackName, const FString& JsonPath)
{
	using Thespeon::Core::IO::RuntimeFileLoader;
	IFileManager& FM = IFileManager::Get();

	LINGO_LOG(EVerbosityLevel::Info, TEXT("Starting deletion of Actor Module: %s"), *ModuleID);

	// 1. Read the module JSON to get all files
	FString ModuleJsonPath = RuntimeFileLoader::GetRuntimeFilePath(JsonPath);
	FString ModuleJsonString = RuntimeFileLoader::LoadFileAsString(ModuleJsonPath);

	TArray<FString> FilesToDelete;
	TArray<FString> AssetPathsToDelete;

	if (!ModuleJsonString.IsEmpty())
	{
		TSharedPtr<FJsonObject> ModuleJsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ModuleJsonString);

		if (FJsonSerializer::Deserialize(Reader, ModuleJsonObject) && ModuleJsonObject.IsValid())
		{
			// Get config files section
			const TSharedPtr<FJsonObject>* ConfigFilesPtr = nullptr;
			if (ModuleJsonObject->TryGetObjectField(TEXT("files"), ConfigFilesPtr) && ConfigFilesPtr)
			{
				const TSharedPtr<FJsonObject>& ConfigFiles = *ConfigFilesPtr;

				// Get modules array
				const TArray<TSharedPtr<FJsonValue>>* ModulesArray = nullptr;
				if (ModuleJsonObject->TryGetArrayField(TEXT("modules"), ModulesArray))
				{
					// Find our module in the modules array
					for (const auto& ModuleValue : *ModulesArray)
					{
						TSharedPtr<FJsonObject> ModuleObj = ModuleValue->AsObject();
						if (ModuleObj.IsValid())
						{
							FString ModuleBaseID;
							if (ModuleObj->TryGetStringField(TEXT("actorpack_base_module_id"), ModuleBaseID) &&
							    ModuleBaseID == ModuleID)
							{
								// Get files section - try 'files' first (new format), then 'onnxfiles' (old format)
								const TSharedPtr<FJsonObject>* OnnxFilesPtr = nullptr;
								if (!ModuleObj->TryGetObjectField(TEXT("files"), OnnxFilesPtr) || !OnnxFilesPtr)
								{
									// Fall back to old format
									ModuleObj->TryGetObjectField(TEXT("onnxfiles"), OnnxFilesPtr);
								}

								if (OnnxFilesPtr)
								{
									for (const auto& Pair : (*OnnxFilesPtr)->Values)
									{
										FString MD5 = Pair.Value->AsString();

										// Find file entry in config files
										const TSharedPtr<FJsonValue>* FileEntryPtr = ConfigFiles->Values.Find(MD5);
										if (FileEntryPtr && (*FileEntryPtr)->AsObject().IsValid())
										{
											TSharedPtr<FJsonObject> FileEntryObj = (*FileEntryPtr)->AsObject();
											FString FileName;
											if (FileEntryObj->TryGetStringField(TEXT("filename"), FileName))
											{
												// ONNX files are imported as .uasset in Content directory
												FString BaseFileName = FPaths::GetBaseFilename(FileName);
												FString UAssetPath = FPaths::Combine(
													FPaths::ProjectContentDir(),
													TEXT("LingotionThespeon/RuntimeData"),
													BaseFileName + TEXT(".uasset")
												);
												FilesToDelete.Add(UAssetPath);
												AssetPathsToDelete.Add(TEXT("/Game/LingotionThespeon/RuntimeData/") + BaseFileName);

												LINGO_LOG(EVerbosityLevel::Debug, TEXT("Marked for deletion: %s"), *UAssetPath);
											}
										}
									}
								}
								break;
							}
						}
					}
				}
			}
		}
	}

	// 2. Check PackManifest.json file_usage to filter out shared files
	TSet<FString> FilesToKeep; // Files used by other modules

	// Extract MD5s from module for reference checking
	TSet<FString> MD5sToDelete;
	if (!ModuleJsonString.IsEmpty())
	{
		TSharedPtr<FJsonObject> ModuleJsonObj;
		TSharedRef<TJsonReader<>> ModJsonReader = TJsonReaderFactory<>::Create(ModuleJsonString);

		if (FJsonSerializer::Deserialize(ModJsonReader, ModuleJsonObj) && ModuleJsonObj.IsValid())
		{
			const TSharedPtr<FJsonObject>* ConfigFilesPtr2 = nullptr;
			if (ModuleJsonObj->TryGetObjectField(TEXT("files"), ConfigFilesPtr2) && ConfigFilesPtr2)
			{
				const TArray<TSharedPtr<FJsonValue>>* ModulesArray2 = nullptr;
				if (ModuleJsonObj->TryGetArrayField(TEXT("modules"), ModulesArray2))
				{
					for (const auto& ModuleValue : *ModulesArray2)
					{
						TSharedPtr<FJsonObject> ModObj = ModuleValue->AsObject();
						if (ModObj.IsValid())
						{
							FString ModBaseID;
							if (ModObj->TryGetStringField(TEXT("actorpack_base_module_id"), ModBaseID) && ModBaseID == ModuleID)
							{
								const TSharedPtr<FJsonObject>* ModFilesPtr = nullptr;
								if (!ModObj->TryGetObjectField(TEXT("files"), ModFilesPtr) || !ModFilesPtr)
								{
									ModObj->TryGetObjectField(TEXT("onnxfiles"), ModFilesPtr);
								}

								if (ModFilesPtr)
								{
									for (const auto& FilePair : (*ModFilesPtr)->Values)
									{
										FString MD5 = FilePair.Value->AsString();
										if (!MD5.IsEmpty())
										{
											MD5sToDelete.Add(MD5);
										}
									}
								}
								break;
							}
						}
					}
				}
			}
		}
	}

	// Check which files are shared
	for (const FString& MD5 : MD5sToDelete)
	{
		int32 UsageCount = 0;
		if (IsFileSharedByMultipleModules(MD5, ModuleJsonString, UsageCount))
		{
			FString FileName = GetFilenameFromMD5(MD5, ModuleJsonString);
			if (!FileName.IsEmpty())
			{
				FString BaseFileName = FPaths::GetBaseFilename(FileName);
				FilesToKeep.Add(BaseFileName);
				LINGO_LOG(EVerbosityLevel::Debug, TEXT("File %s (MD5: %s) is shared by %d modules - will NOT delete"), *BaseFileName, *MD5, UsageCount);
			}
		}
	}

	// Filter out shared files from deletion list
	TArray<FString> FilteredFilesToDelete;
	TArray<FString> FilteredAssetsToDelete;
	for (int32 i = 0; i < FilesToDelete.Num(); i++)
	{
		FString BaseFileName = FPaths::GetBaseFilename(FilesToDelete[i]);
		if (!FilesToKeep.Contains(BaseFileName))
		{
			FilteredFilesToDelete.Add(FilesToDelete[i]);
			if (i < AssetPathsToDelete.Num())
			{
				FilteredAssetsToDelete.Add(AssetPathsToDelete[i]);
			}
		}
		else
		{
			LINGO_LOG(EVerbosityLevel::Debug, TEXT("Skipping shared file: %s"), *FilesToDelete[i]);
		}
	}

	FilesToDelete = FilteredFilesToDelete;
	AssetPathsToDelete = FilteredAssetsToDelete;

	// 3. Delete the module JSON file
	FString JsonFullPath = RuntimeFileLoader::GetRuntimeFilePath(JsonPath);
	FilesToDelete.Add(JsonFullPath);
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Marked JSON for deletion: %s"), *JsonFullPath);

	// 4. Delete all files
	for (const FString& FilePath : FilesToDelete)
	{
		if (FM.FileExists(*FilePath))
		{
			if (FM.Delete(*FilePath, false, true))
			{
				LINGO_LOG(EVerbosityLevel::Debug, TEXT("Deleted file: %s"), *FilePath);
			}
			else
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to delete file: %s"), *FilePath);
			}
		}
	}

	// Note: We don't manually update PackManifest.json here.
	// The UEditorPackWatcher will detect the JSON file deletion and automatically
	// call UpdatePackMappingsInfo() which rebuilds the manifest from the remaining files.
	// This ensures the manifest stays in sync with the file system through the proper workflow.

	LINGO_LOG(EVerbosityLevel::Info, TEXT("Completed deletion of Actor Module: %s. Watcher will update manifest."), *ModuleID);
}

void FEditorModuleDeleter::DeleteLanguageModule(const FString& ModuleID, const FString& PackName, const FString& JsonPath)
{
	using Thespeon::Core::IO::RuntimeFileLoader;
	IFileManager& FM = IFileManager::Get();

	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Starting deletion of Language Module: %s"), *ModuleID);

	// 1. Read the module JSON to get all files
	FString ModuleJsonPath = RuntimeFileLoader::GetRuntimeFilePath(JsonPath);
	FString ModuleJsonString = RuntimeFileLoader::LoadFileAsString(ModuleJsonPath);

	TArray<FString> FilesToDelete;
	TArray<FString> AssetPathsToDelete;
	TSet<FString> MD5sToDelete;

	// Collect files from module using helper function
	CollectLanguageModuleFiles(ModuleID, ModuleJsonString, FilesToDelete, AssetPathsToDelete, MD5sToDelete);

	// 2. Check PackManifest.json file_usage to filter out shared files
	TSet<FString> FilesToKeep; // Files used by other modules

	for (const FString& MD5 : MD5sToDelete)
	{
		int32 UsageCount = 0;
		if (IsFileSharedByMultipleModules(MD5, ModuleJsonString, UsageCount))
		{
			FString FileName = GetFilenameFromMD5(MD5, ModuleJsonString);
			if (!FileName.IsEmpty())
			{
				FString BaseFileName = FPaths::GetBaseFilename(FileName);
				FilesToKeep.Add(BaseFileName);
				LINGO_LOG(EVerbosityLevel::Debug, TEXT("File %s (MD5: %s) is shared by %d modules - will NOT delete"), *BaseFileName, *MD5, UsageCount);
			}
		}
	}

	// Filter out shared files from deletion list
	TArray<FString> FilteredFilesToDelete;
	TArray<FString> FilteredAssetsToDelete;
	for (int32 i = 0; i < FilesToDelete.Num(); i++)
	{
		FString BaseFileName = FPaths::GetBaseFilename(FilesToDelete[i]);
		if (!FilesToKeep.Contains(BaseFileName))
		{
			FilteredFilesToDelete.Add(FilesToDelete[i]);
			if (i < AssetPathsToDelete.Num())
			{
				FilteredAssetsToDelete.Add(AssetPathsToDelete[i]);
			}
		}
		else
		{
			LINGO_LOG(EVerbosityLevel::Debug, TEXT("Skipping shared file: %s"), *FilesToDelete[i]);
		}
	}

	FilesToDelete = FilteredFilesToDelete;
	AssetPathsToDelete = FilteredAssetsToDelete;

	// 3. Delete the module JSON file
	FString JsonFullPath = RuntimeFileLoader::GetRuntimeFilePath(JsonPath);
	FilesToDelete.Add(JsonFullPath);
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Marked JSON for deletion: %s"), *JsonFullPath);

	// 4. Delete all files
	for (const FString& FilePath : FilesToDelete)
	{
		if (FM.FileExists(*FilePath))
		{
			if (FM.Delete(*FilePath, false, true))
			{
				LINGO_LOG(EVerbosityLevel::Debug, TEXT("Deleted file: %s"), *FilePath);
			}
			else
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to delete file: %s"), *FilePath);
			}
		}
	}

	// Note: We don't manually update PackManifest.json here.
	// The UEditorPackWatcher will detect the JSON file deletion and automatically
	// call UpdatePackMappingsInfo() which rebuilds the manifest from the remaining files.
	// This ensures the manifest stays in sync with the file system through the proper workflow.

	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Completed deletion of Language Module: %s. Watcher will update manifest."), *ModuleID);
}

bool FEditorModuleDeleter::IsFileSharedByMultipleModules(const FString& MD5, const FString& ModuleJsonString, int32& OutUsageCount)
{
	using Thespeon::Core::IO::RuntimeFileLoader;

	FString ManifestPath = RuntimeFileLoader::GetManifestPath();
	FString ManifestString = RuntimeFileLoader::LoadFileAsString(ManifestPath);

	if (ManifestString.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> ManifestObj;
	TSharedRef<TJsonReader<>> ManifestReader = TJsonReaderFactory<>::Create(ManifestString);

	if (!FJsonSerializer::Deserialize(ManifestReader, ManifestObj) || !ManifestObj.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* FileUsagePtr = nullptr;
	if (!ManifestObj->TryGetObjectField(TEXT("file_usage"), FileUsagePtr) || !FileUsagePtr)
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* UsersArray = nullptr;
	if (!(*FileUsagePtr)->TryGetArrayField(MD5, UsersArray) || !UsersArray)
	{
		return false;
	}

	OutUsageCount = UsersArray->Num();
	return OutUsageCount > 1;
}

FString FEditorModuleDeleter::GetFilenameFromMD5(const FString& MD5, const FString& ModuleJsonString)
{
	if (ModuleJsonString.IsEmpty())
	{
		return FString();
	}

	TSharedPtr<FJsonObject> ModuleJsonObj;
	TSharedRef<TJsonReader<>> ModJsonReader = TJsonReaderFactory<>::Create(ModuleJsonString);

	if (!FJsonSerializer::Deserialize(ModJsonReader, ModuleJsonObj) || !ModuleJsonObj.IsValid())
	{
		return FString();
	}

	const TSharedPtr<FJsonObject>* ConfigFilesPtr = nullptr;
	if (!ModuleJsonObj->TryGetObjectField(TEXT("files"), ConfigFilesPtr) || !ConfigFilesPtr)
	{
		return FString();
	}

	const TSharedPtr<FJsonValue>* FileEntryPtr = (*ConfigFilesPtr)->Values.Find(MD5);
	if (!FileEntryPtr || !(*FileEntryPtr)->AsObject().IsValid())
	{
		return FString();
	}

	TSharedPtr<FJsonObject> FileEntryObj = (*FileEntryPtr)->AsObject();
	FString FileName;
	FileEntryObj->TryGetStringField(TEXT("filename"), FileName);

	return FileName;
}

void FEditorModuleDeleter::CollectLanguageModuleFiles(const FString& ModuleID, const FString& ModuleJsonString,
                                                          TArray<FString>& OutFilesToDelete, TArray<FString>& OutAssetPaths, TSet<FString>& OutMD5s)
{
	using Thespeon::Core::IO::RuntimeFileLoader;

	if (ModuleJsonString.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> ModuleJsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ModuleJsonString);

	if (!FJsonSerializer::Deserialize(Reader, ModuleJsonObject) || !ModuleJsonObject.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* ConfigFilesPtr = nullptr;
	if (!ModuleJsonObject->TryGetObjectField(TEXT("files"), ConfigFilesPtr) || !ConfigFilesPtr)
	{
		return;
	}

	const TSharedPtr<FJsonObject>& ConfigFiles = *ConfigFilesPtr;

	const TArray<TSharedPtr<FJsonValue>>* ModulesArray = nullptr;
	if (!ModuleJsonObject->TryGetArrayField(TEXT("modules"), ModulesArray))
	{
		return;
	}

	// Find our module in the modules array
	for (const auto& ModuleValue : *ModulesArray)
	{
		TSharedPtr<FJsonObject> ModuleObj = ModuleValue->AsObject();
		if (!ModuleObj.IsValid())
		{
			continue;
		}

		FString ModuleBaseID;
		if (!ModuleObj->TryGetStringField(TEXT("base_module_id"), ModuleBaseID) || ModuleBaseID != ModuleID)
		{
			continue;
		}

		// Found our module - get its files
		const TSharedPtr<FJsonObject>* ModuleFilesPtr = nullptr;
		if (!ModuleObj->TryGetObjectField(TEXT("files"), ModuleFilesPtr) || !ModuleFilesPtr)
		{
			break;
		}

		for (const auto& Pair : (*ModuleFilesPtr)->Values)
		{
			FString MD5 = Pair.Value->AsString();
			OutMD5s.Add(MD5);

			const TSharedPtr<FJsonValue>* FileEntryPtr = ConfigFiles->Values.Find(MD5);
			if (!FileEntryPtr || !(*FileEntryPtr)->AsObject().IsValid())
			{
				continue;
			}

			TSharedPtr<FJsonObject> FileEntryObj = (*FileEntryPtr)->AsObject();
			FString FileName;
			if (!FileEntryObj->TryGetStringField(TEXT("filename"), FileName))
			{
				continue;
			}

			if (FPaths::GetExtension(FileName).Equals(TEXT("onnx"), ESearchCase::IgnoreCase))
			{
				// ONNX files are imported as .uasset in Content directory
				FString BaseFileName = FPaths::GetBaseFilename(FileName);
				FString UAssetPath = FPaths::Combine(
					FPaths::ProjectContentDir(),
					TEXT("LingotionThespeon/RuntimeData"),
					BaseFileName + TEXT(".uasset")
				);
				OutFilesToDelete.Add(UAssetPath);
				OutAssetPaths.Add(TEXT("/Game/LingotionThespeon/RuntimeData/") + BaseFileName);
			}
			else
			{
				// Other files (like lookuptable.json) are in RuntimeData
				FString FilePath = RuntimeFileLoader::GetRuntimeFilePath(FileName);
				OutFilesToDelete.Add(FilePath);
			}

			LINGO_LOG(EVerbosityLevel::Debug, TEXT("Marked for deletion: %s"), *FileName);
		}
		break;
	}
}
