// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/ModelInput.h"
#include "Core/RuntimeThespeonSettings.h"
#include "LingotionBlueprintLibrary.generated.h"

/**
 * Blueprint function library providing Lingotion Thespeon utility functions.
 *
 * Exposes JSON parsing, input validation, audio saving, control character helpers, the warmup session ID
 * and logging verbosity to Blueprints.
 */
UCLASS()
class ULingotionBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

  public:
	/**
	 * @brief Loads a JSON file and parses it into an FLingotionModelInput structure.
	 *
	 * @param FilePath The path to the JSON file to load.
	 * @param OutModelInput Receives the parsed model input on success. May be partly filled on failure.
	 * @return true if parsing succeeded; false if the file has no "actorName" field or a present field fails to parse.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|JSON")
	static bool ParseModelInputFromJson(const FString& FilePath, FLingotionModelInput& OutModelInput);

	/**
	 * @brief Validates that the selected character module (character name + module type) has been imported into the project and modifies ModelInput
	 * with a fallback character module if not. If the character itself is not imported, an arbitrary imported character is
	 * used instead, with a warning.
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
	 * Also cleans text, splits numbers into extra segments and fills keypoints, so the segment count can change.
	 * ModelInput is modified even when this returns false.
	 *
	 * @param ModelInput The model input instance to validate and populate with fallbacks.
	 * @param FallbackModuleType Module type to fall back to if the selected one is unavailable.
	 * @param FallbackLanguage Replaces the input's DefaultLanguage when it is undefined.
	 * @param FallbackEmotion Replaces the input's DefaultEmotion when it is None.
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
	 * @brief Saves synthesized audio samples as a 32-bit float, mono, 44100 Hz .wav file. An existing file is overwritten.
	 *
	 * @param Filename Path of the .wav file to write.
	 * @param Samples The audio samples to save.
	 * @return true if the file was written; false if Samples is empty (with an error logged) or the file write fails.
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
	 * Messages more detailed than this level are not logged. The change is not saved to config, but it writes the settings
	 * object directly, so in the editor it outlives PIE and shows in Project Settings for the rest of the session.
	 *
	 * @param VerbosityLevel The specific level to set the logging to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Settings")
	static void SetVerbosityLevel(EVerbosityLevel VerbosityLevel)
	{
		URuntimeThespeonSettings::Get()->VerbosityLevel = VerbosityLevel;
	}

	/**
	 * @brief Returns the current verbosity level, including any change made by SetVerbosityLevel.
	 *
	 * @return The current verbosity level.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Settings")
	static EVerbosityLevel GetVerbosityLevel()
	{
		return URuntimeThespeonSettings::Get()->VerbosityLevel;
	}
};
