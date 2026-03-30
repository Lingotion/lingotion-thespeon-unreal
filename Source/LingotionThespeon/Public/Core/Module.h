// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Core/BackendType.h"
#include "NNEModelData.h"

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
	int Major = 0;
	int Minor = 0;
	int Patch = 0;

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

	/** Set of backend types for which models have been loaded. */
	TSet<EBackendType> LoadedBackends;

	/** Map of internal file identifiers to their file name and extension. */
	TMap<FString, FModuleFile> InternalFileMappings;

	/**
	 * @brief Loads ONNX models for this module using the specified backend type.
	 *
	 * Skips models whose MD5 hashes are already present in LoadedMD5s
	 * to avoid duplicate loading of shared files.
	 *
	 * @param LoadedMD5s Set of MD5 hashes for models that are already loaded.
	 * @param BackendType The NNE backend to use for model inference.
	 * @param OutLoadedModels Map to populate with loaded model data keyed by model ID.
	 * @return True if all models were loaded successfully, false otherwise.
	 */
	bool LoadModels(const TSet<FString>& LoadedMD5s, EBackendType BackendType, TMap<FString, TStrongObjectPtr<UNNEModelData>>& OutLoadedModels) const;

	/**
	 * @brief Resolves an internal file name to its model ID (content path).
	 *
	 * @param InternalName The internal file identifier to look up.
	 * @return The resolved model ID string, or empty if not found.
	 */
	FString GetInternalModelID(const FString& InternalName) const;

	/**
	 * @brief Adds the workload IDs for all models in this module to the output set.
	 *
	 * @param BackendType The backend type to resolve workload IDs for.
	 * @param OutWorkloadIDs Output set to populate with workload ID strings.
	 * @return True if workload IDs were added successfully, false otherwise.
	 */
	bool AddLoadedWorkloadIDs(EBackendType BackendType, TSet<FString>& OutWorkloadIDs) const;

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

	/**
	 * @brief Returns all workload MD5 hashes for this module.
	 *
	 * @return A set of all workload MD5 hash strings.
	 */
	virtual TSet<FString> GetAllWorkloadMD5s() const = 0;

	/**
	 * @brief Checks if this module is fully included in the provided set of MD5 hashes.
	 *
	 * Used to determine whether all required model files for a given backend
	 * type are already loaded.
	 *
	 * @param MD5s The set of MD5 hashes to check against.
	 * @param BackendType The backend type to check inclusion for.
	 * @return True if all required files are present in the MD5 set, false otherwise.
	 */
	virtual bool IsIncludedIn(const TSet<FString>& MD5s, EBackendType BackendType) const = 0;

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
	static bool LoadModelsAsync(const TArray<FSoftObjectPath>& SoftPaths, TArray<TStrongObjectPtr<UNNEModelData>>& OutLoadedModels);
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
