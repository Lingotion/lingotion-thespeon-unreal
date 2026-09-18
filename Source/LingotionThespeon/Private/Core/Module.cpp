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
#include "GenericPlatform/GenericPlatformFile.h"
#include "Core/meta_graph.pb.h"

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
	RegisteredWorkloads = TMap<EBackendType, TSet<FString>>();
}

// Loads ONNX models from disk as UNNEModelData assets for use by the InferenceWorkloadManager.
// Skips entries that are not inference models and models that already have a pool.
bool Module::LoadModels(
    const TSet<FString>& AlreadyRegisteredWorkloadIDs,
    EBackendType RequestedBackend,
    bool bForceRequestedBackend,
    TMap<FString, TStrongObjectPtr<UNNEModelData>>& OutLoadedModels,
    int32 Priority
) const
{
	TArray<FString> WorkloadIDs;
	TArray<FSoftObjectPath> SoftPaths;
	WorkloadIDs.Reserve(InternalFileMappings.Num());
	SoftPaths.Reserve(InternalFileMappings.Num());

	for (const auto& FilePair : InternalFileMappings)
	{
		if (FilePair.Key.Equals(TEXT("lookuptable"), ESearchCase::IgnoreCase) || FilePair.Key.Equals(TEXT("metagraph"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		FString WorkloadID;
		if (!TryGetNodeWorkloadID(FilePair.Key, RequestedBackend, bForceRequestedBackend, WorkloadID))
		{
			LINGO_LOG(
			    EVerbosityLevel::Error,
			    TEXT("Could not get WorkloadID for node '%s' of module '%s' on backend '%s'."),
			    *FilePair.Key,
			    *ModuleID,
			    *UEnum::GetValueAsString(RequestedBackend)
			);
			return false;
		}
		if (AlreadyRegisteredWorkloadIDs.Contains(WorkloadID))
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
	if (!Module::LoadModelsAsync(SoftPaths, LoadedArray, Priority))
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

TSharedPtr<const metaonnx::MetaGraph, ESPMode::ThreadSafe> Module::GetMetaGraph() const
{
	{
		FReadScopeLock ReadLock(MetaGraphLock);
		if (CachedMetaGraph.IsValid())
		{
			return CachedMetaGraph;
		}
	}

	const FModuleFile* GraphFile = InternalFileMappings.Find(TEXT("metagraph"));
	if (!GraphFile)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Module '%s' has no metagraph file mapping"), *ModuleID);
		return nullptr;
	}
	const FString GraphPath = Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeFilePath(GraphFile->GetFullFileName());

	TUniquePtr<IFileHandle> GraphHandle = Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsStream(GraphPath);
	if (!GraphHandle.IsValid())
	{
		return nullptr; // LoadFileAsStream already logged the reason
	}

	const int64 GraphSize = GraphHandle->Size();
	if (GraphSize <= 0 || GraphSize > MAX_int32)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Metagraph file has unusable size %lld: %s"), GraphSize, *GraphPath);
		return nullptr;
	}

	TArray<uint8> GraphBytes;
	GraphBytes.SetNumUninitialized(static_cast<int32>(GraphSize));
	if (!GraphHandle->Read(GraphBytes.GetData(), GraphSize))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to read metagraph file: %s"), *GraphPath);
		return nullptr;
	}

	TSharedPtr<metaonnx::MetaGraph, ESPMode::ThreadSafe> ParsedGraph = MakeShared<metaonnx::MetaGraph, ESPMode::ThreadSafe>();
	if (!ParsedGraph->ParseFromArray(GraphBytes.GetData(), GraphBytes.Num()))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to parse metagraph protobuf: %s"), *GraphPath);
		return nullptr;
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Parsed metagraph for module '%s' (%lld bytes)"), *ModuleID, GraphSize);

	FWriteScopeLock WriteLock(MetaGraphLock);
	if (!CachedMetaGraph.IsValid())
	{
		CachedMetaGraph = MoveTemp(ParsedGraph);
	}
	return CachedMetaGraph;
}

// Dispatches model loading to the unreal async io thread and blocks the calling
// worker thread with a TPromise/TFuture pair until loading completes (up to 10 second timeout).
// Must NOT be called from the game thread itself, as it would deadlock.
bool Module::LoadModelsAsync(const TArray<FSoftObjectPath>& SoftPaths, TArray<TStrongObjectPtr<UNNEModelData>>& OutLoadedModels, int32 Priority)
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
	    [SoftPaths, Results, Promise, Priority]()
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
		        ),
		        Priority
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

// Maps a metagraph device declaration onto a runtime backend. Returns EBackendType::None when it
// cannot be honoured, in which case the caller keeps the backend it requested.
//
// Quiet by design: ResolveNodeBackend runs on every node execution, so anything logged here would
// repeat once per loop iteration. GetRequiredWorkloads reports these decisions instead, once per
// registration.
static EBackendType BackendFromDeviceType(metaonnx::DeviceType Device)
{
	switch (Device)
	{
		case metaonnx::DEVICE_CPU:
			return EBackendType::CPU;
#if PLATFORM_WINDOWS
		// Off Windows there is no DML backend, so a GPU declaration cannot be honoured — the same
		// constraint ThespeonComponent::ResolveBackend applies to the user's requested backend.
		case metaonnx::DEVICE_GPU:
			return EBackendType::GPU;
#endif
		default:
			return EBackendType::None;
	}
}

// Returns the device a node declares, or DEVICE_UNSPECIFIED if the node is absent or declares none.
static metaonnx::DeviceType FindNodeDeclaredDevice(const metaonnx::MetaGraph& Graph, const FString& NodeID)
{
	for (int32 NodeIndex = 0; NodeIndex < Graph.nodes_size(); ++NodeIndex)
	{
		const metaonnx::Node& Node = Graph.nodes(NodeIndex);
		if (NodeID == FString(UTF8_TO_TCHAR(Node.id().c_str())))
		{
			return Node.preferred_device();
		}
	}
	return metaonnx::DEVICE_UNSPECIFIED;
}

// Explains what happened to a node's declared device. Called only from GetRequiredWorkloads, so each
// message appears once per module per registration rather than once per node execution.
static void ReportNodeBackendDecision(
    const metaonnx::MetaGraph& Graph, const FString& NodeID, EBackendType RequestedBackend, EBackendType EffectiveBackend, bool bForceRequestedBackend
)
{
	const metaonnx::DeviceType Declared = FindNodeDeclaredDevice(Graph, NodeID);
	if (Declared == metaonnx::DEVICE_UNSPECIFIED)
	{
		return; // Nothing declared, nothing to explain
	}

	const EBackendType DeclaredBackend = BackendFromDeviceType(Declared);
	if (DeclaredBackend == EBackendType::None)
	{
		LINGO_LOG(
		    EVerbosityLevel::Warning,
		    TEXT("Node '%s' declares device '%s', which no NNE backend on this platform provides. Running it on the requested '%s'."),
		    *NodeID,
		    UTF8_TO_TCHAR(metaonnx::DeviceType_Name(Declared).c_str()),
		    *UEnum::GetValueAsString(RequestedBackend)
		);
		return;
	}
	if (DeclaredBackend == RequestedBackend)
	{
		return; // Already what the caller asked for
	}

	if (bForceRequestedBackend)
	{
		LINGO_LOG(
		    EVerbosityLevel::Warning,
		    TEXT("Node '%s' declares backend '%s', but ForceRequestedBackend is set — running it on '%s' instead. A model declares a device because "
		         "it performs poorly or incorrectly elsewhere."),
		    *NodeID,
		    *UEnum::GetValueAsString(DeclaredBackend),
		    *UEnum::GetValueAsString(RequestedBackend)
		);
		return;
	}

	LINGO_LOG(
	    EVerbosityLevel::Info,
	    TEXT("Node '%s' declares backend '%s', overriding the requested '%s'."),
	    *NodeID,
	    *UEnum::GetValueAsString(EffectiveBackend),
	    *UEnum::GetValueAsString(RequestedBackend)
	);
}

// The single place a node's device is decided. Registration and workload acquisition both call this,
// which is what guarantees they resolve to the same pool.
//
// Quiet: this runs once per node execution, so GetRequiredWorkloads does the reporting instead.
EBackendType Module::ResolveNodeBackend(const FString& NodeID, EBackendType RequestedBackend, bool bForceRequestedBackend) const
{
	if (bForceRequestedBackend)
	{
		return RequestedBackend;
	}

	TSharedPtr<const metaonnx::MetaGraph, ESPMode::ThreadSafe> Graph = GetMetaGraph();
	if (!Graph.IsValid())
	{
		return EBackendType::None; // GetMetaGraph already logged the reason
	}

	const EBackendType DeclaredBackend = BackendFromDeviceType(FindNodeDeclaredDevice(*Graph, NodeID));
	return DeclaredBackend != EBackendType::None ? DeclaredBackend : RequestedBackend;
}

bool Module::TryGetNodeWorkloadID(const FString& NodeID, EBackendType RequestedBackend, bool bForceRequestedBackend, FString& OutWorkloadID) const
{
	const FModuleFile* FileInfo = InternalFileMappings.Find(NodeID);
	if (!FileInfo)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Module '%s' has no file for node '%s'."), *ModuleID, *NodeID);
		return false;
	}

	const EBackendType EffectiveBackend = ResolveNodeBackend(NodeID, RequestedBackend, bForceRequestedBackend);
	if (EffectiveBackend == EBackendType::None)
	{
		return false; // Already logged
	}
	return Thespeon::Core::TryGetRuntimeWorkloadID(FileInfo->FileName, EffectiveBackend, OutWorkloadID);
}

