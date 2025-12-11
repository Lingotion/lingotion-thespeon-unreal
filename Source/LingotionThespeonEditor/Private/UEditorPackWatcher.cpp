// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "UEditorPackWatcher.h"
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
#include "Core/PackManifestHandler.h"

using Thespeon::Core::IO::RuntimeFileLoader;

IDirectoryWatcher* _watcher;

void UEditorPackWatcher::VerifyRuntimeFiles()
{
    IFileManager& FM = IFileManager::Get();
    FString runtimeDir = RuntimeFileLoader::GetRuntimeFileDir();
    if(!FM.DirectoryExists(*runtimeDir))
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("Missing directory %s, creating..."), *runtimeDir);
        FM.MakeDirectory(*runtimeDir, true);
    }
}

void UEditorPackWatcher::UpdatePackMappingsInfo()
{
    VerifyRuntimeFiles();

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> ActorModules = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> LanguageModules = MakeShared<FJsonObject>();

    // Find all .json files and validate them as actorpacks or languagepacks
    IFileManager& FM = IFileManager::Get();
    TArray<FString> JsonFilesToCheck;
    FM.FindFilesRecursive(JsonFilesToCheck, *RuntimeFileLoader::GetRuntimeFileDir(), TEXT("*.json"), /*Files=*/true, /*Directories=*/false);
    if(JsonFilesToCheck.Num() == 0) return;

    FString JsonStr;
    TSet<FString> PackNames;
    for(const FString& File : JsonFilesToCheck )
    {
        JsonStr = RuntimeFileLoader::LoadFileAsString(File);
        if (!JsonStr.IsEmpty())

        {
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
            TSharedPtr<FJsonObject> Contents;

            if (FJsonSerializer::Deserialize(Reader, Contents) && Contents.IsValid())
            {
                FString Type;
                if(Contents->TryGetStringField(TEXT("type"), Type))
                {
                    if(Type == "ACTORPACK")
                    {
                        TArray<TSharedPtr<FJsonValue>> Modules = Contents->GetArrayField(TEXTVIEW("modules"));
                        FString PackName = Contents->GetStringField(TEXT("name"));
                        PackNames.Add(PackName);
                        for(const auto& Module : Modules)
                        {
                            TSharedPtr<FJsonObject> ModuleObj = Module->AsObject();
                            FString Identifier = ModuleObj->GetStringField(TEXT("actorpack_base_module_id"));
                            FString ModuleName = ModuleObj->GetStringField(TEXT("name"));
                            
                            TSharedRef<FJsonObject> ModuleResultObj = MakeShared<FJsonObject>();
                            ModuleResultObj->SetStringField(TEXT("modulename"), ModuleName);
                            ModuleResultObj->SetStringField(TEXT("packname"), PackName);
                            ModuleResultObj->SetStringField(TEXT("jsonpath"), FPaths::GetCleanFilename(File));
                            FString QualityLevel = ModuleObj->GetObjectField(TEXT("tags"))->GetStringField(TEXT("quality"));
                            
                            ModuleResultObj->SetStringField(TEXT("quality"), QualityLevel);
                            
                            // Create languages list for module
                            TArray<TSharedPtr<FJsonValue>> Languages = ModuleObj->GetObjectField(TEXT("language_options"))
                            ->GetArrayField(TEXTVIEW("languages"));
                            TArray<TSharedPtr<FJsonValue>> ResultingLangObjs;

                            for(const auto& Language : Languages)
                            {
                                TSharedRef<FJsonObject> ResultingLangObj = MakeShared<FJsonObject>();
                                // Add iso639_2 as key in the required languages
                                TSharedPtr<FJsonObject> Lang = Language->AsObject();
                                FString LangKey = Lang->GetStringField(TEXT("iso639_2"));
                                
                                FString NameInEnglish = Lang->GetObjectField(TEXT("languagecode"))->GetStringField(TEXT("nameinenglish"));
                                FString Dialect = Lang->GetStringField(TEXT("iso3166_1"));
                                
                                ResultingLangObj->SetStringField(TEXT("iso639_2"), LangKey);
                                ResultingLangObj->SetStringField(TEXT("iso3166_1"), Dialect);
                                ResultingLangObj->SetStringField(TEXT("nameinenglish"), NameInEnglish);
                                ResultingLangObjs.Add(MakeShared<FJsonValueObject>(ResultingLangObj));
                            }
                            ModuleResultObj->SetArrayField(TEXT("languages"), ResultingLangObjs);

                            // Create actors list
                            TArray<TSharedPtr<FJsonValue>> Actors = ModuleObj->GetObjectField(TEXT("actor_options"))
                            ->GetArrayField(TEXTVIEW("actors"));
                            
                            TArray<TSharedPtr<FJsonValue>> ResultingActors;
                            for(const auto& Actor : Actors)
                            {
                                // Add iso639_2 as key in the required languages
                                TSharedPtr<FJsonObject> ActorObj = Actor->AsObject();
                                FString UserName = ActorObj->GetStringField(TEXT("username"));
                                ResultingActors.Add(MakeShared<FJsonValueString>(UserName));
                            }
                            
                            ModuleResultObj->SetArrayField(TEXT("actors"), ResultingActors);
                            ActorModules->SetObjectField(Identifier, ModuleResultObj);
                        }
                    
                    }
                    else if(Type == "LANGUAGEPACK")
                    {
                        TArray<TSharedPtr<FJsonValue>> Modules = Contents->GetArrayField(TEXTVIEW("modules"));
                        FString PackName = Contents->GetStringField(TEXT("name"));
                        PackNames.Add(PackName);
                        for(const auto& Module : Modules)
                        {
                            TSharedPtr<FJsonObject> ModuleObj = Module->AsObject();
                            FString Identifier = ModuleObj->GetStringField(TEXT("base_module_id"));
                            FString ModuleName = ModuleObj->GetStringField(TEXT("name"));
                            
                            TSharedRef<FJsonObject> ModuleResultObj = MakeShared<FJsonObject>();
                            ModuleResultObj->SetStringField(TEXT("modulename"), ModuleName);
                            ModuleResultObj->SetStringField(TEXT("packname"), PackName);
                            ModuleResultObj->SetStringField(TEXT("jsonpath"), FPaths::GetCleanFilename(File));
                            
                            // Create languages list for module
                            TArray<TSharedPtr<FJsonValue>> Languages = ModuleObj->GetArrayField(TEXTVIEW("languages"));
                            TArray<TSharedPtr<FJsonValue>> ResultingLangObjs;

                            for(const auto& Language : Languages)
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
                    }
                }
            }
        }
    }
    
    
    Root->SetObjectField(TEXT("actorpack_modules"), ActorModules);
    Root->SetObjectField(TEXT("languagepack_modules"), LanguageModules);
    TArray<TSharedPtr<FJsonValue>> PackNameJson;
    for (const FString& packName : PackNames)
    {
        PackNameJson.Add(MakeShared<FJsonValueString>(packName));
    }
    Root->SetArrayField(TEXT("imported_packs"), PackNameJson);
    
    // Build file usage map: MD5 -> array of module IDs using that file
    TSharedRef<FJsonObject> FileUsage = MakeShared<FJsonObject>();
    TMap<FString, TArray<FString>> MD5ToModules; // Maps MD5 hash to list of module IDs
    
    // Scan all JSON files again to build file usage map
    for(const FString& File : JsonFilesToCheck)
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
        
        FString Type;
        if (!Contents->TryGetStringField(TEXT("type"), Type))
        {
            continue;
        }
        
        const TSharedPtr<FJsonObject>* ConfigFilesPtr = nullptr;
        if (!Contents->TryGetObjectField(TEXT("files"), ConfigFilesPtr) || !ConfigFilesPtr)
        {
            continue;
        }
        
        const TArray<TSharedPtr<FJsonValue>>* ModulesArray = nullptr;
        if (!Contents->TryGetArrayField(TEXT("modules"), ModulesArray))
        {
            continue;
        }
        
        for(const auto& Module : *ModulesArray)
        {
            TSharedPtr<FJsonObject> ModuleObj = Module->AsObject();
            if (!ModuleObj.IsValid())
            {
                continue;
            }
            
            FString ModuleID;
            if (Type == "ACTORPACK")
            {
                ModuleObj->TryGetStringField(TEXT("actorpack_base_module_id"), ModuleID);
            }
            else if (Type == "LANGUAGEPACK")
            {
                ModuleObj->TryGetStringField(TEXT("base_module_id"), ModuleID);
            }
            
            if (ModuleID.IsEmpty())
            {
                continue;
            }
            
            // Get module's files - try 'files' first, then 'onnxfiles'
            const TSharedPtr<FJsonObject>* ModuleFilesPtr = nullptr;
            if (!ModuleObj->TryGetObjectField(TEXT("files"), ModuleFilesPtr) || !ModuleFilesPtr)
            {
                ModuleObj->TryGetObjectField(TEXT("onnxfiles"), ModuleFilesPtr);
            }
            
            if (!ModuleFilesPtr)
            {
                continue;
            }
            
            for (const auto& FilePair : (*ModuleFilesPtr)->Values)
            {
                FString MD5 = FilePair.Value->AsString();
                if (MD5.IsEmpty())
                {
                    continue;
                }
                
                if (!MD5ToModules.Contains(MD5))
                {
                    MD5ToModules.Add(MD5, TArray<FString>());
                }
                MD5ToModules[MD5].AddUnique(ModuleID);
            }
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
    FJsonSerializer::Serialize(Root, Writer);    FString manifestFilePath = RuntimeFileLoader::GetManifestPath();
    FFileHelper::SaveStringToFile(JsonStr, *manifestFilePath);
    UPackManifestHandler::Get()->ReadPackManifest();
}

void UEditorPackWatcher::OnDirectoryChanged(const TArray<struct FFileChangeData>& FileChanges)
{

        // filter changes based on type
        for (const FFileChangeData& changeData : FileChanges)
        {
            
            switch (changeData.Action)
            {
            case FFileChangeData::EFileChangeAction::FCA_Added:
                UpdatePackMappingsInfo();
                break;
            
            case FFileChangeData::EFileChangeAction::FCA_Removed:
                UpdatePackMappingsInfo();
                break;
            default:
                break;
            }
        }
        
}

void UEditorPackWatcher::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UpdatePackMappingsInfo();

    // find watcher 
    static FDirectoryWatcherModule &DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
    _watcher = DirectoryWatcherModule.Get();
    if(_watcher)
    {
        _watcher->RegisterDirectoryChangedCallback_Handle(
            *RuntimeFileLoader::GetRuntimeFileDir(), 
            IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UEditorPackWatcher::OnDirectoryChanged),
            DirectoryChangedHandle
        );
    } else
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Watcher initialization failed."));
    }
}

void UEditorPackWatcher::Deinitialize()
{
    // Unregister the directory watcher callback if the handle is valid
    if (_watcher && DirectoryChangedHandle.IsValid())
    {
        _watcher->UnregisterDirectoryChangedCallback_Handle(
            *Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeFileDir(),
            DirectoryChangedHandle
        );
        DirectoryChangedHandle.Reset();
    }

}
