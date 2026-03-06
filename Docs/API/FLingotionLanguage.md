# Struct `FLingotionLanguage`

*Defined in: `LingotionThespeon/Public/Core/Language.h`*

Identifies a language and dialect using standard linguistic codes.
Multiple code systems are supported (ISO 639, Glottocode, ISO 3166) to allow
precise specification of languages and regional dialects. Used as a key in
character module language mappings. Can be used as a TMap key (provides GetTypeHash).
A default-constructed instance represents an undefined language ("NOLANG").

## Properties

### `ISO639_2`
ISO 639-2 three-letter language code (e.g. "eng", "swe"). Primary identifier used for language matching.

```cpp
FString ISO639_2;
```

### `ISO639_3`
ISO 639-3 three-letter language code. More specific than ISO 639-2 for distinguishing individual languages.

```cpp
FString ISO639_3;
```

### `Glottocode`
Glottocode identifier for the language family or dialect (e.g. "stan1293" for Standard English).

```cpp
FString Glottocode;
```

### `ISO3166_1`
ISO 3166-1 country code (e.g. "US", "GB"). Used to distinguish regional variants of a language.

```cpp
FString ISO3166_1;
```

### `ISO3166_2`
ISO 3166-2 subdivision code (e.g. "US-TX"). Identifies sub-national language variants.

```cpp
FString ISO3166_2;
```

### `CustomDialect`
Free-form dialect identifier for variants not covered by standard codes.

```cpp
FString CustomDialect;
```

### `Name`
Human-readable display name, auto-generated from ISO639_2 and ISO3166_1 (e.g. "eng US").

```cpp
FString Name;
```