// Enumerates every model this module needs, mapped to the backend its pool must be built on.
// Non-inference entries (lookuptable, metagraph) are skipped — they have no workload.
//
// This is also where device declarations are reported. It runs once per module per registration,
// unlike ResolveNodeBackend which the acquire path re-derives on every node execution.
bool Module::GetRequiredWorkloads(EBackendType RequestedBackend, bool bForceRequestedBackend, TMap<FString, EBackendType>& OutWorkloads) const
{
	TSharedPtr<const metaonnx::MetaGraph, ESPMode::ThreadSafe> Graph = GetMetaGraph();
	if (!Graph.IsValid())
	{
		// Without the graph we cannot tell which device each model belongs on, and inference would
		// fail on the same graph anyway. Fail rather than silently assuming "nothing declared".
		return false;
	}

	for (const auto& FilePair : InternalFileMappings)
	{
		if (FilePair.Key.Equals(TEXT("lookuptable"), ESearchCase::IgnoreCase) || FilePair.Key.Equals(TEXT("metagraph"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		const EBackendType EffectiveBackend = ResolveNodeBackend(FilePair.Key, RequestedBackend, bForceRequestedBackend);
		FString WorkloadID;
		if (EffectiveBackend == EBackendType::None || !Thespeon::Core::TryGetRuntimeWorkloadID(FilePair.Value.FileName, EffectiveBackend, WorkloadID))
		{
			LINGO_LOG(
			    EVerbosityLevel::Error,
			    TEXT("Could not resolve a workload ID for node '%s' of module '%s' on backend '%s'."),
			    *FilePair.Key,
			    *ModuleID,
			    *UEnum::GetValueAsString(RequestedBackend)
			);
			return false;
		}
		ReportNodeBackendDecision(*Graph, FilePair.Key, RequestedBackend, EffectiveBackend, bForceRequestedBackend);
		OutWorkloads.Add(MoveTemp(WorkloadID), EffectiveBackend);
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
