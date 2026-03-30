// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Subsystems/EngineSubsystem.h"
#include "Containers/Map.h"
#include "Core/ModelInput.h"
#include "Module.h"
#include "Dom/JsonObject.h"
#include "Language.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ManifestHandler.generated.h"

/**
 * Display information for a character module entry, used by editor UI.
 *
 * Contains the module identifier, originating name, JSON path,
 * character name, and quality tier string.
 * For now the ModuleId is the same as the Name.
 */
struct FCharacterModuleInfo
{
	/** The unique module identifier. */
	FString ModuleID;

	/** The name of the origin file. */
	FString Name;

	/** The path to the JSON definition file. */
	FString JsonPath;

	/** The human-readable character name. */
	FString CharacterName;

	/** The quality tier string (e.g., "XS", "S", "M", "L", "XL"). */
	FString Quality;
};

/**
 * Display information for a language module entry, used by editor UI.
 *
 * Contains the module identifier, originating name, JSON path,
 * language name, and ISO 639-2 language code.
 */
struct FLanguageModuleInfo
{
	/** The unique module identifier. */
	FString ModuleID;

	/** The name of the origin file. */
	FString Name;

	/** The path to the JSON definition file. */
	FString JsonPath;

	/** The human-readable language name. */
	FString LanguageName;

	/** The ISO 639-2 (3-letter) language code. */
	FString Iso639_2;
};

/**
 * Multicast delegate broadcast when manifest data changes.
 *
 * Can be used by editor UI to refresh displayed module information.
 */
DECLARE_MULTICAST_DELEGATE(FOnDataChanged);

/**
 * Manages imported character and languages via the LingotionThespeonManifest.json registry.
 *
 * This engine subsystem reads and parses the manifest on initialization,
 * providing query methods for looking up character modules, language modules,
 * available characters, and supported languages. It serves as the central
 * registry for all imported models.
 */
UCLASS()
class LINGOTIONTHESPEON_API UManifestHandler : public UEngineSubsystem
{
	GENERATED_BODY()
  public:
	/**
	 * Initializes the subsystem and reads the manifest.
	 *
	 * @param Collection The subsystem collection this subsystem belongs to.
	 */
	void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Cleans up subsystem resources on shutdown.
	 */
	void Deinitialize() override;

	/**
	 * Reads and parses the LingotionThespeonManifest.json file from the RuntimeData directory.
	 *
	 * Populates the internal JSON root used by all query methods.
	 */
	void ReadManifest();

	/**
	 * Returns the singleton instance of this subsystem.
	 *
	 * @return Pointer to the UManifestHandler instance, or nullptr if GEngine is unavailable.
	 */
	static UManifestHandler* Get()
	{
		return GEngine ? GEngine->GetEngineSubsystem<UManifestHandler>() : nullptr;
	}

	/**
	 * Matches a string to a module type enum value.
	 *
	 * Accepts both internal strings (e.g., "ultralow", "mid", "ultrahigh")
	 * and front-end strings (e.g., "XS", "M", "XL").
	 *
	 * @param ModuleTypeString The string to match against known module type names.
	 * @return The matching EThespeonModuleType, or EThespeonModuleType::None if no match is found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	EThespeonModuleType FindModuleType(const FString& ModuleTypeString) const;

	/**
	 * Retrieves the module entry for a character at the specified quality tier.
	 *
	 * @param CharacterName The name of the character to look up.
	 * @param ModuleType The quality tier of the desired module.
	 * @return The matching FModuleEntry, or an empty entry if not found.
	 */
	Thespeon::Core::FModuleEntry GetCharacterModuleEntry(const FString& CharacterName, EThespeonModuleType ModuleType) const;

	/**
	 * Retrieves the module entry for a language module by its module name.
	 *
	 * @param ModuleName The name or identifier of the language module.
	 * @return The matching FModuleEntry, or an empty entry if not found.
	 */
	Thespeon::Core::FModuleEntry GetLanguageModuleEntry(const FString& ModuleName) const;

	/**
	 * Returns all languages supported by a character module.
	 *
	 * Creates a list of language objects for all languages that the
	 * specified character module can synthesize.
	 *
	 * @param ModuleName The character module name to query.
	 * @return An array of FLingotionLanguage objects for all supported languages.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	TArray<FLingotionLanguage> GetAllLanguagesInCharacterModule(const FString& ModuleName) const;

	/**
	 * Returns the names of all available characters across all imported files.
	 *
	 * @return A set of character name strings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	TSet<FString> GetAllAvailableCharacters() const;

	/**
	 * Returns a mapping of module quality tiers to module ID strings for a character.
	 *
	 * @param CharacterName The character name to look up.
	 * @return A map from EThespeonModuleType to the corresponding module ID string.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	TMap<EThespeonModuleType, FString> GetModuleTypesOfCharacter(const FString& CharacterName) const;

	/**
	 * Returns all character modules for display in the editor UI.
	 *
	 * @return An array of FCharacterModuleInfo structs with display information.
	 */
	TArray<FCharacterModuleInfo> GetAllCharacterModules() const;

