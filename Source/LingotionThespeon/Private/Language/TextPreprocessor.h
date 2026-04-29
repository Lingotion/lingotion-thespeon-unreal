// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Regex.h"
#include "Core/ModelInput.h"
#include "Core/Language.h"
#include "Language/TextPosition.h"

// Forward declarations
class UThespeonInput;

/**
 * Internal static utility class for preprocessing text in the Thespeon AI text pipeline.
 *
 * Provides text cleaning functionality including:
 * - Apostrophe normalization (26 Unicode variants → standard ')
 * - Pattern removal (<<<\d+>>>)
 * - Whitespace normalization
 * - Lowercase conversion
 * - Number to word/phoneme conversion (language-specific)
 */
class LINGOTIONTHESPEON_API FTextPreprocessor
{
  public:
	// Deleted constructors - static-only utility class
	FTextPreprocessor() = delete;
	FTextPreprocessor(const FTextPreprocessor&) = delete;
	FTextPreprocessor& operator=(const FTextPreprocessor&) = delete;

	/**
	 * Cleans the input text by normalizing apostrophes, converting to lowercase,
	 * removing unwanted patterns, and normalizing whitespace.
	 * @param Input The text to clean
	 * @param Position The position of the text segment (First, Middle, Last, Only) for context-aware cleaning
	 * @return The cleaned text
	 */
	static FString CleanText(const FString& Input, ETextPosition Position);

	/**
	 * Splits a segment into multiple segments when numbers are found.
	 * Numbers are converted to phonemes and placed in separate segments marked as CustomPronounced.
	 * This prevents the phonemizer from double-processing the already-phonemized numbers.
	 *
	 * Example: "I have 44 apples" with emotion Interest becomes:
	 *   - Segment 1: "i have " (bIsCustomPronounced: false, emotion: Interest)
	 *   - Segment 2: "fˈɔːɹɾi fˈoːɹ" (bIsCustomPronounced: true, emotion: Interest)
	 *   - Segment 3: " apples" (bIsCustomPronounced: false, emotion: Interest)
	 *
	 * @param Segment The input segment to process (should already be cleaned and have language set)
	 * @return Array of segments with numbers converted and marked as CustomPronounced
	 */
	[[nodiscard]] static TArray<FLingotionInputSegment> SplitSegmentByNumbers(const FLingotionInputSegment& Segment);

	/**
	 * Extracts words from text using UE5 native methods that match the word regex pattern.
	 * Matches words including contractions (e.g., "don't", "it's").
	 * Pattern: [\p{L}\p{M}\p{N}]+(?:[''][\p{L}\p{M}\p{N}]+)*
	 * @param Text The text to extract words from
	 * @return Array of extracted words
	 */
	static TArray<FString> ExtractWords(const FString& Text);

  private:
	// Set of ambiguous apostrophe characters that should be normalized to standard apostrophe
	static const TSet<TCHAR> AmbiguousApostrophes;

	// Standard apostrophe character (U+0027) used for normalization
	static constexpr TCHAR StandardApostrophe = TEXT('\'');

	/**
	 * Helper function to check if a character is part of a word.
	 * Includes just letters
	 */
	static bool IsWordCharacter(TCHAR Char);

	/**
	 * Helper function to check if a character is an apostrophe (standard or ambiguous)
	 */
	static bool IsApostrophe(TCHAR Char);

	/**
	 * Gets the appropriate regex pattern for detecting numbers in the given language.
	 * Different languages may have different number formats.
	 *
	 * @param Iso639_2 The ISO 639-2 language code
	 * @return The regex pattern for matching numbers (including ordinal suffixes where applicable)
	 */
	static const FRegexPattern& GetNumberPattern(const FString& Iso639_2);

	/**
	 * Trims the text based on its position in the input stream.
	 *
	 * @param Text The text to trim
	 * @param Position The position of the text segment
	 * @return The trimmed text
	 */
	static FString TrimBasedOnPosition(const FString& Text, ETextPosition Position);
};
