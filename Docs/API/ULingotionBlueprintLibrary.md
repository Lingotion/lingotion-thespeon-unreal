# Class `ULingotionBlueprintLibrary`

*Defined in: `LingotionThespeon/Public/Core/LingotionBlueprintLibrary.h`*

Blueprint function library providing Lingotion Thespeon utility functions.
Exposes JSON parsing, audio saving, and control character helpers to Blueprints.

## Functions

### `ParseModelInputFromJson`
Parses a JSON string into a FLingotionModelInput structure.

**Parameters:**
- `FilePath`: The path to the JSON file to parse.
- `OutModelInput`: Receives the parsed model input on success.

**Returns:** true if parsing succeeded.

```cpp
static bool ParseModelInputFromJson(const FString& FilePath, FLingotionModelInput& OutModelInput);
```

### `ValidateCharacterModule`
Validates that the selected character module (character name + module type) has been imported into the project and modifies ModelInput
with a fallback character module if not.

**Parameters:**
- `ModelInput`: The model input instance to validate and populate with fallbacks.
- `FallbackModuleType`: The preferred module type to fall back to if the current one is invalid but the character exists.
- `OutModelInput`: Receives the validated and potentially modified model input.

**Returns:** true if the character module is valid or a fallback was set, false if no valid character module could be found.

```cpp
static bool ValidateCharacterModule( UPARAM(ref) FLingotionModelInput& ModelInput, EThespeonModuleType FallbackModuleType, FLingotionModelInput& OutModelInput );
```

### `ValidateAndPopulate`
Validates that an entire input instance contains valid selections for currently loaded character modules.
If any part is invalid, it will attempt to set fallbacks based on what is available.

**Parameters:**
- `ModelInput`: The model input instance to validate and populate with fallbacks.
- `FallbackModuleType`: Module type to fall back to if the selected one is unavailable.
- `FallbackLanguage`: Language to fall back to for segments with undefined languages.
- `FallbackEmotion`: Emotion to fall back to for segments with None emotion.
- `OutModelInput`: Receives the validated and potentially modified model input.

**Returns:** true if the input is valid or was successfully corrected with fallbacks.

```cpp
static bool ValidateAndPopulate( UPARAM(ref) FLingotionModelInput& ModelInput, EThespeonModuleType FallbackModuleType, FLingotionLanguage FallbackLanguage, EEmotion FallbackEmotion, FLingotionModelInput& OutModelInput );
```

### `SaveAudioAsWav`
Saves returned Lingotion Synthesized data as a .wav file at the target location.

**Parameters:**
- `Filename`: Path to save file name
- `Samples`: Array of samples

**Returns:** true if succeeded, false if not.

```cpp
static bool SaveAudioAsWav(const FString& Filename, const TArray<float>& Samples);
```

### `Pause`
Returns the pause control character for inserting silence in generated dialogue.

**Returns:** A single-character string containing the pause control character.

```cpp
static FString Pause();
```

### `AudioSampleRequest`
Returns the audio sample request control character for marking positions in input text.
Thespeon finds the audio sample which best corresponds to each marked position.

**Returns:** A single-character string containing the audio sample request control character.

```cpp
static FString AudioSampleRequest();
```

### `WarmupSessionID`
Returns a warmup SessionID string that can be used to run synthesis without returning audio.

**Returns:** A string containing the specific SessionID for warmup sessions.

```cpp
static FString WarmupSessionID();
```

### `SetVerbosityLevel`
Sets the current logging level to the specified value.
Logs below that verbosity level will not be visible.

**Parameters:**
- `VerbosityLevel`: The specific level to set the logging to.

```cpp
static void SetVerbosityLevel(EVerbosityLevel VerbosityLevel);
```

### `GetVerbosityLevel`
Gets the current logging level as specified in the settings.

**Returns:** VerbosityLevel The current verbosity level

```cpp
static EVerbosityLevel GetVerbosityLevel();
```
