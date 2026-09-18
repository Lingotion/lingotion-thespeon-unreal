// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Core/KeypointUtils.h"

#include "Core/LingotionLogger.h"

namespace
{
// Will later be replaced by animcurves and its interpolation methods,
// but for now we can use a simple piecewise-linear curve to represent keypoints.
template <typename ValueType> struct TCurvePoint
{
	int32 Position = 0;
	ValueType Value{};
};

// Speed and loudness have no "unset" representation - every segment's value is always
// meaningful, so it always contributes a keypoint to the curve.
bool SanitizeScalar(float&)
{
	return true;
}

float InterpolateScalar(const float& Start, const float& End, const float Alpha)
{
	return FMath::Lerp(Start, End, Alpha);
}

template <typename ValueType, typename InterpolateFn>
ValueType SampleCurve(const TArray<TCurvePoint<ValueType>>& Curve, const int32 Position, const bool bSampleRightSide, InterpolateFn&& Interpolate)
{
	if (Position < Curve[0].Position)
	{
		return Curve[0].Value;
	}
	if (Position > Curve.Last().Position)
	{
		return Curve.Last().Value;
	}

	int32 FirstAtOrAfter = 0;
	while (FirstAtOrAfter < Curve.Num() && Curve[FirstAtOrAfter].Position < Position)
	{
		++FirstAtOrAfter;
	}
	// If the position is exactly at a keypoint, sample the left side when
	// bSampleRightSide is false, and the right side when it is true.
	if (FirstAtOrAfter < Curve.Num() && Curve[FirstAtOrAfter].Position == Position)
	{
		int32 LastAtPosition = FirstAtOrAfter;
		// There should be at most 2 keypoints at the same position (a discontinuity),
		// but handle more than 2 just in case by picking the last one.
		while (LastAtPosition + 1 < Curve.Num() && Curve[LastAtPosition + 1].Position == Position)
		{
			++LastAtPosition;
		}
		return Curve[bSampleRightSide ? LastAtPosition : FirstAtOrAfter].Value;
	}

	const TCurvePoint<ValueType>& StartPoint = Curve[FirstAtOrAfter - 1];
	const TCurvePoint<ValueType>& EndPoint = Curve[FirstAtOrAfter];
	const float Alpha = static_cast<float>(Position - StartPoint.Position) / static_cast<float>(EndPoint.Position - StartPoint.Position);
	return Interpolate(StartPoint.Value, EndPoint.Value, Alpha);
}

/**
 * Generic engine shared by every keypoint curve type. Builds a piecewise-linear curve from
 * each segment's start/end boundary values (via StartField/EndField) and resamples every
 * segment's boundaries at its global character position, so per-segment values stay
 * continuous across segment splits.
 *
 * @param Sanitize             Validates/clamps a value in place; returning false excludes it from the curve
 *                              (used for emotion's "no opinion" state - scalar curves always return true).
 * @param Interpolate          Blends between two curve values at Alpha in [0, 1].
 * @param DefaultValue         Seeds a single keypoint at position 0 when the curve ends up empty.
 * @param bDefaultValueIsValid Whether DefaultValue may be used as that fallback; if false and the curve
 *                              is empty, population fails.
 * @param CurveName            Human-readable curve name used in log messages (e.g. "emotion", "speed").
 */
template <typename ValueType, typename SanitizeFn, typename InterpolateFn>
bool PopulateKeypointCurve(
    TArray<FLingotionInputSegment>& Segments,
    ValueType FLingotionInputSegment::*StartField,
    ValueType FLingotionInputSegment::*EndField,
    SanitizeFn&& Sanitize,
    InterpolateFn&& Interpolate,
    const ValueType& DefaultValue,
    const bool bDefaultValueIsValid,
    const TCHAR* CurveName
)
{
	TArray<TCurvePoint<ValueType>> Curve;
	TArray<int32> SegmentStartPositions;
	TArray<int32> SegmentEndPositions;
	SegmentStartPositions.Reserve(Segments.Num());
	SegmentEndPositions.Reserve(Segments.Num());

	int32 GlobalPosition = 0;
	for (FLingotionInputSegment& Segment : Segments)
	{
		if (Segment.Text.IsEmpty())
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Cannot populate %s keypoints for an empty segment."), CurveName);
			return false;
		}

		const int32 StartPosition = GlobalPosition;
		const int32 EndPosition = StartPosition + Segment.Text.Len() - 1;
		SegmentStartPositions.Add(StartPosition);
		SegmentEndPositions.Add(EndPosition);

		if (Sanitize(Segment.*StartField))
		{
			Curve.Add({StartPosition, Segment.*StartField});
		}
		if (Sanitize(Segment.*EndField))
		{
			Curve.Add({EndPosition, Segment.*EndField});
		}

		GlobalPosition += Segment.Text.Len();
	}

