// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

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
class FTextPreprocessingRules;

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

	/** @brief Returns the module type identifier ("language") without needing an instance.
	 *
	 *  The static counterpart to GetModuleType(), used by UModuleManager::GetModule<T> to verify a
	 *  stored module really is a T before downcasting it.
	 *
	 *  @return The string "language". */
	static FString StaticModuleType()
	{
		return TEXT("language");
	}

	/** @brief Returns the module type identifier ("language").
	 *  @return The string "language". */
	FString GetModuleType() const override
	{
		return StaticModuleType();
	}

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

	/** @brief Gets the text preprocessing rules for this module's language, compiled once when the module loads.
	 *  @return The rules, or nullptr if the module ships without valid ones. */
	TSharedPtr<const FTextPreprocessingRules, ESPMode::ThreadSafe> GetTextPreprocessingRules() const
	{
		return TextPreprocessingRules;
	}

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
	void LoadTextPreprocessingRules();

	// Immutable once compiled, so shared by every synthesis session without locking.
	TSharedPtr<const FTextPreprocessingRules, ESPMode::ThreadSafe> TextPreprocessingRules;
};
} // namespace Language
} // namespace Thespeon
