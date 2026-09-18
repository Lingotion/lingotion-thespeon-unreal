// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "Core/ModelInput.h"

namespace Thespeon::Core
{
/**
 * Removes EEmotion::None, clamps each weight to [0, 1], drops non-positive entries, and normalizes the remaining weights to sum to 1.
 *
 * @param Blend Modified in place.
 * @return true if at least one positive-weight emotion remained; false if the blend was emptied.
 */
bool SanitizeEmotionBlend(TMap<EEmotion, float>& Blend);

/**
 * Linearly interpolates between two emotion blends and returns a normalized blend.
 * Emotions absent from either endpoint are treated as having zero weight there.
 */
[[nodiscard]] TMap<EEmotion, float>
InterpolateEmotionKeypoints(const TMap<EEmotion, float>& StartBlend, const TMap<EEmotion, float>& EndBlend, float Alpha);

/**
 * Sanitizes the supplied emotion keypoints and populates every segment boundary
 * by sampling the resulting piecewise-linear curve at global character indices.
 */
[[nodiscard]] bool PopulateEmotionKeypoints(TArray<FLingotionInputSegment>& Segments, EEmotion DefaultEmotion);

/**
 * Populates every segment's StartSpeed/EndSpeed by sampling the piecewise-linear curve
 * formed by all segments' speed keypoints at global character indices.
 */
[[nodiscard]] bool PopulateSpeedKeypoints(TArray<FLingotionInputSegment>& Segments);

/**
 * Populates every segment's StartLoudness/EndLoudness by sampling the piecewise-linear curve
 * formed by all segments' loudness keypoints at global character indices.
 */
[[nodiscard]] bool PopulateLoudnessKeypoints(TArray<FLingotionInputSegment>& Segments);
} // namespace Thespeon::Core
