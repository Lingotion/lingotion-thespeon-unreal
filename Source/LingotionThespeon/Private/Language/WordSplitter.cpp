// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Language/WordSplitter.h"
#include "Core/text_preprocessing.pb.h"

namespace Thespeon::Language
{
bool FWordSplitter::Init(const lingotion::textpreprocessing::WordDefinition& Definition, const FCharacterClasses& Classes, FString& OutError)
{
	Start = FindCharacterClass(Classes, CodePoints::Utf8ToString(Definition.start_class()), OutError);
	Continuation = FindCharacterClass(Classes, CodePoints::Utf8ToString(Definition.continue_class()), OutError);
	Joiner = FindCharacterClass(Classes, CodePoints::Utf8ToString(Definition.joiner_class()), OutError);
	bJoinerMayEnd = Definition.joiner_may_end();
	return Start && Continuation && Joiner;
}

TArray<FWord> FWordSplitter::Split(const FString& Text, TFunctionRef<bool(const FString&)> IsKnown) const
{
	return SplitImpl(Text, &IsKnown);
}

TArray<FWord> FWordSplitter::Split(const FString& Text) const
{
	return SplitImpl(Text, nullptr);
}

TArray<FWord> FWordSplitter::SplitImpl(const FString& Text, const TFunctionRef<bool(const FString&)>* IsKnown) const
{
	const TArray<int32> Points = CodePoints::FromString(Text);
	const int32 Count = Points.Num();
	// The TCHAR index of each code point, since words are reported in TCHARs but found in code points.
	TArray<int32> CharIndex;
	CharIndex.SetNumUninitialized(Count + 1);
	CharIndex[0] = 0;
	for (int32 k = 0; k < Count; ++k)
	{
		CharIndex[k + 1] = CharIndex[k] + CodePoints::Utf16Length(Points[k]);
	}

	TArray<FWord> Words;
	int32 i = 0;
	while (i < Count)
	{
		if (!InWord(Points, i))
		{
			++i;
			continue;
		}
		int32 WordStart = i;
		while (i < Count && InWord(Points, i))
		{
			++i;
		}
		// A word does not start with characters of the continue class (marks); they are left out of it.
		while (WordStart < i && Continuation->Contains(Points[WordStart]) && !Start->Contains(Points[WordStart]))
		{
			++WordStart;
		}
		int32 WordEnd = i;
		if (bJoinerMayEnd && IsKnown)
		{
			int32 After = WordEnd;
			while (After < Count && Joiner->Contains(Points[After]))
			{
				++After;
			}
			if (After > WordEnd && (*IsKnown)(Text.Mid(CharIndex[WordStart], CharIndex[After] - CharIndex[WordStart])))
			{
				WordEnd = After;
			}
		}
		if (WordEnd > WordStart)
		{
			Words.Add({CharIndex[WordStart], Text.Mid(CharIndex[WordStart], CharIndex[WordEnd] - CharIndex[WordStart])});
		}
		i = FMath::Max(i, WordEnd);
	}
	return Words;
}

// A joiner belongs to a word only between two characters of the word classes - voicemodel-lara's contraction
// rule, so that the voice models get their words split the way they were trained.
bool FWordSplitter::InWord(const TArray<int32>& Text, int32 Index) const
{
	const int32 CodePoint = Text[Index];
	if (Start->Contains(CodePoint) || Continuation->Contains(CodePoint))
	{
		return true;
	}
	return Joiner->Contains(CodePoint) && Index > 0 && Index < Text.Num() - 1 && IsWordCharacter(Text[Index - 1]) && IsWordCharacter(Text[Index + 1]);
}

bool FWordSplitter::ContainsWord(const FString& Text) const
{
	for (const int32 CodePoint : CodePoints::FromString(Text))
	{
		if (Start->Contains(CodePoint))
		{
			return true;
		}
	}
	return false;
}
} // namespace Thespeon::Language
