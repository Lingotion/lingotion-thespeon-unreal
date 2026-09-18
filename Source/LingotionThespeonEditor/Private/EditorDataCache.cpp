// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "EditorDataCache.h"
#include "Core/LingotionLogger.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FCriticalSection& FEditorDataCache::GetLock()
{
	static FCriticalSection Lock;
	return Lock;
}

FString FEditorDataCache::GetCacheFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Lingotion"), TEXT("Thespeon.datacache"));
}

FEditorDataCache::FSynthData FEditorDataCache::LoadFromFile()
{
	FSynthData Result;

	const FString Path = GetCacheFilePath();
	if (!IFileManager::Get().FileExists(*Path))
	{
		return Result;
	}

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path) || Json.IsEmpty())
	{
		return Result;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Failed to parse data cache at %s; treating as empty."), *Path);
		return Result;
	}

	for (const auto& ModulePair : Root->Values)
	{
		const TSharedPtr<FJsonObject>* InnerObj = nullptr;
		if (!ModulePair.Value.IsValid() || !ModulePair.Value->TryGetObject(InnerObj) || !InnerObj)
		{
			continue;
		}
		TMap<FString, double>& Inner = Result.Data.FindOrAdd(FString(*ModulePair.Key));
		for (const auto& KeyPair : (*InnerObj)->Values)
		{
			double Value = 0.0;
			if (KeyPair.Value.IsValid() && KeyPair.Value->TryGetNumber(Value))
			{
				Inner.Add(FString(*KeyPair.Key), Value);
			}
		}
	}

	return Result;
}

void FEditorDataCache::SaveToFile(const FSynthData& Data)
{
	const FString Path = GetCacheFilePath();

	// Empty payload => remove the file entirely rather than leave a stale "{}".
	if (Data.Data.Num() == 0)
	{
		if (IFileManager::Get().FileExists(*Path))
		{
			IFileManager::Get().Delete(*Path);
		}
		return;
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	for (const TPair<FString, TMap<FString, double>>& ModulePair : Data.Data)
	{
		const TSharedRef<FJsonObject> Inner = MakeShared<FJsonObject>();
		for (const TPair<FString, double>& KeyPair : ModulePair.Value)
		{
			Inner->SetNumberField(KeyPair.Key, KeyPair.Value);
		}
		Root->SetObjectField(ModulePair.Key, Inner);
	}

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);

	// Ensure the containing directory exists before writing.
	const FString Dir = FPaths::GetPath(Path);
	if (!IFileManager::Get().DirectoryExists(*Dir))
	{
		IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
	}

	if (!FFileHelper::SaveStringToFile(Out, *Path))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to write data cache to %s"), *Path);
	}
}

void FEditorDataCache::AddToCache(const FSynthData& Data)
{
	if (Data.Data.Num() == 0)
	{
		return;
	}

	FScopeLock ScopeLock(&GetLock());
	FSynthData Merged = LoadFromFile();
	for (const TPair<FString, TMap<FString, double>>& ModulePair : Data.Data)
	{
		TMap<FString, double>& Inner = Merged.Data.FindOrAdd(ModulePair.Key);
		for (const TPair<FString, double>& KeyPair : ModulePair.Value)
		{
			Inner.FindOrAdd(KeyPair.Key) += KeyPair.Value;
		}
	}
	SaveToFile(Merged);
}

FEditorDataCache::FSynthData FEditorDataCache::Snapshot()
{
	FScopeLock ScopeLock(&GetLock());
	return LoadFromFile();
}

TSharedPtr<FJsonObject> FEditorDataCache::ToJsonObject(const FSynthData& Data)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	for (const TPair<FString, TMap<FString, double>>& ModulePair : Data.Data)
	{
		const TSharedRef<FJsonObject> Inner = MakeShared<FJsonObject>();
		for (const TPair<FString, double>& KeyPair : ModulePair.Value)
		{
			Inner->SetNumberField(KeyPair.Key, KeyPair.Value);
		}
		Root->SetObjectField(ModulePair.Key, Inner);
	}
	return Root;
}

void FEditorDataCache::SubtractAndPersist(const FSynthData& Sent)
{
	FScopeLock ScopeLock(&GetLock());
	FSynthData Remainder = LoadFromFile();

	for (const TPair<FString, TMap<FString, double>>& ModulePair : Sent.Data)
	{
		TMap<FString, double>* Inner = Remainder.Data.Find(ModulePair.Key);
		if (!Inner)
		{
			continue;
		}
		for (const TPair<FString, double>& KeyPair : ModulePair.Value)
		{
			if (double* Current = Inner->Find(KeyPair.Key))
			{
				*Current -= KeyPair.Value;
				// Remove keys drained to (approximately) zero; guards against FP drift.
				if (*Current <= 1e-6)
				{
					Inner->Remove(KeyPair.Key);
				}
			}
		}
		if (Inner->Num() == 0)
		{
			Remainder.Data.Remove(ModulePair.Key);
		}
	}

	SaveToFile(Remainder);
}
