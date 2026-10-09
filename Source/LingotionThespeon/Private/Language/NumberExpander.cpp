// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Language/NumberExpander.h"
#include "Language/RuleBasedNumberFormatter.h"
#include "Core/text_preprocessing.pb.h"

namespace
{
bool StartsWith(const TArray<int32>& Text, int32 Position, const TArray<int32>& Prefix)
{
	if (Position + Prefix.Num() > Text.Num())
	{
		return false;
	}
	for (int32 i = 0; i < Prefix.Num(); ++i)
	{
		if (Text[Position + i] != Prefix[i])
		{
			return false;
		}
	}
	return true;
}

// A digit's value is its distance from the start of its range in the digit class, modulo 10.
bool DigitValue(const Thespeon::Language::FCodePointSet& Digits, int32 CodePoint, int32& OutValue, FString& OutError)
{
	const int32 RangeStart = Digits.RangeStart(CodePoint);
	if (RangeStart < 0)
	{
		OutError = FString::Printf(TEXT("U+%04X is not in the digit class '%s'."), CodePoint, *Digits.GetName());
		return false;
	}
	OutValue = (CodePoint - RangeStart) % 10;
	return true;
}
} // namespace

namespace Thespeon::Language
{
bool FNumberExpander::Init(
    const google::protobuf::RepeatedPtrField<lingotion::textpreprocessing::NumberForm>& InForms,
    const FCharacterClasses& Classes,
    const FRuleBasedNumberFormatter& InFormatter,
    FString& OutError
)
{
	Formatter = &InFormatter;
	Forms.Reserve(InForms.size());
	for (const lingotion::textpreprocessing::NumberForm& Message : InForms)
	{
		FForm& Form = Forms.AddDefaulted_GetRef();
		Form.Template = CodePoints::Utf8ToString(Message.template_());
		for (const lingotion::textpreprocessing::FormElement& Element : Message.elements())
		{
			if (!CompileElement(Element, Classes, Form, OutError))
			{
				return false;
			}
		}
		if (Form.Elements.Num() == 0)
		{
			OutError = FString::Printf(TEXT("Number form '%s' has no elements."), *Form.Template);
			return false;
		}
		if (!ParseTemplate(Form, OutError))
		{
			return false;
		}
	}
	return true;
}

bool FNumberExpander::CompileElement(
    const lingotion::textpreprocessing::FormElement& Message, const FCharacterClasses& Classes, FForm& Form, FString& OutError
)
{
	FElement Element;
	Element.bOptional = Message.optional();
	switch (Message.kind_case())
	{
		case lingotion::textpreprocessing::FormElement::kDigits:
			Element.Kind = EElementKind::Digits;
			Element.Class = FindCharacterClass(Classes, CodePoints::Utf8ToString(Message.digits()), OutError);
			if (!Element.Class)
			{
				return false;
			}
			break;
		case lingotion::textpreprocessing::FormElement::kLiteral:
			if (Message.literal().empty())
			{
				OutError = FString::Printf(TEXT("Number form '%s': empty literal."), *Form.Template);
				return false;
			}
			Element.Kind = EElementKind::Literal;
			Element.Texts.Add(CodePoints::FromUtf8(Message.literal()));
			break;
		case lingotion::textpreprocessing::FormElement::kOneOf:
			Element.Kind = EElementKind::OneOf;
			for (const std::string& Option : Message.one_of().options())
			{
				if (Option.empty())
				{
					Element.Texts.Reset();
					break;
				}
				Element.Texts.Add(CodePoints::FromUtf8(Option));
			}
			if (Element.Texts.Num() == 0)
			{
				OutError = FString::Printf(TEXT("Number form '%s': one_of needs non-empty options."), *Form.Template);
				return false;
			}
			break;
		default:
			OutError = FString::Printf(TEXT("Number form '%s': empty element."), *Form.Template);
			return false;
	}
	Form.Elements.Add(MoveTemp(Element));
	return true;
}

bool FNumberExpander::ParseTemplate(FForm& Form, FString& OutError) const
{
	const FString& Template = Form.Template;
	FString Literal;
	int32 i = 0;
	while (i < Template.Len())
	{
		const TCHAR C = Template[i];
		if (C == TEXT('}'))
		{
			OutError = FString::Printf(TEXT("Template '%s': '}' without '{'."), *Template);
			return false;
		}
		if (C != TEXT('{'))
		{
			Literal.AppendChar(C);
			++i;
			continue;
		}
		int32 End = INDEX_NONE;
		for (int32 k = i + 1; k < Template.Len(); ++k)
		{
			if (Template[k] == TEXT('}'))
			{
				End = k;
				break;
			}
		}
		if (End == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Template '%s': unclosed '{'."), *Template);
			return false;
		}
		TArray<FString> Fields;
		Template.Mid(i + 1, End - i - 1).ParseIntoArray(Fields, TEXT(":"), /*InCullEmpty=*/false);
		bool bValidFields = (Fields.Num() == 2 || (Fields.Num() == 3 && Fields[2] == TEXT("each"))) && !Fields[0].IsEmpty();
		for (int32 k = 0; bValidFields && k < Fields[0].Len(); ++k)
		{
			bValidFields = Fields[0][k] >= TEXT('0') && Fields[0][k] <= TEXT('9');
		}
		if (!bValidFields)
		{
			OutError = FString::Printf(TEXT("Template '%s': malformed substitution '%s'."), *Template, *Template.Mid(i, End - i + 1));
			return false;
		}
		// Anything longer than nine digits is far past any form's element count, and would overflow int32.
		const int64 Element = Fields[0].Len() > 9 ? MAX_int64 : FCString::Atoi64(*Fields[0]);
		if (Element < 1 || Element > Form.Elements.Num())
		{
			OutError =
			    FString::Printf(TEXT("Template '%s': element %s does not exist; the form has %d."), *Template, *Fields[0], Form.Elements.Num());
			return false;
		}
		if (Form.Elements[Element - 1].Kind != EElementKind::Digits)
		{
			OutError = FString::Printf(TEXT("Template '%s': element %lld is not a digit run."), *Template, Element);
			return false;
		}
		if (!Formatter->HasRuleSet(Fields[1]))
		{
			OutError = FString::Printf(TEXT("Template '%s': unknown rule set '%s'."), *Template, *Fields[1]);
			return false;
		}
		if (!Literal.IsEmpty())
		{
			Form.Parts.Add({MoveTemp(Literal), INDEX_NONE, FString(), false});
			Literal.Reset();
		}
		Form.Parts.Add({FString(), static_cast<int32>(Element), Fields[1], Fields.Num() == 3});
		i = End + 1;
	}
	if (!Literal.IsEmpty())
	{
		Form.Parts.Add({MoveTemp(Literal), INDEX_NONE, FString(), false});
	}
	return true;
}

bool FNumberExpander::FForm::CanStartWith(int32 CodePoint) const
{
	for (const FElement& Element : Elements)
	{
		bool bStartsElement = false;
		if (Element.Kind == EElementKind::Digits)
		{
			bStartsElement = Element.Class->Contains(CodePoint);
		}
		else
		{
			for (const TArray<int32>& Text : Element.Texts)
			{
				if (Text[0] == CodePoint)
				{
					bStartsElement = true;
					break;
				}
			}
		}
		if (bStartsElement)
		{
			return true;
		}
		// An optional element may match nothing, so the next one may start the match.
		if (!Element.bOptional)
		{
			return false;
		}
	}
	return false;
}

bool FNumberExpander::FForm::Match(const TArray<int32>& Text, int32 Position, TArray<FMatch>& OutMatched) const
{
	// Greedy and without backtracking: a digit run takes every digit it can, a one_of takes the first option
	// that matches, and an optional element that does not match takes nothing.
	OutMatched.Reset();
	for (const FElement& Element : Elements)
	{
		int32 Length = 0;
		if (Element.Kind == EElementKind::Digits)
		{
			while (Position + Length < Text.Num() && Element.Class->Contains(Text[Position + Length]))
			{
				++Length;
			}
		}
		else
		{
			for (const TArray<int32>& Option : Element.Texts)
			{
				if (StartsWith(Text, Position, Option))
				{
					Length = Option.Num();
					break;
				}
			}
		}
		if (Length == 0 && !Element.bOptional)
		{
			return false;
		}
		OutMatched.Add({Position, Length});
		Position += Length;
	}
	return true;
}

int32 FNumberExpander::MatchLengthAt(const TArray<int32>& Text, int32 Position, TArray<FMatch>& Scratch) const
{
	for (const FForm& Form : Forms)
	{
		// Partition tries every position of the text, and almost none can start a number, so they are ruled
		// out before anything is matched.
		if (!Form.CanStartWith(Text[Position]))
		{
			continue;
		}
		if (Form.Match(Text, Position, Scratch))
		{
			int32 Length = 0;
			for (const FMatch& Matched : Scratch)
			{
				Length += Matched.Length;
			}
			if (Length > 0)
			{
				return Length;
			}
		}
	}
	return 0;
}

TArray<FNumberExpander::FPart> FNumberExpander::Partition(const FString& Text) const
{
	const TArray<int32> Points = CodePoints::FromString(Text);
	TArray<FPart> Parts;
	TArray<FMatch> Scratch;
	int32 Last = 0;
	int32 Position = 0;
	while (Position < Points.Num())
	{
		const int32 Length = MatchLengthAt(Points, Position, Scratch);
		if (Length == 0)
		{
			++Position;
			continue;
		}
		if (Position > Last)
		{
			Parts.Add({CodePoints::ToString(Points, Last, Position - Last), false});
		}
		Parts.Add({CodePoints::ToString(Points, Position, Length), true});
		Position += Length;
		Last = Position;
	}
	if (Last < Points.Num())
	{
		Parts.Add({CodePoints::ToString(Points, Last, Points.Num() - Last), false});
	}
	return Parts;
}

bool FNumberExpander::Expand(const FString& Number, FString& OutPhonemes, FString& OutError) const
{
	const TArray<int32> Points = CodePoints::FromString(Number);
	TArray<FMatch> Matched;
	for (const FForm& Form : Forms)
	{
		if (!Form.Match(Points, 0, Matched))
		{
			continue;
		}
		int32 Length = 0;
		for (const FMatch& Element : Matched)
		{
			Length += Element.Length;
		}
		if (Length == Points.Num())
		{
			return Render(Form, Points, Matched, OutPhonemes, OutError);
		}
	}
	OutError = FString::Printf(TEXT("'%s' is not written in any of the number forms."), *Number);
	return false;
}

bool FNumberExpander::Render(const FForm& Form, const TArray<int32>& Text, const TArray<FMatch>& Matched, FString& Out, FString& OutError) const
{
	Out.Reset();
	FString Spoken;
	for (const FTemplatePart& Part : Form.Parts)
	{
		if (Part.Element == INDEX_NONE)
		{
			Out += Part.Literal;
			continue;
		}
		const FMatch& Element = Matched[Part.Element - 1];
		if (Element.Length == 0)
		{
			OutError = FString::Printf(TEXT("Number form '%s': element %d matched nothing."), *Form.Template, Part.Element);
			return false;
		}
		const FCodePointSet& Digits = *Form.Elements[Part.Element - 1].Class;
		if (Part.bEachDigit)
		{
			for (int32 i = Element.Start; i < Element.Start + Element.Length; ++i)
			{
				int32 Digit = 0;
				if (!DigitValue(Digits, Text[i], Digit, OutError) || !Formatter->Format(Digit, Part.RuleSet, Spoken, OutError))
				{
					return false;
				}
				if (i > Element.Start)
				{
					Out.AppendChar(TEXT(' '));
				}
				Out += Spoken;
			}
		}
		else
		{
			int64 Value = 0;
			for (int32 i = Element.Start; i < Element.Start + Element.Length; ++i)
			{
				int32 Digit = 0;
				if (!DigitValue(Digits, Text[i], Digit, OutError))
				{
					return false;
				}
				if (Value > (MAX_int64 - Digit) / 10)
				{
					OutError =
					    FString::Printf(TEXT("The number %s is too large to speak out."), *CodePoints::ToString(Text, Element.Start, Element.Length));
					return false;
				}
				Value = Value * 10 + Digit;
			}
			if (!Formatter->Format(Value, Part.RuleSet, Spoken, OutError))
			{
				return false;
			}
			Out += Spoken;
		}
	}
	return true;
}
} // namespace Thespeon::Language
