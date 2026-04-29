// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "UEditorFileWatcher.h"
#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "JsonObjectConverter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#include "Modules/ModuleManager.h"
#include "Core/LingotionLogger.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "Core/ManifestHandler.h"

using Thespeon::Core::IO::RuntimeFileLoader;

IDirectoryWatcher* _watcher;

void UEditorFileWatcher::VerifyRuntimeFiles()
{
	IFileManager& FM = IFileManager::Get();
	const FString& runtimeDir = RuntimeFileLoader::GetRuntimeFileDir();
	if (!FM.DirectoryExists(*runtimeDir))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Missing directory %s, creating..."), *runtimeDir);
		FM.MakeDirectory(*runtimeDir, true);
	}
}

void UEditorFileWatcher::UpdateMappingsInfo()
{
	VerifyRuntimeFiles();

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> CharacterModules = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> LanguageModules = MakeShared<FJsonObject>();

	// Find all .json files and validate them as character or language
	IFileManager& FM = IFileManager::Get();
	TArray<FString> JsonFilesToCheck;
	FM.FindFilesRecursive(JsonFilesToCheck, *RuntimeFileLoader::GetRuntimeFileDir(), TEXT("*.json"), /*Files=*/true, /*Directories=*/false);

	if (JsonFilesToCheck == CachedModuleFiles)
	{
		return;
	}
	CachedModuleFiles = JsonFilesToCheck;

	if (JsonFilesToCheck.Num() == 0)
	{
		return;
	}

	FString JsonStr;
	TSet<FString> Names;
	for (const FString& File : JsonFilesToCheck)
	{
		JsonStr = RuntimeFileLoader::LoadFileAsString(File);
		if (!JsonStr.IsEmpty())

		{
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
			TSharedPtr<FJsonObject> Contents;

			if (FJsonSerializer::Deserialize(Reader, Contents) && Contents.IsValid())
			{
				FString Type;
				if (Contents->TryGetStringField(TEXT("type"), Type))
				{
					if (Type == "lara")
					{
						ProcessCharacterModule(Contents, Names, File, CharacterModules);
					}
					else if (Type == "phonemizer")
					{
						ProcessLanguageModule(Contents, Names, File, LanguageModules);
					}
					else
					{
						LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Invalid config type found, ignoring."));
						continue;
					}
				}
			}
		}
	}

	Root->SetObjectField(TEXT("character_modules"), CharacterModules);
	Root->SetObjectField(TEXT("language_modules"), LanguageModules);
	TArray<TSharedPtr<FJsonValue>> ImportedModules;
	for (const FString& Name : Names)
	{
		ImportedModules.Add(MakeShared<FJsonValueString>(Name));
	}
	Root->SetArrayField(TEXT("imported"), ImportedModules);

	// Build file usage map: MD5 -> array of module IDs using that file
	TSharedRef<FJsonObject> FileUsage = MakeShared<FJsonObject>();
	TMap<FString, TArray<FString>> MD5ToModules; // Maps MD5 hash to list of module IDs

	// Scan all JSON files again to build file usage map
	for (const FString& File : JsonFilesToCheck)
	{
		JsonStr = RuntimeFileLoader::LoadFileAsString(File);
		if (JsonStr.IsEmpty())
		{
			continue;
		}

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
		TSharedPtr<FJsonObject> Contents;
		if (!FJsonSerializer::Deserialize(Reader, Contents) || !Contents.IsValid())
		{
			continue;
		}

		FString ModuleID;
		Contents->TryGetStringField(TEXT("source_id"), ModuleID);

		if (ModuleID.IsEmpty())
		{
			continue;
		}

		// Get module files
		const TArray<TSharedPtr<FJsonValue>>* ModuleFilesPtr = nullptr;
		if (!Contents->TryGetArrayField(TEXT("files"), ModuleFilesPtr) || !ModuleFilesPtr)
		{
			LINGO_LOG(
			    EVerbosityLevel::Error,
			    TEXT("Module: %s is malformed. Try re-downloading and re-importing .lingotion files or contact support if the issue persists."),
			    *ModuleID
			);
			continue;
		}

		for (const auto& ModuleFileEntry : *ModuleFilesPtr)
		{
			TSharedPtr<FJsonObject> FileObj = ModuleFileEntry->AsObject();
			FString MD5 = FileObj->GetStringField(TEXT("md5"));

			if (!MD5ToModules.Contains(MD5))
			{
				MD5ToModules.Add(MD5, TArray<FString>());
			}
			MD5ToModules[MD5].AddUnique(ModuleID);
		}
	}

	// Convert map to JSON format
	for (const auto& Pair : MD5ToModules)
	{
		TArray<TSharedPtr<FJsonValue>> ModuleArray;
		for (const FString& ModuleID : Pair.Value)
		{
			ModuleArray.Add(MakeShared<FJsonValueString>(ModuleID));
		}
		FileUsage->SetArrayField(Pair.Key, ModuleArray);
	}

	Root->SetObjectField(TEXT("file_usage"), FileUsage);

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
	FJsonSerializer::Serialize(Root, Writer);
	const FString& manifestFilePath = RuntimeFileLoader::GetManifestPath();
	FFileHelper::SaveStringToFile(JsonStr, *manifestFilePath);
	UManifestHandler::Get()->ReadManifest();
}

