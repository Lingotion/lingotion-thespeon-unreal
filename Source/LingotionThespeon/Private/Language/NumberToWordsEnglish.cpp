// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "NumberToWordsEnglish.h"
#include "Core/LingotionLogger.h"

using namespace Thespeon::Language;
FString FNumberToWordsEnglish::ConvertNumber(const FString& Input)
{
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Converting: %s"), *Input);

	FString OrdinalSuffix;
	FString NumberPart = Input;

	// Check for ordinal suffix (st, nd, rd, th)
	if (Input.Len() >= 3)
	{
		FString SuffixCandidate;
		int32 SuffixScanIndex = Input.Len() - 1;
		while (SuffixCandidate.Len() < 2 && SuffixScanIndex >= 0)
		{
			if (Input[SuffixScanIndex] != Thespeon::ControlCharacters::AudioSampleRequest)
			{
				SuffixCandidate.InsertAt(0, Input[SuffixScanIndex]);
			}
			SuffixScanIndex--;
		}
		const FString Suffix = SuffixCandidate.ToLower();
		if (ValidOrdinalSuffixes.Contains(Suffix))
		{
			OrdinalSuffix = Suffix;
			NumberPart = Input.Left(SuffixScanIndex + 1);
		}
	}

	return ToWords(NumberPart, !OrdinalSuffix.IsEmpty());
}

FString FNumberToWordsEnglish::ToWords(const FString& NumberWithASR, bool bAsOrdinal)
{
	// Find, record and remove all ASR positions in the number string
	FString Number;
	for (int32 i = 0; i < NumberWithASR.Len(); ++i)
	{
		if (NumberWithASR[i] != Thespeon::ControlCharacters::AudioSampleRequest)
		{
			Number.AppendChar(NumberWithASR[i]);
		}
	}
	// Try to parse as double using FCString::Atod (returns 0.0 on failure)
	// We need additional validation since Atod doesn't indicate parse failure
	double NumDecimal = FCString::Atod(*Number);

	// Validate: check that all characters are valid number characters
	// This catches cases like "abc" or "12abc" which would be partially parsed
	// Note: We accept '-' for negative numbers but not '+' as numbers in text don't use + prefix
	bool bIsValidNumber = !Number.IsEmpty();
	for (const TCHAR Char : Number)
	{
		if (!FChar::IsDigit(Char) && Char != TEXT('.') && Char != TEXT('-'))
		{
			bIsValidNumber = false;
			break;
		}
	}

	if (!bIsValidNumber)
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Not a valid number: %s"), *Number);
		return Number; // Return original if parsing fails
	}

	if (!IsSafeNumber(NumDecimal))
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Number out of safe range: %s"), *Number);
		return Number; // Return original if out of range
	}

	const double Truncated = FMath::TruncToDouble(NumDecimal);

	// Check if it's a whole number (no decimal part)
	if (FMath::IsNearlyEqual(Truncated, NumDecimal))
	{
		const int64 N = static_cast<int64>(NumDecimal);
		FString AsPhoneme = ConvertWholeNumberToWords(N);
		return bAsOrdinal ? MakeOrdinal(AsPhoneme, static_cast<int32>(N)) : AsPhoneme;
	}

	// Handle decimal numbers: "3.14" -> "three point one four"
	TArray<FString> Parts;
	Number.ParseIntoArray(Parts, TEXT("."));

	if (Parts.Num() < 2)
	{
		return Number; // Shouldn't happen for valid decimal numbers
	}

	const int64 IntegerPart = FCString::Atoi64(*Parts[0]);
	const FString IntegerWords = ConvertWholeNumberToWords(IntegerPart);

	// Convert each decimal digit individually
	TArray<FString> DecimalWords;
	for (const TCHAR Digit : Parts[1])
	{
		if (FChar::IsDigit(Digit))
		{
			const int32 D = Digit - TEXT('0');
			if (D >= 0 && D < LessThanTwenty.Num())
			{
				DecimalWords.Add(LessThanTwenty[D]);
			}
		}
	}

	return FString::Printf(TEXT("%s %s %s"), *IntegerWords, *PointPhoneme, *FString::Join(DecimalWords, TEXT(" ")));
}

