// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"

enum class ETextPosition
{
	First,
	Middle,
	Last,
	Only // For single-segment case
};

inline ETextPosition GetSegmentPosition(int32 SegmentIndex, int32 TotalSegments)
{
	if (TotalSegments == 1)
	{
		return ETextPosition::Only;
	}

	if (SegmentIndex == 0)
	{
		return ETextPosition::First;
	}
	else if (SegmentIndex == TotalSegments - 1)
	{
		return ETextPosition::Last;
	}

	return ETextPosition::Middle;
}