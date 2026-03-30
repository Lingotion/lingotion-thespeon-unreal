// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "TextPreprocessor.h"
#include "Core/LingotionLogger.h"
#include "NumberConverter.h"

const TSet<TCHAR> FTextPreprocessor::AmbiguousApostrophes = {
    0x2018, // LEFT SINGLE QUOTATION MARK
    0x2019, // RIGHT SINGLE QUOTATION MARK
    0x201B, // SINGLE HIGH-REVERSED-9 QUOTATION MARK
    0x02BC, // MODIFIER LETTER APOSTROPHE
    0x02BB, // MODIFIER LETTER TURNED COMMA
    0xFF07, // FULLWIDTH APOSTROPHE
    0x0060, // GRAVE ACCENT
    0x00B4, // ACUTE ACCENT
    0x2032, // PRIME
    0x275B, // HEAVY SINGLE TURNED COMMA QUOTATION MARK ORNAMENT
    0x275C, // HEAVY SINGLE COMMA QUOTATION MARK ORNAMENT
    0x02C8, // MODIFIER LETTER VERTICAL LINE
    0x02CA, // MODIFIER LETTER ACUTE ACCENT
    0x02CB, // MODIFIER LETTER GRAVE ACCENT
    0x1FEF, // GREEK VARIA
    0x1FFD, // GREEK OXIA
    0x1FBF, // GREEK PSILI
    0x1FFE, // GREEK DASIA
    0x0374, // GREEK NUMERAL SIGN
    0x0384, // GREEK TONOS
    0x055A, // ARMENIAN APOSTROPHE
    0x07F4, // NKO HIGH TONE APOSTROPHE
    0x07F5, // NKO LOW TONE APOSTROPHE
    0x05F3, // HEBREW PUNCTUATION GERESH
    0x05F4, // HEBREW PUNCTUATION GERSHAYIM
    0xFE32  // PRESENTATION FORM FOR VERTICAL EN DASH
};

bool FTextPreprocessor::IsWordCharacter(TCHAR Char)
{
	// Check for letters
	return FChar::IsAlpha(Char) || IsApostrophe(Char);
}

bool FTextPreprocessor::IsApostrophe(TCHAR Char)
{
	// Explicitly check for standard apostrophe since amibiguous apostrophes are turned into it
	return Char == StandardApostrophe;
}

TArray<FString> FTextPreprocessor::ExtractWords(const FString& Text)
{
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("called with text: %s"), *Text);
	TArray<FString> Words;
	int32 i = 0;
	int32 TextLen = Text.Len();
	const TCHAR* CharPtr = *Text;

	while (i < TextLen)
	{
		// Skip non-word characters
		while (i < TextLen && !IsWordCharacter(CharPtr[i]))
		{
			++i;
		}

		int32 WordStart = i;

		// Collect word characters (including apostrophes now)
		while (i < TextLen && IsWordCharacter(CharPtr[i]))
		{
			++i;
		}

		if (i > WordStart)
		{
			Words.Add(Text.Mid(WordStart, i - WordStart));
		}
	}

	return Words;
}

FString FTextPreprocessor::TrimBasedOnPosition(const FString& Text, ETextPosition Position)
{
	switch (Position)
	{
		case ETextPosition::First:
			return Text.TrimStart();
		case ETextPosition::Last:
			return Text.TrimEnd();
		case ETextPosition::Only:
			return Text.TrimStartAndEnd();
		case ETextPosition::Middle:
		default:
			return Text; // No trimming
	}
}

FString FTextPreprocessor::CleanText(const FString& Input, ETextPosition Position)
{
	if (Input.IsEmpty())
	{
		return Input;
	}

	FString Result = TrimBasedOnPosition(Input, Position);

	// Convert to lowercase
	Result = Result.ToLower();

	FString TempResult;

	// Replace multiple whitespace characters with single space
	TempResult.Reset();
	TempResult.Reserve(Result.Len());

	bool bPrevWasSpace = false;
	bool bPrevWasASRChar = false;
	for (int32 i = 0; i < Result.Len(); ++i)
	{
		if (FChar::IsWhitespace(Result[i]))
		{
			if (!bPrevWasSpace)
			{
				TempResult.AppendChar(TEXT(' '));
				bPrevWasSpace = true;
			}
			bPrevWasASRChar = false;
		}
		else if (Result[i] == Thespeon::ControlCharacters::AudioSampleRequest) // filter duplicate ASR chars
		{
			if (!bPrevWasASRChar)
			{
				TempResult.AppendChar(Thespeon::ControlCharacters::AudioSampleRequest);
				bPrevWasASRChar = true;
			}
			bPrevWasSpace = false;
		}
		else
		{
			TempResult.AppendChar(Result[i]);
			bPrevWasSpace = false;
			bPrevWasASRChar = false;
		}
	}

	Result = TempResult;

	// Normalize ambiguous apostrophes to standard apostrophe
	TArray<TCHAR> Chars = Result.GetCharArray();
	bool bModified = false;

	for (int32 i = 0; i < Chars.Num() - 1; ++i) // -1 to exclude null terminator
	{
		if (AmbiguousApostrophes.Contains(Chars[i]))
		{
			Chars[i] = StandardApostrophe;
			bModified = true;
		}
	}

	if (bModified)
	{
		Result = FString(Chars.GetData());
	}

	return Result;
}

