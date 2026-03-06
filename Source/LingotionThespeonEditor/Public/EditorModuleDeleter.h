// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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
