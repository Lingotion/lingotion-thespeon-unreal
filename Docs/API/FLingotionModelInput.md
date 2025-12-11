# Struct `FLingotionModelInput`

Model input data structure for inference.
This is a struct (not UObject) to avoid thread safety issues and simplify copying/passing.
Can be used in Blueprints as a data structure.

## Properties

### `Segments`
```cpp
TArray<FLingotionInputSegment> Segments;
```

### `ModuleType`
```cpp
EThespeonModuleType ModuleType = EThespeonModuleType::L;
```

### `CharacterName`
```cpp
FString CharacterName;
```

### `DefaultEmotion`
```cpp
EEmotion DefaultEmotion = EEmotion::Interest;
```

### `DefaultLanguage`
```cpp
FLingotionLanguage DefaultLanguage;
```
