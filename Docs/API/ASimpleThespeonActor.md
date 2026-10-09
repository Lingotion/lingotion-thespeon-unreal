# Class `ASimpleThespeonActor`

*Defined in: `LingotionThespeon/Public/Samples/MinimalActorExample/SimpleThespeonActor.h`*

A simple ready-to-use actor with a built-in UThespeonComponent for quick speech synthesis.
Drop this actor into a level and configure its test properties in the Details panel
to quickly test text-to-speech without creating a custom actor. If bAutoSynthesizeOnBeginPlay
is enabled, synthesis starts about one second after the game begins.
The emotion is fixed to Interest; build your own FLingotionModelInput to use other emotions.

## Functions

### `OnAudioReceived`
Forwards received audio to AudioStreamComponent for playback. Bound in BeginPlay.

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
Name of the character to use for test synthesis. Must be set to an imported character name.

```cpp
FString TestCharacterName = TEXT("DefaultCharacter");
```

### `TestModuleType`
Module quality tier (XL to XS) to use for test synthesis. None uses the project's default module type.

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
When true, synthesizes TestTextToSynthesize about one second after BeginPlay (skipped if the text is empty), with session ID
"AutoConfigSession".

```cpp
bool bAutoSynthesizeOnBeginPlay = false;
```
