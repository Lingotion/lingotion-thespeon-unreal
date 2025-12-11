// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UEditorPackWatcher.generated.h"

UCLASS()
class UEditorPackWatcher : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    FDelegateHandle DirectoryChangedHandle;

    void VerifyRuntimeFiles();
    void UpdatePackMappingsInfo();
    void OnDirectoryChanged(const TArray<struct FFileChangeData>& FileChanges);
};
