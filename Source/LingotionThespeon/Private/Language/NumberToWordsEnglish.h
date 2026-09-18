// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "NumberConverter.h"

namespace Thespeon
{
namespace Language
{

/**
 * Converts numbers in a string to their English word representation in phonemes.
 *
 * Handles:
 * - Cardinal numbers: "123" -> phoneme representation of "one hundred and twenty three"
 * - Ordinal numbers: "1st", "2nd", "3rd", "4th" -> phoneme representation of "first", "second", etc.
 * - Decimal numbers: "3.14" -> phoneme representation of "three point one four"
 * - Large numbers: Up to quintillions (10^18)
 * - Negative numbers: "-5" -> phoneme representation of "minus five"
 *
 * All output is in IPA phonemes suitable for character module encoding.
 */
class FNumberToWordsEnglish : public FNumberConverter
{
  public:
	/**
	 * Converts a numeric string to its English phoneme representation.
	 *
	 * Detects optional ordinal suffixes (st, nd, rd, th) and converts accordingly.
	 * Examples:
	 *   "42" -> cardinal phonemes for "forty two"
	 *   "42nd" -> ordinal phonemes for "forty second"
	 *   "3.14" -> "three point one four" in phonemes
	 *
	 * @param Input - The numeric string to convert (may include ordinal suffix)
	 * @return The phoneme representation of the number
	 */
	FString ConvertNumber(const FString& Input) override;

  private:
	// ============================================================================
	// Phoneme Data - All phoneme constants centralized here
	// ============================================================================

	/** Valid ordinal suffixes (st, nd, rd, th) */
	static inline const TSet<FString> ValidOrdinalSuffixes = {TEXT("st"), TEXT("nd"), TEXT("rd"), TEXT("th")};

	/** Phonemes for numbers 0-19 */
	static inline const TArray<FString> LessThanTwenty = {
	    TEXT("zˈiəɹoʊ"),     // zero
	    TEXT("wˈʌn"),        // one
	    TEXT("tˈuː"),        // two
	    TEXT("θɹˈiː"),       // three
	    TEXT("fˈoːɹ"),       // four
	    TEXT("fˈaɪv"),       // five
	    TEXT("sˈɪks"),       // six
	    TEXT("sˈɛvən"),      // seven
	    TEXT("ˈeɪt"),        // eight
	    TEXT("nˈaɪn"),       // nine
	    TEXT("tˈɛn"),        // ten
	    TEXT("ᵻ lˈɛvən"),    // eleven
	    TEXT("twˈɛlv"),      // twelve
	    TEXT("θˈɜːtiːn"),    // thirteen
	    TEXT("fˈoːɹtiːn"),   // fourteen
	    TEXT("fˈɪftiːn"),    // fifteen
	    TEXT("sˈɪkstiːn"),   // sixteen
	    TEXT("sˈɛvəntˌiːn"), // seventeen
	    TEXT("ˈeɪtiːn"),     // eighteen
	    TEXT("nˈaɪntiːn")    // nineteen
	};

	/** Phonemes for tens (10, 20, 30, ..., 90) */
	static inline const TArray<FString> Tens = {
	    TEXT("zˈiəɹoʊ"),  // zero (placeholder)
	    TEXT("tˈɛn"),     // ten
	    TEXT("twˈɛnti"),  // twenty
	    TEXT("θˈɜːɾi"),   // thirty
	    TEXT("fˈɔːɹɾi"),  // forty
	    TEXT("fˈɪfti"),   // fifty
	    TEXT("sˈɪksti"),  // sixty
	    TEXT("sˈɛvɛnti"), // seventy
	    TEXT("ˈeɪɾi"),    // eighty
	    TEXT("nˈaɪnti")   // ninety
	};

