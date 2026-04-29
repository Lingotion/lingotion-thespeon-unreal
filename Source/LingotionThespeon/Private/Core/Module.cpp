// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

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
Module::Module(const Thespeon::Core::FModuleEntry& Entry) : ModuleID(Entry.ModuleID), JSONPath(Entry.JsonPath), Version(Entry.Version)
{
	// Validate module entry parameters
	if (Entry.ModuleID.IsEmpty() || Entry.JsonPath.IsEmpty())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Module entry parameter is invalid: ModuleID or JsonPath is empty"));
	}
	LoadedBackends = TSet<EBackendType>();
}

// Loads ONNX models from disk as UNNEModelData assets for use by the InferenceWorkloadManager.
// Skips entries that are not inference models (lookuptable, metagraph) and models already loaded
// (present in LoadedMD5s). Dispatches actual asset loading to LoadModelsAsync.
bool Module::LoadModels(const TSet<FString>& LoadedMD5s, EBackendType BackendType, TMap<FString, TStrongObjectPtr<UNNEModelData>>& OutLoadedModels)
    const
{
	TArray<FString> WorkloadIDs;
	TArray<FSoftObjectPath> SoftPaths;
	WorkloadIDs.Reserve(InternalFileMappings.Num());
	SoftPaths.Reserve(InternalFileMappings.Num());

	for (const auto& FilePair : InternalFileMappings)
	{
		FString WorkloadID;
		if (!Thespeon::Core::TryGetRuntimeWorkloadID(FilePair.Value.FileName, BackendType, WorkloadID))
		{
			LINGO_LOG(
			    EVerbosityLevel::Error,
			    TEXT("Could not get WorkloadID for file MD5 '%s' on backend '%s'."),
			    *FilePair.Value.FileName,
			    *UEnum::GetValueAsString(BackendType)
			);
			return false;
		}
		if (FilePair.Key.Equals(TEXT("lookuptable"), ESearchCase::IgnoreCase) || FilePair.Key.Equals(TEXT("metagraph"), ESearchCase::IgnoreCase) ||
		    LoadedMD5s.Contains(WorkloadID))
		{
			continue;
		}
		const FString ModelAssetPath = Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeModelPath(FilePair.Value.GetFullFileName());
		WorkloadIDs.Add(WorkloadID);
		SoftPaths.Add(FSoftObjectPath(ModelAssetPath));
	}

	if (SoftPaths.Num() == 0)
	{
		return true; // Nothing to load, all models already loaded
	}

	TArray<TStrongObjectPtr<UNNEModelData>> LoadedArray;
	if (!Module::LoadModelsAsync(SoftPaths, LoadedArray))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to load models for module: %s"), *ModuleID);
		return false;
	}
	for (int32 i = 0; i < WorkloadIDs.Num(); ++i)
	{
		if (LoadedArray.IsValidIndex(i) && LoadedArray[i].IsValid())
		{
			OutLoadedModels.Add(WorkloadIDs[i], MoveTemp(LoadedArray[i]));
		}
	}
	return true;
}

FString Module::GetInternalModelID(const FString& InternalName) const
{
	const FModuleFile* FileInfo = InternalFileMappings.Find(InternalName);
	if (!FileInfo)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("File not found: %s. Try re-importing the current character."), *InternalName);
		return FString(); // Return empty string instead of throwing exception in Unreal
	}
	return FileInfo->FileName;
}

