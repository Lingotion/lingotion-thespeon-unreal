# Class `ULingotionBlueprintLibrary`

## Functions

### `ParseModelInputFromJson`
```cpp
static bool ParseModelInputFromJson(const FString& JsonString, FLingotionModelInput& OutModelInput);
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
