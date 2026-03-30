// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "Modules/ModuleManager.h"

/**
 * Runtime module for the Lingotion Thespeon plugin.
 *
 * Handles plugin startup and shutdown lifecycle. This is the entry point
 * registered with Unreal's module system when the plugin is loaded.
 */
class LINGOTIONTHESPEON_API FLingotionThespeonModule : public IModuleInterface
{
  public:
	/** Called when the module is loaded. Performs plugin initialization. */
	void StartupModule() override;
	/** Called when the module is unloaded. Performs plugin cleanup. */
	void ShutdownModule() override;
};
