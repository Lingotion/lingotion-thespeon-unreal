// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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