	/** Ordinal phonemes for 0th through 19th */
	static inline const TArray<FString> OrdinalLessThanTwenty = {
	    TEXT("zˈiəɹoʊθ"),     // zeroth
	    TEXT("fˈɜːst"),       // first
	    TEXT("sˈɛkənd"),      // second
	    TEXT("θˈɜːd"),        // third
	    TEXT("fˈoːɹθ"),       // fourth
	    TEXT("fˈɪfθ"),        // fifth
	    TEXT("sˈɪksθ"),       // sixth
	    TEXT("sˈɛvənθ"),      // seventh
	    TEXT("ˈeɪtθ"),        // eighth
	    TEXT("nˈaɪnθ"),       // ninth
	    TEXT("tˈɛnθ"),        // tenth
	    TEXT("ᵻ lˈɛvənθ"),    // eleventh
	    TEXT("twˈɛlvθ"),      // twelfth
	    TEXT("θˈɜːtiːnθ"),    // thirteenth
	    TEXT("fˈoːɹtiːnθ"),   // fourteenth
	    TEXT("fˈɪftiːnθ"),    // fifteenth
	    TEXT("sˈɪkstiːnθ"),   // sixteenth
	    TEXT("sˈɛvəntˌiːnθ"), // seventeenth
	    TEXT("ˈeɪtiːnθ"),     // eighteenth
	    TEXT("nˈaɪntiːnθ")    // nineteenth
	};

	/** Scale names (thousand, million, etc.) */
	static inline const TArray<FString> ScaleNames = {
	    TEXT(""),             // 10^0 (nothing spoken)
	    TEXT("θˈaʊzənd"),     // thousand
	    TEXT("mˈɪliən"),      // million
	    TEXT("bˈɪliən"),      // billion
	    TEXT("tɹˈɪliən"),     // trillion
	    TEXT("kwɑːdɹˈɪliən"), // quadrillion
	    TEXT("kwɪntˈɪliən")   // quintillion
	};

	/** Phoneme for "point" (decimal separator) */
	static inline const FString PointPhoneme = TEXT("pˈɔɪnt");

	/** Phoneme for "minus" (negative sign) */
	static inline const FString MinusPhoneme = TEXT("mˈaɪnəs");

	/** Phoneme for "hundred" */
	static inline const FString HundredPhoneme = TEXT("hˈʌndɹəd");

	/** Phoneme for "and" (used in "one hundred and twenty") */
	static inline const FString AndPhoneme = TEXT("ˈænd");

	/** Phoneme suffix for ordinals ending in tens (-eth, e.g., "twentieth") */
	static inline const FString OrdinalEthSuffix = TEXT("əθ");

	/** Phoneme suffix for default ordinals (-th) */
	static inline const FString OrdinalThSuffix = TEXT("θ");

	// ============================================================================
	// Conversion Methods
	// ============================================================================

	/**
	 * Converts a numeric string to words, optionally as ordinal.
	 *
	 * @param Number - The numeric string (without ordinal suffix)
	 * @param bAsOrdinal - If true, output ordinal form (first, second, etc.)
	 * @return The phoneme representation
	 */
	static FString ToWords(const FString& NumberWithASR, bool bAsOrdinal = false);

	/**
	 * Converts a whole number (integer) to word phonemes.
	 * Handles grouping by thousands (million, billion, etc.)
	 *
	 * @param Number - The integer to convert
	 * @return The phoneme representation
	 */
	static FString ConvertWholeNumberToWords(int64 Number);

	/**
	 * Converts a number from 0-999 to word phonemes.
	 *
	 * @param Number - The number to convert (0-999)
	 * @return The phoneme representation
	 */
	static FString ConvertHundreds(int32 Number);

	/**
	 * Converts a number from 0-99 to word phonemes.
	 *
	 * @param Number - The number to convert (0-99)
	 * @return The phoneme representation
	 */
	static FString ConvertTens(int32 Number);

	/**
	 * Converts a cardinal phoneme string to its ordinal form.
	 *
	 * @param Cardinal - The cardinal phoneme string
	 * @param Number - The original number (used to determine correct ordinal suffix)
	 * @return The ordinal phoneme representation
	 */
	static FString MakeOrdinal(const FString& Cardinal, int32 Number);

	/**
	 * Checks if a number is within the safe conversion range.
	 *
	 * @param Value - The value to check
	 * @return true if the value can be safely converted
	 */
	static bool IsSafeNumber(double Value);
};

} // namespace Language
} // namespace Thespeon