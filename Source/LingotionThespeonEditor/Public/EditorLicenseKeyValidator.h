// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "EditorDataCache.h"

/** Outcome of a license check. Only Valid and Rejected change the stored ValidationState. */
enum class ELicenseCheckResult : uint8
{
	/** Server answered 200. */
	Valid,
	/** Server answered 400, 401 or 403. */
	Rejected,
	/** No definitive answer (offline, timeout, other status codes, local failure, stale key). */
	Inconclusive
};

class LINGOTIONTHESPEONEDITOR_API FEditorLicenseKeyValidator
{
  public:
	// Define callback delegate
	DECLARE_DELEGATE_OneParam(FOnLicenseValidationResult, ELicenseCheckResult);

	/**
	 * Verifies the current license key against the portal and applies a definitive result to
	 * UEditorThespeonSettings::ValidationState. The callback fires after the state has been applied.
	 */
	static void ValidateLicenseAsync(FOnLicenseValidationResult ResultCallback);

  private:
	static void BuildJsonPayload(const FString& LicenseKey, const FEditorDataCache::FSynthData& DataSnapshot, FString& OutJson);

	/** Writes Valid/Rejected into the settings and saves the ini if the state changed. Inconclusive is a no-op. */
	static void ApplyResult(ELicenseCheckResult Result);

	/** Guards against overlapping verifications double-sending / double-subtracting a snapshot. Game-thread only. */
	static bool bVerifyInFlight;

	/** Set when a verification is requested while one is in flight; a new one is dispatched once the current one completes. */
	static bool bRevalidatePending;
	/** Callbacks of requests that arrived while in flight; all are fired by the re-dispatched verification. */
	static TArray<FOnLicenseValidationResult> PendingCallbacks;
};
