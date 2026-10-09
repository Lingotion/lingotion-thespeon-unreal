// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Language/TextPreprocessingRules.h"
#include "Core/ModelInput.h"
#include "Core/text_preprocessing.pb.h"

namespace Thespeon::Language
{
/** One compiled normalization step. */
class FTextPreprocessingStep
{
  public:
	virtual ~FTextPreprocessingStep() = default;

	/** Appends the step's output for Text to Out, which is empty. */
	virtual void Apply(const TArray<int32>& Text, TArray<int32>& Out) const = 0;
};

namespace
{
class FComposeStep final : public FTextPreprocessingStep
{
  public:
	FComposer Composer;

	void Apply(const TArray<int32>& Text, TArray<int32>& Out) const override
	{
		Composer.Apply(Text, Out);
	}
};

class FLowercaseStep final : public FTextPreprocessingStep
{
  public:
	FLowercaser Lowercaser;

	void Apply(const TArray<int32>& Text, TArray<int32>& Out) const override
	{
		Lowercaser.Apply(Text, Out);
	}
};

class FCollapseWhitespaceStep final : public FTextPreprocessingStep
{
  public:
	explicit FCollapseWhitespaceStep(const FCodePointSet& InSpace) : Space(InSpace) {}

	void Apply(const TArray<int32>& Text, TArray<int32>& Out) const override
	{
		bool bPreviousWasSpace = false;
		for (const int32 CodePoint : Text)
		{
			if (Space.Contains(CodePoint))
			{
				if (!bPreviousWasSpace)
				{
					Out.Add(TEXT(' '));
				}
				bPreviousWasSpace = true;
			}
			else
			{
				Out.Add(CodePoint);
				bPreviousWasSpace = false;
			}
		}
	}

  private:
	const FCodePointSet& Space;
};

class FReplaceCharsStep final : public FTextPreprocessingStep
{
  public:
	FReplaceCharsStep(const TArray<int32>& InCharacters, TArray<int32> InReplacement) : Characters(InCharacters), Replacement(MoveTemp(InReplacement))
	{
	}

	void Apply(const TArray<int32>& Text, TArray<int32>& Out) const override
	{
		for (const int32 CodePoint : Text)
		{
			if (Characters.Contains(CodePoint))
			{
				Out.Append(Replacement);
			}
			else
			{
				Out.Add(CodePoint);
			}
		}
	}

  private:
	TSet<int32> Characters;
	TArray<int32> Replacement;
};

class FReplaceTextStep final : public FTextPreprocessingStep
{
  public:
	FReplaceTextStep(TArray<int32> InTarget, TArray<int32> InReplacement, bool bInWholeWord, const FWordSplitter& InWords)
	    : Target(MoveTemp(InTarget)), Replacement(MoveTemp(InReplacement)), bWholeWord(bInWholeWord), Words(InWords)
	{
	}

	void Apply(const TArray<int32>& Text, TArray<int32>& Out) const override
	{
		int32 i = 0;
		while (i < Text.Num())
		{
			if (Matches(Text, i) && (!bWholeWord || AtWordBoundaries(Text, i)))
			{
				Out.Append(Replacement);
				i += Target.Num();
			}
			else
			{
				Out.Add(Text[i]);
				++i;
			}
		}
	}

  private:
	bool Matches(const TArray<int32>& Text, int32 Position) const
	{
		if (Position + Target.Num() > Text.Num())
		{
			return false;
		}
		for (int32 i = 0; i < Target.Num(); ++i)
		{
			if (Text[Position + i] != Target[i])
			{
				return false;
			}
		}
		return true;
	}

	bool AtWordBoundaries(const TArray<int32>& Text, int32 Position) const
	{
		const int32 End = Position + Target.Num();
		const bool bStartsWord = Position == 0 || !Words.IsWordCharacter(Text[Position - 1]);
		const bool bEndsWord = End == Text.Num() || !Words.IsWordCharacter(Text[End]);
		return bStartsWord && bEndsWord;
	}

	TArray<int32> Target;
	TArray<int32> Replacement;
	bool bWholeWord;
	const FWordSplitter& Words;
};

class FKeepOnlyStep final : public FTextPreprocessingStep
{
  public:
	explicit FKeepOnlyStep(const FCodePointSet& InKeep) : Keep(InKeep) {}

	void Apply(const TArray<int32>& Text, TArray<int32>& Out) const override
	{
		// The format guarantees the audio sample request marker survives keep_only, so that no pack's rules
		// can remove what synthesis needs to find again.
		const int32 Marker = static_cast<int32>(Thespeon::ControlCharacters::AudioSampleRequest);
		for (const int32 CodePoint : Text)
		{
			if (Keep.Contains(CodePoint) || CodePoint == Marker)
			{
				Out.Add(CodePoint);
			}
		}
	}

