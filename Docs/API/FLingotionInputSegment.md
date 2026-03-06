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
The emotion to apply to this segment. None uses the default emotion from the parent FLingotionModelInput.

```cpp
EEmotion Emotion;
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
