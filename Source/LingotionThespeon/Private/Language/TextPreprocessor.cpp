// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "TextPreprocessor.h"
#include "Core/KeypointUtils.h"
#include "Core/LingotionLogger.h"
#include "Language/TextPreprocessingRules.h"

namespace
{
bool IsOnlyMarkers(const FString& Text)
{
	for (const TCHAR C : Text)
	{
		if (C != Thespeon::ControlCharacters::AudioSampleRequest)
		{
			return false;
		}
	}
	return true;
}
} // namespace

bool FTextPreprocessor::PreprocessSegments(TArray<FLingotionInputSegment>& Segments, FRulesForLanguage RulesForLanguage, EEmotion DefaultEmotion)
{
	TArray<FLingotionInputSegment> Result;
	Result.Reserve(Segments.Num());
	for (const FLingotionInputSegment& Segment : Segments)
	{
		// The language's rules are not applied to custom pronunciation: several characters they rewrite are IPA
		// symbols, such as U+02C8 (primary stress, also an apostrophe look-alike) and U+0303 (nasalisation,
		// which composition would fold into the letter before it).
		if (Segment.bIsCustomPronounced)
		{
			FLingotionInputSegment& Copy = Result.Add_GetRef(Segment);
			Copy.Text = CollapseAudioSampleRequests(Segment.Text);
			continue;
		}
		const Thespeon::Language::FTextPreprocessingRules* Rules = RulesForLanguage(Segment.Language);
		if (!Rules)
		{
			return false; // RulesForLanguage logged why
		}
		if (!PreprocessSegment(Segment, *Rules, Result))
		{
			return false;
		}
	}
	if (Result.Num() == 0)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("No text is left to synthesize after text preprocessing. Please make sure the segments contain text the language can read.")
		);
		return false;
	}
	if (!Thespeon::Core::PopulateEmotionKeypoints(Result, DefaultEmotion) || !Thespeon::Core::PopulateSpeedKeypoints(Result) ||
	    !Thespeon::Core::PopulateLoudnessKeypoints(Result))
	{
		return false;
	}
	Segments = MoveTemp(Result);
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Preprocessed into %d segments"), Segments.Num());
	return true;
}

bool FTextPreprocessor::PreprocessSegment(
    const FLingotionInputSegment& Segment, const Thespeon::Language::FTextPreprocessingRules& Rules, TArray<FLingotionInputSegment>& OutSegments
)
{
	const FString Cleaned = CollapseAudioSampleRequests(Rules.ApplySteps(Segment.Text));
	if (Cleaned.IsEmpty())
	{
		// The voice models' training drops a segment its cleanup empties, so the same is done here.
		LINGO_LOG(
		    EVerbosityLevel::Warning,
		    TEXT("Segment '%s' has no text left after text preprocessing and is left out, together with its keypoints."),
		    *Segment.Text
		);
		return true;
	}

	TArray<FSegmentPart> Parts;
	bool bHasNumber = false;
	for (Thespeon::Language::FNumberExpander::FPart& Part : Rules.GetNumbers().Partition(Cleaned))
	{
		bHasNumber |= Part.bIsNumber;
		Parts.Add({MoveTemp(Part.Text), Part.bIsNumber, {}});
	}
	if (!bHasNumber)
	{
		FLingotionInputSegment& Copy = OutSegments.Add_GetRef(Segment);
		Copy.Text = Cleaned;
		return true;
	}
	int32 MarkerIndex = INDEX_NONE;
	if (Segment.Text.FindChar(Thespeon::ControlCharacters::AudioSampleRequest, MarkerIndex))
	{
		Parts = MergeMarkerSeparatedNumbers(Parts);
	}

	TArray<FLingotionInputSegment> SplitSegments;
	FLingotionInputSegment Current = MakeSplitSegment(Segment, FString(), false);
	for (const FSegmentPart& Part : Parts)
	{
		FString PartText = Part.Text;
		if (Part.bIsNumber)
		{
			FString Error;
			if (!Rules.GetNumbers().Expand(Part.Text, PartText, Error))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Cannot speak out the number '%s': %s"), *Part.Text, *Error);
				return false;
			}
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Converted number '%s' -> '%s' (CustomPronounced)"), *Part.Text, *PartText);
		}
		if (Part.MarkerFractions.Num() > 0)
		{
			// Put the markers back at the same fraction of the spoken number as of the written one.
			TArray<int32> Indices;
			for (const float Fraction : Part.MarkerFractions)
			{
				// Half to even, as Unity's Mathf.RoundToInt does, so that both place a marker alike.
				Indices.Add(static_cast<int32>(FMath::RoundHalfToEven(FMath::Clamp(Fraction, 0.0f, 1.0f) * PartText.Len())));
			}
			Indices.Sort();
			int32 Added = 0;
			for (const int32 Index : Indices)
			{
				PartText.InsertAt(FMath::Clamp(Index, 0, PartText.Len() - Added) + Added, Thespeon::ControlCharacters::AudioSampleRequest);
				++Added;
			}
		}
		if (IsOnlyMarkers(Current.Text))
		{
			Current.bIsCustomPronounced = Part.bIsNumber;
		}
		// Text with nothing to pronounce (punctuation, spaces, markers) joins the segment before it rather than
		// starting one. A number always starts one: it is made of digits, which the word definition does not
		// count as a word.
		const bool bNothingToPronounce = !Part.bIsNumber && !Rules.GetWords().ContainsWord(Part.Text);
		if (Current.bIsCustomPronounced == Part.bIsNumber || bNothingToPronounce)
		{
			Current.Text += PartText;
		}
		else
		{
			if (!Current.Text.IsEmpty())
			{
				SplitSegments.Add(MoveTemp(Current));
			}
			Current = MakeSplitSegment(Segment, PartText, Part.bIsNumber);
		}
	}
	if (!Current.Text.IsEmpty())
	{
		SplitSegments.Add(MoveTemp(Current));
	}
	if (SplitSegments.Num() == 0)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Segment preprocessing unexpectedly produced no segments."));
		return false;
	}
	ApplySplitKeypoints(Segment, SplitSegments);
	OutSegments.Append(MoveTemp(SplitSegments));
	return true;
}

