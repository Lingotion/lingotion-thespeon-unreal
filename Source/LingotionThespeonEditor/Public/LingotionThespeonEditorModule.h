// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

// Forward declarations
class FEditorModuleDeleter;
class FEditorInfoWindow;

/**
 * This is the module definition for the editor mode. You can implement custom functionality
 * as your plugin module starts up and shuts down. See IModuleInterface for more extensibility options.
 * 
 * This module coordinates three main components:
 * 1. Pack import (FEditorPackImporter)
 * 2. Module deletion (FEditorModuleDeleter)
 * 3. Info window GUI with integrated license validation (FEditorInfoWindow)
 */
class FLingotionThespeonEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OpenLingotionThespeonEditorTab();
	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs);

	// Component instances
	TSharedPtr<FEditorModuleDeleter> ModuleDeleter;
	TSharedPtr<FEditorInfoWindow> InfoWindow;
};