	if (Curve.Num() == 0)
	{
		if (!bDefaultValueIsValid)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("No %s keypoints were supplied and no valid default value was given."), CurveName);
			return false;
		}
		Curve.Add({0, DefaultValue});
	}

	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		Segments[SegmentIndex].*StartField = SampleCurve<ValueType>(Curve, SegmentStartPositions[SegmentIndex], false, Interpolate);
		Segments[SegmentIndex].*EndField = SampleCurve<ValueType>(Curve, SegmentEndPositions[SegmentIndex], true, Interpolate);
	}

	return true;
}
} // namespace

bool Thespeon::Core::SanitizeEmotionBlend(TMap<EEmotion, float>& Blend)
{
	Blend.Remove(EEmotion::None);

	float WeightSum = 0.0f;
	for (auto It = Blend.CreateIterator(); It; ++It)
	{
		const float OriginalWeight = It.Value();
		float ClampedWeight = OriginalWeight;
		if (!FMath::IsFinite(OriginalWeight))
		{
			ClampedWeight = !FMath::IsNaN(OriginalWeight) && OriginalWeight > 0.0f ? 1.0f : 0.0f;
		}
		else
		{
			ClampedWeight = FMath::Clamp(OriginalWeight, 0.0f, 1.0f);
		}

		if (!FMath::IsNearlyEqual(OriginalWeight, ClampedWeight))
		{
			LINGO_LOG(
			    EVerbosityLevel::Warning,
			    TEXT("Emotion weight for %s must be between 0 and 1. Clamping %g to %g."),
			    *UEnum::GetValueAsString(It.Key()),
			    OriginalWeight,
			    ClampedWeight
			);
		}

		if (ClampedWeight <= 0.0f)
		{
			It.RemoveCurrent();
			continue;
		}

		It.Value() = ClampedWeight;
		WeightSum += ClampedWeight;
	}

	if (WeightSum <= 0.0f)
	{
		Blend.Reset();
		return false;
	}

	for (TPair<EEmotion, float>& Pair : Blend)
	{
		Pair.Value /= WeightSum;
	}
	return true;
}

TMap<EEmotion, float>
Thespeon::Core::InterpolateEmotionKeypoints(const TMap<EEmotion, float>& StartBlend, const TMap<EEmotion, float>& EndBlend, const float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	TMap<EEmotion, float> Result;
	for (const TPair<EEmotion, float>& Pair : StartBlend)
	{
		Result.Add(Pair.Key, Pair.Value * (1.0f - ClampedAlpha));
	}
	for (const TPair<EEmotion, float>& Pair : EndBlend)
	{
		Result.FindOrAdd(Pair.Key) += Pair.Value * ClampedAlpha;
	}

	// Interpolation of L1 unit vectors remains an L1 unit vector. Normalize once
	// more to remove zero components and absorb floating-point accumulation error.
	SanitizeEmotionBlend(Result);
	return Result;
}

bool Thespeon::Core::PopulateEmotionKeypoints(TArray<FLingotionInputSegment>& Segments, const EEmotion DefaultEmotion)
{
	TMap<EEmotion, float> DefaultBlend;
	const bool bDefaultValueIsValid = DefaultEmotion != EEmotion::None;
	if (bDefaultValueIsValid)
	{
		DefaultBlend.Add(DefaultEmotion, 1.0f);
	}

	return PopulateKeypointCurve<TMap<EEmotion, float>>(
	    Segments,
	    &FLingotionInputSegment::StartEmotion,
	    &FLingotionInputSegment::EndEmotion,
	    &SanitizeEmotionBlend,
	    &Thespeon::Core::InterpolateEmotionKeypoints,
	    DefaultBlend,
	    bDefaultValueIsValid,
	    TEXT("emotion")
	);
}

bool Thespeon::Core::PopulateSpeedKeypoints(TArray<FLingotionInputSegment>& Segments)
{
	return PopulateKeypointCurve<float>(
	    Segments,
	    &FLingotionInputSegment::StartSpeed,
	    &FLingotionInputSegment::EndSpeed,
	    &SanitizeScalar,
	    &InterpolateScalar,
	    1.0f,
	    true,
	    TEXT("speed")
	);
}

bool Thespeon::Core::PopulateLoudnessKeypoints(TArray<FLingotionInputSegment>& Segments)
{
	return PopulateKeypointCurve<float>(
	    Segments,
	    &FLingotionInputSegment::StartLoudness,
	    &FLingotionInputSegment::EndLoudness,
	    &SanitizeScalar,
	    &InterpolateScalar,
	    1.0f,
	    true,
	    TEXT("loudness")
	);
}
