// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Language/UnicodeTables.h"
#include "Language/RuleBasedNumberFormatter.h"
#include "Language/NumberExpander.h"
#include "Language/WordSplitter.h"

namespace lingotion::textpreprocessing
{
class TextPreprocessing;
class Step;
} // namespace lingotion::textpreprocessing

namespace Thespeon::Language
{
class FTextPreprocessingStep;

/**
 * The text preprocessing of one language, compiled from a language pack's text preprocessing (.textprep)
 * file: the steps that normalize natural language text, how its numbers are spoken, and what a word is.
 *
 * Everything that depends on Unicode comes from the file, so text is read the same way on every platform and
 * engine version, and the same way as in the Unity package and model-meta-graph's textprep_runner.py. See
 * text_preprocessing.proto in model-meta-graph for the format.
 *
 * Immutable once compiled, so one instance is shared by every synthesis session on any thread.
 */
class FTextPreprocessingRules
{
  public:
	/** The major version of the text preprocessing format this version of Thespeon reads. */
	static constexpr uint32 SupportedMajorVersion = 3;

	/**
	 * @brief Parses and compiles the contents of a .textprep file.
	 * @param Data The file's bytes.
	 * @param Size The number of bytes.
	 * @param OutError Receives the reason when the file cannot be parsed or its rules are invalid.
	 * @return The compiled rules, or nullptr on failure.
	 */
	static TSharedPtr<const FTextPreprocessingRules, ESPMode::ThreadSafe> Load(const uint8* Data, int32 Size, FString& OutError);

	/**
	 * @brief Compiles parsed text preprocessing rules.
	 * @param Message The parsed file.
	 * @param OutError Receives the reason when the file is of a format version this version of Thespeon does
	 *        not read, or a table, step, number form, rule set or the word definition is malformed.
	 * @return The compiled rules, or nullptr on failure.
	 */
	static TSharedPtr<const FTextPreprocessingRules, ESPMode::ThreadSafe>
	Compile(const lingotion::textpreprocessing::TextPreprocessing& Message, FString& OutError);

	~FTextPreprocessingRules();

	/** Runs the normalization steps, in order, on natural language text. Not for custom pronunciation. */
	FString ApplySteps(const FString& Text) const;

	/** The numbers of the language: where they are in a text, and how they are spoken. */
	const FNumberExpander& GetNumbers() const
	{
		return Numbers;
	}

	/** What a word of the language is: the unit that is looked up in the lookup table and phonemized. */
	const FWordSplitter& GetWords() const
	{
		return Words;
	}

	/** The ISO 639-2 code of the language these rules are for. */
	const FString& GetIso639_2() const
	{
		return Iso639_2;
	}

	/** The file's format version, as major.minor.patch. */
	const FString& GetVersion() const
	{
		return Version;
	}

	/** The first lowercase step, or nullptr if there is none. For the conformance tests. */
	const FLowercaser* GetLowercaser() const
	{
		return Lowercaser;
	}

	/** The character class of the given name, or nullptr. For the conformance tests. */
	const FCodePointSet* FindClass(const FString& Name) const
	{
		const TUniquePtr<FCodePointSet>* Found = Classes.Find(Name);
		return Found ? Found->Get() : nullptr;
	}

  private:
	FTextPreprocessingRules();

	bool Init(const lingotion::textpreprocessing::TextPreprocessing& Message, FString& OutError);
	TUniquePtr<FTextPreprocessingStep> CompileStep(const lingotion::textpreprocessing::Step& Message, FString& OutError);

	FString Iso639_2;
	FString Version;
	FCharacterClasses Classes;
	FWordSplitter Words;
	TArray<TUniquePtr<FTextPreprocessingStep>> Steps;
	const FLowercaser* Lowercaser = nullptr;
	FRuleBasedNumberFormatter Formatter;
	FNumberExpander Numbers;
};
} // namespace Thespeon::Language
