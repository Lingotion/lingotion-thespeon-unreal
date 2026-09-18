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

	/** Path to the JSON file that defines this module. */
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
 * Manages ONNX model files and loads them asynchronously via FStreamableManager.
 */
class Module
{
  public:
	virtual ~Module() = default;

	/** Unique identifier for this module. */
	FString ModuleID;

	/** Path to the JSON definition file for this module. */
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
	 * loaded twice.
	 *
	 * @param AlreadyRegisteredWorkloadIDs Workload IDs that already have a pool.
	 * @param RequestedBackend The NNE backend the caller asked for.
	 * @param bForceRequestedBackend Ignore metagraph device pins and use RequestedBackend throughout.
	 * @param OutLoadedModels Map to populate with loaded model data keyed by workload ID.
	 * @param Priority Forwarded to FStreamableManager::RequestAsyncLoad. Use 0 for default, 100 for high
	 *                 (FStreamableManager::AsyncLoadHighPriority). Used so that a synth-triggered load can
	 *                 jump ahead of in-flight preload loads in the streamable manager's queue.
	 * @return True if all models were loaded successfully, false otherwise.
	 */
	bool LoadModels(
	    const TSet<FString>& AlreadyRegisteredWorkloadIDs,
	    EBackendType RequestedBackend,
	    bool bForceRequestedBackend,
	    TMap<FString, TStrongObjectPtr<UNNEModelData>>& OutLoadedModels,
	    int32 Priority = 0
	) const;

	/**
	 * @brief Resolves an internal file name to its model ID (content path).
	 *
	 * @param InternalName The internal file identifier to look up.
	 * @return The resolved model ID string, or empty if not found.
	 */
	FString GetInternalModelID(const FString& InternalName) const;

	/**
	 * @brief Returns this module's parsed metagraph, reading and caching it on first call.
	 * Immutable once parsed, so a single* instance is shared by every caller.
	 *
	 * Safe to call from any thread.
	 *
	 * @return The parsed graph, or nullptr if this module declares no metagraph file or the
	 *         file could not be read.
	 */
	TSharedPtr<const metaonnx::MetaGraph, ESPMode::ThreadSafe> GetMetaGraph() const;

	/**
	 * @brief Returns the backend the given metagraph node must run on.
	 *
	 * @param NodeID The metagraph node id, which is also its InternalFileMappings key.
	 * @param RequestedBackend The backend the caller asked for.
	 * @param bForceRequestedBackend Ignore any declared device and return RequestedBackend.
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
	 * @return False if the metagraph could not be read, in which case the caller must not register.
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

  private:
	/**
	 * @brief Asynchronously loads model data from the given soft object paths.
	 *
	 * @param SoftPaths Array of asset paths to load.
	 * @param OutLoadedModels Array to populate with the loaded model data.
	 * @return True if all models were loaded successfully, false otherwise.
	 */
	static bool
	LoadModelsAsync(const TArray<FSoftObjectPath>& SoftPaths, TArray<TStrongObjectPtr<UNNEModelData>>& OutLoadedModels, int32 Priority = 0);

	mutable TSharedPtr<const metaonnx::MetaGraph, ESPMode::ThreadSafe> CachedMetaGraph;

	mutable FRWLock MetaGraphLock;
};

/**
 * @brief Attempts to resolve a module's file mapping to a UE content path for runtime use.
 *
 * Constructs the expected workload ID from the module ID and backend type,
 * then checks if it exists in the content directory.
 *
 * @param ModuleID The module identifier to resolve.
 * @param BackendType The backend type to resolve for.
 * @param OutWorkloadID Output parameter that receives the resolved content path.
 * @return True if the workload ID was resolved successfully, false otherwise.
 */
bool TryGetRuntimeWorkloadID(const FString& ModuleID, EBackendType BackendType, FString& OutWorkloadID);
} // namespace Core
} // namespace Thespeon
