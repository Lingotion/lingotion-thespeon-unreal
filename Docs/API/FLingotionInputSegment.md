# Struct `FLingotionInputSegment`

*Defined in: `LingotionThespeon/Public/Core/ModelInput.h`*

A single segment of text input for synthesis, with its own emotion and language.
Multiple segments can be combined in FLingotionModelInput to produce speech
with per-segment emotion and language control.

## Properties

### `Text`
The text content to synthesize. May include control characters (Pause, AudioSampleRequest).

```cpp
FString Text;
```

### `Emotion`
Legacy single emotion used by the current inference path. The Advanced GUI edits StartEmotion and EndEmotion instead.

```cpp
EEmotion Emotion = EEmotion::None;
```

### `StartEmotion`
The emotion to apply to the start of this segment. TMap keys are emotions, values are their intensities (Sums to 1)

```cpp
TMap<EEmotion, float> StartEmotion;
```

### `EndEmotion`
The emotion to apply to the end of this segment. TMap keys are emotions, values are their intensities (Sums to 1)

```cpp
TMap<EEmotion, float> EndEmotion;
```

### `Language`
The language/dialect of this segment. Undefined uses the default language from the parent FLingotionModelInput.

```cpp
FLingotionLanguage Language;
```

### `bIsCustomPronounced`
When true, the Text is treated as a custom phonetic pronunciation (IPA) rather than normal text.

```cpp
bool bIsCustomPronounced = false;
```

### `StartSpeed`
```cpp
float StartSpeed = 1.0f;
```

### `EndSpeed`
```cpp
float EndSpeed = 1.0f;
```

### `StartLoudness`
```cpp
float StartLoudness = 1.0f;
```

### `EndLoudness`
```cpp
float EndLoudness = 1.0f;
```
