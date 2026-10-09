// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Language/UnicodeTables.h"

namespace lingotion::textpreprocessing
{
class WordDefinition;
} // namespace lingotion::textpreprocessing

namespace Thespeon::Language
{
/** A word found in a text: the unit that is looked up in the lookup table and phonemized. */
struct FWord
{
	/** Where the word starts in the text, in TCHARs. */
	int32 Index = 0;

	/** The word. Its length in TCHARs is Text.Len(). */
	FString Text;
};

/**
 * Splits text into words by a language pack's word definition.
 */
class FWordSplitter
{
  public:
	/**
	 * @brief Builds the splitter from the file's word definition.
	 * @param Definition The word definition.
	 * @param Classes The file's character classes, which must outlive this splitter.
	 * @return False with OutError set if the definition names an unknown class.
	 */
	bool Init(const lingotion::textpreprocessing::WordDefinition& Definition, const FCharacterClasses& Classes, FString& OutError);

	/**
	 * @brief Splits a text into its words.
	 * @param Text The text to split.
	 * @param IsKnown Whether a word is in the lookup table, which decides whether joiners right after a word
	 *        stay part of it when the definition allows that.
	 * @return The words in order.
	 */
	TArray<FWord> Split(const FString& Text, TFunctionRef<bool(const FString&)> IsKnown) const;

	/** Splits a text into its words, as if no word were in the lookup table. */
	TArray<FWord> Split(const FString& Text) const;

	/** Whether some character of Text starts a word. */
	bool ContainsWord(const FString& Text) const;

	/** Whether CodePoint is a character of a word class (start, continue or joiner). */
	bool IsWordCharacter(int32 CodePoint) const
	{
		return Start->Contains(CodePoint) || Continuation->Contains(CodePoint) || Joiner->Contains(CodePoint);
	}

  private:
	TArray<FWord> SplitImpl(const FString& Text, const TFunctionRef<bool(const FString&)>* IsKnown) const;
	bool InWord(const TArray<int32>& Text, int32 Index) const;

	const FCodePointSet* Start = nullptr;
	const FCodePointSet* Continuation = nullptr;
	const FCodePointSet* Joiner = nullptr;
	bool bJoinerMayEnd = false;
};
} // namespace Thespeon::Language
