// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "NNEModelData.h"
#include "Core/Language.h"
#include "Core/Module.h"
#include "Core/BackendType.h"

namespace Thespeon
{
namespace Language
{
/**
 * Language-specific module handling grapheme-to-phoneme (G2P) conversion, vocabularies, and lookup tables.
 *
 * Owns the grapheme/phoneme vocabularies used for encoding text into G2P model input and decoding
 * the output.
 * Also manages the word-to-phoneme lookup table for fast common-word resolution.
 * Loaded from a "phonemizer" type JSON file.
 */
class LanguageModule : public Thespeon::Core::Module
{
  public:
	/** The language this module handles. */
	const FLingotionLanguage ModuleLanguage;

	LanguageModule(const Thespeon::Core::FModuleEntry& ModuleInfo);

	/** @brief Returns the module type identifier ("language").
	 *  @return The string "language". */
	FString GetModuleType() const override
	{
		return TEXT("language");
	}

	/** @brief Gets all MD5s of the workload files in this module.
	 *  @return Set of MD5 hash strings for every workload file in this module. */
	TSet<FString> GetAllWorkloadMD5s() const override;

	/** @brief Checks if this module's files are fully included in the provided MD5 set for the given backend.
	 *  @param MD5s Set of MD5 hashes to check against.
	 *  @param BackendType The backend type to check for.
	 *  @return True if all module files are present in MD5s. */
	bool IsIncludedIn(const TSet<FString>& MD5s, EBackendType BackendType) const override;

	/** @brief Encodes graphemes into their corresponding IDs based on the phonemizer vocabulary.
	 *  @param Graphemes The grapheme string to encode.
	 *  @return Array of encoded grapheme IDs. */
	TArray<int64> EncodeGraphemes(const FString& Graphemes);

	/** @brief Decodes phoneme IDs back into their string representation.
	 *  @param PhonemeIDs Array of phoneme IDs to decode.
	 *  @return The decoded phoneme string. */
	FString DecodePhonemes(const TArray<int64>& PhonemeIDs);

	/** @brief Inserts start-of-string and end-of-string boundary tokens into the word ID array.
	 *  @param WordIDs Array of word IDs to modify in-place. */
	void InsertStringBoundaries(TArray<int64>& WordIDs);

	/** @brief Gets the word-to-phoneme lookup table for text normalization.
	 *  @return Map of words to their phoneme representations. */
	TMap<FString, FString> GetLookupTable();

	/** @brief Gets the MD5 hash of the lookup table file.
	 *  @return The MD5 hash string. */
	FString GetLookupTableID() const;

  protected:
	/** @brief Initializes the language module from a JSON definition string.
	 *  @param JsonString The JSON content to parse.
	 *  @return True if initialization succeeded. */
	bool InitializeFromJSON(const FString& JsonString) final;

  private:
	int32 lookupTableSize;

	TMap<FString, int64> GraphemeToID;
	TMap<FString, int64> PhonemeToID;
	TMap<int64, FString> IDToPhoneme;

	bool ParseJSON(const TSharedPtr<FJsonObject>& JsonObject, const TArray<TSharedPtr<FJsonValue>>& ModuleFiles);
	void LoadVocabularies(const TSharedPtr<FJsonObject>& ModuleObj);
};
} // namespace Language
} // namespace Thespeon
