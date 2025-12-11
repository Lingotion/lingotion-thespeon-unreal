# Class `UThespeonComponent`

Main component needed to use Lingotion Thespeon. Needs to be added to an Actor.

## Functions

### `Synthesize`
Schedules an audio generation job. Characters that have not been preloaded will be loaded before audio generation starts.

**Parameters:**
- `Input`: The LingotionModelInput to use.
- `SessionId`: Optional: An identifier for this specific task.
- `InferenceConfig`: Optional: A specific configuration to take into account during synthesis.

```cpp
void Synthesize(FLingotionModelInput Input, FString SessionId = TEXT(""), FInferenceConfig InferenceConfig = FInferenceConfig());
```

### `TryPreloadCharacter`
Attempts to load the files for a specific character and module type into memory.

**Parameters:**
- `CharacterName`: The name of the character.
- `ModuleType`: The specific module type to load.
- `InferenceConfig`: Optional: A specific configuration to take into account when loading.

**Returns:** True if loading succeeded.

```cpp
bool TryPreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());
```

### `TryUnloadCharacter`
Attempts to purge the data for a specific character and module type from memory.

**Parameters:**
- `CharacterName`: The name of the character.
- `ModuleType`: The specific module type to unload.

**Returns:** True if loading succeeded.

```cpp
bool TryUnloadCharacter(FString CharacterName, EThespeonModuleType ModuleType);
```

## Properties

### `OnAudioReceived`
Called whenever synthesized audio is available. Replaces basic audio playback if bound.

```cpp
FOnAudioReceived OnAudioReceived;
```

## Delegates

### Delegate
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioReceived, const TArray<float>&, AudioData);
```
