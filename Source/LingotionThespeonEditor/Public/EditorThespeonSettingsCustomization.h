// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "IDetailCustomization.h"

class FEditorThespeonSettingsCustomization : public IDetailCustomization
{
  public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	void OnLicenseKeyChanged(IDetailLayoutBuilder* DetailBuilder);
};
