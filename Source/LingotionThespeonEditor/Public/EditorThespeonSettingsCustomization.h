// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "IDetailCustomization.h"

class FEditorThespeonSettingsCustomization : public IDetailCustomization
{
  public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	void OnLicenseKeyChanged(IDetailLayoutBuilder* DetailBuilder);
};
