// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "TextPreprocessor.h"
#include "Core/LingotionLogger.h"

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
    if (Char == StandardApostrophe){
		return true;
	}
	else {
		return false;
	}
}

TArray<FString> FTextPreprocessor::ExtractWords(const FString& Text)
{
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("FTextPreprocessor::ExtractWords called with text: %s"), *Text);

	TArray<FString> Words;
    int32 i = 0;
    int32 TextLen = Text.Len();
    const TCHAR* CharPtr = *Text;


    while (i < TextLen)
    {
        // Skip non-word characters
        while (i < TextLen && !IsWordCharacter(CharPtr[i]))
            ++i;
            
        int32 WordStart = i;
        
        // Collect word characters (including apostrophes now)
        while (i < TextLen && IsWordCharacter(CharPtr[i]))
            ++i;
            
        if (i > WordStart)
            Words.Add(Text.Mid(WordStart, i - WordStart));
    }
    
    return Words;
}

void FTextPreprocessor::PreprocessInput(FLingotionModelInput& Input)
{
	if (Input.Segments[0].Text.IsEmpty())
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("FTextPreprocessor::PreprocessInput - Input text is empty"));
	}

	FString OriginalText = Input.Segments[0].Text;
	Input.Segments[0].Text = CleanText(Input.Segments[0].Text);

	LINGO_LOG(EVerbosityLevel::Debug, TEXT("FTextPreprocessor::PreprocessInput - Original: '%s', Cleaned: '%s'"), 
		*OriginalText, *Input.Segments[0].Text);
}




FString FTextPreprocessor::CleanText(const FString& Input)
{
	if (Input.IsEmpty())
	{
		return Input;
	}

	FString Result = Input;

	// Convert to lowercase
	Result = Result.ToLower();

	FString TempResult;
	
	// Replace multiple whitespace characters with single space
	TempResult.Reset();
	TempResult.Reserve(Result.Len());
	
	bool bPrevWasSpace = false;
	for (int32 i = 0; i < Result.Len(); ++i)
	{
		if (FChar::IsWhitespace(Result[i]))
		{
			if (!bPrevWasSpace)
			{
				TempResult.AppendChar(TEXT(' '));
				bPrevWasSpace = true;
			}
		}
		else
		{
			TempResult.AppendChar(Result[i]);
			bPrevWasSpace = false;
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
			TCHAR OldChar = Chars[i]; // Save it BEFORE replacing
			Chars[i] = StandardApostrophe;
			bModified = true;
		}
	}

	if (bModified)
	{
		Result = FString(Chars.GetData());
	}

	// Trim leading and trailing whitespace
	Result.TrimStartAndEndInline();

	return Result;
}
