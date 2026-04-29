// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "EditorModuleDeleter.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "HAL/FileManager.h"
#include "Core/LingotionLogger.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Core/ManifestHandler.h"
#include "EditorAssetLibrary.h"

void FEditorModuleDeleter::DeleteModule(const FString& JsonFileName)
{
	using Thespeon::Core::IO::RuntimeFileLoader;
	IFileManager& FM = IFileManager::Get();
	FString JsonFilePath = RuntimeFileLoader::GetRuntimeFilePath(JsonFileName);
	FString JsonStr = RuntimeFileLoader::LoadFileAsString(JsonFilePath);
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	TSharedPtr<FJsonObject> Contents;
	auto* Manifest = UManifestHandler::Get();
	if (!Manifest)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ManifestHandler instance is null"));
		return;
	}
	if (FJsonSerializer::Deserialize(Reader, Contents) && Contents.IsValid())
	{
		TArray<FString> FilesToDelete;

		// Get module files
		const TArray<TSharedPtr<FJsonValue>>* ModuleFilesPtr = nullptr;
		if (!Contents->TryGetArrayField(TEXT("files"), ModuleFilesPtr) || !ModuleFilesPtr)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Cannot delete module due to invalid config file: %s"), *JsonFileName);
			return;
		}

		for (const TSharedPtr<FJsonValue>& ModuleFileEntry : *ModuleFilesPtr)
		{
			const TSharedPtr<FJsonObject> FileObj = ModuleFileEntry->AsObject();
			if (!FileObj.IsValid())
			{
				continue;
			}

			FString MD5;
			FString Extension;

			if (!FileObj->TryGetStringField(TEXT("md5"), MD5) || MD5.IsEmpty())
			{
				continue;
			}
			if (!FileObj->TryGetStringField(TEXT("extension"), Extension) || Extension.IsEmpty())
			{
				continue;
			}

			if (Manifest->IsFileShared(MD5))
			{
				continue;
			}

			// file locations differ depending on their extension
			FString FilePath;
			if (Extension == TEXT("onnx"))
			{
				FilePath = RuntimeFileLoader::GetRuntimeModelPath(MD5);
				if (UEditorAssetLibrary::DoesAssetExist(FilePath))
				{
					if (UEditorAssetLibrary::DeleteAsset(FilePath))
					{
						LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Deleted asset: %s"), *FilePath);
					}
					else
					{
						LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to delete asset: %s"), *FilePath);
					}
				}
			}
			else
			{
				FilePath = RuntimeFileLoader::GetRuntimeFilePath(MD5 + "." + Extension);
				if (FM.FileExists(*FilePath))
				{
					if (FM.Delete(*FilePath, false, true))
					{
						LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Deleted file: %s"), *FilePath);
					}
					else
					{
						LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to delete file: %s"), *FilePath);
					}
				}
			}
		}

		if (FM.Delete(*JsonFilePath, false, true))
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Deleted file: %s"), *JsonFilePath);
		}
		else
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to delete file: %s"), *JsonFilePath);
		}
	}
}
