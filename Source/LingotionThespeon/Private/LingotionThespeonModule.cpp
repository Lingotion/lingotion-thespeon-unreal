// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "LingotionThespeonModule.h"
#include "Core/LingotionLogger.h"

#define LOCTEXT_NAMESPACE "FLingotionThespeonModule"

void FLingotionThespeonModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("LingotionThespeon module started"));
}

void FLingotionThespeonModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLingotionThespeonModule, LingotionThespeon)