void UEditorFileWatcher::ProcessCharacterModule(
    const TSharedPtr<FJsonObject>& Contents, TSet<FString>& Names, const FString& File, TSharedRef<FJsonObject>& CharacterModules
)
{
	FString Name = Contents->GetStringField(TEXT("name"));
	Names.Add(Name);
	FString Identifier = Contents->GetStringField(TEXTVIEW("source_id"));

	TSharedRef<FJsonObject> ModuleResultObj = MakeShared<FJsonObject>();
	ModuleResultObj->SetStringField(TEXT("name"), Name);
	ModuleResultObj->SetStringField(TEXT("jsonpath"), FPaths::GetCleanFilename(File));
	FString QualityLevel = Contents->GetObjectField(TEXT("tags"))->GetStringField(TEXT("module_type"));

	ModuleResultObj->SetStringField(TEXT("quality"), QualityLevel);

	// Create languages list for module
	TArray<TSharedPtr<FJsonValue>> Languages = Contents->GetArrayField(TEXTVIEW("languages"));
	TArray<TSharedPtr<FJsonValue>> ResultingLangObjs;

	for (const auto& Language : Languages)
	{
		TSharedRef<FJsonObject> ResultingLangObj = MakeShared<FJsonObject>();
		// Add iso639_2 as key in the required languages
		TSharedPtr<FJsonObject> Lang = Language->AsObject();
		FString LangKey = Lang->GetStringField(TEXT("iso639_2"));

		FString NameInEnglish = Lang->GetStringField(TEXT("nameinenglish"));
		FString Dialect = Lang->GetStringField(TEXT("iso3166_1"));

		ResultingLangObj->SetStringField(TEXT("iso639_2"), LangKey);
		ResultingLangObj->SetStringField(TEXT("iso3166_1"), Dialect);
		ResultingLangObj->SetStringField(TEXT("nameinenglish"), NameInEnglish);
		ResultingLangObjs.Add(MakeShared<FJsonValueObject>(ResultingLangObj));
	}
	ModuleResultObj->SetArrayField(TEXT("languages"), ResultingLangObjs);

	// Create characters list
	TArray<TSharedPtr<FJsonValue>> ResultingCharacters;
	FString UserName = Contents->GetObjectField(TEXT("character"))->GetStringField(TEXT("charactername"));
	ResultingCharacters.Add(MakeShared<FJsonValueString>(UserName));

	ModuleResultObj->SetArrayField(TEXT("characters"), ResultingCharacters);
	CharacterModules->SetObjectField(Identifier, ModuleResultObj);
}

void UEditorFileWatcher::ProcessLanguageModule(
    const TSharedPtr<FJsonObject>& Contents, TSet<FString>& Names, const FString& File, TSharedRef<FJsonObject>& LanguageModules
)
{
	FString Name = Contents->GetStringField(TEXT("name"));
	Names.Add(Name);
	FString Identifier = Contents->GetStringField(TEXT("base_module_id"));

	TSharedRef<FJsonObject> ModuleResultObj = MakeShared<FJsonObject>();
	ModuleResultObj->SetStringField(TEXT("name"), Name);
	ModuleResultObj->SetStringField(TEXT("jsonpath"), FPaths::GetCleanFilename(File));

	// Create languages list for module
	TArray<TSharedPtr<FJsonValue>> Languages = Contents->GetArrayField(TEXTVIEW("languages"));
	TArray<TSharedPtr<FJsonValue>> ResultingLangObjs;

	for (const auto& Language : Languages)
	{
		TSharedRef<FJsonObject> ResultingLangObj = MakeShared<FJsonObject>();
		// Add iso639_2 as key in the required languages
		TSharedPtr<FJsonObject> Lang = Language->AsObject();
		FString LangKey = Lang->GetStringField(TEXT("iso639_2"));

		FString NameInEnglish = Lang->GetStringField(TEXT("nameinenglish"));

		ResultingLangObj->SetStringField(TEXT("iso639_2"), LangKey);
		ResultingLangObj->SetStringField(TEXT("nameinenglish"), NameInEnglish);
		ResultingLangObjs.Add(MakeShared<FJsonValueObject>(ResultingLangObj));
	}
	ModuleResultObj->SetArrayField(TEXT("languages"), ResultingLangObjs);

	LanguageModules->SetObjectField(Identifier, ModuleResultObj);
}

void UEditorFileWatcher::OnDirectoryChanged(const TArray<struct FFileChangeData>& FileChanges)
{

	// filter changes based on type
	for (const FFileChangeData& changeData : FileChanges)
	{

		switch (changeData.Action)
		{
			case FFileChangeData::EFileChangeAction::FCA_Added:
			case FFileChangeData::EFileChangeAction::FCA_Removed:
				UpdateMappingsInfo();
				break;
			default:
				break;
		}
	}
}

void UEditorFileWatcher::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UpdateMappingsInfo();

	// find watcher
	static FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	_watcher = DirectoryWatcherModule.Get();
	if (_watcher)
	{
		_watcher->RegisterDirectoryChangedCallback_Handle(
		    *RuntimeFileLoader::GetRuntimeFileDir(),
		    IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UEditorFileWatcher::OnDirectoryChanged),
		    DirectoryChangedHandle
		);
	}
	else
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Watcher initialization failed."));
	}
}

void UEditorFileWatcher::Deinitialize()
{
	// Unregister the directory watcher callback if the handle is valid
	if (_watcher && DirectoryChangedHandle.IsValid())
	{
		_watcher->UnregisterDirectoryChangedCallback_Handle(*Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeFileDir(), DirectoryChangedHandle);
		DirectoryChangedHandle.Reset();
	}
}
