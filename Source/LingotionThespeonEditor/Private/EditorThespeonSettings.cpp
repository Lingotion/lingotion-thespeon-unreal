// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "EditorThespeonSettings.h"
#include "EditorLicenseKeyValidator.h"
#include "CoreMinimal.h"

UEditorThespeonSettings::UEditorThespeonSettings()
{
	CategoryName = "Plugins";
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
            FEditorLicenseKeyValidator::ValidateLicenseAsync(
                FEditorLicenseKeyValidator::FOnLicenseValidationResult::CreateLambda(
                    [](bool bValid)
                    {
                        auto* Settings = GetMutableDefault<UEditorThespeonSettings>();
                        Settings -> ValidationState =
                            bValid ? ELicenseValidationState::Valid : ELicenseValidationState::Invalid;
                        // save new validation state to the ini file
                        Settings->SaveConfig(CPF_Config, *Settings->GetDefaultConfigFilename());

                    }
                ));
        }
    }
    
}

