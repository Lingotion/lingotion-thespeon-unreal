# Class `UThespeonComponent`

*Defined in: `LingotionThespeon/Public/Engine/ThespeonComponent.h`*

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

### `PreloadCharacter`
Attempts to load the files for a specific character and module type into memory.
Fires OnPreloadComplete when done. For grouped preloads use PreloadCharacterGroup.

**Parameters:**
- `CharacterName`: The name of the character.
- `ModuleType`: The specific module type to load.
- `InferenceConfig`: Optional: A specific configuration to take into account when loading.

```cpp
void PreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());
```

### `PreloadCharacterGroup`
Preloads all entries in a single atomic group.
All counts are registered before any thread starts, so OnPreloadGroupComplete
cannot fire prematurely regardless of how fast individual preloads complete.
Prefer this over calling PreloadCharacter in a loop with a shared group ID.

**Parameters:**
- `Characters`: The list of characters and module types to preload.
- `PreloadGroupId`: Identifier for the group. OnPreloadGroupComplete fires once when all are done.

```cpp
void PreloadCharacterGroup(TArray<FPreloadEntry> Characters, FString PreloadGroupId);
```

### `TryUnloadCharacter`
Attempts to unload a character of the given module type on the provided backend.
If no backend is provided, unload will be attempted for all loaded backends.
Unload fails if no loaded character matches the specified combination of character name, module type, and backend type.

**Parameters:**
- `CharacterName`: The name of the character.
- `ModuleType`: The specific module type to unload.
- `BackendType`: The specific backend type to unload. Use EBackendType::None (default) to unload all backends.

**Returns:** True if unloading succeeded.

```cpp
bool TryUnloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, EBackendType BackendType = EBackendType::None);
```

### `CancelSynthesis`
Cancels the current synthesis session if one is running.

```cpp
void CancelSynthesis();
```

### `IsSynthesizing`
Returns true if a synthesis session is currently running.

```cpp
bool IsSynthesizing();
```

## Properties

### `OnAudioReceived`
Called whenever synthesized audio is available. Replaces basic audio playback if bound.

**Parameters:**
- `SessionID`: The Session that the data comes from.
- `SynthesisData`: The audio data as an array of floats.

```cpp
FOnAudioReceived OnAudioReceived;
```

### `OnAudioSampleRequestReceived`
Called when the sample indices corresponding to the trigger input character are ready.
Combined with FOnAudioReceived, these can be used to trigger specific events when a specific word is spoken.

**Parameters:**
- `SessionID`: The Session that the data comes from.
- `TriggerAudioSamples`: The ordered audio samples corresponding to the trigger input characters.

```cpp
FOnAudioSampleRequestReceived OnAudioSampleRequestReceived;
```

### `OnSynthesisComplete`
Called when final packet is delivered to caller.

**Parameters:**
- `SessionID`: The Session that the data comes from.

```cpp
FOnSynthesisComplete OnSynthesisComplete;
```

### `OnPreloadComplete`
Called when an individual preload is complete.

**Parameters:**
- `PreloadSuccess`: True if preload succeeded. False otherwise.
- `CharacterName`: The name of the character that was attempted to be preloaded.
- `ModuleType`: The module type that was attempted to be preloaded.
- `BackendType`: The backend type that was attempted to be preloaded.

```cpp
FOnPreloadComplete OnPreloadComplete;
```

### `OnPreloadGroupComplete`
Called when all preloads in a group are complete.

**Parameters:**
- `PreloadGroupId`: The group ID passed to PreloadCharacter calls.
- `bAllSucceeded`: True if every preload in the group succeeded.

```cpp
FOnPreloadGroupComplete OnPreloadGroupComplete;
```

### `OnSynthesisFailed`
Called when synthesis has failed.

**Parameters:**
- `SessionID`: The session ID of the failed synthesis.

```cpp
FOnSynthesisFailed OnSynthesisFailed;
```
