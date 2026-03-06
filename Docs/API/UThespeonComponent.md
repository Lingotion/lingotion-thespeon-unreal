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

**Parameters:**
- `CharacterName`: The name of the character.
- `ModuleType`: The specific module type to load.
- `InferenceConfig`: Optional: A specific configuration to take into account when loading.

```cpp
void PreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());
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
Called when preload is complete.

**Parameters:**
- `PreloadSuccess`: True if preload succeeded. False otherwise.
- `CharacterName`: The name of the character that was attempted to be preloaded.
- `ModuleType`: The module type that was attempted to be preloaded.
- `BackendType`: The backend type that was attempted to be preloaded.

```cpp
FOnPreloadComplete OnPreloadComplete;
```

### `OnSynthesisFailed`
Called when synthesis has failed.

**Parameters:**
- `SessionID`: The session ID of the failed synthesis.

```cpp
FOnSynthesisFailed OnSynthesisFailed;
```

## Delegates

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioReceived, FString, SessionID, const TArray<float>&, SynthesisData);
```

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioSampleRequestReceived, FString, SessionID, const TArray<int64>&, TriggerAudioSamples);
```

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSynthesisComplete, FString, SessionID);
```

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams( FOnPreloadComplete, bool, PreloadSuccess, FString, CharacterName, EThespeonModuleType, ModuleType, EBackendType, BackendType );
```

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSynthesisFailed, FString, SessionID);
```
