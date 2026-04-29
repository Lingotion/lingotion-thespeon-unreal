// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "EditorThespeonSettingsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "DetailWidgetRow.h"
#include "EditorThespeonSettings.h"

TSharedRef<IDetailCustomization> FEditorThespeonSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FEditorThespeonSettingsCustomization);
}

void FEditorThespeonSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{

	auto GetWarningText = []() -> FText
	{
		const auto* Settings = GetDefault<UEditorThespeonSettings>();
		switch (Settings->ValidationState)
		{
			case ELicenseValidationState::Empty:
				return FText::FromString("No license key entered.");
			case ELicenseValidationState::Invalid:
				return FText::FromString("Invalid license key.");
			case ELicenseValidationState::Valid:
				return FText::FromString("License key is valid!");
			default:
				return FText::GetEmpty();
		}
	};

	auto GetTextColor = []() -> FSlateColor
	{
		const auto* Settings = GetDefault<UEditorThespeonSettings>();
		switch (Settings->ValidationState)
		{
			case ELicenseValidationState::Valid:
				return FSlateColor(FLinearColor::Green);
			default:
				return FSlateColor(FLinearColor::Yellow);
		}
	};

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("License");
	auto row = Category.AddCustomRow(FText::FromString("LicenseWarning"))
	               .WholeRowContent()[SNew(STextBlock)
	                                      .Text(TAttribute<FText>::CreateLambda(GetWarningText))
	                                      .ColorAndOpacity(TAttribute<FSlateColor>::CreateLambda(GetTextColor))];

	// We show the warningtext instead of the value of the enum, so hide the enum prop
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UEditorThespeonSettings, ValidationState));
}
