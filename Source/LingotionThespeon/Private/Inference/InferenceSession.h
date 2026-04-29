// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "NNETypes.h"
#include "Containers/Queue.h"
#include "SessionTensorPool.h"
#include "Core/ModelInput.h"
#include "Engine/InferenceConfig.h"
#include "HAL/Runnable.h"
#include "HAL/Event.h"
#include "HAL/CriticalSection.h"
#include "Core/ThespeonDataPacket.h"
#include <atomic>

namespace Thespeon
{

namespace Inference
{
/** Reason for session cancellation/stop */
enum class ESessionStopReason : uint8
{
	None = 0,            // Not stopped
	Completed,           // Normal completion
	UserCancelled,       // User requested cancellation
	ComponentDestroyed,  // Owning component is being destroyed
	NewSessionRequested, // A new synthesis session was requested
	Error                // Stopped due to an error
};

/** @brief Converts ESessionStopReason to a human-readable string. */
const TCHAR* LexToString(ESessionStopReason Reason);

/**
 * Abstract base class for inference sessions that handle a complete inference chain from input to output.
 * Runs several workloads according to the particular implementation's inference scheme.
 */
class InferenceSession : public FRunnable
{
  public:
	InferenceSession(const FLingotionModelInput& InInput, FInferenceConfig InConfig, const FString& InSessionID);

	~InferenceSession() override;

	bool Init() override
	{
		return true;
	}

	uint32 Run() override = 0;

	/**
	 * @brief Requests the session to stop execution.
	 * Called when thread termination is requested (e.g., component destruction, new session).
	 * Sets the stop flag.
	 */
	void Stop() override;

	/**
	 * @brief Called after Run() completes (whether normally or due to Stop).
	 */
	void Exit() override
	{
		// Base implementation - subclasses can override for additional cleanup
	}

	/**
	 * @brief Requests the session to stop with a specific reason.
	 * @param Reason The reason for stopping the session
	 * @return true if stop was initiated by this call, false if already stopped
	 */
	bool StopWithReason(ESessionStopReason Reason);

	/** @brief Returns true if stop has been requested. */
	bool ShouldStop() const
	{
		return bStopRequested.load();
	}

	/** @brief Returns the reason for stopping. */
	ESessionStopReason GetStopReason() const
	{
		FScopeLock Lock(&StopLock);
		return StopReason;
	}

	/** Delegate for ThespeonComponent to bind to its packet handler function. */
	DECLARE_DELEGATE_TwoParams(FOnDataSynthesized, const FString& /*SessionID*/, const Thespeon::Core::FThespeonDataPacket& /*DataPacket*/);
	FOnDataSynthesized OnDataSynthesized;

  protected:
	FLingotionModelInput Input;
	FInferenceConfig InputConfig;
	FString SessionID;
	SessionTensorPool TensorPool;

	/** Thread-safe flag indicating stop has been requested */
	std::atomic<bool> bStopRequested{false};
	/** Reason for the stop request — protected by StopLock */
	ESessionStopReason StopReason = ESessionStopReason::None;
	/** Protects StopReason against concurrent reads/writes */
	mutable FCriticalSection StopLock;
	/** Shared token used to detect object lifetime in async lambdas */
	TSharedRef<std::atomic<bool>> AliveToken = MakeShared<std::atomic<bool>>(true);
};

} // namespace Inference
} // namespace Thespeon
