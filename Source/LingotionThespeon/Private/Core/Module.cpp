// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Core/Module.h"
#include "Core/LingotionLogger.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "UObject/SoftObjectPath.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"

namespace Thespeon
{
    namespace Core
    {
        Module::Module(const Thespeon::Core::FModuleEntry& Entry)
            : ModuleID(Entry.ModuleID)
            , JSONPath(Entry.JsonPath)
            , Version(Entry.Version)
        {
            // Validate module entry parameters
            if (Entry.ModuleID.IsEmpty() || Entry.JsonPath.IsEmpty())
            {
                LINGO_LOG(EVerbosityLevel::Warning, TEXT("Module entry parameter is invalid: ModuleID or JsonPath is empty"));
            }
        }

        
        // Like CreateRuntimeBindings Loads onnx from disc and creates NNEModelData for each and returns. To be used by WorkloadManager to create workloads.
        // Obs only builds and returns models that are not already in LoadedMD5s to avoid duplicates in workload manager.
        bool Module::LoadModels(const TSet<FString>& LoadedMD5s, TMap<FString, TStrongObjectPtr<UNNEModelData>>& OutLoadedModels) const
        {
            TArray<FString> Md5s;
            TArray<FSoftObjectPath> SoftPaths;
            Md5s.Reserve(InternalFileMappings.Num());
            SoftPaths.Reserve(InternalFileMappings.Num());

            for (const auto& FilePair : InternalFileMappings)
            {
                if (FilePair.Key.Equals(TEXT("lookuptable"), ESearchCase::IgnoreCase) || LoadedMD5s.Contains(FilePair.Value.MD5))
                {
                    continue;
                }
                const FString ModelAssetPath = Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeModelPath(FPaths::GetBaseFilename(FilePair.Value.FileName));
                Md5s.Add(FilePair.Value.MD5);
                SoftPaths.Add(FSoftObjectPath(ModelAssetPath));
            }

            if (SoftPaths.Num() == 0)
            {
                return true; // Nothing to load, all models already loaded
            }

            TArray<TStrongObjectPtr<UNNEModelData>> LoadedArray;
            if(!Module::LoadModelsAsync(SoftPaths, LoadedArray))
            {
                LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to load models for module: %s"), *ModuleID);
                return false;
            }
            for (int32 i = 0; i < Md5s.Num(); ++i)
            {
                if (LoadedArray.IsValidIndex(i) && LoadedArray[i].IsValid())
                {
                    OutLoadedModels.Add(Md5s[i], MoveTemp(LoadedArray[i]));
                }
            }
            return true;
        }

        FString Module::GetInternalModelID(const FString& InternalName) const
        {
            const FString* ModelMD5 = InternalModelMappings.Find(InternalName);
            if (!ModelMD5)
            {
                LINGO_LOG(EVerbosityLevel::Warning, TEXT("Internal module not found: %s"), *InternalName);
                return FString(); // Return empty string instead of throwing exception in Unreal
            }
            return *ModelMD5;
        }

        // Helper method to dispatch the task of loading models asynchronously on the game thread and wait for completion.
        // Called from worker thread and stalls the thread until complete.
        bool Module::LoadModelsAsync(const TArray<FSoftObjectPath>& SoftPaths, TArray<TStrongObjectPtr<UNNEModelData>>& OutLoadedModels) const
        {
            if (SoftPaths.Num() == 0)
            {
                return true; // Nothing to load, that is okay (means everything is already loaded)
            }

            auto Results = MakeShared<TArray<TStrongObjectPtr<UNNEModelData>>, ESPMode::ThreadSafe>();
            Results->SetNum(SoftPaths.Num());

            FEvent* Done = FPlatformProcess::GetSynchEventFromPool(true);

            AsyncTask(ENamedThreads::GameThread, [SoftPaths, Results, Done]()
            {
                FStreamableManager& SM = UAssetManager::GetStreamableManager();
                SM.RequestAsyncLoad(
                    SoftPaths,
                    FStreamableDelegate::CreateLambda([SoftPaths, Results, Done]()
                    {
                        for (int32 i = 0; i < SoftPaths.Num(); ++i)
                        {
                            UObject* Obj = SoftPaths[i].ResolveObject();
                            if (!Obj)
                            {
                                LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to resolve loaded object for path: %s"), *SoftPaths[i].ToString());
                                continue;
                            }
                            UNNEModelData* ModelData = Cast<UNNEModelData>(Obj);
                            if (!ModelData)
                            {
                                LINGO_LOG(EVerbosityLevel::Error, TEXT("Asset is not UNNEModelData: %s"), *SoftPaths[i].ToString());
                                continue;
                            }
                            (*Results)[i] = TStrongObjectPtr<UNNEModelData>(ModelData);
                        }
                        Done->Trigger();
                    })
                );
            });

            if(!Done->Wait(10000)) // Wait up to 10 seconds for loading to complete
            {
                FPlatformProcess::ReturnSynchEventToPool(Done);
                LINGO_LOG(EVerbosityLevel::Error, TEXT("Model loading timed out."));
                return false;
            }
            FPlatformProcess::ReturnSynchEventToPool(Done);

            OutLoadedModels = MoveTemp(*Results);
            return true;
        }

    }
}