	/**
	 * Returns all language modules for display in the editor UI.
	 *
	 * @return An array of FLanguageModuleInfo structs with display information.
	 */
	TArray<FLanguageModuleInfo> GetAllLanguageModules() const;

	/**
	 * Checks whether a file with the given MD5 hash is shared across multiple modules.
	 *
	 * @param MD5 The MD5 hash of the file to check.
	 * @return True if the file is shared by more than one module, false otherwise.
	 */
	bool IsFileShared(const FString& MD5) const;

  private:
	/** The parsed root JSON object from LingotionThespeonManifest.json  */
	TSharedPtr<FJsonObject> Root;

	/** Maps EThespeonModuleType enum values to their internal string representation (e.g., "ultralow", "mid"). */
	static inline const TMap<EThespeonModuleType, FString> ModuleTypeToString = {
	    {EThespeonModuleType::XS, TEXT("ultralow")},
	    {EThespeonModuleType::S, TEXT("low")},
	    {EThespeonModuleType::M, TEXT("mid")},
	    {EThespeonModuleType::L, TEXT("high")},
	    {EThespeonModuleType::XL, TEXT("ultrahigh")}
	};

	/** Maps EThespeonModuleType enum values to their front-end display string (e.g., "XS", "M", "XL"). */
	static inline const TMap<EThespeonModuleType, FString> ModuleTypeToFrontEndString = {
	    {EThespeonModuleType::XS, TEXT("XS")},
	    {EThespeonModuleType::S, TEXT("S")},
	    {EThespeonModuleType::M, TEXT("M")},
	    {EThespeonModuleType::L, TEXT("L")},
	    {EThespeonModuleType::XL, TEXT("XL")}
	};

	/** Maps string representations (both internal and front-end) to their EThespeonModuleType enum value. */
	static inline const TMap<FString, EThespeonModuleType> StringToModuleType = {
	    {TEXT("ultralow"), EThespeonModuleType::XS},
	    {TEXT("low"), EThespeonModuleType::S},
	    {TEXT("mid"), EThespeonModuleType::M},
	    {TEXT("high"), EThespeonModuleType::L},
	    {TEXT("ultrahigh"), EThespeonModuleType::XL},
	    {TEXT("XS"), EThespeonModuleType::XS},
	    {TEXT("S"), EThespeonModuleType::S},
	    {TEXT("M"), EThespeonModuleType::M},
	    {TEXT("L"), EThespeonModuleType::L},
	    {TEXT("XL"), EThespeonModuleType::XL}
	};

	/**
	 * Checks whether the Root JSON object is null or invalid.
	 *
	 * @return True if Root is invalid and cannot be used for queries, false otherwise.
	 */
	bool IsRootInvalid() const;

	/**
	 * Attempts to retrieve the character modules JSON object from the manifest.
	 *
	 * @param OutCharacterModulesPtr Output parameter that receives the character modules JSON object.
	 * @return True if the character modules were found and extracted, false otherwise.
	 */
	bool TryGetCharacterModules(TSharedPtr<FJsonObject>& OutCharacterModulesPtr) const;

	/**
	 * Iterates all valid character module entries, invoking Callback for each.
	 *
	 * Handles root validation and character_modules extraction before iterating.
	 * Skips entries whose value is not a valid JSON object.
	 *
	 * @param Callback Invoked with the module ID and module JSON object for each valid entry.
	 */
	void IterateCharacterModules(TFunctionRef<void(const FString&, const TSharedPtr<FJsonObject>&)> Callback) const;

	/**
	 * Reads a version object from a JSON module definition.
	 *
	 * @param ModuleObj The JSON object containing version fields (major, minor, patch).
	 * @return The parsed FVersion struct.
	 */
	Thespeon::Core::FVersion ReadVersionObject(const TSharedPtr<FJsonObject>& ModuleObj) const;

	/**
	 * Gets the front-end display string for a given module quality.
	 *
	 * @param QualityString The internal string representation of the quality.
	 * @return The front-end display string.
	 */
	FString GetQuality(const FString& QualityString, const FString& ModuleID) const;
};
