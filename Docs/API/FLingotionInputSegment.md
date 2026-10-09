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
Legacy single emotion. Not read by inference: set StartEmotion and EndEmotion instead. The constructor that takes an EEmotion copies it into
both.

```cpp
EEmotion Emotion = EEmotion::None;
```

### `StartEmotion`
The emotion blend at the start of this segment. Keys are emotions, values are their intensities. Weights are clamped to [0, 1], None keys are
removed, and the rest is normalized to sum to 1. The default {None: 1} means unset: the value is interpolated from neighbouring segments, or
DefaultEmotion is used if no segment sets one.

```cpp
TMap<EEmotion, float> StartEmotion;
```

### `EndEmotion`
The emotion blend at the end of this segment. Keys are emotions, values are their intensities. Weights are clamped to [0, 1], None keys are
removed, and the rest is normalized to sum to 1. The default {None: 1} means unset: the value is interpolated from neighbouring segments, or
DefaultEmotion is used if no segment sets one.

```cpp
TMap<EEmotion, float> EndEmotion;
```

### `Language`
The language/dialect of this segment. If undefined, or not spoken by the character, the parent FLingotionModelInput's DefaultLanguage is used.

```cpp
FLingotionLanguage Language;
```

### `bIsCustomPronounced`
When true, the Text is treated as a custom phonetic pronunciation (IPA) rather than normal text.

```cpp
bool bIsCustomPronounced = false;
```

### `StartSpeed`
Speech-rate multiplier at the start of this segment (1.0 = normal). Interpolated linearly to EndSpeed within the segment. Not range-checked.

```cpp
float StartSpeed = 1.0f;
```

### `EndSpeed`
Speech-rate multiplier at the end of this segment (1.0 = normal). Not range-checked.

```cpp
float EndSpeed = 1.0f;
```

### `StartLoudness`
Loudness multiplier at the start of this segment (1.0 = normal). Interpolated linearly to EndLoudness within the segment. Not range-checked.

```cpp
float StartLoudness = 1.0f;
```

### `EndLoudness`
Loudness multiplier at the end of this segment (1.0 = normal). Not range-checked.

```cpp
float EndLoudness = 1.0f;
```
