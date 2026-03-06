// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace Thespeon
{
namespace Core
{
namespace IO
{
/**
 * Static utility class for loading files at runtime.
 *
 * Provides methods to resolve paths within the plugin's RuntimeData directory
 * and to load files from disk. All hard-coded file paths used by the plugin
 * are centralized in this class.
 */
class LINGOTIONTHESPEON_API RuntimeFileLoader
{
  public:
	/**
	 * @brief Returns the absolute path to the plugin's root directory.
	 *
	 * @return A const reference to the cached plugin directory path string.
	 */
	static const FString& GetPluginDir();

	/**
	 * @brief Returns the absolute path to the plugin's RuntimeData directory.
	 *
	 * This is where non-asset runtime files (JSON manifests, lookup tables, etc.) are stored.
	 *
	 * @return A const reference to the cached runtime file directory path string.
	 */
	static const FString& GetRuntimeFileDir();

	/**
	 * @brief Returns the absolute path to the LingotionThespeonManifest.json file.
	 *
	 * @return A const reference to the cached manifest file path string.
	 */
	static const FString& GetManifestPath();

	/** The file name of the manifest JSON file. */
	static constexpr const TCHAR* ManifestName = TEXT("LingotionThespeonManifest.json");

	/**
	 * @brief Returns the content directory path for runtime model assets.
	 *
	 * This path works both at runtime and in the editor for loading
	 * ONNX model assets via StaticLoadObject.
	 *
	 * @return A const reference to the cached runtime model directory path string.
	 */
	static const FString& GetRuntimeModelDir();

	/**
	 * @brief Constructs the full file path for a runtime file given its name.
	 *
	 * @param FileName The name of the file within the RuntimeData directory.
	 * @return The full absolute path to the specified runtime file.
	 */
	static FString GetRuntimeFilePath(const FString& FileName);

	/**
	 * @brief Constructs the full content path for a runtime model asset given its name.
	 *
	 * @param ModelName The name of the model asset.
	 * @return The full content path to the specified model asset.
	 */
	static FString GetRuntimeModelPath(const FString& ModelName);

	/**
	 * @brief Loads a file from disk and returns its contents as a string.
	 *
	 * Wraps FFileHelper::LoadFileToString with failure checking and logging.
	 *
	 * @param FilePath The absolute path to the file to load.
	 * @return The file contents as an FString, or an empty string on failure.
	 */
	static FString LoadFileAsString(const FString& FilePath);

	/**
	 * @brief Loads a file from disk and returns it as a stream handle.
	 *
	 * Opens the file for sequential reading. Logs an error if the file is not found.
	 *
	 * @param FilePath The absolute path to the file to load.
	 * @return A unique pointer to the file handle, or nullptr if the file was not found.
	 */
	static TUniquePtr<IFileHandle> LoadFileAsStream(const FString& FilePath);
};
} // namespace IO
} // namespace Core
} // namespace Thespeon
