# Class `UManifestHandler`

*Defined in: `LingotionThespeon/Public/Core/ManifestHandler.h`*

Manages imported characters and languages via the LingotionThespeonManifest.json registry.
This engine subsystem reads and parses the manifest on initialization,
providing query methods for looking up character modules, language modules,
available characters, and supported languages. It serves as the central
registry for all imported models.
Thread safety: the query methods are safe to call from any thread. Each takes a
snapshot of the parsed manifest under a read lock and works from that, so a
concurrent ReloadManifestFromDisk() cannot pull the data out from under an
in-flight query. ReloadManifestFromDisk() itself is game-thread only.

## Functions

### `FindModuleType`
Matches a string to a module type enum value.
Accepts both size tiers (e.g., "XS", "M", "XL") and the legacy quality
names (e.g., "ultralow", "mid", "ultrahigh").

**Parameters:**
- `ModuleTypeString`: The string to match against known module type names.

**Returns:** The matching EThespeonModuleType, or EThespeonModuleType::None if no match is found.

```cpp
EThespeonModuleType FindModuleType(const FString& ModuleTypeString) const;
```

### `GetAllLanguagesInCharacterModule`
Returns all languages supported by a character module.
Creates a list of language objects for all languages that the
specified character module can synthesize.

**Parameters:**
- `ModuleName`: The character module ID to query (e.g. a value returned by GetModuleTypesOfCharacter).

**Returns:** An array of FLingotionLanguage objects for all supported languages.

```cpp
TArray<FLingotionLanguage> GetAllLanguagesInCharacterModule(const FString& ModuleName) const;
```

### `GetAllAvailableCharacters`
Returns the names of all characters across all imported character modules.

**Returns:** A set of character name strings.

```cpp
TSet<FString> GetAllAvailableCharacters() const;
```

### `GetModuleTypesOfCharacter`
Returns a mapping of module size tiers to module ID strings for a character.

**Parameters:**
- `CharacterName`: The character name to look up.

**Returns:** A map from EThespeonModuleType to the corresponding module ID string.

```cpp
TMap<EThespeonModuleType, FString> GetModuleTypesOfCharacter(const FString& CharacterName) const;
```
