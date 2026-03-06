// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Core/ModelInput.h"
#include "Core/Language.h"
#include "Core/LingotionLogger.h"
#include "Core/BackendType.h"
#include "Core/RuntimeThespeonSettings.h"
#include "InferenceConfig.generated.h"

/**
 * Configuration parameters for a text-to-speech inference session.
 *
 * Bundles together the backend type, audio buffering, module quality tier,
 * fallback emotion/language, and thread priority settings used when
 * running synthesis on a ThespeonComponent.
 */
USTRUCT(BlueprintType)
struct LINGOTIONTHESPEON_API FInferenceConfig
{
	GENERATED_BODY()

	/** The NNE backend to use for model inference (e.g., CPU, GPU). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Configuration")
	EBackendType BackendType;

	/** Seconds of audio to buffer before starting playback. Must be >= 0. */
	UPROPERTY(EditAnywhere, Category = "Component Configuration", meta = (ClampMin = "0.0"))
	float BufferSeconds;

	/** The quality tier of the character module to use (XS, S, M, L, XL). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Configuration")
	EThespeonModuleType ModuleType;

	/** The emotion to use when no emotion is specified per-segment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Configuration")
	EEmotion FallbackEmotion;

	/** The language to use when no language is specified per-segment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Configuration", meta = (ShowOnlyInnerProperties))
	FLingotionLanguage FallbackLanguage;

	/** The thread priority for the inference worker thread. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Configuration")
	EThreadPriorityWrapper ThreadPriority;

	/** Default constructor. Initializes values from runtime settings. */
	FInferenceConfig();

	/**
	 * @brief Returns a human-readable string representation of all configuration values.
	 *
	 * @return A formatted string describing this configuration.
	 */
	FString ToString() const;

  private:
	/**
	 * @brief Converts a settings-level backend type enum to the runtime backend type enum.
	 *
	 * @param SettingType The backend type from the project settings.
	 * @return The corresponding runtime EBackendType value.
	 */
	EBackendType SettingToBackendType(ESettingBackendType SettingType) const;
};
