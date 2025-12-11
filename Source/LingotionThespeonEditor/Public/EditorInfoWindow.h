// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Docking/SDockTab.h"
#include "EditorModuleDeleter.h"

// Forward declarations
class SButton;
class SVerticalBox;

/**
 * Manages the Lingotion Thespeon Info Window UI.
 * Responsible for creating and managing the visual GUI components for the info window,
 * including module listing, selection, and interaction with license validation and module deletion.
 */
class FEditorInfoWindow : public TSharedFromThis<FEditorInfoWindow>
{
public:
	FEditorInfoWindow();
	~FEditorInfoWindow();

	/**
	 * Creates the main tab content for the Thespeon Info Window.
	 * @param Args - Spawn tab arguments
	 * @param InModuleDeleter - Module deleter for handling deletion operations
	 * @return The constructed tab widget
	 */
	TSharedRef<SDockTab> CreateTab(const FSpawnTabArgs& Args,
	                                TSharedPtr<FEditorModuleDeleter> InModuleDeleter);

	/**
	 * Refreshes the tab content, rebuilding the module list.
	 */
	void RefreshTab();

	/**
	 * Gets the current tab reference.
	 * @return The current tab, or null if not created
	 */
	TSharedPtr<SDockTab> GetCurrentTab() const { return CurrentTab; }

private:
	/**
	 * Rebuilds the tab content with current module data.
	 */
	void RebuildTabContent();

	/**
	 * Selects a module for deletion.
	 * @param ModuleID - The module ID
	 * @param PackName - The pack name
	 * @param JsonPath - Path to the module JSON
	 * @param bIsActorModule - Whether this is an actor module (vs language module)
	 */
	void SelectModule(const FString& ModuleID, const FString& PackName, const FString& JsonPath, bool bIsActorModule);

	/**
	 * Handles delete button click.
	 * @return Reply indicating the event was handled
	 */
	FReply OnDeleteModuleClicked();

	/**
	 * Handles import pack button click.
	 * @return Reply indicating the event was handled
	 */
	FReply OnImportPackClicked();

	/**
	 * Gets the background color for a module based on selection state.
	 * @param ModuleID - The module ID to check
	 * @return The background color
	 */
	FSlateColor GetModuleBackgroundColor(FString ModuleID) const;

	/**
	 * Handles mouse button events on module entries.
	 * @param MyGeometry - Geometry of the widget
	 * @param MouseEvent - Mouse event data
	 * @param ModuleID - Module ID
	 * @param PackName - Pack name
	 * @param JsonPath - JSON path
	 * @param InfoText - Information text (actor name or language name)
	 * @param InfoText2 - Secondary information text (quality or empty)
	 * @param bIsActorModule - Whether this is an actor module
	 * @return Reply indicating how the event was handled
	 */
	FReply OnModuleMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent,
	                              FString ModuleID, FString PackName, FString JsonPath,
	                              FString InfoText, FString InfoText2, bool bIsActorModule);

	/**
	 * Validates the license key and updates UI visibility based on validation result.
	 * Uses cached validation to work offline.
	 */
	void ValidateAndGate();

	/**
	 * Toggles visibility of license gate UI and functional UI based on license validity.
	 * @param bValid - true if license is valid, false otherwise
	 */
	void ToggleLicenseGate(bool bValid);

	/**
	 * Sets the UI roots for license gate and functional content.
	 * Must be called before ValidateAndGate() or ToggleLicenseGate().
	 * @param InLicenseRoot - UI box shown when license is invalid
	 * @param InFunctionalRoot - UI box shown when license is valid
	 */
	void SetUIRoots(TSharedPtr<SVerticalBox> InLicenseRoot, TSharedPtr<SVerticalBox> InFunctionalRoot);

	// Module selection state
	FModuleSelectionInfo SelectedModule;

	// UI Components
	TSharedPtr<SButton> DeleteButton;
	TSharedPtr<SDockTab> CurrentTab;
	TSharedPtr<SVerticalBox> ContentBox;

	// License UI components
	TSharedPtr<SVerticalBox> LicenseRoot;
	TSharedPtr<SVerticalBox> FunctionalRoot;
	bool bIsLicenseValid;

	// Component dependencies
	TSharedPtr<FEditorModuleDeleter> ModuleDeleter;
};
