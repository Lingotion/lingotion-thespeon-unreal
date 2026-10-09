// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Core/ModelInput.h"
#include "Core/Language.h"

namespace Thespeon::Language
{
class FTextPreprocessingRules;
} // namespace Thespeon::Language

/**
 * Internal static utility class that preprocesses the segments of an input before phonemization.
 *
 * What depends on the language - normalization, how numbers are spoken, what a word is - comes from the
 * segment's language pack (FTextPreprocessingRules). What does not stays here: splitting segments around
 * numbers, audio sample request markers, and speed/loudness/emotion keypoints.
 */
class LINGOTIONTHESPEON_API FTextPreprocessor
{
  public:
	// Deleted constructors - static-only utility class
	FTextPreprocessor() = delete;
	FTextPreprocessor(const FTextPreprocessor&) = delete;
	FTextPreprocessor& operator=(const FTextPreprocessor&) = delete;

	/**
	 * Returns the text preprocessing rules for a segment's language, or nullptr (having logged why) if there are
	 * none, which fails preprocessing.
	 */
	using FRulesForLanguage = TFunctionRef<const Thespeon::Language::FTextPreprocessingRules*(const FLingotionLanguage&)>;

	/**
	 * Preprocesses the segments in place, then populates their emotion, speed and loudness keypoints.
	 *
	 * Each natural language segment is run through its language's rules, and each number in it is spoken out in
	 * phonemes as a segment of its own, marked as custom pronounced so that it is not phonemized again:
	 *
	 *   "I have 44 apples" becomes "i have " (natural), "fˈɔːɹɾi fˈoːɹ" (custom pronounced), " apples" (natural).
	 *
	 * Custom pronunciation (IPA) segments are left alone except that runs of audio sample request markers are
	 * collapsed - several characters the rules rewrite are IPA symbols, such as U+02C8 (primary stress).
	 * A segment left with no text is dropped, as the voice models' training does.
	 *
	 * @param Segments The segments to preprocess, with their languages resolved.
	 * @param RulesForLanguage Returns the rules for a segment's language. Only asked for natural language segments.
	 * @param DefaultEmotion The emotion for the keypoints when no segment gives one.
	 * @return False if a segment's language has no rules, a number cannot be spoken, no text is left, or the
	 *         keypoints cannot be populated. The reason is logged.
	 */
	[[nodiscard]] static bool
	PreprocessSegments(TArray<FLingotionInputSegment>& Segments, FRulesForLanguage RulesForLanguage, EEmotion DefaultEmotion);

	/** Collapses every run of audio sample request markers into one. Not a language rule, so it needs none. */
	static FString CollapseAudioSampleRequests(const FString& Text);

  private:
	/** A part of a partitioned segment: text or a number, and for a merged number its markers' positions. */
	struct FSegmentPart
	{
		FString Text;
		bool bIsNumber = false;
		// Where the markers that were inside a merged number were, as fractions of its length.
		TArray<float> MarkerFractions;
	};

	static bool PreprocessSegment(
	    const FLingotionInputSegment& Segment, const Thespeon::Language::FTextPreprocessingRules& Rules, TArray<FLingotionInputSegment>& OutSegments
	);

	/** Merges numbers separated by nothing but markers into one number, remembering where the markers were. */
	static TArray<FSegmentPart> MergeMarkerSeparatedNumbers(const TArray<FSegmentPart>& Parts);

	/** A sub-segment of Original with empty emotion blends, so that splitting adds no interior emotion keypoints. */
	static FLingotionInputSegment MakeSplitSegment(const FLingotionInputSegment& Original, const FString& Text, bool bIsCustomPronounced);

	/**
	 * Restores the original segment's keypoints onto the sub-segments splitting produced. Emotion keeps only the
	 * outer endpoints (interior blends stay empty so the curve passes straight through), while speed and loudness,
	 * which have no unset representation, are resampled from the original linear ramp at each new boundary.
	 */
	static void ApplySplitKeypoints(const FLingotionInputSegment& Original, TArray<FLingotionInputSegment>& SplitSegments);
};
