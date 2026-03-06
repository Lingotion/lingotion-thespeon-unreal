# Struct `FLingotionModelInput`

*Defined in: `LingotionThespeon/Public/Core/ModelInput.h`*

Complete input for a Thespeon speech synthesis request.
Contains one or more text segments, each with optional per-segment emotion and language overrides,
plus the character, module type, and default emotion/language that apply to the entire request.

## Properties

### `Segments`
Ordered list of text segments to synthesize. Each segment can have its own emotion and language.

```cpp
TArray<FLingotionInputSegment> Segments;
```

### `ModuleType`
Which module type of the current Thespeon character to use for synthesis. Must match an imported character module.

```cpp
EThespeonModuleType ModuleType;
```

### `CharacterName`
Name of the Thespeon character to use for synthesis. Must match an imported character module.

```cpp
FString CharacterName;
```

### `DefaultEmotion`
Default emotion applied to segments that do not specify their own emotion.

```cpp
EEmotion DefaultEmotion;
```

### `DefaultLanguage`
Default language applied to segments that do not specify their own language.

```cpp
FLingotionLanguage DefaultLanguage;
```
