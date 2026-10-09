// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Language/UnicodeTables.h"

namespace google::protobuf
{
template <typename Element> class RepeatedPtrField;
} // namespace google::protobuf

namespace lingotion::textpreprocessing
{
class NumberForm;
class FormElement;
} // namespace lingotion::textpreprocessing

namespace Thespeon::Language
{
class FRuleBasedNumberFormatter;

/**
 * Finds the numbers in a text by a language pack's number forms, and speaks them out in phonemes with its
 * rule sets.
 */
class FNumberExpander
{
  public:
	/** A part of a partitioned text: text, or a number as written. */
	struct FPart
	{
		FString Text;
		bool bIsNumber = false;
	};

	/**
	 * @brief Compiles the number forms of a text preprocessing file.
	 * @param Forms The number forms, in the order they are tried.
	 * @param Classes The file's character classes, which must outlive this expander.
	 * @param InFormatter The file's rule sets, which must outlive this expander.
	 * @param OutError Receives the reason when a form or its template is malformed.
	 * @return False if a form is malformed.
	 */
	bool Init(
	    const google::protobuf::RepeatedPtrField<lingotion::textpreprocessing::NumberForm>& Forms,
	    const FCharacterClasses& Classes,
	    const FRuleBasedNumberFormatter& InFormatter,
	    FString& OutError
	);

	/**
	 * @brief Splits a text into alternating parts of text and numbers. At each position the forms are tried in
	 * order, and the first one that matches there makes a number part.
	 */
	TArray<FPart> Partition(const FString& Text) const;

	/**
	 * @brief Speaks out a number, written in one of the number forms, in phonemes.
	 * @param Number The number as written, e.g. "21st".
	 * @param OutPhonemes Receives the number in phonemes.
	 * @param OutError Receives the reason on failure.
	 * @return False if no number form matches the whole of Number, the number is too large, or its rule sets
	 *         cannot format it.
	 */
	bool Expand(const FString& Number, FString& OutPhonemes, FString& OutError) const;

  private:
	enum class EElementKind : uint8
	{
		Digits,
		Literal,
		OneOf,
	};

	struct FElement
	{
		EElementKind Kind = EElementKind::Digits;
		const FCodePointSet* Class = nullptr;
		TArray<TArray<int32>> Texts;
		bool bOptional = false;
	};

	struct FTemplatePart
	{
		// The literal text; used when Element is INDEX_NONE.
		FString Literal;
		// The 1-based element the substitution speaks out, or INDEX_NONE for a literal.
		int32 Element = INDEX_NONE;
		FString RuleSet;
		bool bEachDigit = false;
	};

	struct FMatch
	{
		int32 Start = 0;
		int32 Length = 0;
	};

	struct FForm
	{
		FString Template;
		TArray<FElement> Elements;
		TArray<FTemplatePart> Parts;

		bool CanStartWith(int32 CodePoint) const;
		bool Match(const TArray<int32>& Text, int32 Position, TArray<FMatch>& OutMatched) const;
	};

	static bool
	CompileElement(const lingotion::textpreprocessing::FormElement& Message, const FCharacterClasses& Classes, FForm& Form, FString& OutError);
	bool ParseTemplate(FForm& Form, FString& OutError) const;
	int32 MatchLengthAt(const TArray<int32>& Text, int32 Position, TArray<FMatch>& Scratch) const;
	bool Render(const FForm& Form, const TArray<int32>& Text, const TArray<FMatch>& Matched, FString& Out, FString& OutError) const;

	TArray<FForm> Forms;
	const FRuleBasedNumberFormatter* Formatter = nullptr;
};
} // namespace Thespeon::Language
