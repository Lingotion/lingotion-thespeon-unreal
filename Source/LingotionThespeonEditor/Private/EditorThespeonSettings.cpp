// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "EditorThespeonSettings.h"
#include "EditorLicenseKeyValidator.h"
#include "CoreMinimal.h"

UEditorThespeonSettings::UEditorThespeonSettings()
{
	CategoryName = "Plugins";
	SectionName = "Lingotion Thespeon (Editor Tools)";
}

void UEditorThespeonSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UEditorThespeonSettings, LicenseKey))
	{
		if (LicenseKey.IsEmpty())
		{
			ValidationState = ELicenseValidationState::Empty;
		}
		else
		{
			ValidationState = ELicenseValidationState::None;
			FEditorLicenseKeyValidator::ValidateLicenseAsync(FEditorLicenseKeyValidator::FOnLicenseValidationResult::CreateLambda(
			    [](bool bValid)
			    {
				    auto* Settings = GetMutableDefault<UEditorThespeonSettings>();
				    Settings->ValidationState = bValid ? ELicenseValidationState::Valid : ELicenseValidationState::Invalid;
				    // save new validation state to the ini file
				    Settings->SaveConfig(CPF_Config, *Settings->GetDefaultConfigFilename());
			    }
			));
		}
	}
}
