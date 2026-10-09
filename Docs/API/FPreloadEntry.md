# Struct `FPreloadEntry`

*Defined in: `LingotionThespeon/Public/Engine/ThespeonComponent.h`*

A single entry in a PreloadCharacterGroup call.

## Properties

### `CharacterName`
The name of the character to preload. Must exactly match an imported character.

```cpp
FString CharacterName;
```

### `ModuleType`
The module type to preload. Must exactly match an imported module of the character.

```cpp
EThespeonModuleType ModuleType = EThespeonModuleType::None;
```

### `InferenceConfig`
Optional: selects the backend to load on. Only BackendType and bForceRequestedBackend are used for preloading.
Duplicate detection ignores bForceRequestedBackend, so a request differing only in that flag is dropped.

```cpp
FInferenceConfig InferenceConfig;
```
