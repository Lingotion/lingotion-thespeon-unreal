# Class `ASimpleThespeonActor`

## Functions

### `Synthesize`
```cpp
void Synthesize(FLingotionModelInput Input, FString SessionId = TEXT(""), FInferenceConfig InferenceConfig = FInferenceConfig());
```

### `TryPreloadCharacter`
```cpp
bool TryPreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());
```

## Properties

### `ThespeonComponent`
```cpp
UThespeonComponent* ThespeonComponent;
```

### `TestCharacterName`
```cpp
FString TestCharacterName = TEXT("DefaultCharacter");
```

### `TestModuleType`
```cpp
EThespeonModuleType TestModuleType;
```

### `TestLanguage`
```cpp
FLingotionLanguage TestLanguage;
```

### `TestTextToSynthesize`
```cpp
FString TestTextToSynthesize = TEXT("Hello World");
```

### `bAutoSynthesizeOnBeginPlay`
```cpp
bool bAutoSynthesizeOnBeginPlay = false;
```
