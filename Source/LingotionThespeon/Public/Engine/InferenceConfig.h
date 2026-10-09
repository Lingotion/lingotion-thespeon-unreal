// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

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
 * running synthesis on a UThespeonComponent.
 */
USTRUCT(BlueprintType)
struct LINGOTIONTHESPEON_API FInferenceConfig
{
	GENERATED_BODY()

	/** The NNE backend to use for model inference (CPU or GPU). None uses the project default. GPU is Windows-only; other platforms fall back to CPU.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Configuration")
	EBackendType BackendType;

	/**
	 * Forces every model to run on BackendType even when its metagraph node declares a preferred device.
	 */
	UPROPERTY(
	    EditAnywhere,
	    BlueprintReadWrite,
	    Category = "Default Configuration",
	    meta =
	        (ToolTip =
	             "Run every model on the selected backend even when its metagraph declares a preferred device. Models pin a device because they perform poorly or incorrectly elsewhere, so enable this only for debugging. Applies when a character is first preloaded on a backend."
	        )
	)
	bool bForceRequestedBackend;

	/**
	 * Seconds of audio to buffer before the first OnAudioReceived broadcast. Must be >= 0.
	 * Currently always read from the project's runtime settings; a per-request value is ignored.
	 */
	UPROPERTY(EditAnywhere, Category = "Component Configuration", meta = (ClampMin = "0.0"))
	float BufferSeconds;

	/** Fallback module type (XS to XL), used when the model input's ModuleType is None or not imported. Preload and unload calls ignore it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Configuration")
	EThespeonModuleType ModuleType;

	/**
	 * Used when the model input's DefaultEmotion is None. It only takes effect if no segment sets an emotion;
	 * otherwise segments without one interpolate from their neighbours.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Configuration")
	EEmotion FallbackEmotion;

	/** Replaces an undefined DefaultLanguage on the model input. The result is matched against the character's languages, falling back to the first
	 * one supported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Configuration", meta = (ShowOnlyInnerProperties))
	FLingotionLanguage FallbackLanguage;

	/** The thread priority for the synthesis worker thread. Preload threads always run at normal priority. */
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
