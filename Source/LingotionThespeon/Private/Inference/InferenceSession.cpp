// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "InferenceSession.h"

namespace Thespeon
{
namespace Inference
{
const TCHAR* LexToString(ESessionStopReason Reason)
{
	switch (Reason)
	{
		case ESessionStopReason::None:
			return TEXT("None");
		case ESessionStopReason::Completed:
			return TEXT("Completed");
		case ESessionStopReason::UserCancelled:
			return TEXT("UserCancelled");
		case ESessionStopReason::ComponentDestroyed:
			return TEXT("ComponentDestroyed");
		case ESessionStopReason::NewSessionRequested:
			return TEXT("NewSessionRequested");
		case ESessionStopReason::Error:
			return TEXT("Error");
		default:
			return TEXT("Unknown");
	}
}

InferenceSession::InferenceSession(const FLingotionModelInput& InInput, const FInferenceConfig InConfig, const FString& InSessionID)
    : InputConfig(InConfig), SessionID(InSessionID)
{
	// Copy all input data from struct
	Input = InInput;
}

InferenceSession::~InferenceSession()
{
	AliveToken->store(false);
	Stop();
}

void InferenceSession::Stop()
{
	FScopeLock Lock(&StopLock);
	if (!bStopRequested.exchange(true))
	{
		// Default to user cancelled if no specific reason was set
		if (StopReason == ESessionStopReason::None)
		{
			StopReason = ESessionStopReason::UserCancelled;
		}
	}
}

bool InferenceSession::StopWithReason(ESessionStopReason Reason)
{
	FScopeLock Lock(&StopLock);
	if (bStopRequested.load())
	{
		return false; // Already stopped - return false to indicate we didn't set the reason
	}
	StopReason = Reason;
	bStopRequested.store(true);
	return true;
}

} // namespace Inference
} // namespace Thespeon
