// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "EditorInfoWindow.h"
#include "EditorModuleDeleter.h"
#include "EditorFileImporter.h"
#include "EditorThespeonSettings.h"
#include "EditorLicenseKeyValidator.h"
#include "Core/ManifestHandler.h"
#include "Core/LingotionLogger.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"
#include "Editor.h"
#include "TimerManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/Application/SlateApplication.h"
#include "UEditorFileWatcher.h"

#define LOCTEXT_NAMESPACE "EditorInfoWindow"

FEditorInfoWindow::FEditorInfoWindow() : bIsLicenseValid(false) {}

FEditorInfoWindow::~FEditorInfoWindow() {}

TSharedRef<SDockTab> FEditorInfoWindow::CreateTab(const FSpawnTabArgs& Args)
{
	// Reset selection when opening tab
	SelectedModule.Reset();

	// Create the main vertical box to hold everything
	TSharedPtr<SVerticalBox> MainBox;

	SAssignNew(MainBox, SVerticalBox);

	// --- License Key Section (shows when license is invalid) ---
	SAssignNew(LicenseRoot, SVerticalBox).Visibility(EVisibility::Collapsed); // Start collapsed, will be shown if license invalid

	// Error help box with clickable link
	LicenseRoot->AddSlot().AutoHeight().Padding(
	    10.0f, 10.0f, 10.0f, 5.0f
	)[SNew(SBorder)
	      .BorderImage(FCoreStyle::Get().GetBrush("ErrorReporting.Box"))
	      .BorderBackgroundColor(FLinearColor(0.7f, 0.2f, 0.2f, 0.8f))
	      .Padding(10.0f
	      )[SNew(STextBlock)
	            .Text(LOCTEXT(
	                "LicenseRequired",
	                "License Key Required\n\n"
	                "Your license key is required to use Lingotion Thespeon.\n Add it in Project Settings -> Plugins -> Lingotion Thespeon -> License. \n Please close and reopen this window after validating the license."
	            ))
	            .ColorAndOpacity(FLinearColor(0.95f, 0.95f, 0.95f))
	            .AutoWrapText(true)]];

	MainBox->AddSlot().AutoHeight()[LicenseRoot.ToSharedRef()];

	// --- Functional Content Section (shows when license is valid) ---
	SAssignNew(FunctionalRoot, SVerticalBox).Visibility(EVisibility::Collapsed); // Start collapsed, will be shown if license valid

	// Create the content box
	ContentBox = SNew(SVerticalBox);

	// Build the initial content
	RebuildTabContent();

	FunctionalRoot->AddSlot().FillHeight(1.0f)[SNew(SScrollBox) + SScrollBox::Slot()[ContentBox.ToSharedRef()]];

	MainBox->AddSlot().FillHeight(1.0f)[FunctionalRoot.ToSharedRef()];

	TSharedRef<SDockTab> NewTab = SNew(SDockTab)
	                                  .TabRole(ETabRole::NomadTab)
	                                  .OnTabClosed_Lambda(
	                                      [this](TSharedRef<SDockTab> ClosedTab)
	                                      {
		                                      // Clean up references when tab closes
		                                      ContentBox.Reset();
		                                      DeleteButton.Reset();
		                                      CurrentTab.Reset();
		                                      SelectedModule.Reset();
	                                      }
	                                  )[MainBox.ToSharedRef()];

	// Store reference to tab for refresh
	CurrentTab = NewTab;

	// Set UI roots and trigger validation
	SetUIRoots(LicenseRoot, FunctionalRoot);
	ValidateAndGate();

	return NewTab;
}