FString FTextPreprocessor::CollapseAudioSampleRequests(const FString& Text)
{
	FString Result;
	Result.Reserve(Text.Len());
	for (const TCHAR C : Text)
	{
		if (C == Thespeon::ControlCharacters::AudioSampleRequest && !Result.IsEmpty() && Result[Result.Len() - 1] == C)
		{
			continue;
		}
		Result.AppendChar(C);
	}
	return Result;
}

TArray<FTextPreprocessor::FSegmentPart> FTextPreprocessor::MergeMarkerSeparatedNumbers(const TArray<FSegmentPart>& Parts)
{
	auto IsMarkersOnly = [](const FSegmentPart& Part) { return !Part.bIsNumber && !Part.Text.IsEmpty() && IsOnlyMarkers(Part.Text); };

	TArray<FSegmentPart> Result;
	int32 i = 0;
	while (i < Parts.Num())
	{
		if (!Parts[i].bIsNumber)
		{
			Result.Add(Parts[i]);
			++i;
			continue;
		}
		// A chain number, markers, number, ... ends before part j.
		int32 j = i + 1;
		bool bCanMerge = false;
		int32 TotalDigitsLength = Parts[i].Text.Len();
		while (j < Parts.Num() && (Parts[j].bIsNumber || IsMarkersOnly(Parts[j])))
		{
			if (Parts[j].bIsNumber)
			{
				bCanMerge = true;
				TotalDigitsLength += Parts[j].Text.Len();
			}
			++j;
		}
		if (!bCanMerge)
		{
			Result.Add(Parts[i]);
			++i;
			continue;
		}
		// The merged number without its markers, each marker kept as where it was, as a fraction of its length.
		FSegmentPart Merged{Parts[i].Text, true, {}};
		for (int32 k = i + 1; k < j; ++k)
		{
			if (Parts[k].bIsNumber)
			{
				Merged.Text += Parts[k].Text;
			}
			else
			{
				const float Fraction = FMath::Clamp(static_cast<float>(Merged.Text.Len()) / TotalDigitsLength, 0.0f, 1.0f);
				for (int32 m = 0; m < Parts[k].Text.Len(); ++m)
				{
					Merged.MarkerFractions.Add(Fraction);
				}
			}
		}
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Merged number '%s' with %d markers"), *Merged.Text, Merged.MarkerFractions.Num());
		Result.Add(MoveTemp(Merged));
		i = j;
	}
	return Result;
}

FLingotionInputSegment FTextPreprocessor::MakeSplitSegment(const FLingotionInputSegment& Original, const FString& Text, bool bIsCustomPronounced)
{
	FLingotionInputSegment Segment = Original;
	Segment.Text = Text;
	Segment.bIsCustomPronounced = bIsCustomPronounced;
	Segment.StartEmotion.Reset();
	Segment.EndEmotion.Reset();
	return Segment;
}

void FTextPreprocessor::ApplySplitKeypoints(const FLingotionInputSegment& Original, TArray<FLingotionInputSegment>& SplitSegments)
{
	SplitSegments[0].StartEmotion = Original.StartEmotion;
	SplitSegments.Last().EndEmotion = Original.EndEmotion;

	int32 TotalLength = 0;
	for (const FLingotionInputSegment& Segment : SplitSegments)
	{
		TotalLength += Segment.Text.Len();
	}
	if (SplitSegments.Num() == 1 || TotalLength <= 1)
	{
		SplitSegments[0].StartSpeed = Original.StartSpeed;
		SplitSegments[0].EndSpeed = Original.EndSpeed;
		SplitSegments[0].StartLoudness = Original.StartLoudness;
		SplitSegments[0].EndLoudness = Original.EndLoudness;
		return;
	}
	int32 Cursor = 0;
	for (FLingotionInputSegment& Segment : SplitSegments)
	{
		const int32 EndPosition = Cursor + Segment.Text.Len() - 1;
		const float StartAlpha = static_cast<float>(Cursor) / static_cast<float>(TotalLength - 1);
		const float EndAlpha = static_cast<float>(EndPosition) / static_cast<float>(TotalLength - 1);
		Segment.StartSpeed = FMath::Lerp(Original.StartSpeed, Original.EndSpeed, StartAlpha);
		Segment.EndSpeed = FMath::Lerp(Original.StartSpeed, Original.EndSpeed, EndAlpha);
		Segment.StartLoudness = FMath::Lerp(Original.StartLoudness, Original.EndLoudness, StartAlpha);
		Segment.EndLoudness = FMath::Lerp(Original.StartLoudness, Original.EndLoudness, EndAlpha);
		Cursor = EndPosition + 1;
	}
}
