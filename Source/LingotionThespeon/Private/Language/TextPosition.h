// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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