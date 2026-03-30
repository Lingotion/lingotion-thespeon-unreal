# Class `UManifestHandler`

*Defined in: `LingotionThespeon/Public/Core/ManifestHandler.h`*

Manages imported character and languages via the LingotionThespeonManifest.json registry.
This engine subsystem reads and parses the manifest on initialization,
providing query methods for looking up character modules, language modules,
available characters, and supported languages. It serves as the central
registry for all imported models.

## Functions

### `FindModuleType`
Matches a string to a module type enum value.
Accepts both internal strings (e.g., "ultralow", "mid", "ultrahigh")
and front-end strings (e.g., "XS", "M", "XL").

**Parameters:**
- `ModuleTypeString`: The string to match against known module type names.

**Returns:** The matching EThespeonModuleType, or EThespeonModuleType::None if no match is found.

```cpp
EThespeonModuleType FindModuleType(const FString& ModuleTypeString);
```

### `GetAllLanguagesInCharacterModule`
Returns all languages supported by a character module.
Creates a list of language objects for all languages that the
specified character module can synthesize.

**Parameters:**
- `ModuleName`: The character module name to query.

**Returns:** An array of FLingotionLanguage objects for all supported languages.

```cpp
TArray<FLingotionLanguage> GetAllLanguagesInCharacterModule(const FString& ModuleName);
```

### `GetAllAvailableCharacters`
Returns the names of all available characters across all imported files.

**Returns:** A set of character name strings.

```cpp
TSet<FString> GetAllAvailableCharacters();
```

### `GetModuleTypesOfCharacter`
Returns a mapping of module quality tiers to module ID strings for a character.

**Parameters:**
- `CharacterName`: The character name to look up.

**Returns:** A map from EThespeonModuleType to the corresponding module ID string.

```cpp
TMap<EThespeonModuleType, FString> GetModuleTypesOfCharacter(const FString& CharacterName);
```
