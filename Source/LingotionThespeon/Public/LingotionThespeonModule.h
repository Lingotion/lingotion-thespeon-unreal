// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "Modules/ModuleManager.h"

class LINGOTIONTHESPEON_API FLingotionThespeonModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	static void SomeFunction();
};
