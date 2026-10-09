// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Core/BackendType.h"
#include "HAL/CriticalSection.h"
#include "NNEModelData.h"
#include "UObject/StrongObjectPtr.h"

namespace metaonnx
{
class MetaGraph;
}

namespace Thespeon
{
namespace Core
{
/**
 * Represents a semantic version number with Major, Minor, and Patch components.
 *
 * Used for module version tracking and collision detection when importing.
 * Supports comparison operators for version ordering.
 */
struct FVersion
{
	int Major = -1;
	int Minor = -1;
	int Patch = -1;

	FVersion() = default;
	FVersion(int InMajor, int InMinor, int InPatch) : Major(InMajor), Minor(InMinor), Patch(InPatch) {}

	/**
	 * @brief Converts the version to a human-readable string representation.
	 *
	 * @param bPrefixV If true, prefixes the string with "v" (e.g., "v1.2.3"). Defaults to true.
	 * @return The formatted version string.
	 */
	FString ToString(bool bPrefixV = true) const
	{
		return FString::Printf(TEXT("%s%d.%d.%d"), bPrefixV ? TEXT("v") : TEXT(""), Major, Minor, Patch);
	}

	bool IsValid() const
	{
		return (Major >= 0 && Minor >= 0 && Patch >= 0);
	}

	bool operator==(const FVersion& Other) const
	{
		return Major == Other.Major && Minor == Other.Minor && Patch == Other.Patch;
	}
	bool operator!=(const FVersion& Other) const
	{
		return !(*this == Other);
	}
	bool operator<(const FVersion& Other) const
	{
		if (Major != Other.Major)
		{
			return Major < Other.Major;
		}
		if (Minor != Other.Minor)
		{
			return Minor < Other.Minor;
		}
		return Patch < Other.Patch;
	}
	bool operator>(const FVersion& Other) const
	{
		return Other < *this;
	}
	bool operator<=(const FVersion& Other) const
	{
		return !(*this > Other);
	}
	bool operator>=(const FVersion& Other) const
	{
		return !(*this < Other);
	}
};

/**
 * Represents a module entry from the manifest.
 *
 * Contains the module identifier, the JSON file path for the module definition,
 * and an optional version used for collision detection during import.
 */
struct FModuleEntry
{
	/** Unique identifier for this module. */
	FString ModuleID;

	/** File name of the JSON config that defines this module, relative to the RuntimeData directory. */
	FString JsonPath;

	/** Optional version for collision detection and compatibility checks. */
	Thespeon::Core::FVersion Version;

	/** Default constructor. */
	FModuleEntry() = default;

	/**
	 * @brief Constructs a module entry with the given ID, JSON path, and version.
	 *
	 * @param InID The unique module identifier.
	 * @param InPath The path to the module JSON definition.
	 * @param InVersion The semantic version of the module.
	 */
	FModuleEntry(const FString& InID, const FString& InPath, const Thespeon::Core::FVersion& InVersion)
	    : ModuleID(InID), JsonPath(InPath), Version(InVersion)
	{
	}

	/**
	 * @brief Checks whether this module entry is empty (has no ID or path).
	 *
	 * @return True if the ModuleID or JsonPath is empty, false otherwise.
	 */
	bool IsEmpty() const
	{
		return ModuleID.IsEmpty() || JsonPath.IsEmpty();
	}
};

/**
 * Represents a single file within a module.
 *
 * Stores the file name and extension separately. ONNX files are treated
 * specially since they are stored as Unreal Assets.
 */
struct FModuleFile
{
	/** The base file name without extension. */
	FString FileName;

	/** The file extension (e.g., "onnx", "json"). */
	FString FileExtension;

	/**
	 * @brief Constructs a module file with the given name and extension.
	 *
	 * @param fileName The base file name.
	 * @param extension The file extension.
	 */
	FModuleFile(FString fileName, FString extension) : FileName(MoveTemp(fileName)), FileExtension(MoveTemp(extension)) {}

	/**
	 * @brief Returns the full file name including extension.
	 *
	 * ONNX files are stored as Unreal Assets and use a special naming
	 * convention (FileName.FileName) instead of the standard FileName.Extension.
	 *
	 * @return The full file name string.
	 */
	FString GetFullFileName() const
	{
		// ONNX files are stored as Unreal Assets
		if (FileExtension == TEXT("onnx"))
		{
			return FileName + "." + FileName;
		}
		return FileName + "." + FileExtension;
	}
};

/**
 * Base class for modules - Core abstraction for managing AI model resources.
 *
 * Represents units of functionality for text-to-speech synthesis.
 * Derived classes (CharacterModule, LanguageModule) specialize this for
 * character voice models and language phonemizer models respectively.
 * Manages ONNX model files and loads them through FStreamableManager, blocking the calling worker thread until they arrive.
 */
class Module
{
  public:
	virtual ~Module() = default;

