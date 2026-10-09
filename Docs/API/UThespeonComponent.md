# Class `UThespeonComponent`

*Defined in: `LingotionThespeon/Public/Engine/ThespeonComponent.h`*

Main component needed to use Lingotion Thespeon. Needs to be added to an Actor; if the Actor has no root component, one is created on register.

## Functions

### `Synthesize`
Schedules an audio generation job. Requests are queued and run one at a time per component.
Characters that have not been preloaded are loaded before audio generation starts.
An unknown CharacterName or unavailable ModuleType falls back to an imported one, with a warning.
Bind delegates before calling: a rejected request may broadcast OnSynthesisFailed before this returns.

**Parameters:**
- `Input`: The FLingotionModelInput to use.
- `SessionId`: Optional: An identifier for this specific task.
- `InferenceConfig`: Optional: A specific configuration to take into account during synthesis.

```cpp
void Synthesize(FLingotionModelInput Input, FString SessionId = TEXT(""), FInferenceConfig InferenceConfig = FInferenceConfig());
```

### `PreloadCharacter`
Attempts to load the files for a specific character and module type into memory.
Fires OnPreloadComplete when done. For grouped preloads use PreloadCharacterGroup.
The name and module type must match an imported module exactly (no fallback; None always fails).
A call that duplicates a queued preload is ignored. Queued preloads wait while a synthesis is running or
queued, and at most 4 preloads run at once. If a queued synthesis needs the same model, the preload is
folded into it and OnPreloadComplete is not broadcast.

**Parameters:**
- `CharacterName`: The name of the character.
- `ModuleType`: The specific module type to load.
- `InferenceConfig`: Optional: A specific configuration to take into account when loading.

```cpp
void PreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());
```

### `PreloadCharacterGroup`
Preloads all entries as one group.
All entries are registered before any preload starts, so OnPreloadGroupComplete
cannot fire early, however fast individual preloads complete.
Prefer this over calling PreloadCharacter in a loop when you need to know when all of them are done.
Requires a non-empty PreloadGroupId. Entries that duplicate queued requests are not counted, and if none
remain, OnPreloadGroupComplete is not broadcast.

**Parameters:**
- `Characters`: The list of characters and module types to preload.
- `PreloadGroupId`: Identifier for the group. OnPreloadGroupComplete fires once when all are done.

```cpp
void PreloadCharacterGroup(TArray<FPreloadEntry> Characters, FString PreloadGroupId);
```

### `IsLoaded`
Checks whether a character of the given module type is fully loaded on the backend
the given config resolves to, whether it was loaded by a preload or on demand by Synthesize.
Fully loaded means the character module and every imported language module it uses are loaded,
so this stays false until a running preload has finished loading everything.

**Parameters:**
- `CharacterName`: The name of the character.
- `ModuleType`: The specific module type to check.
- `InferenceConfig`: Optional: the configuration whose backend the check applies to.

**Returns:** True if the character and its language modules are loaded on that backend.

```cpp
bool IsLoaded(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());
```

### `TryUnloadCharacter`
Attempts to unload a character of the given module type on the provided backend.
If no backend is provided, unload will be attempted for all loaded backends.
Unload fails if no loaded character matches the specified combination of character name, module type, and backend type.
Runs synchronously on the calling thread and also unloads language modules no other loaded character uses.
Does not check for a running synthesis or preload of the character.

**Parameters:**
- `CharacterName`: The name of the character.
- `ModuleType`: The specific module type to unload.
- `BackendType`: The specific backend type to unload. Use EBackendType::None (default) to unload all backends.

**Returns:** True if unloading succeeded.

```cpp
bool TryUnloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, EBackendType BackendType = EBackendType::None);
```

### `CancelSynthesis`
Cancels the running synthesis session, if any. Blocks until the worker thread exits, then starts the next
queued request. Queued requests are not cleared, and no delegate is broadcast. Audio already broadcast is not
cleared; call UAudioStreamComponent::ResetBuffer to stop audio already queued for playback.

```cpp
void CancelSynthesis();
```

### `IsSynthesizing`
Returns true if a synthesis session is currently running. Stays false while a request is only queued.

```cpp
bool IsSynthesizing() const;
```

## Properties

### `OnAudioReceived`
Called whenever a chunk of synthesized audio is ready. The component does not play audio itself:
bind this and pass the samples to e.g. a UAudioStreamComponent. Broadcast on the game thread. Not called for warmup sessions.

**Parameters:**
- `SessionID`: The session that the data comes from.
- `SynthesisData`: Mono 32-bit float samples at 44100 Hz.

```cpp
FOnAudioReceived OnAudioReceived;
```

### `OnAudioSampleRequestReceived`
Called when the sample indices for the AudioSampleRequest control characters in the input are ready.
Combined with OnAudioReceived, these can be used to trigger events when a specific word is spoken.
Broadcast on the game thread, also for warmup sessions.

**Parameters:**
- `SessionID`: The session that the data comes from.
- `TriggerAudioSamples`: The audio sample indices of the AudioSampleRequest characters, in left-to-right order.

```cpp
FOnAudioSampleRequestReceived OnAudioSampleRequestReceived;
```

### `OnSynthesisComplete`
Called after the final audio packet of a session has been delivered. Broadcast on the game thread, also for
warmup sessions, after IsSynthesizing has turned false and before the next queued request starts.

**Parameters:**
- `SessionID`: The session that the data comes from.

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
- `PreloadGroupId`: The group ID passed to PreloadCharacterGroup.
- `bAllSucceeded`: True if every preload in the group succeeded.

```cpp
FOnPreloadGroupComplete OnPreloadGroupComplete;
```

### `OnSynthesisFailed`
Called on the game thread when synthesis fails, or synchronously from Synthesize or queue processing when
a request is rejected before it starts (no segments, or no imported character or module to fall back to).
Not called on CancelSynthesis, or for requests still queued when the component unregisters.

**Parameters:**
- `SessionID`: The session ID of the failed synthesis.

```cpp
FOnSynthesisFailed OnSynthesisFailed;
```
