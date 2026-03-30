// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/ModelInput.h"
#include "Core/RuntimeThespeonSettings.h"
#include "LingotionBlueprintLibrary.generated.h"

/**
 * Blueprint function library providing Lingotion Thespeon utility functions.
 *
 * Exposes JSON parsing, audio saving, and control character helpers to Blueprints.
 */
UCLASS()
class ULingotionBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

  public:
	/**
	 * @brief Parses a JSON string into a FLingotionModelInput structure.
	 *
	 * @param FilePath The path to the JSON file to parse.
	 * @param OutModelInput Receives the parsed model input on success.
	 * @return true if parsing succeeded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|JSON")
	static bool ParseModelInputFromJson(const FString& FilePath, FLingotionModelInput& OutModelInput);

	/**
	 * @brief Validates that the selected character module (character name + module type) has been imported into the project and modifies ModelInput
	 * with a fallback character module if not.
	 *
	 * @param ModelInput The model input instance to validate and populate with fallbacks.
	 * @param FallbackModuleType The preferred module type to fall back to if the current one is invalid but the character exists.
	 * @param OutModelInput Receives the validated and potentially modified model input.
	 * @return true if the character module is valid or a fallback was set, false if no valid character module could be found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Input Validation")
	static bool ValidateCharacterModule(
	    UPARAM(ref) FLingotionModelInput& ModelInput, EThespeonModuleType FallbackModuleType, FLingotionModelInput& OutModelInput
	);

	/**
	 * @brief Validates that an entire input instance contains valid selections for currently loaded character modules.
	 * If any part is invalid, it will attempt to set fallbacks based on what is available.
	 *
	 * @param ModelInput The model input instance to validate and populate with fallbacks.
	 * @param FallbackModuleType Module type to fall back to if the selected one is unavailable.
	 * @param FallbackLanguage Language to fall back to for segments with undefined languages.
	 * @param FallbackEmotion Emotion to fall back to for segments with None emotion.
	 * @param OutModelInput Receives the validated and potentially modified model input.
	 * @return true if the input is valid or was successfully corrected with fallbacks.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Input Validation")
	static bool ValidateAndPopulate(
	    UPARAM(ref) FLingotionModelInput& ModelInput,
	    EThespeonModuleType FallbackModuleType,
	    FLingotionLanguage FallbackLanguage,
	    EEmotion FallbackEmotion,
	    FLingotionModelInput& OutModelInput
	);

	/**
	 * @brief Saves returned Lingotion Synthesized data as a .wav file at the target location.
	 *
	 * @param Filename Path to save file name
	 * @param Samples Array of samples
	 * @return true if succeeded, false if not.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Audio")
	static bool SaveAudioAsWav(const FString& Filename, const TArray<float>& Samples);

	/**
	 * @brief Returns the pause control character for inserting silence in generated dialogue.
	 * @return A single-character string containing the pause control character.
	 */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Constants", meta = (DisplayName = "GetPauseControlCharacter"))
	static FString Pause()
	{
		return FString(1, &Thespeon::ControlCharacters::Pause);
	}

	/**
	 * @brief Returns the audio sample request control character for marking positions in input text.
	 * Thespeon finds the audio sample which best corresponds to each marked position.
	 * @return A single-character string containing the audio sample request control character.
	 */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Constants", meta = (DisplayName = "GetAudioSampleRequestControlCharacter"))
	static FString AudioSampleRequest()
	{
		return FString(1, &Thespeon::ControlCharacters::AudioSampleRequest);
	}

	/**
	 * @brief Returns a warmup SessionID string that can be used to run synthesis without returning audio.
	 * @return A string containing the specific SessionID for warmup sessions.
	 */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Constants", meta = (DisplayName = "GetWarmupSessionID"))
	static FString WarmupSessionID()
	{
		return TEXT("LINGOTION_WARMUP");
	}

	/**
	 * @brief Sets the current logging level to the specified value.
	 * Logs below that verbosity level will not be visible.
	 *
	 * @param VerbosityLevel The specific level to set the logging to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Settings")
	static void SetVerbosityLevel(EVerbosityLevel VerbosityLevel)
	{
		URuntimeThespeonSettings::Get()->VerbosityLevel = VerbosityLevel;
	}

	/**
	 * @brief Gets the current logging level as specified in the settings.
	 *
	 * @return VerbosityLevel The current verbosity level
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Settings")
	static EVerbosityLevel GetVerbosityLevel()
	{
		return URuntimeThespeonSettings::Get()->VerbosityLevel;
	}
};