	/** Unique identifier for this module. */
	FString ModuleID;

	/** File name of the JSON config for this module, relative to the RuntimeData directory. */
	FString JSONPath;

	/** Semantic version of this module. */
	Thespeon::Core::FVersion Version;

	/**
	 * Workload IDs this module actually registered, keyed by the backend that was *requested*.
	 *
	 * Recorded at registration rather than re-derived later: a workload ID names the backend its
	 * instances run on, which a metagraph device pin may make different from the requested one, so
	 * unload cannot reconstruct these IDs from the requested backend alone.
	 */
	TMap<EBackendType, TSet<FString>> RegisteredWorkloads;

	/** Map of internal file identifiers to their file name and extension. */
	TMap<FString, FModuleFile> InternalFileMappings;

	/**
	 * @brief Loads ONNX models for this module, keyed by the workload ID each will be pooled under.
	 *
	 * Skips workload IDs already present in AlreadyRegisteredWorkloadIDs so shared files are not
	 * loaded twice, and skips lookup-table and metagraph entries. Must not be called on the game thread;
	 * blocks for up to 10 seconds.
	 *
	 * @param AlreadyRegisteredWorkloadIDs Workload IDs that already have a pool.
	 * @param RequestedBackend The NNE backend the caller asked for.
	 * @param bForceRequestedBackend Ignore metagraph device pins and use RequestedBackend throughout.
	 * @param OutLoadedModels Map to populate with loaded model data keyed by workload ID.
	 * @param Priority Forwarded to FStreamableManager::RequestAsyncLoad. Use 0 for default, 100 for high
	 *                 (FStreamableManager::AsyncLoadHighPriority). Used so that a synth-triggered load can
	 *                 jump ahead of in-flight preload loads in the streamable manager's queue.
	 * @return False on timeout, a game-thread call or an unresolvable workload ID. Models that fail to resolve or
	 *         load are logged and left out of OutLoadedModels without making the call fail.
	 */
	bool LoadModels(
	    const TSet<FString>& AlreadyRegisteredWorkloadIDs,
	    EBackendType RequestedBackend,
	    bool bForceRequestedBackend,
	    TMap<FString, TStrongObjectPtr<UNNEModelData>>& OutLoadedModels,
	    int32 Priority = 0
	) const;

	/**
	 * @brief Resolves an internal file name to the model's base file name (its MD5).
	 *
	 * @param InternalName The internal file identifier to look up.
	 * @return The model's base file name, or empty (with an error logged) if not found.
	 */
	FString GetInternalModelID(const FString& InternalName) const;

	/**
	 * @brief Returns this module's parsed metagraph, reading and caching it on first call.
	 * Immutable once parsed, so one instance is shared by every caller.
	 *
	 * Safe to call from any thread.
	 *
	 * @return The parsed graph, or nullptr if this module declares no metagraph file or the
	 *         file could not be read, has a bad size or fails to parse. Failures are not cached,
	 *         so every call retries and logs again.
	 */
	TSharedPtr<const metaonnx::MetaGraph, ESPMode::ThreadSafe> GetMetaGraph() const;

	/**
	 * @brief Returns the backend the given metagraph node must run on.
	 *
	 * @param NodeID The metagraph node id, which is also its InternalFileMappings key.
	 * @param RequestedBackend The backend the caller asked for.
	 * @param bForceRequestedBackend Ignore any declared device and return RequestedBackend.
	 * A GPU device pin is only honoured on Windows; elsewhere, and for unsupported devices, RequestedBackend is used.
	 * @return The effective backend, or EBackendType::None if the metagraph could not be read.
	 */
	EBackendType ResolveNodeBackend(const FString& NodeID, EBackendType RequestedBackend, bool bForceRequestedBackend) const;

