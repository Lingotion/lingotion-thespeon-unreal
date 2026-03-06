# Struct `FInferenceConfig`

*Defined in: `LingotionThespeon/Public/Engine/InferenceConfig.h`*

Configuration parameters for a text-to-speech inference session.
Bundles together the backend type, audio buffering, module quality tier,
fallback emotion/language, and thread priority settings used when
running synthesis on a ThespeonComponent.

## Properties

### `BackendType`
The NNE backend to use for model inference (e.g., CPU, GPU).

```cpp
EBackendType BackendType;
```

### `BufferSeconds`
Seconds of audio to buffer before starting playback. Must be >= 0.

```cpp
float BufferSeconds;
```

### `ModuleType`
The quality tier of the character module to use (XS, S, M, L, XL).

```cpp
EThespeonModuleType ModuleType;
```

### `FallbackEmotion`
The emotion to use when no emotion is specified per-segment.

```cpp
EEmotion FallbackEmotion;
```

### `FallbackLanguage`
The language to use when no language is specified per-segment.

```cpp
FLingotionLanguage FallbackLanguage;
```

### `ThreadPriority`
The thread priority for the inference worker thread.

```cpp
EThreadPriorityWrapper ThreadPriority;
```