// Dispatches model loading to the unreal async io thread and blocks the calling
// worker thread with a TPromise/TFuture pair until loading completes (up to 10 second timeout).
// Must NOT be called from the game thread itself, as it would deadlock.
bool Module::LoadModelsAsync(const TArray<FSoftObjectPath>& SoftPaths, TArray<TStrongObjectPtr<UNNEModelData>>& OutLoadedModels)
{
	if (SoftPaths.Num() == 0)
	{
		return true; // Nothing to load, that is okay (means everything is already loaded)
	}

	if (IsInGameThread())
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("LoadModelsAsync must not be called from the game thread. Make sure not to defer to game thread when calling this function.")
		);
		return false;
	}

	auto Results = MakeShared<TArray<TStrongObjectPtr<UNNEModelData>>, ESPMode::ThreadSafe>();
	Results->SetNum(SoftPaths.Num());

	TSharedRef<TPromise<bool>, ESPMode::ThreadSafe> Promise = MakeShared<TPromise<bool>, ESPMode::ThreadSafe>();
	TFuture<bool> Future = Promise->GetFuture();

	AsyncTask(
	    ENamedThreads::GameThread,
	    [SoftPaths, Results, Promise]()
	    {
		    FStreamableManager& SM = UAssetManager::GetStreamableManager();
		    SM.RequestAsyncLoad(
		        SoftPaths,
		        FStreamableDelegate::CreateLambda(
		            [SoftPaths, Results, Promise]()
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
			            Promise->SetValue(true);
		            }
		        )
		    );
	    }
	);

	if (!Future.WaitFor(FTimespan::FromSeconds(10))) // Wait up to 10 seconds for loading to complete
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Model loading timed out."));
		return false;
	}

	OutLoadedModels = MoveTemp(*Results);
	return true;
}

TSet<FString> Module::GetAllFileNames() const
{
	TSet<FString> AllFileNames;

	// Collect MD5s from internal file mappings
	for (const auto& FilePair : InternalFileMappings)
	{
		AllFileNames.Add(FilePair.Value.FileName);
	}

	return AllFileNames;
}

// This gets all potentially fetched workloads by iterating over all workload file MD5s returned by GetAllWorkloadMD5s().
// GetAllWorkloadMD5s() now excludes non-workload files (such as lookuptable and metagraph files), so only actual workload files are considered.
bool Module::AddLoadedWorkloadIDs(EBackendType BackendType, TSet<FString>& OutWorkloadIDs) const
{
	for (const FString& md5 : this->GetAllWorkloadMD5s())
	{
		if (BackendType == EBackendType::None) // Add for all loaded backends
		{
			for (EBackendType LoadedBackend : this->LoadedBackends)
			{
				FString WorkloadID;
				if (!Thespeon::Core::TryGetRuntimeWorkloadID(md5, LoadedBackend, WorkloadID))
				{
					LINGO_LOG(
					    EVerbosityLevel::Error,
					    TEXT("Could not get WorkloadID for file MD5 '%s' on backend '%s'."),
					    *md5,
					    *UEnum::GetValueAsString(LoadedBackend)
					);
					return false;
				}
				OutWorkloadIDs.Add(WorkloadID);
			}
		}
		else if (this->LoadedBackends.Contains(BackendType)) // Add for specified backend if present
		{
			FString WorkloadID;
			if (!Thespeon::Core::TryGetRuntimeWorkloadID(md5, BackendType, WorkloadID))
			{
				LINGO_LOG(
				    EVerbosityLevel::Error,
				    TEXT("Could not get WorkloadID for file MD5 '%s' on backend '%s'."),
				    *md5,
				    *UEnum::GetValueAsString(BackendType)
				);
				return false;
			}
			OutWorkloadIDs.Add(WorkloadID);
		}
		else // add nothing
		{
			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug,
			    TEXT("Module '%s' is not loaded on backend '%s'. Skipping."),
			    *this->ModuleID,
			    *UEnum::GetValueAsString(BackendType)
			);
		}
	}
	return true;
}

// Constructs a composite workload key of the form "BackendType_MD5" to uniquely identify
// a model loaded on a specific backend. Returns false if BackendType is None.
bool TryGetRuntimeWorkloadID(const FString& ModuleID, EBackendType BackendType, FString& OutWorkloadID)
{
	if (BackendType == EBackendType::None)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("called with BackendType::None for ModuleID: %s"), *ModuleID);
		return false;
	}
	OutWorkloadID = FString::Printf(TEXT("%s_%s"), *UEnum::GetValueAsString(BackendType), *ModuleID);
	return true;
}
} // namespace Core
} // namespace Thespeon