	/**
	 * @brief Resolves the workload ID (pool identity) for the given metagraph node.
	 *
	 * The ID names the node's effective backend, not the requested one, so nodes that declare
	 * different devices get separate pools instead of contending for one.
	 *
	 * @param NodeID The metagraph node id, which is also its InternalFileMappings key.
	 * @param RequestedBackend The backend the caller asked for.
	 * @param bForceRequestedBackend Ignore any declared device and use RequestedBackend.
	 * @param OutWorkloadID Receives the resolved workload ID.
	 * @return False if the node is unknown or the backend could not be resolved.
	 */
	bool TryGetNodeWorkloadID(const FString& NodeID, EBackendType RequestedBackend, bool bForceRequestedBackend, FString& OutWorkloadID) const;

	/**
	 * @brief Returns every workload this module needs on the given requested backend.
	 *
	 * @param RequestedBackend The backend the caller asked for.
	 * @param bForceRequestedBackend Ignore metagraph device pins and use RequestedBackend throughout.
	 * @param OutWorkloads Receives workload ID -> effective backend for every model in this module.
	 * @return False if the metagraph could not be read or a node's workload ID could not be resolved (e.g. RequestedBackend
	 *         None), in which case the caller must not register. OutWorkloads is not cleared and may be partly filled.
	 */
	bool GetRequiredWorkloads(EBackendType RequestedBackend, bool bForceRequestedBackend, TMap<FString, EBackendType>& OutWorkloads) const;

	/**
	 * @brief Returns all file names associated with this module.
	 *
	 * @return A set of all file name strings.
	 */
	TSet<FString> GetAllFileNames() const;

	/**
	 * @brief Returns the type identifier string for this module (e.g., "character" or "language").
	 *
	 * This is the project's hand-rolled type discriminator: Module is not a UObject, so Cast<T> does
	 * not apply, and RTTI is off (the Unreal default), so dynamic_cast is unavailable.
	 *
	 * Every derived class must also expose a matching `static FString StaticModuleType()` returning
	 * the same string, so the type can be named without an instance. UModuleManager::GetModule<T>
	 * compares the two before downcasting a stored module.
	 *
	 * @return The module type string.
	 */
	virtual FString GetModuleType() const = 0;

  protected:
	/**
	 * @brief Constructs a Module from a module entry.
	 *
	 * Protected constructor - only derived classes can create modules.
	 *
	 * @param Entry The module entry containing ID, path, and version information.
	 */
	Module(const Thespeon::Core::FModuleEntry& Entry);

	/**
	 * @brief Initializes the module from a JSON definition string.
	 *
	 * Derived classes must implement this to parse module-specific JSON data
	 * and populate their internal file mappings and metadata.
	 *
	 * @param JsonString The raw JSON string to parse.
	 * @return True if initialization succeeded, false otherwise.
	 */
	virtual bool InitializeFromJSON(const FString& JsonString) = 0;

	/**
	 * @brief Reads one of the module's data files (not an ONNX model) into memory.
	 *
	 * @param File The file, from InternalFileMappings.
	 * @param OutBytes Receives the file's contents.
	 * @return False if the file could not be read; the reason is logged.
	 */
	bool ReadModuleFile(const FModuleFile& File, TArray<uint8>& OutBytes) const;

  private:
	/**
	 * @brief Requests the given assets through FStreamableManager on the game thread and blocks the calling
	 * thread until they arrive (up to 10 seconds).
	 *
	 * @param SoftPaths Array of asset paths to load.
	 * @param OutLoadedModels Array to populate with the loaded model data.
	 * @param Priority Forwarded to FStreamableManager::RequestAsyncLoad.
	 * @return False on timeout or a game-thread call. Assets that fail to load are logged and left out.
	 */
	static bool
	LoadModelsAsync(const TArray<FSoftObjectPath>& SoftPaths, TArray<TStrongObjectPtr<UNNEModelData>>& OutLoadedModels, int32 Priority = 0);

	mutable TSharedPtr<const metaonnx::MetaGraph, ESPMode::ThreadSafe> CachedMetaGraph;

	mutable FRWLock MetaGraphLock;
};

/**
 * @brief Builds the workload pool ID for a model file on a backend.
 *
 * Only formats the ID; it does not check that the model exists.
 *
 * @param ModuleID The model's base file name (its MD5), despite the parameter name.
 * @param BackendType The backend type to build the ID for.
 * @param OutWorkloadID Receives "<Backend>_<ModuleID>".
 * @return False only if BackendType is None.
 */
bool TryGetRuntimeWorkloadID(const FString& ModuleID, EBackendType BackendType, FString& OutWorkloadID);
} // namespace Core
} // namespace Thespeon