void FEditorInfoWindow::RebuildTabContent()
{
	if (!ContentBox.IsValid())
	{
		return;
	}

	// Clear existing content
	ContentBox->ClearChildren();

	// Get the ManifestHandler to read module data
	UManifestHandler* ManifestHandler = UManifestHandler::Get();

	// Add Import File button
	ContentBox->AddSlot().AutoHeight().Padding(5.0f
	)[SNew(SButton).Text(LOCTEXT("RunSimpleTool", "Import file")).OnClicked_Lambda([this]() { return OnImportFileClicked(); })];

	// Add Delete button
	ContentBox->AddSlot().AutoHeight().Padding(5.0f)[SAssignNew(DeleteButton, SButton)
	                                                     .Text(LOCTEXT("DeleteModule", "Delete Selected Module"))
	                                                     .IsEnabled(false)
	                                                     .OnClicked_Lambda([this]() { return OnDeleteModuleClicked(); })];

	if (ManifestHandler)
	{
		// ===== CHARACTER MODULES SECTION =====
		ContentBox->AddSlot().AutoHeight().Padding(
		    5.0f, 10.0f, 5.0f, 5.0f
		)[SNew(STextBlock).Text(LOCTEXT("CharacterModulesHeader", "Character Modules")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))];

		TArray<FCharacterModuleInfo> CharacterModules = ManifestHandler->GetAllCharacterModules();

		if (CharacterModules.Num() > 0)
		{
			for (const FCharacterModuleInfo& ModuleInfo : CharacterModules)
			{
				FString ModuleID = ModuleInfo.ModuleID;
				FString Name = ModuleInfo.Name;
				FString JsonPath = ModuleInfo.JsonPath;
				FString CharacterName = ModuleInfo.CharacterName.IsEmpty() ? TEXT("Unknown") : ModuleInfo.CharacterName;
				FString Quality = ModuleInfo.Quality.IsEmpty() ? TEXT("Unknown") : ModuleInfo.Quality;

				// Create a selectable horizontal box for this character module with context menu
				ContentBox->AddSlot().AutoHeight().Padding(
				    15.0f, 2.0f, 5.0f, 2.0f
				)[SNew(SBorder)
				      .BorderImage(FCoreStyle::Get().GetBrush("Border"))
				      .BorderBackgroundColor(this, &FEditorInfoWindow::GetModuleBackgroundColor, ModuleID)
				      .Padding(5.0f)
				      .OnMouseButtonUp(
				          this, &FEditorInfoWindow::OnModuleMouseButtonUp, ModuleID, Name, JsonPath, CharacterName, Quality, true
				      )[SNew(SHorizontalBox) +
				        SHorizontalBox::Slot().AutoWidth().Padding(
				            0.0f, 0.0f, 10.0f, 0.0f
				        )[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("• Character: %s"), *CharacterName)))] +
				        SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock)
				                                               .Text(FText::FromString(FString::Printf(TEXT("Module Type: %s"), *Quality)))
				                                               .ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))]]];
			}
		}
		else
		{
			ContentBox->AddSlot().AutoHeight().Padding(15.0f, 2.0f, 5.0f, 2.0f)[SNew(STextBlock)
			                                                                        .Text(LOCTEXT("NoCharacterModules", "No character modules found"))
			                                                                        .ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))];
		}

		// Add separator
		ContentBox->AddSlot().AutoHeight().Padding(5.0f, 10.0f, 5.0f, 5.0f)[SNew(SBorder)
		                                                                        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		                                                                        .BorderBackgroundColor(FLinearColor(0.3f, 0.3f, 0.3f, 0.5f))
		                                                                        .Padding(0.0f)[SNew(SBox).HeightOverride(1.0f)]];

		// ===== LANGUAGE MODULES SECTION =====
		ContentBox->AddSlot().AutoHeight().Padding(
		    5.0f, 5.0f, 5.0f, 5.0f
		)[SNew(STextBlock).Text(LOCTEXT("LanguageModulesHeader", "Language Modules")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))];

		TArray<FLanguageModuleInfo> LanguageModules = ManifestHandler->GetAllLanguageModules();

		if (LanguageModules.Num() > 0)
		{
			for (const FLanguageModuleInfo& ModuleInfo : LanguageModules)
			{
				FString ModuleID = ModuleInfo.ModuleID;
				FString Name = ModuleInfo.Name;
				FString JsonPath = ModuleInfo.JsonPath;
				FString LanguageName = ModuleInfo.LanguageName.IsEmpty() ? TEXT("Unknown") : ModuleInfo.LanguageName;

				// Create selectable entry for this language module with context menu
				ContentBox->AddSlot().AutoHeight().Padding(
				    15.0f, 2.0f, 5.0f, 2.0f
				)[SNew(SBorder)
				      .BorderImage(FCoreStyle::Get().GetBrush("Border"))
				      .BorderBackgroundColor(this, &FEditorInfoWindow::GetModuleBackgroundColor, ModuleID)
				      .Padding(5.0f)
				      .OnMouseButtonUp(
				          this, &FEditorInfoWindow::OnModuleMouseButtonUp, ModuleID, Name, JsonPath, LanguageName, FString(), false
				      )[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("• Language: %s"), *LanguageName)))]];
			}
		}
		else
		{
			ContentBox->AddSlot().AutoHeight().Padding(15.0f, 2.0f, 5.0f, 2.0f)[SNew(STextBlock)
			                                                                        .Text(LOCTEXT("NoLanguageModules", "No language modules found"))
			                                                                        .ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))];
		}
	}
	else
	{
		ContentBox->AddSlot().AutoHeight().Padding(5.0f)[SNew(STextBlock)
		                                                     .Text(LOCTEXT("ManifestHandlerNotFound", "ManifestHandler not available"))
		                                                     .ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f)))];
	}
}

void FEditorInfoWindow::SelectModule(const FString& ModuleID, const FString& Name, const FString& JsonPath, bool bIsCharacterModule)
{
	SelectedModule.ModuleID = ModuleID;
	SelectedModule.Name = Name;
	SelectedModule.JsonPath = JsonPath;
	SelectedModule.bIsCharacterModule = bIsCharacterModule;

	// Enable delete button
	if (DeleteButton.IsValid())
	{
		DeleteButton->SetEnabled(true);
	}

	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug, TEXT("Selected %s module: %s (Name: %s)"), bIsCharacterModule ? TEXT("Character") : TEXT("Language"), *ModuleID, *Name
	);
}

