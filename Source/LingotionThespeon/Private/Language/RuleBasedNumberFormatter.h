// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"

namespace google::protobuf
{
template <typename Element> class RepeatedPtrField;
} // namespace google::protobuf

namespace lingotion::textpreprocessing
{
class NumberRuleSet;
} // namespace lingotion::textpreprocessing

namespace Thespeon::Language
{
/**
 * Spells out numbers with the rule-based number format (RBNF) rule sets of a language pack's text
 * preprocessing file. The rule syntax is described in text_preprocessing.proto in model-meta-graph.
 */
class FRuleBasedNumberFormatter
{
  public:
	/**
	 * @brief Compiles and validates the rule sets of a text preprocessing file.
	 * @param InRuleSets The rule sets to compile.
	 * @param OutError Receives the reason when a rule set is malformed or refers to one that does not exist.
	 * @return False if the rule sets are malformed.
	 */
	bool Init(const google::protobuf::RepeatedPtrField<lingotion::textpreprocessing::NumberRuleSet>& InRuleSets, FString& OutError);

	/** Whether a rule set of the given name, including its leading '%', exists. */
	bool HasRuleSet(const FString& Name) const
	{
		return RuleSets.Contains(Name);
	}

	/**
	 * @brief Spells out a non-negative integer with the named rule set.
	 * @param Number The number to spell out.
	 * @param RuleSet The name of the rule set to use.
	 * @param OutText Receives the number in words, as the rule set writes them.
	 * @param OutError Receives the reason on failure.
	 * @return False if the number is negative, the rule set is unknown or has no rule for the number, or the
	 *         rules recurse without end.
	 */
	bool Format(int64 Number, const FString& RuleSet, FString& OutText, FString& OutError) const;

  private:
	enum class ETokenKind : uint8
	{
		Literal,
		Quotient,
		Remainder,
		Self,
		// An optional [ ... ] section; Section indexes the rule's Sections.
		Optional,
	};

	struct FToken
	{
		ETokenKind Kind = ETokenKind::Literal;
		// The literal text, or the substitution's rule set name (empty for the rule's own set).
		FString Text;
		int32 Section = INDEX_NONE;
	};

	struct FRule
	{
		int64 BaseValue = 0;
		int64 Divisor = 1;
		TArray<FToken> Tokens;
		TArray<TArray<FToken>> Sections;
	};

	static bool ParseRule(const FString& Text, FRule& OutRule, FString& OutError);

	bool FormatInto(int64 Number, const FString& RuleSet, int32 Depth, FString& Out, FString& OutError) const;

	bool RenderInto(
	    const FRule& Rule,
	    const TArray<FToken>& Tokens,
	    int64 Number,
	    int64 Quotient,
	    int64 Remainder,
	    const FString& RuleSet,
	    int32 Depth,
	    FString& Out,
	    FString& OutError
	) const;

	TMap<FString, TArray<FRule>> RuleSets;
};
} // namespace Thespeon::Language
