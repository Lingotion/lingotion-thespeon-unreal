# Class `URuntimeThespeonSettings`

*Defined in: `LingotionThespeon/Public/Core/RuntimeThespeonSettings.h`*

Project-wide default settings for Lingotion Thespeon.
Configured via Project Settings > Plugins > Lingotion Thespeon (Runtime).
These are the initial values of every newly constructed FInferenceConfig; editing them does not change configs
that already exist. BufferSeconds, and BackendType when a config says Default, are also read at synthesis time.

## Properties

### `BufferSeconds`
Seconds of audio to buffer before the first OnAudioReceived broadcast. Higher values increase latency but reduce stuttering.
Always used: per-component InferenceConfig.BufferSeconds values are currently ignored.

```cpp
float BufferSeconds = 0.5f;
```

### `BackendType`
Default NNE backend for inference. Applies when no backend is specified in a per-component InferenceConfig.

```cpp
ESettingBackendType BackendType = ESettingBackendType::CPU;
```

### `ModuleType`
Default character module size tier (XS to XL). Applies when no module type is specified in a per-component InferenceConfig.

```cpp
EThespeonModuleType ModuleType = EThespeonModuleType::L;
```

### `Emotion`
Default emotion for synthesis. Applies when no emotion is specified in a per-component InferenceConfig.

```cpp
EEmotion Emotion = EEmotion::Interest;
```

### `Language`
Default language for synthesis. Applies when no language is specified in a per-component InferenceConfig.

```cpp
FLingotionLanguage Language = FLingotionLanguage(TEXT("eng"));
```

### `ThreadPriority`
Thread priority for the synthesis thread (preload threads run at normal priority). Higher priority helps real-time generation at the cost of
higher resource use.

```cpp
EThreadPriorityWrapper ThreadPriority = EThreadPriorityWrapper::AboveNormal;
```

### `VerbosityLevel`
Controls which log messages are emitted. Messages more detailed than this level are suppressed.

```cpp
EVerbosityLevel VerbosityLevel = EVerbosityLevel::Warning;
```
