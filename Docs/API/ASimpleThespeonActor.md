# Class `ASimpleThespeonActor`

*Defined in: `LingotionThespeon/Public/ASimpleThespeonActor.h`*

A simple ready-to-use actor with a built-in UThespeonComponent for quick speech synthesis.
Drop this actor into a level and configure its test properties in the Details panel
to quickly test text-to-speech without creating a custom actor. If bAutoSynthesizeOnBeginPlay
is enabled, synthesis starts automatically when the game begins.

## Functions

### `OnAudioReceived`
```cpp
void OnAudioReceived(FString SessionID, const TArray<float>& SynthData);
```

## Properties

### `ThespeonComponent`
The Thespeon component that handles speech synthesis.

```cpp
TObjectPtr<UThespeonComponent> ThespeonComponent;
```

### `AudioStreamComponent`
The audio stream component that plays the synthesized audio.

```cpp
TObjectPtr<UAudioStreamComponent> AudioStreamComponent;
```

### `TestCharacterName`
Name of the character voice to use for test synthesis.

```cpp
FString TestCharacterName = TEXT("DefaultCharacter");
```

### `TestModuleType`
Quality tier of the module to use for test synthesis.

```cpp
EThespeonModuleType TestModuleType;
```

### `TestLanguage`
Language to use for test synthesis.

```cpp
FLingotionLanguage TestLanguage;
```

### `TestTextToSynthesize`
Text content to synthesize during testing.

```cpp
FString TestTextToSynthesize = TEXT("Hello World");
```

### `bAutoSynthesizeOnBeginPlay`
When true, automatically starts synthesis using the test config when BeginPlay is called.

```cpp
bool bAutoSynthesizeOnBeginPlay = false;
```
