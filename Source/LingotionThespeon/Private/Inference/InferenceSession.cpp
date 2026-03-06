// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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
    : InputConfig(InConfig), SessionID(InSessionID), StopReason(ESessionStopReason::None)
{
	// Copy all input data from struct
	Input = InInput;
	bStopRequested.Store(false);
}

InferenceSession::~InferenceSession()
{
	AliveToken->Store(false);
	Stop();
}

void InferenceSession::Stop()
{
	// Only process stop once - use exchange to atomically set and check previous value
	if (!bStopRequested.Exchange(true))
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
	if (bStopRequested.Load())
	{
		return true; // Already stopped
	}
	StopReason = Reason;
	Stop();
	return true;
}

} // namespace Inference
} // namespace Thespeon
