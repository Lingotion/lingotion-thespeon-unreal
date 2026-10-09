// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Language/UnicodeTables.h"
#include "Algo/BinarySearch.h"
#include "Core/text_preprocessing.pb.h"

namespace
{
constexpr int32 MaxCodePoint = 0x10FFFF;
constexpr int32 ReplacementCharacter = 0xFFFD;

bool IsHighSurrogate(uint32 Unit)
{
	return Unit >= 0xD800 && Unit <= 0xDBFF;
}

bool IsLowSurrogate(uint32 Unit)
{
	return Unit >= 0xDC00 && Unit <= 0xDFFF;
}

// The index of the last of the ascending Firsts that is <= CodePoint, or -1 if there is none.
int32 LastAtOrBefore(const TArray<int32>& Firsts, int32 CodePoint)
{
	return Algo::UpperBound(Firsts, CodePoint) - 1;
}
} // namespace

namespace Thespeon::Language
{
void CodePoints::Append(const FString& Text, TArray<int32>& Out)
{
	const int32 Length = Text.Len();
	const TCHAR* Chars = *Text;
	Out.Reserve(Out.Num() + Length);
	for (int32 i = 0; i < Length; ++i)
	{
		const uint32 Unit = static_cast<uint32>(Chars[i]);
		if (IsHighSurrogate(Unit) && i + 1 < Length && IsLowSurrogate(static_cast<uint32>(Chars[i + 1])))
		{
			const uint32 Low = static_cast<uint32>(Chars[i + 1]);
			Out.Add(static_cast<int32>(0x10000 + ((Unit - 0xD800) << 10) + (Low - 0xDC00)));
			++i;
		}
		else
		{
			Out.Add(static_cast<int32>(Unit));
		}
	}
}

int32 CodePoints::LengthAt(const FString& Text, int32 Index)
{
	return Index + 1 < Text.Len() && IsHighSurrogate(static_cast<uint32>(Text[Index])) && IsLowSurrogate(static_cast<uint32>(Text[Index + 1])) ? 2
	                                                                                                                                           : 1;
}

TArray<int32> CodePoints::FromString(const FString& Text)
{
	TArray<int32> Result;
	Append(Text, Result);
	return Result;
}

TArray<int32> CodePoints::FromUtf8(const std::string& Text)
{
	TArray<int32> Result;
	Result.Reserve(static_cast<int32>(Text.size()));
	const size_t Size = Text.size();
	size_t i = 0;
	while (i < Size)
	{
		const uint8 Lead = static_cast<uint8>(Text[i]);
		int32 Continuations = 0;
		int32 CodePoint = 0;
		int32 Minimum = 0;
		if (Lead < 0x80)
		{
			CodePoint = Lead;
		}
		else if ((Lead & 0xE0) == 0xC0)
		{
			Continuations = 1;
			CodePoint = Lead & 0x1F;
			Minimum = 0x80;
		}
		else if ((Lead & 0xF0) == 0xE0)
		{
			Continuations = 2;
			CodePoint = Lead & 0x0F;
			Minimum = 0x800;
		}
		else if ((Lead & 0xF8) == 0xF0)
		{
			Continuations = 3;
			CodePoint = Lead & 0x07;
			Minimum = 0x10000;
		}
		else
		{
			Result.Add(ReplacementCharacter);
			++i;
			continue;
		}
		bool bValid = i + static_cast<size_t>(Continuations) < Size;
		for (int32 k = 1; bValid && k <= Continuations; ++k)
		{
			const uint8 Byte = static_cast<uint8>(Text[i + k]);
			if ((Byte & 0xC0) != 0x80)
			{
				bValid = false;
				break;
			}
			CodePoint = (CodePoint << 6) | (Byte & 0x3F);
		}
		if (!bValid || CodePoint < Minimum || CodePoint > MaxCodePoint)
		{
			Result.Add(ReplacementCharacter);
			++i;
			continue;
		}
		Result.Add(CodePoint);
		i += static_cast<size_t>(Continuations) + 1;
	}
	return Result;
}

FString CodePoints::Utf8ToString(const std::string& Text)
{
	return ToString(FromUtf8(Text));
}

void CodePoints::AppendTo(FString& Text, int32 CodePoint)
{
	if (CodePoint > 0xFFFF)
	{
		const uint32 Offset = static_cast<uint32>(CodePoint) - 0x10000;
		Text.AppendChar(static_cast<TCHAR>(0xD800 + (Offset >> 10)));
		Text.AppendChar(static_cast<TCHAR>(0xDC00 + (Offset & 0x3FF)));
	}
	else
	{
		Text.AppendChar(static_cast<TCHAR>(CodePoint));
	}
}

FString CodePoints::ToString(const TArray<int32>& CodePoints, int32 Start, int32 Count)
{
	FString Text;
	Text.Reserve(Count);
	for (int32 i = Start; i < Start + Count; ++i)
	{
		AppendTo(Text, CodePoints[i]);
	}
	return Text;
}

FString CodePoints::ToString(const TArray<int32>& CodePoints)
{
	return ToString(CodePoints, 0, CodePoints.Num());
}

bool FCodePointSet::Init(const FString& InName, const uint32* Ranges, int32 Count, FString& OutError)
{
	Name = InName;
	if (Count % 2 != 0)
	{
		OutError = FString::Printf(TEXT("Character class '%s' must list its ranges as pairs, got %d values."), *Name, Count);
		return false;
	}
	Firsts.Reset(Count / 2);
	Lasts.Reset(Count / 2);
	int64 PreviousLast = -1;
	for (int32 i = 0; i < Count; i += 2)
	{
		const uint32 First = Ranges[i];
		const uint32 Last = Ranges[i + 1];
		if (First > Last || static_cast<int64>(First) <= PreviousLast || Last > static_cast<uint32>(MaxCodePoint))
		{
			OutError = FString::Printf(TEXT("Character class '%s': ranges must be ascending, disjoint and within U+10FFFF."), *Name);
			return false;
		}
		Firsts.Add(static_cast<int32>(First));
		Lasts.Add(static_cast<int32>(Last));
		PreviousLast = Last;
	}
	for (int32 CodePoint = 0; CodePoint < 128; ++CodePoint)
	{
		bAscii[CodePoint] = RangeStart(CodePoint) >= 0;
	}
	return true;
}

int32 FCodePointSet::RangeStart(int32 CodePoint) const
{
	const int32 Index = LastAtOrBefore(Firsts, CodePoint);
	if (Index >= 0 && CodePoint <= Lasts[Index])
	{
		return Firsts[Index];
	}
	return -1;
}

const FCodePointSet* FindCharacterClass(const FCharacterClasses& Classes, const FString& Name, FString& OutError)
{
	const TUniquePtr<FCodePointSet>* Found = Classes.Find(Name);
	if (!Found)
	{
		OutError = FString::Printf(TEXT("Unknown character class '%s'."), *Name);
		return nullptr;
	}
	return Found->Get();
}

bool FLowercaser::Init(const lingotion::textpreprocessing::Lowercase& Message, const FCharacterClasses& Classes, FString& OutError)
{
	const int32 Count = Message.runs_size();
	if (Count % 4 != 0)
	{
		OutError = FString::Printf(TEXT("Lowercase runs must come in groups of four, got %d values."), Count);
		return false;
	}
	int32 PreviousLast = -1;
	for (int32 i = 0; i < Count; i += 4)
	{
		const int32 First = Message.runs(i);
		const int32 Last = Message.runs(i + 1);
		const int32 Stride = Message.runs(i + 2);
		if (Stride < 1 || First > Last || First <= PreviousLast)
		{
			OutError = TEXT("Lowercase runs must be ascending and disjoint, with a positive stride.");
			return false;
		}
		Firsts.Add(First);
		Lasts.Add(Last);
		Strides.Add(Stride);
		Deltas.Add(Message.runs(i + 3));
		PreviousLast = Last;
	}
	for (const lingotion::textpreprocessing::SpecialLowercase& Entry : Message.special())
	{
		Special.Add(static_cast<int32>(Entry.code_point()), CodePoints::FromUtf8(Entry.lowercase()));
	}
	for (const lingotion::textpreprocessing::ContextualLowercase& Entry : Message.contextual())
	{
		FContextual Context;
		Context.FinalForm = static_cast<int32>(Entry.final_form());
		Context.Cased = FindCharacterClass(Classes, CodePoints::Utf8ToString(Entry.cased_class()), OutError);
		Context.Ignorable = FindCharacterClass(Classes, CodePoints::Utf8ToString(Entry.ignorable_class()), OutError);
		if (!Context.Cased || !Context.Ignorable)
		{
			return false;
		}
		Contextual.Add(static_cast<int32>(Entry.code_point()), Context);
	}
	TArray<int32> Lowered;
	for (int32 CodePoint = 0; CodePoint < 128; ++CodePoint)
	{
		Lowered.Reset();
		Lower(CodePoint, Lowered);
		AsciiLower[CodePoint] = Lowered.Num() == 1 ? Lowered[0] : -1;
	}
	return true;
}

void FLowercaser::Lower(int32 CodePoint, TArray<int32>& Out) const
{
	if (const TArray<int32>* Found = Special.Find(CodePoint))
	{
		Out.Append(*Found);
		return;
	}
	const int32 Index = LastAtOrBefore(Firsts, CodePoint);
	if (Index >= 0 && CodePoint <= Lasts[Index] && (CodePoint - Firsts[Index]) % Strides[Index] == 0)
	{
		Out.Add(CodePoint + Deltas[Index]);
		return;
	}
	Out.Add(CodePoint);
}

void FLowercaser::Apply(const TArray<int32>& Text, TArray<int32>& Out) const
{
	Out.Reserve(Out.Num() + Text.Num());
	for (int32 i = 0; i < Text.Num(); ++i)
	{
		const int32 CodePoint = Text[i];
		if (CodePoint >= 0 && CodePoint < 128 && AsciiLower[CodePoint] >= 0)
		{
			Out.Add(AsciiLower[CodePoint]);
			continue;
		}
		const FContextual* Context = Contextual.Find(CodePoint);
		if (Context && IsFinal(Text, i, *Context))
		{
			Out.Add(Context->FinalForm);
		}
		else
		{
			Lower(CodePoint, Out);
		}
	}
}

bool FLowercaser::IsFinal(const TArray<int32>& Text, int32 Index, const FContextual& Context)
{
	int32 Before = Index - 1;
	while (Before >= 0 && Context.Ignorable->Contains(Text[Before]))
	{
		--Before;
	}
	if (Before < 0 || !Context.Cased->Contains(Text[Before]))
	{
		return false;
	}
	int32 After = Index + 1;
	while (After < Text.Num() && Context.Ignorable->Contains(Text[After]))
	{
		++After;
	}
	return After == Text.Num() || !Context.Cased->Contains(Text[After]);
}

bool FComposer::Init(const lingotion::textpreprocessing::Compose& Message, FString& OutError)
{
	if (Message.pairs_size() % 3 != 0 || Message.combining_classes_size() % 3 != 0)
	{
		OutError = TEXT("Compose pairs and combining classes must come in groups of three.");
		return false;
	}
	for (int32 i = 0; i < Message.pairs_size(); i += 3)
	{
		Pairs.Add(Key(static_cast<int32>(Message.pairs(i)), static_cast<int32>(Message.pairs(i + 1))), static_cast<int32>(Message.pairs(i + 2)));
	}
	int32 PreviousLast = -1;
	for (int32 i = 0; i < Message.combining_classes_size(); i += 3)
	{
		const int32 First = static_cast<int32>(Message.combining_classes(i));
		const int32 Last = static_cast<int32>(Message.combining_classes(i + 1));
		if (First > Last || First <= PreviousLast)
		{
			OutError = TEXT("Combining classes must be ascending and disjoint.");
			return false;
		}
		Firsts.Add(First);
		Lasts.Add(Last);
		Classes.Add(static_cast<int32>(Message.combining_classes(i + 2)));
		PreviousLast = Last;
	}
	return true;
}

void FComposer::Apply(const TArray<int32>& Text, TArray<int32>& Out) const
{
	Out.Reserve(Out.Num() + Text.Num());
	int32 Starter = -1;
	// The class of the last character kept after the starter, or -1 when there is none - a mark is blocked
	// from the starter by a character between them of class 0 or of a class at least its own (UAX #15).
	int32 LastClass = -1;
	for (const int32 CodePoint : Text)
	{
		const int32 Class = CombiningClass(CodePoint);
		if (Starter >= 0)
		{
			const bool bBlocked = LastClass >= 0 && (LastClass == 0 || LastClass >= Class);
			if (!bBlocked)
			{
				if (const int32* Composed = Pairs.Find(Key(Out[Starter], CodePoint)))
				{
					Out[Starter] = *Composed;
					continue;
				}
			}
		}
		Out.Add(CodePoint);
		if (Class == 0)
		{
			Starter = Out.Num() - 1;
			LastClass = -1;
		}
		else
		{
			LastClass = Class;
		}
	}
}

int32 FComposer::CombiningClass(int32 CodePoint) const
{
	const int32 Index = LastAtOrBefore(Firsts, CodePoint);
	if (Index >= 0 && CodePoint <= Lasts[Index])
	{
		return Classes[Index];
	}
	return 0;
}
} // namespace Thespeon::Language
