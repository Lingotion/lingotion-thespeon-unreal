// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once
#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Editor.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "UEditorFileWatcher.generated.h"

UCLASS()
class UEditorFileWatcher : public UEditorSubsystem
{
	GENERATED_BODY()
  public:
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
	void UpdateMappingsInfo();
	static UEditorFileWatcher* Get()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UEditorFileWatcher>() : nullptr;
	}

  private:
	FDelegateHandle DirectoryChangedHandle;
	TArray<FString> CachedModuleFiles;
	void VerifyRuntimeFiles();
	void ProcessCharacterModule(
	    const TSharedPtr<FJsonObject>& Contents, TSet<FString>& Names, const FString& File, TSharedRef<FJsonObject>& CharacterModules
	);
	void ProcessLanguageModule(
	    const TSharedPtr<FJsonObject>& Contents, TSet<FString>& Names, const FString& File, TSharedRef<FJsonObject>& LanguageModules
	);
	void OnDirectoryChanged(const TArray<struct FFileChangeData>& FileChanges);
};
