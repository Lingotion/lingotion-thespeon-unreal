// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Core/Module.h"
#include "Core/Language.h"
#include "Core/BackendType.h"
#include "NNEModelData.h"

namespace Thespeon
{
namespace Character
{
/**
 * Character-specific module containing voice synthesis models, language support, and phoneme encoding.
 *
 * Represents a virtual character and owns the phoneme-to-encoder vocabulary, language key mappings,
 * and character embedding key needed for the TTS encoder.
 * Loaded from a "lara" type JSON file.
 */
class CharacterModule : public Thespeon::Core::Module
{
  public:
	/** Maps language codes to their corresponding LanguageModule IDs. */
	TMap<FString, FString> LanguageModuleIDs;

	CharacterModule(const Thespeon::Core::FModuleEntry& ModuleInfo);

	/** @brief Returns the module type identifier ("character").
	 *  @return The string "character". */
	FString GetModuleType() const override
	{
		return TEXT("character");
	}

	/** @brief Gets all MD5s of the workload files in this module.
	 *  @return Set of MD5 hash strings for every workload file in this module. */
	TSet<FString> GetAllWorkloadMD5s() const override;

	/** @brief Checks if this module's files are fully included in the provided MD5 set for the given backend.
	 *  @param MD5s Set of MD5 hashes to check against.
	 *  @param BackendType The backend type to check for.
	 *  @return True if all module files are present in MD5s. */
	bool IsIncludedIn(const TSet<FString>& MD5s, EBackendType BackendType) const override;

	/**
	 * @brief Encodes a phoneme string into neural network input IDs.
	 * @param Phonemes The phoneme string to encode.
	 * @return Array of encoded IDs (-1 for phonemes not found in the vocabulary).
	 */
	TArray<int64> EncodePhonemes(const FString& Phonemes);

	/** @brief Gets the language key for a given language, used as a numerical identifier by the ML model.
	 *  @param Language The language to look up.
	 *  @return The language key for the ML model. */
	int64 GetLangKey(const FLingotionLanguage& Language);

	/** @brief Gets the character embedding key used by the ML model.
	 *  @return The character key. */
	int64 GetCharacterKey() const;

	/** @brief Gets all languages supported by this character module.
	 *  @return Array of supported languages. */
	TArray<FLingotionLanguage> GetSupportedLanguages() const;

  protected:
	/** @brief Initializes the character module from a JSON definition string.
	 *  @param JsonString The JSON content to parse.
	 *  @return True if initialization succeeded. */
	bool InitializeFromJSON(const FString& JsonString) final;

  private:
	TMap<FLingotionLanguage, int64> LangToLangKey;
	int64 CharacterKey;
	TMap<FString, int64> PhonemeToEncoderKeys;

	bool LoadCharacterConfiguration(const TSharedPtr<FJsonObject>& JsonObject);
	bool ParseJSON(const TSharedPtr<FJsonObject>& JsonObject, const TArray<TSharedPtr<FJsonValue>>& ModuleFiles);
};
} // namespace Character
} // namespace Thespeon
