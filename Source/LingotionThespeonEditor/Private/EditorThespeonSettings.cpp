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
		if (GetLicenseKey().IsEmpty())
		{
			ValidationState = ELicenseValidationState::Empty;
		}
		else
		{
			if (ValidationState == ELicenseValidationState::Empty)
			{
				ValidationState = ELicenseValidationState::None;
			}
			FEditorLicenseKeyValidator::ValidateLicenseAsync(FEditorLicenseKeyValidator::FOnLicenseValidationResult());
		}
	}
}
