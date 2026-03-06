// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "LingotionThespeonEditorModule.h"
#include "EditorInfoWindow.h"
#include "EditorThespeonSettings.h"
#include "EditorThespeonSettingsCustomization.h"
#include "PropertyEditorModule.h"
#include "Core/LingotionLogger.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "Framework/Docking/TabManager.h"

#define LOCTEXT_NAMESPACE "LingotionThespeonEditorModule"

static const FName LingotionThespeonEditorTabName("Lingotion Thespeon Info");

void FLingotionThespeonEditorModule::StartupModule()
{
	// Initialize the settings and its custom tab
	UEditorThespeonSettings::StaticClass();
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
	    UEditorThespeonSettings::StaticClass()->GetFName(),
	    FOnGetDetailCustomizationInstance::CreateStatic(&FEditorThespeonSettingsCustomization::MakeInstance)
	);

	// Create component instances
	InfoWindow = MakeShared<FEditorInfoWindow>();

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FLingotionThespeonEditorModule::RegisterMenus));
}

void FLingotionThespeonEditorModule::ShutdownModule()
{
	// Clean up component instances
	InfoWindow.Reset();

	if (UToolMenus* Menus = UToolMenus::Get())
	{
		Menus->UnregisterOwner(this);
	}
	if (FGlobalTabmanager::Get()->HasTabSpawner(LingotionThespeonEditorTabName))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LingotionThespeonEditorTabName);
	}
}

void FLingotionThespeonEditorModule::RegisterMenus()
{
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("RegisterMenus() running"));
	FToolMenuOwnerScoped OwnerScoped(this);

	// Register the tab spawner
	FGlobalTabmanager::Get()
	    ->RegisterNomadTabSpawner(LingotionThespeonEditorTabName, FOnSpawnTab::CreateRaw(this, &FLingotionThespeonEditorModule::OnSpawnPluginTab))
	    .SetDisplayName(LOCTEXT("ThespeonInfoWindowTabTitle", "Lingotion Thespeon Info"))
	    .SetMenuType(ETabSpawnerMenuType::Hidden);

	// Extend the Level Editor Window menu
	if (UToolMenus* Menus = UToolMenus::Get())
	{
		UToolMenu* WindowMenu = Menus->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = WindowMenu->FindOrAddSection("Lingotion Thespeon");

		Section.AddMenuEntry(
		    "OpenThespeonInfoWindow",
		    LOCTEXT("ThespeonInfoWindowMenuEntry", "Lingotion Thespeon Info"),
		    LOCTEXT("ThespeonInfoWindowMenuEntryTooltip", "Open the Thespeon Info window."),
		    FSlateIcon(),
		    FUIAction(FExecuteAction::CreateRaw(this, &FLingotionThespeonEditorModule::OpenLingotionThespeonEditorTab))
		);
	}
}

void FLingotionThespeonEditorModule::OpenLingotionThespeonEditorTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(LingotionThespeonEditorTabName);
}

TSharedRef<SDockTab> FLingotionThespeonEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& Args)
{
	if (!InfoWindow.IsValid())
	{
		InfoWindow = MakeShared<FEditorInfoWindow>();
	}

	return InfoWindow->CreateTab(Args);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLingotionThespeonEditorModule, LingotionThespeonEditor)
