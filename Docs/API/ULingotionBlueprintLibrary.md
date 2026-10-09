# Class `ULingotionBlueprintLibrary`

*Defined in: `LingotionThespeon/Public/Core/LingotionBlueprintLibrary.h`*

Blueprint function library providing Lingotion Thespeon utility functions.
Exposes JSON parsing, input validation, audio saving, control character helpers, the warmup session ID
and logging verbosity to Blueprints.

## Functions

### `ParseModelInputFromJson`
Loads a JSON file and parses it into an FLingotionModelInput structure.

**Parameters:**
- `FilePath`: The path to the JSON file to load.
- `OutModelInput`: Receives the parsed model input on success. May be partly filled on failure.

**Returns:** true if parsing succeeded; false if the file has no "actorName" field or a present field fails to parse.

```cpp
static bool ParseModelInputFromJson(const FString& FilePath, FLingotionModelInput& OutModelInput);
```

### `ValidateCharacterModule`
Validates that the selected character module (character name + module type) has been imported into the project and modifies ModelInput
with a fallback character module if not. If the character itself is not imported, an arbitrary imported character is
used instead, with a warning.

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
Also cleans text, splits numbers into extra segments and fills keypoints, so the segment count can change.
ModelInput is modified even when this returns false.

**Parameters:**
- `ModelInput`: The model input instance to validate and populate with fallbacks.
- `FallbackModuleType`: Module type to fall back to if the selected one is unavailable.
- `FallbackLanguage`: Replaces the input's DefaultLanguage when it is undefined.
- `FallbackEmotion`: Replaces the input's DefaultEmotion when it is None.
- `OutModelInput`: Receives the validated and potentially modified model input.

**Returns:** true if the input is valid or was successfully corrected with fallbacks.

```cpp
static bool ValidateAndPopulate( UPARAM(ref) FLingotionModelInput& ModelInput, EThespeonModuleType FallbackModuleType, FLingotionLanguage FallbackLanguage, EEmotion FallbackEmotion, FLingotionModelInput& OutModelInput );
```

### `SaveAudioAsWav`
Saves synthesized audio samples as a 32-bit float, mono, 44100 Hz .wav file. An existing file is overwritten.

**Parameters:**
- `Filename`: Path of the .wav file to write.
- `Samples`: The audio samples to save.

**Returns:** true if the file was written; false if Samples is empty (with an error logged) or the file write fails.

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
Messages more detailed than this level are not logged. The change is not saved to config, but it writes the settings
object directly, so in the editor it outlives PIE and shows in Project Settings for the rest of the session.

**Parameters:**
- `VerbosityLevel`: The specific level to set the logging to.

```cpp
static void SetVerbosityLevel(EVerbosityLevel VerbosityLevel);
```

### `GetVerbosityLevel`
Returns the current verbosity level, including any change made by SetVerbosityLevel.

**Returns:** The current verbosity level.

```cpp
static EVerbosityLevel GetVerbosityLevel();
```
