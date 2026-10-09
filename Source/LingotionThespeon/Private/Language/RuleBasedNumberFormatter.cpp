// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Language/RuleBasedNumberFormatter.h"
#include "Language/UnicodeTables.h"
#include "Core/text_preprocessing.pb.h"

namespace
{
// Deep enough for any number up to 10^18 in any sensible rule set; a rule set that recurses further is
// looping. model-meta-graph's textprep_runner.py uses the same limit.
constexpr int32 MaxDepth = 64;
} // namespace

namespace Thespeon::Language
{
bool FRuleBasedNumberFormatter::Init(
    const google::protobuf::RepeatedPtrField<lingotion::textpreprocessing::NumberRuleSet>& InRuleSets, FString& OutError
)
{
	for (const lingotion::textpreprocessing::NumberRuleSet& RuleSet : InRuleSets)
	{
		const FString Name = CodePoints::Utf8ToString(RuleSet.name());
		if (!Name.StartsWith(TEXT("%"), ESearchCase::CaseSensitive))
		{
			OutError = FString::Printf(TEXT("Rule set name '%s' must start with '%%'."), *Name);
			return false;
		}
		if (RuleSets.Contains(Name))
		{
			OutError = FString::Printf(TEXT("Rule set '%s' is defined twice."), *Name);
			return false;
		}
		if (RuleSet.rules_size() == 0)
		{
			OutError = FString::Printf(TEXT("Rule set '%s' has no rules."), *Name);
			return false;
		}
		TArray<FRule> Rules;
		Rules.Reserve(RuleSet.rules_size());
		for (const lingotion::textpreprocessing::NumberRule& Message : RuleSet.rules())
		{
			FRule& Rule = Rules.AddDefaulted_GetRef();
			Rule.BaseValue = Message.base_value();
			while (Rule.Divisor <= Rule.BaseValue / 10)
			{
				Rule.Divisor *= 10;
			}
			if (!ParseRule(CodePoints::Utf8ToString(Message.text()), Rule, OutError))
			{
				return false;
			}
		}
		if (Rules[0].BaseValue < 0)
		{
			OutError = FString::Printf(TEXT("Rule set '%s': negative base values are not supported."), *Name);
			return false;
		}
		for (int32 i = 1; i < Rules.Num(); ++i)
		{
			if (Rules[i].BaseValue <= Rules[i - 1].BaseValue)
			{
				OutError = FString::Printf(
				    TEXT("Rule set '%s': rules must be in ascending order of base value (%lld then %lld)."),
				    *Name,
				    Rules[i - 1].BaseValue,
				    Rules[i].BaseValue
				);
				return false;
			}
		}
		RuleSets.Add(Name, MoveTemp(Rules));
	}

	for (const TPair<FString, TArray<FRule>>& Pair : RuleSets)
	{
		for (const FRule& Rule : Pair.Value)
		{
			auto CheckTokens = [&](const TArray<FToken>& Tokens)
			{
				for (const FToken& Token : Tokens)
				{
					if (Token.Kind == ETokenKind::Literal || Token.Kind == ETokenKind::Optional)
					{
						continue;
					}
					if (!Token.Text.IsEmpty() && !RuleSets.Contains(Token.Text))
					{
						OutError = FString::Printf(TEXT("Rule set '%s', rule %lld: unknown rule set '%s'."), *Pair.Key, Rule.BaseValue, *Token.Text);
						return false;
					}
					if (Token.Kind == ETokenKind::Self && Token.Text == Pair.Key)
					{
						OutError = FString::Printf(
						    TEXT("Rule set '%s', rule %lld: '=' names its own rule set and would never end."), *Pair.Key, Rule.BaseValue
						);
						return false;
					}
				}
				return true;
			};
			if (!CheckTokens(Rule.Tokens))
			{
				return false;
			}
			for (const TArray<FToken>& Section : Rule.Sections)
			{
				if (!CheckTokens(Section))
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool FRuleBasedNumberFormatter::ParseRule(const FString& Text, FRule& OutRule, FString& OutError)
{
	// The section being parsed, or INDEX_NONE outside [ ... ].
	int32 Section = INDEX_NONE;
	FString Literal;

	auto Target = [&]() -> TArray<FToken>& { return Section == INDEX_NONE ? OutRule.Tokens : OutRule.Sections[Section]; };
	auto Flush = [&]()
	{
		if (!Literal.IsEmpty())
		{
			Target().Add({ETokenKind::Literal, MoveTemp(Literal), INDEX_NONE});
			Literal.Reset();
		}
	};

	int32 i = 0;
	while (i < Text.Len())
	{
		const TCHAR C = Text[i];
		if (C == TEXT('<') || C == TEXT('>') || C == TEXT('='))
		{
			int32 End = INDEX_NONE;
			for (int32 k = i + 1; k < Text.Len(); ++k)
			{
				if (Text[k] == C)
				{
					End = k;
					break;
				}
			}
			if (End == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Rule '%s': unclosed '%c' substitution."), *Text, C);
				return false;
			}
			const FString Name = Text.Mid(i + 1, End - i - 1);
			if (!Name.IsEmpty() && !Name.StartsWith(TEXT("%"), ESearchCase::CaseSensitive))
			{
				OutError = FString::Printf(TEXT("Rule '%s': '%s' is not a rule set name."), *Text, *Name);
				return false;
			}
			if (C == TEXT('=') && Name.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Rule '%s': '==' would format the number with its own rule and never end."), *Text);
				return false;
			}
			Flush();
			const ETokenKind Kind = C == TEXT('<') ? ETokenKind::Quotient : (C == TEXT('>') ? ETokenKind::Remainder : ETokenKind::Self);
			Target().Add({Kind, Name, INDEX_NONE});
			i = End + 1;
		}
		else if (C == TEXT('['))
		{
			if (Section != INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Rule '%s': optional sections cannot be nested."), *Text);
				return false;
			}
			Flush();
			Section = OutRule.Sections.AddDefaulted();
			++i;
		}
		else if (C == TEXT(']'))
		{
			if (Section == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Rule '%s': ']' without '['."), *Text);
				return false;
			}
			Flush();
			OutRule.Tokens.Add({ETokenKind::Optional, FString(), Section});
			Section = INDEX_NONE;
			++i;
		}
		else
		{
			Literal.AppendChar(C);
			++i;
		}
	}
	if (Section != INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("Rule '%s': unclosed '['."), *Text);
		return false;
	}
	Flush();
	return true;
}

bool FRuleBasedNumberFormatter::Format(int64 Number, const FString& RuleSet, FString& OutText, FString& OutError) const
{
	if (!RuleSets.Contains(RuleSet))
	{
		OutError = FString::Printf(TEXT("Unknown rule set '%s'."), *RuleSet);
		return false;
	}
	OutText.Reset();
	return FormatInto(Number, RuleSet, 0, OutText, OutError);
}

bool FRuleBasedNumberFormatter::FormatInto(int64 Number, const FString& RuleSet, int32 Depth, FString& Out, FString& OutError) const
{
	if (Depth > MaxDepth)
	{
		OutError = FString::Printf(TEXT("Rule set '%s' recursed more than %d times formatting %lld."), *RuleSet, MaxDepth, Number);
		return false;
	}
	if (Number < 0)
	{
		OutError = FString::Printf(TEXT("Cannot format the negative number %lld."), Number);
		return false;
	}
	const FRule* Rule = nullptr;
	for (const FRule& Candidate : RuleSets.FindChecked(RuleSet))
	{
		if (Candidate.BaseValue > Number)
		{
			break;
		}
		Rule = &Candidate;
	}
	if (!Rule)
	{
		OutError = FString::Printf(TEXT("Rule set '%s' has no rule for %lld."), *RuleSet, Number);
		return false;
	}
	return RenderInto(*Rule, Rule->Tokens, Number, Number / Rule->Divisor, Number % Rule->Divisor, RuleSet, Depth, Out, OutError);
}

bool FRuleBasedNumberFormatter::RenderInto(
    const FRule& Rule,
    const TArray<FToken>& Tokens,
    int64 Number,
    int64 Quotient,
    int64 Remainder,
    const FString& RuleSet,
    int32 Depth,
    FString& Out,
    FString& OutError
) const
{
	for (const FToken& Token : Tokens)
	{
		switch (Token.Kind)
		{
			case ETokenKind::Literal:
				Out += Token.Text;
				break;
			case ETokenKind::Optional:
				if (Remainder != 0 && !RenderInto(Rule, Rule.Sections[Token.Section], Number, Quotient, Remainder, RuleSet, Depth, Out, OutError))
				{
					return false;
				}
				break;
			default:
			{
				const int64 Value = Token.Kind == ETokenKind::Quotient ? Quotient : (Token.Kind == ETokenKind::Remainder ? Remainder : Number);
				if (!FormatInto(Value, Token.Text.IsEmpty() ? RuleSet : Token.Text, Depth + 1, Out, OutError))
				{
					return false;
				}
				break;
			}
		}
	}
	return true;
}
} // namespace Thespeon::Language
