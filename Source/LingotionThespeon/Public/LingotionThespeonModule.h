// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

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
	/** Called when the module is loaded. Only logs; the plugin's subsystems do the real setup. */
	void StartupModule() override;
	/** Called when the module is unloaded. Currently does nothing. */
	void ShutdownModule() override;
};