const TMap<FString, FRegexPattern>& GetNumberPatterns()
{
	static const TMap<FString, FRegexPattern> NumberPatterns = {
	    {TEXT("eng"), FRegexPattern(TEXT("(\\d+(\\.\\d+)?)(st|nd|rd|th)?"))}
	    // Add new language patterns here:
	    // { TEXT("swe"), FRegexPattern(TEXT("(\\d+)")) }
	};
	return NumberPatterns;
}

const FRegexPattern& FTextPreprocessor::GetNumberPattern(const FString& Iso639_2)
{
	const FString LowerLang = Iso639_2.ToLower();
	if (const FRegexPattern* Pattern = GetNumberPatterns().Find(LowerLang))
	{
		return *Pattern;
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("No pattern for '%s', using English default"), *Iso639_2);
	return GetNumberPatterns().FindChecked(TEXT("eng"));
}

TArray<FLingotionInputSegment> FTextPreprocessor::SplitSegmentByNumbers(const FLingotionInputSegment& Segment)
{
	TArray<FLingotionInputSegment> ResultSegments;

	// If segment is already custom pronounced, don't process it
	if (Segment.bIsCustomPronounced)
	{
		ResultSegments.Add(Segment);
		return ResultSegments;
	}

	const FString Iso639_2 = Segment.Language.ISO639_2.ToLower();

	// Create the number converter for this language (returns nullptr if not supported)
	TUniquePtr<Thespeon::Language::FNumberConverter> Converter = Thespeon::Language::FNumberConverterFactory::Create(Iso639_2);
	if (!Converter)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Language '%s' not supported, returning segment unchanged"), *Iso639_2);
		ResultSegments.Add(Segment);
		return ResultSegments;
	}
	FString TextWithoutASR;
	TArray<int32> ASRPositions;
	ASRPositions.Reserve(Segment.Text.Len());

	for (int32 i = 0; i < Segment.Text.Len(); ++i)
	{
		if (Segment.Text[i] == Thespeon::ControlCharacters::AudioSampleRequest)
		{
			ASRPositions.Add(i);
		}
		else
		{
			TextWithoutASR.AppendChar(Segment.Text[i]);
		}
	}

	// Get the regex pattern for this language
	const FRegexPattern& Pattern = GetNumberPattern(Iso639_2);
	FRegexMatcher Matcher(Pattern, TextWithoutASR);

	int32 LastEnd = 0;
	bool bFoundNumbers = false;

	while (Matcher.FindNext())
	{
		bFoundNumbers = true;
		int32 MatchBegin = Matcher.GetMatchBeginning();
		int32 MatchEnd = Matcher.GetMatchEnding();

		// Count ASR characters before and within the match to adjust positions
		int32 ASRCountBeforeMatch = 0;
		int32 ASRCountWithinMatch = 0;

		for (int32 i = 0; i < ASRPositions.Num(); ++i)
		{
			// Convert ASR position from original text to stripped text coordinates
			// Since we iterate in order, i is the count of ASR chars before this one
			int32 StrippedPos = ASRPositions[i] - i;

			if (StrippedPos < MatchBegin)
			{
				ASRCountBeforeMatch++;
			}
			else if (StrippedPos < MatchEnd)
			{
				ASRCountWithinMatch++;
			}
			else
			{
				break; // No more ASR chars affect this match
			}
		}

		// Adjust match positions to original text coordinates
		MatchBegin += ASRCountBeforeMatch;
		MatchEnd += ASRCountBeforeMatch + ASRCountWithinMatch;

		// Add text segment before the number (if any)
		if (MatchBegin > LastEnd)
		{
			ResultSegments.Add(FLingotionInputSegment(Segment.Text.Mid(LastEnd, MatchBegin - LastEnd), Segment.Emotion, Segment.Language, false));

			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Added text segment: '%s'"), *ResultSegments.Last().Text);
		}

		// Get the matched number string and convert it to phonemes
		const FString NumberStr = Segment.Text.Mid(MatchBegin, MatchEnd - MatchBegin);
		const FString ConvertedNumber = Converter->ConvertNumber(NumberStr);

		// Add the converted number as a custom pronounced segment
		ResultSegments.Add(FLingotionInputSegment(
		    ConvertedNumber,
		    Segment.Emotion,
		    Segment.Language,
		    true // Mark as custom pronounced to skip phonemizer
		));

		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Converted number '%s' -> '%s' (CustomPronounced)"), *NumberStr, *ConvertedNumber);

		LastEnd = MatchEnd;
	}

	// Add any remaining text after the last number
	if (bFoundNumbers && LastEnd < Segment.Text.Len())
	{
		ResultSegments.Add(FLingotionInputSegment(Segment.Text.Mid(LastEnd), Segment.Emotion, Segment.Language, false));

		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Added trailing text segment: '%s'"), *ResultSegments.Last().Text);
	}

	// If no numbers were found, return the original segment unchanged
	if (!bFoundNumbers)
	{
		ResultSegments.Add(Segment);
	}

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Split into %d segments"), ResultSegments.Num());

	return ResultSegments;
}
