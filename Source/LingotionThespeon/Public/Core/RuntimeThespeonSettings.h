// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/ModelInput.h"
#include "Core/Language.h"
#include "RuntimeThespeonSettings.generated.h"

/**
 * Verbosity level for Lingotion Thespeon logging. Each level also includes all less detailed levels (e.g. Info includes Warning and Error).
 */
UENUM(BlueprintType)
enum class EVerbosityLevel : uint8
{
	/** Logging disabled. */
	None UMETA(DisplayName = "None"),
	/** Only errors are logged. */
	Error UMETA(DisplayName = "Error"),
	/** Errors and warnings are logged. */
	Warning UMETA(DisplayName = "Warning"),
	/** Errors, warnings, and informational messages are logged. */
	Info UMETA(DisplayName = "Info"),
	/** All messages including debug details are logged. */
	Debug UMETA(DisplayName = "Debug"),
};

/**
 * Backend selection for settings UI. Mirrors EBackendType but without the None/Default option,
 * since settings must specify a concrete backend. GPU is Windows-only; other platforms fall back to CPU with a warning.
 */
UENUM(BlueprintType)
enum class ESettingBackendType : uint8
{
	/** Run inference on the CPU. */
	CPU UMETA(DisplayName = "CPU"),
	/** Run inference on the GPU. */
	GPU UMETA(DisplayName = "GPU"),
};

/**
 * Controls the OS priority of the synthesis thread. A higher priority helps real-time generation at the cost of higher resource use.
 * Preload threads always run at normal priority.
 */
UENUM(BlueprintType)
enum class EThreadPriorityWrapper : uint8
{
	/** Default OS thread priority. */
	Normal UMETA(DisplayName = "Normal"),
	/** Slightly elevated priority for smoother real-time generation. */
	AboveNormal UMETA(DisplayName = "Above Normal"),
	/** Reduced priority to conserve resources. */
	BelowNormal UMETA(DisplayName = "Below Normal"),
	/** Maximum non-critical priority. */
	Highest UMETA(DisplayName = "Highest"),
	/** Minimum thread priority. */
	Lowest UMETA(DisplayName = "Lowest"),
	/** Marginally below normal priority. */
	SlightlyBelowNormal UMETA(DisplayName = "Slightly Below Normal"),
	/** Highest possible priority. Use with caution as it may starve other threads. */
	TimeCritical UMETA(DisplayName = "Time Critical"),
};

/**
 * Project-wide default settings for Lingotion Thespeon.
 *
 * Configured via Project Settings > Plugins > Lingotion Thespeon (Runtime).
 * These are the initial values of every newly constructed FInferenceConfig; editing them does not change configs
 * that already exist. BufferSeconds, and BackendType when a config says Default, are also read at synthesis time.
 */
UCLASS(Config = Plugins, DefaultConfig, meta = (DisplayName = "Lingotion Thespeon"))
class LINGOTIONTHESPEON_API URuntimeThespeonSettings : public UDeveloperSettings
{
	GENERATED_BODY()

  public:
#if WITH_EDITOR
	FText GetSectionText() const override
	{
		return NSLOCTEXT("LingotionThespeonRuntimeSettings", "RuntimeSettingsDisplayName", "Lingotion Thespeon (Runtime)");
	}
#endif

	FName GetSectionName() const override
	{
		return TEXT("LingotionThespeonRuntime");
	}
	URuntimeThespeonSettings();
	static URuntimeThespeonSettings* Get()
	{
		return GetMutableDefault<URuntimeThespeonSettings>();
	}
	/**
	 * Seconds of audio to buffer before the first OnAudioReceived broadcast. Higher values increase latency but reduce stuttering.
	 * Always used: per-component InferenceConfig.BufferSeconds values are currently ignored.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Component Configuration", meta = (ClampMin = "0.0"))
	float BufferSeconds = 0.5f;

	/** Default NNE backend for inference. Applies when no backend is specified in a per-component InferenceConfig. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Default Configuration",
	    DisplayName = "Default Backend",
	    meta =
	        (ToolTip =
	             "Select the default backend for inference. GPU is Windows-only; other platforms fall back to CPU. Applies when no local backend is specified in the inference config."
	        )
	)
	ESettingBackendType BackendType = ESettingBackendType::CPU;

	/** Default character module size tier (XS to XL). Applies when no module type is specified in a per-component InferenceConfig. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Default Configuration",
	    DisplayName = "Default Module Type",
	    meta = (ToolTip = "Select the default module type for inference. Applies when no module type is specified in the inference config.")
	)
	EThespeonModuleType ModuleType = EThespeonModuleType::L;

	/** Default emotion for synthesis. Applies when no emotion is specified in a per-component InferenceConfig. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Default Configuration",
	    DisplayName = "Default Emotion",
	    meta = (ToolTip = "Select the default emotion for inference. Applies when no emotion is specified in the inference config.")
	)
	EEmotion Emotion = EEmotion::Interest;

	/** Default language for synthesis. Applies when no language is specified in a per-component InferenceConfig. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Default Configuration",
	    DisplayName = "Default Language",
	    meta = (ToolTip = "Set the default language for inference. Applies when no language is specified in the inference config.")
	)
	FLingotionLanguage Language = FLingotionLanguage(TEXT("eng"));

	/** Thread priority for the synthesis thread (preload threads run at normal priority). Higher priority helps real-time generation at the cost of
	 * higher resource use. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Default Configuration",
	    DisplayName = "Default Inference Thread Priority",
	    meta =
	        (ToolTip =
	             "Sets the thread priority for the synthesis thread. Higher priority helps real-time generation at the cost of higher resource use.")
	)
	EThreadPriorityWrapper ThreadPriority = EThreadPriorityWrapper::AboveNormal;

	/** Controls which log messages are emitted. Messages more detailed than this level are suppressed. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Logging",
	    DisplayName = "Verbosity Level",
	    meta = (ToolTip = "Sets the verbosity level for Lingotion Thespeon logging. Messages with a more detailed level than this will be ignored.")
	)
	EVerbosityLevel VerbosityLevel = EVerbosityLevel::Warning;
};
