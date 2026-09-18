// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "EditorDataCache.h"

class LINGOTIONTHESPEONEDITOR_API FEditorLicenseKeyValidator
{
  public:
	// Define callback delegate
	DECLARE_DELEGATE_OneParam(FOnLicenseValidationResult, bool);

	static void ValidateLicenseAsync(FOnLicenseValidationResult ResultCallback);

  private:
	static void BuildJsonPayload(const FString& LicenseKey, const FEditorDataCache::FSynthData& DataSnapshot, FString& OutJson);

	/** Guards against overlapping verifications double-sending / double-subtracting a snapshot. Game-thread only. */
	static bool bVerifyInFlight;
};
