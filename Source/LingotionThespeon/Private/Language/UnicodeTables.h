// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include <string>

// Everything in this file reads Unicode from a language pack's text preprocessing file rather than from the
// engine (FChar, FString::ToLower, ICU), whose Unicode data depends on the platform and engine version.
// model-meta-graph's textprep_runner.py is the reference implementation; the conformance tests hold this port
// to it.

namespace lingotion::textpreprocessing
{
class Compose;
class Lowercase;
} // namespace lingotion::textpreprocessing

namespace Thespeon::Language
{
/**
 * Converts between FString and Unicode code points. FString is UTF-16, so a code point outside the Basic
 * Multilingual Plane takes two TCHARs.
 */
namespace CodePoints
{
/** Appends the code points of Text to Out. A lone surrogate is kept as it is, so that no input text fails. */
void Append(const FString& Text, TArray<int32>& Out);

/** Returns the code points of Text. */
TArray<int32> FromString(const FString& Text);

/** Returns the code points of a UTF-8 string, as protobuf stores strings. Invalid UTF-8 becomes U+FFFD. */
TArray<int32> FromUtf8(const std::string& Text);

/** Returns a UTF-8 string, as protobuf stores strings, as an FString. */
FString Utf8ToString(const std::string& Text);

/** Appends one code point to Text. */
void AppendTo(FString& Text, int32 CodePoint);

/** Returns Count code points of CodePoints, from Start, as an FString. */
FString ToString(const TArray<int32>& CodePoints, int32 Start, int32 Count);

/** Returns the code points as an FString. */
FString ToString(const TArray<int32>& CodePoints);

/** Returns how many TCHARs the code point starting at Index of Text takes: 2 for a surrogate pair, else 1. */
int32 LengthAt(const FString& Text, int32 Index);

/** Returns how many TCHARs the code point takes in an FString. */
inline int32 Utf16Length(int32 CodePoint)
{
	return CodePoint > 0xFFFF ? 2 : 1;
}
} // namespace CodePoints

/**
 * A character class of a text preprocessing file: ascending, disjoint inclusive ranges of code points.
 */
class FCodePointSet
{
  public:
	/**
	 * @brief Builds the class from the file's ranges.
	 * @param InName The class name, for messages.
	 * @param Ranges Inclusive ranges as pairs: first, last, first, last, ...
	 * @param Count Number of values in Ranges.
	 * @param OutError Receives the reason when the ranges are malformed.
	 * @return False if the ranges are not pairs, ascending, disjoint and within U+10FFFF.
	 */
	bool Init(const FString& InName, const uint32* Ranges, int32 Count, FString& OutError);

	/** Whether CodePoint is in the class. */
	bool Contains(int32 CodePoint) const
	{
		// Almost all text is ASCII, and every character of it is classified at least once per step and per
		// number and word search, so ASCII is answered from a table rather than by binary search.
		if (CodePoint >= 0 && CodePoint < 128)
		{
			return bAscii[CodePoint];
		}
		return RangeStart(CodePoint) >= 0;
	}

	/** The first code point of the range holding CodePoint, or -1 if it is not in the class. */
	int32 RangeStart(int32 CodePoint) const;

	/** The class name. */
	const FString& GetName() const
	{
		return Name;
	}

  private:
	FString Name;
	TArray<int32> Firsts;
	TArray<int32> Lasts;
	bool bAscii[128] = {};
};

/** The character classes of a text preprocessing file, by name. */
using FCharacterClasses = TMap<FString, TUniquePtr<FCodePointSet>>;

/**
 * @brief Looks up a character class by name.
 * @return The class, or nullptr with OutError set if there is no class of that name.
 */
const FCodePointSet* FindCharacterClass(const FCharacterClasses& Classes, const FString& Name, FString& OutError);

/**
 * The lowercase step: lowercasing by the file's tables, as Python's str.lower() does it.
 */
class FLowercaser
{
  public:
	/**
	 * @brief Builds the lowercaser from the file's lowercase step.
	 * @return False with OutError set if the tables are malformed or name an unknown class.
	 */
	bool Init(const lingotion::textpreprocessing::Lowercase& Message, const FCharacterClasses& Classes, FString& OutError);

	/** Appends the lowercase of one code point on its own, without context, to Out. */
	void Lower(int32 CodePoint, TArray<int32>& Out) const;

	/** Appends the lowercase of Text, context included (final sigma), to Out. */
	void Apply(const TArray<int32>& Text, TArray<int32>& Out) const;

  private:
	struct FContextual
	{
		int32 FinalForm = 0;
		const FCodePointSet* Cased = nullptr;
		const FCodePointSet* Ignorable = nullptr;
	};

	static bool IsFinal(const TArray<int32>& Text, int32 Index, const FContextual& Context);

	TArray<int32> Firsts;
	TArray<int32> Lasts;
	TArray<int32> Strides;
	TArray<int32> Deltas;
	TMap<int32, TArray<int32>> Special;
	TMap<int32, FContextual> Contextual;
	// The lowercase of each ASCII code point that lowercases to one code point, or -1.
	int32 AsciiLower[128] = {};
};

/**
 * The compose step: canonical composition by the file's tables.
 */
class FComposer
{
  public:
	/**
	 * @brief Builds the composer from the file's compose step.
	 * @return False with OutError set if the tables are malformed.
	 */
	bool Init(const lingotion::textpreprocessing::Compose& Message, FString& OutError);

	/** Appends the composition of Text to Out. */
	void Apply(const TArray<int32>& Text, TArray<int32>& Out) const;

  private:
	int32 CombiningClass(int32 CodePoint) const;

	static uint64 Key(int32 First, int32 Second)
	{
		return (static_cast<uint64>(static_cast<uint32>(First)) << 21) | static_cast<uint32>(Second);
	}

	TMap<uint64, int32> Pairs;
	TArray<int32> Firsts;
	TArray<int32> Lasts;
	TArray<int32> Classes;
};
} // namespace Thespeon::Language