FString FNumberToWordsEnglish::ConvertWholeNumberToWords(int64 Number)
{
	if (Number == 0)
	{
		return LessThanTwenty[0]; // "zero"
	}

	if (Number < 0)
	{
		return MinusPhoneme + TEXT(" ") + ConvertWholeNumberToWords(FMath::Abs(Number));
	}

	// Break the number into groups of three digits
	TArray<FString> Words;
	int32 ScaleIndex = 0;

	while (Number > 0)
	{
		const int32 Chunk = static_cast<int32>(Number % 1000);
		if (Chunk > 0)
		{
			FString ChunkWords = ConvertHundreds(Chunk);

			if (ScaleIndex < ScaleNames.Num() && !ScaleNames[ScaleIndex].IsEmpty())
			{
				ChunkWords += TEXT(" ") + ScaleNames[ScaleIndex];
			}

			Words.Insert(ChunkWords, 0);
		}

		Number /= 1000;
		ScaleIndex++;
	}

	return FString::Join(Words, TEXT(" ")).TrimStartAndEnd();
}

FString FNumberToWordsEnglish::ConvertHundreds(int32 Number)
{
	if (Number < 0 || Number > 999)
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Number out of range: %d"), Number);
		return TEXT("");
	}

	if (Number < 20)
	{
		return LessThanTwenty[Number];
	}

	if (Number < 100)
	{
		return ConvertTens(Number);
	}

	const int32 Hundreds = Number / 100;
	const int32 Remainder = Number % 100;

	FString HundredsPart = LessThanTwenty[Hundreds] + TEXT(" ") + HundredPhoneme;

	if (Remainder > 0)
	{
		return HundredsPart + TEXT(" ") + AndPhoneme + TEXT(" ") + ConvertTens(Remainder);
	}

	return HundredsPart;
}

FString FNumberToWordsEnglish::ConvertTens(int32 Number)
{
	if (Number < 0 || Number > 99)
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Number out of range: %d"), Number);
		return TEXT("");
	}

	if (Number < 20)
	{
		return LessThanTwenty[Number];
	}

	const int32 TensPart = Number / 10;
	const int32 OnesPart = Number % 10;

	FString Result = Tens[TensPart];
	if (OnesPart > 0)
	{
		Result += TEXT(" ") + LessThanTwenty[OnesPart];
	}

	return Result;
}

FString FNumberToWordsEnglish::MakeOrdinal(const FString& Cardinal, int32 Number)
{
	const int32 LastTwoDigits = FMath::Abs(Number) % 100;

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Number: %d, LastTwoDigits: %d"), Number, LastTwoDigits);

	// Special cases for 0-19: handles numbers like 11-19, 111-119, 211-219, etc.
	// Also handles 0 for "zeroth"
	if ((LastTwoDigits > 0 && LastTwoDigits < 20) || Number == 0)
	{
		TArray<FString> Parts;
		Cardinal.ParseIntoArray(Parts, TEXT(" "));
		if (Parts.Num() > 0 && LastTwoDigits < OrdinalLessThanTwenty.Num())
		{
			Parts.Last() = OrdinalLessThanTwenty[LastTwoDigits];
			return FString::Join(Parts, TEXT(" "));
		}
	}

	// Check if cardinal ends with tens sound (twenty, thirty, forty, etc.)
	// Note: "thirty" and "forty" end with flapped R (ɾi), others end with "ti"
	if (Cardinal.EndsWith(TEXT("ti")) || Cardinal.EndsWith(TEXT("ɾi")))
	{
		return Cardinal + OrdinalEthSuffix;
	}

	// Handle cases like "twenty-one" -> "twenty-first"
	const int32 LastDigit = FMath::Abs(Number) % 10;
	if (LastDigit != 0 && LastDigit < OrdinalLessThanTwenty.Num())
	{
		TArray<FString> Parts;
		Cardinal.ParseIntoArray(Parts, TEXT(" "));
		if (Parts.Num() > 0)
		{
			Parts.Last() = OrdinalLessThanTwenty[LastDigit];
			return FString::Join(Parts, TEXT(" "));
		}
	}

	// Default: add "-th" phoneme suffix
	return Cardinal + OrdinalThSuffix;
}

bool FNumberToWordsEnglish::IsSafeNumber(double Value)
{
	return FMath::Abs(Value) <= static_cast<double>(MAX_SAFE_INTEGER);
}
