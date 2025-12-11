// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"

/**
 * Module selection information for deletion operations.
 */
struct FModuleSelectionInfo
{
	FString ModuleID;
	FString PackName;
	FString JsonPath;
	bool bIsActorModule;

	FModuleSelectionInfo() : bIsActorModule(false) {}

	bool IsValid() const { return !ModuleID.IsEmpty(); }
	void Reset() { ModuleID.Empty(); PackName.Empty(); JsonPath.Empty(); bIsActorModule = false; }
};

/**
 * Handles deletion of actor and language modules with proper file reference counting.
 * Ensures files shared by multiple modules are not deleted.
 */
class FEditorModuleDeleter
{
public:
	FEditorModuleDeleter();
	~FEditorModuleDeleter();

	/**
	 * Deletes an actor module, including all associated files not shared by other modules.
	 * @param ModuleID - The module ID to delete
	 * @param PackName - The pack name containing the module
	 * @param JsonPath - Path to the module's JSON file
	 */
	void DeleteActorModule(const FString& ModuleID, const FString& PackName, const FString& JsonPath);

	/**
	 * Deletes a language module, including all associated files not shared by other modules.
	 * @param ModuleID - The module ID to delete
	 * @param PackName - The pack name containing the module
	 * @param JsonPath - Path to the module's JSON file
	 */
	void DeleteLanguageModule(const FString& ModuleID, const FString& PackName, const FString& JsonPath);

private:
	/**
	 * Checks if a file (identified by MD5) is shared by multiple modules.
	 * @param MD5 - The MD5 hash of the file
	 * @param ModuleJsonString - JSON string of the module being deleted
	 * @param OutUsageCount - Number of modules using this file
	 * @return true if file is shared by more than one module
	 */
	bool IsFileSharedByMultipleModules(const FString& MD5, const FString& ModuleJsonString, int32& OutUsageCount);

	/**
	 * Gets the filename from an MD5 hash by looking it up in module JSON.
	 * @param MD5 - The MD5 hash to look up
	 * @param ModuleJsonString - JSON string containing file mappings
	 * @return The filename associated with the MD5, or empty string if not found
	 */
	FString GetFilenameFromMD5(const FString& MD5, const FString& ModuleJsonString);

	/**
	 * Collects all files associated with a language module.
	 * @param ModuleID - The module ID to collect files for
	 * @param ModuleJsonString - JSON string of the module
	 * @param OutFilesToDelete - Array to populate with file paths to delete
	 * @param OutAssetPaths - Array to populate with asset paths
	 * @param OutMD5s - Set to populate with MD5 hashes of files
	 */
	void CollectLanguageModuleFiles(const FString& ModuleID, const FString& ModuleJsonString,
	                                 TArray<FString>& OutFilesToDelete, TArray<FString>& OutAssetPaths, TSet<FString>& OutMD5s);
};
