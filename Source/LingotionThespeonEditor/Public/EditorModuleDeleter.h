// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"

/**
 * Module selection information for deletion operations.
 */
struct FModuleSelectionInfo
{
	FString ModuleID;
	FString Name;
	FString JsonPath;
	bool bIsCharacterModule;

	FModuleSelectionInfo() : bIsCharacterModule(false) {}

	bool IsValid() const
	{
		return !ModuleID.IsEmpty();
	}
	void Reset()
	{
		ModuleID.Empty();
		Name.Empty();
		JsonPath.Empty();
		bIsCharacterModule = false;
	}
};

/**
 * Handles deletion of character and language modules with proper file reference counting.
 * Ensures files shared by multiple modules are not deleted.
 */
class FEditorModuleDeleter
{
  public:
	/**
	 * Deletes a module, including all associated files not shared by other modules.
	 * @param JsonPath - Path to the module's JSON definition file
	 */
	static void DeleteModule(const FString& JsonPath);
};