  private:
	const FCodePointSet& Keep;
};
} // namespace

FTextPreprocessingRules::FTextPreprocessingRules() = default;

FTextPreprocessingRules::~FTextPreprocessingRules() = default;

TSharedPtr<const FTextPreprocessingRules, ESPMode::ThreadSafe> FTextPreprocessingRules::Load(const uint8* Data, int32 Size, FString& OutError)
{
	lingotion::textpreprocessing::TextPreprocessing Message;
	if (!Message.ParseFromArray(Data, Size))
	{
		OutError = TEXT("The file is not a valid text preprocessing file.");
		return nullptr;
	}
	return Compile(Message, OutError);
}

TSharedPtr<const FTextPreprocessingRules, ESPMode::ThreadSafe>
FTextPreprocessingRules::Compile(const lingotion::textpreprocessing::TextPreprocessing& Message, FString& OutError)
{
	TSharedPtr<FTextPreprocessingRules, ESPMode::ThreadSafe> Rules = MakeShareable(new FTextPreprocessingRules());
	if (!Rules->Init(Message, OutError))
	{
		return nullptr;
	}
	return Rules;
}

bool FTextPreprocessingRules::Init(const lingotion::textpreprocessing::TextPreprocessing& Message, FString& OutError)
{
	Version = FString::Printf(TEXT("%u.%u.%u"), Message.major_version(), Message.minor_version(), Message.patch_version());
	if (Message.major_version() != SupportedMajorVersion)
	{
		OutError = FString::Printf(
		    TEXT("Text preprocessing version %s is not supported by this version of Thespeon, which reads major version %u."),
		    *Version,
		    SupportedMajorVersion
		);
		return false;
	}
	Iso639_2 = CodePoints::Utf8ToString(Message.iso639_2());

	for (const lingotion::textpreprocessing::CharacterClass& ClassMessage : Message.character_classes())
	{
		const FString Name = CodePoints::Utf8ToString(ClassMessage.name());
		if (Classes.Contains(Name))
		{
			OutError = FString::Printf(TEXT("Character class '%s' is defined twice."), *Name);
			return false;
		}
		TUniquePtr<FCodePointSet> Class = MakeUnique<FCodePointSet>();
		if (!Class->Init(Name, ClassMessage.ranges().data(), ClassMessage.ranges_size(), OutError))
		{
			return false;
		}
		Classes.Add(Name, MoveTemp(Class));
	}

	// The words come before the steps, since a whole-word replace_text step asks them what a word character is.
	if (!Message.has_words())
	{
		OutError = FString::Printf(TEXT("Text preprocessing rules for '%s' define no words."), *Iso639_2);
		return false;
	}
	if (!Words.Init(Message.words(), Classes, OutError))
	{
		return false;
	}

	for (const lingotion::textpreprocessing::Step& StepMessage : Message.steps())
	{
		TUniquePtr<FTextPreprocessingStep> Step = CompileStep(StepMessage, OutError);
		if (!Step)
		{
			return false;
		}
		Steps.Add(MoveTemp(Step));
	}

	return Formatter.Init(Message.rule_sets(), OutError) && Numbers.Init(Message.number_forms(), Classes, Formatter, OutError);
}

TUniquePtr<FTextPreprocessingStep> FTextPreprocessingRules::CompileStep(const lingotion::textpreprocessing::Step& Message, FString& OutError)
{
	using lingotion::textpreprocessing::Step;
	switch (Message.kind_case())
	{
		case Step::kCompose:
		{
			TUniquePtr<FComposeStep> Compose = MakeUnique<FComposeStep>();
			if (!Compose->Composer.Init(Message.compose(), OutError))
			{
				return nullptr;
			}
			return Compose;
		}
		case Step::kLowercase:
		{
			TUniquePtr<FLowercaseStep> Lowercase = MakeUnique<FLowercaseStep>();
			if (!Lowercase->Lowercaser.Init(Message.lowercase(), Classes, OutError))
			{
				return nullptr;
			}
			if (!Lowercaser)
			{
				Lowercaser = &Lowercase->Lowercaser;
			}
			return Lowercase;
		}
		case Step::kCollapseWhitespace:
		{
			const FCodePointSet* Space = FindCharacterClass(Classes, CodePoints::Utf8ToString(Message.collapse_whitespace().space_class()), OutError);
			return Space ? MakeUnique<FCollapseWhitespaceStep>(*Space) : nullptr;
		}
		case Step::kReplaceChars:
			return MakeUnique<FReplaceCharsStep>(
			    CodePoints::FromUtf8(Message.replace_chars().characters()), CodePoints::FromUtf8(Message.replace_chars().replacement())
			);
		case Step::kReplaceText:
		{
			TArray<int32> Target = CodePoints::FromUtf8(Message.replace_text().text());
			if (Target.Num() == 0)
			{
				OutError = TEXT("replace_text needs a text to replace.");
				return nullptr;
			}
			return MakeUnique<FReplaceTextStep>(
			    MoveTemp(Target), CodePoints::FromUtf8(Message.replace_text().replacement()), Message.replace_text().whole_word(), Words
			);
		}
		case Step::kKeepOnly:
		{
			const FCodePointSet* Keep = FindCharacterClass(Classes, CodePoints::Utf8ToString(Message.keep_only().character_class()), OutError);
			return Keep ? MakeUnique<FKeepOnlyStep>(*Keep) : nullptr;
		}
		default:
			// Also what a step of a kind added to the format after this version looks like.
			OutError = TEXT("A text preprocessing step is empty, or of a kind this version of Thespeon does not know.");
			return nullptr;
	}
}

FString FTextPreprocessingRules::ApplySteps(const FString& Text) const
{
	TArray<int32> Current = CodePoints::FromString(Text);
	TArray<int32> Next;
	for (const TUniquePtr<FTextPreprocessingStep>& Step : Steps)
	{
		Next.Reset();
		Step->Apply(Current, Next);
		Swap(Current, Next);
	}
	return CodePoints::ToString(Current);
}
} // namespace Thespeon::Language
