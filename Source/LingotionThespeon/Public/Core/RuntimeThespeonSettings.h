// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/ModelInput.h"
#include "Core/Language.h"
#include "RuntimeThespeonSettings.generated.h"

/**
 * Verbosity level for Lingotion Thespeon logging. Each level includes all levels above it.
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
 * since settings must specify a concrete backend.
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
 * Thread Priority enum which controls the priority of synthesis threads. A higher priority will enable real time generation at the cost of higher
 * resource use.
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
 * These values are used as fallbacks when no per-component InferenceConfig is specified.
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
	/** Number of seconds of audio to buffer before playback begins. Higher values increase latency but reduce stuttering. */
	UPROPERTY(Config, EditAnywhere, Category = "Component Configuration", meta = (ClampMin = "0.0"))
	float BufferSeconds = 0.5f;

	/** Default NNE backend for inference. Applies when no backend is specified in a per-component InferenceConfig. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Default Configuration",
	    DisplayName = "Default Backend",
	    meta = (ToolTip = "Enter your preferred default backend for inference. Applies when no local backend is specified in the inference config.")
	)
	ESettingBackendType BackendType = ESettingBackendType::CPU;

	/** Default model quality tier. Applies when no module type is specified in a per-component InferenceConfig. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Default Configuration",
	    DisplayName = "Default Module Type",
	    meta = (ToolTip = "Enter your preferred default module type for inference. Applies when no module type is specified in the inference config.")
	)
	EThespeonModuleType ModuleType = EThespeonModuleType::L;

	/** Default emotion for synthesis. Applies when no emotion is specified in a per-component InferenceConfig. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Default Configuration",
	    DisplayName = "Default Emotion",
	    meta = (ToolTip = "Enter your preferred default emotion for inference. Applies when no emotion is specified in the inference config.")
	)
	EEmotion Emotion = EEmotion::Interest;

	/** Default language for synthesis. Applies when no language is specified in a per-component InferenceConfig. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Default Configuration",
	    DisplayName = "Default Language",
	    meta = (ToolTip = "Enter your preferred default language for inference. Applies when no language is specified in the inference config.")
	)
	FLingotionLanguage Language = FLingotionLanguage(TEXT("eng"));

	/** Thread priority for inference processing threads. Higher priority enables real-time generation at the cost of higher resource use. */
	UPROPERTY(
	    Config,
	    EditAnywhere,
	    Category = "Default Configuration",
	    DisplayName = "Default Inference Thread Priority",
	    meta =
	        (ToolTip =
	             "Sets the thread priority for inference processing threads. Higher priority will enable real time generation at the cost of higher resource use."
	        )
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
