// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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
