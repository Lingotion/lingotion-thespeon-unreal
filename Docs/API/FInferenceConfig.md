# Struct `FInferenceConfig`

*Defined in: `LingotionThespeon/Public/Engine/InferenceConfig.h`*

Configuration parameters for a text-to-speech inference session.
Bundles together the backend type, audio buffering, module quality tier,
fallback emotion/language, and thread priority settings used when
running synthesis on a UThespeonComponent.

## Properties

### `BackendType`
The NNE backend to use for model inference (CPU or GPU). None uses the project default. GPU is Windows-only; other platforms fall back to CPU.

```cpp
EBackendType BackendType;
```

### `bForceRequestedBackend`
Forces every model to run on BackendType even when its metagraph node declares a preferred device.

```cpp
bool bForceRequestedBackend;
```

### `BufferSeconds`
Seconds of audio to buffer before the first OnAudioReceived broadcast. Must be >= 0.
Currently always read from the project's runtime settings; a per-request value is ignored.

```cpp
float BufferSeconds;
```

### `ModuleType`
Fallback module type (XS to XL), used when the model input's ModuleType is None or not imported. Preload and unload calls ignore it.

```cpp
EThespeonModuleType ModuleType;
```

### `FallbackEmotion`
Used when the model input's DefaultEmotion is None. It only takes effect if no segment sets an emotion;
otherwise segments without one interpolate from their neighbours.

```cpp
EEmotion FallbackEmotion;
```

### `FallbackLanguage`
Replaces an undefined DefaultLanguage on the model input. The result is matched against the character's languages, falling back to the first
one supported.

```cpp
FLingotionLanguage FallbackLanguage;
```

### `ThreadPriority`
The thread priority for the synthesis worker thread. Preload threads always run at normal priority.

```cpp
EThreadPriorityWrapper ThreadPriority;
```