FReply FEditorInfoWindow::OnDeleteModuleClicked()
{
	if (!SelectedModule.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("No module selected for deletion"));
		return FReply::Handled();
	}

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Deleting module: %s"), *SelectedModule.ModuleID);

	FEditorModuleDeleter::DeleteModule(SelectedModule.JsonPath);
	if (UEditorFileWatcher* Watcher = UEditorFileWatcher::Get())
	{
		Watcher->UpdateMappingsInfo();
	}

	// Reload manifest and refresh UI after import
	if (UManifestHandler* Handler = UManifestHandler::Get())
	{
		Handler->ReadManifest();
	}
	// Clear selection and disable delete button
	SelectedModule.Reset();
	if (DeleteButton.IsValid())
	{
		DeleteButton->SetEnabled(false);
	}

	RefreshTab();
	return FReply::Handled();
}

FReply FEditorInfoWindow::OnImportFileClicked()
{
	FEditorFileImporter::Import();

	if (UEditorFileWatcher* Watcher = UEditorFileWatcher::Get())
	{
		Watcher->UpdateMappingsInfo();
	}

	// Reload manifest and refresh UI after import
	if (UManifestHandler* Handler = UManifestHandler::Get())
	{
		Handler->ReadManifest();
	}
	RebuildTabContent();

	return FReply::Handled();
}

void FEditorInfoWindow::RefreshTab()
{
	// Only rebuild if the tab and content box are still valid
	if (CurrentTab.IsValid() && ContentBox.IsValid())
	{
		RebuildTabContent();
	}
}

FSlateColor FEditorInfoWindow::GetModuleBackgroundColor(FString ModuleID) const
{
	return (SelectedModule.IsValid() && SelectedModule.ModuleID == ModuleID) ? FLinearColor(0.3f, 0.5f, 0.8f, 0.5f)
	                                                                         : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

FReply FEditorInfoWindow::OnModuleMouseButtonUp(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent,
    FString ModuleID,
    FString Name,
    FString JsonPath,
    FString InfoText,
    FString InfoText2,
    bool bIsCharacterModule
)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// Create context menu
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.AddMenuEntry(
		    LOCTEXT("CopyModuleInfo", "Copy Module Info"),
		    LOCTEXT("CopyModuleInfoTooltip", "Copy module information to clipboard"),
		    FSlateIcon(),
		    FUIAction(FExecuteAction::CreateLambda(
		        [InfoText, InfoText2, bIsCharacterModule]()
		        {
			        FString ClipboardText;
			        if (bIsCharacterModule && !InfoText2.IsEmpty())
			        {
				        ClipboardText = FString::Printf(TEXT("Character: %s, Module Type: %s"), *InfoText, *InfoText2);
			        }
			        else
			        {
				        ClipboardText = FString::Printf(TEXT("Language: %s"), *InfoText);
			        }
			        FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);
		        }
		    ))
		);

		MenuBuilder.AddSeparator();

		MenuBuilder.AddMenuEntry(
		    LOCTEXT("DeleteModuleContext", "Delete Module"),
		    LOCTEXT("DeleteModuleContextTooltip", "Delete this module"),
		    FSlateIcon(),
		    FUIAction(FExecuteAction::CreateLambda(
		        [this, ModuleID, Name, JsonPath, bIsCharacterModule]()
		        {
			        SelectModule(ModuleID, Name, JsonPath, bIsCharacterModule);
			        OnDeleteModuleClicked();
		        }
		    ))
		);

		FSlateApplication::Get().PushMenu(
		    FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef(),
		    FWidgetPath(),
		    MenuBuilder.MakeWidget(),
		    MouseEvent.GetScreenSpacePosition(),
		    FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);

		return FReply::Handled();
	}
	else if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		SelectModule(ModuleID, Name, JsonPath, bIsCharacterModule);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void FEditorInfoWindow::SetUIRoots(TSharedPtr<SVerticalBox> InLicenseRoot, TSharedPtr<SVerticalBox> InFunctionalRoot)
{
	LicenseRoot = InLicenseRoot;
	FunctionalRoot = InFunctionalRoot;
}

void FEditorInfoWindow::ValidateAndGate()
{
	// Validate the license, but do not rely on it to enable ThespeonWindow
	// since it will not work if the user is offline. Use cached validation.

	// Empty delegate as callback
	FEditorLicenseKeyValidator::ValidateLicenseAsync(FEditorLicenseKeyValidator::FOnLicenseValidationResult());
	const UEditorThespeonSettings* Settings = GetDefault<UEditorThespeonSettings>();
	ToggleLicenseGate(Settings->ValidationState == ELicenseValidationState::Valid);
}

void FEditorInfoWindow::ToggleLicenseGate(bool bValid)
{
	bIsLicenseValid = bValid;

	if (LicenseRoot.IsValid() && FunctionalRoot.IsValid())
	{
		if (bValid)
		{
			// Hide license UI, show functional UI
			LicenseRoot->SetVisibility(EVisibility::Collapsed);
			FunctionalRoot->SetVisibility(EVisibility::Visible);
		}
		else
		{
			// Show license UI, hide functional UI
			LicenseRoot->SetVisibility(EVisibility::Visible);
			FunctionalRoot->SetVisibility(EVisibility::Collapsed);
		}
	}
}

#undef LOCTEXT_NAMESPACE
