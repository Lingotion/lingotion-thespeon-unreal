// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "Engine/DeveloperSettings.h"
#include "EditorThespeonSettings.generated.h"

UENUM()
enum class ELicenseValidationState : uint8
{
	None,
	Empty,
	Valid,
	Invalid
};

UCLASS(Config = Plugins, DefaultConfig, meta = (DisplayName = "Lingotion Thespeon"))
class LINGOTIONTHESPEONEDITOR_API UEditorThespeonSettings : public UDeveloperSettings
{
	GENERATED_BODY()

  protected:
  public:
#if WITH_EDITOR
	FText GetSectionText() const override
	{
		return NSLOCTEXT("LingotionThespeonEditorSettings", "EditorSettingsDisplayName", "Lingotion Thespeon (Editor Tools)");
	}
#endif

	FName GetSectionName() const override
	{
		return TEXT("LingotionThespeonEditor");
	}
	UEditorThespeonSettings();
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	static FString GetLicenseKey()
	{
		return Get()->LicenseKey.TrimStartAndEnd();
	}
	static UEditorThespeonSettings* Get()
	{
		return GetMutableDefault<UEditorThespeonSettings>();
	}
	UPROPERTY(Config, EditAnywhere, Category = "License")
	ELicenseValidationState ValidationState;
	UPROPERTY(Config, EditAnywhere, Category = "License", DisplayName = "License Key", meta = (ToolTip = "Enter your Lingotion Thespeon license key"))
	FString LicenseKey;
};
