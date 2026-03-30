// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Core/BackendType.h"
#include "Core/ModelInput.h"
#include <atomic>

// Forward declarations — subsystem pointers captured on game thread and passed at construction
class UInferenceWorkloadManager;
class UModuleManager;
class ULookupTableManager;
class UManifestHandler;

namespace Thespeon
{
namespace Inference
{

/**
 * FRunnable that orchestrates parallel preloading of all modules for a character.
 *
 * Dispatches per-module work (GetModule + RegisterModuleWorkload + RegisterLookupTable)
 * as independent TaskGraph tasks, achieving parallelism bounded by the thread pool size.
 * Supports cooperative cancellation via Stop() and deterministic join via FRunnableThread::WaitForCompletion().
 *
 * Replaces the previous fire-and-forget AsyncTask pattern in RunPreloadRequest, which was
 * non-joinable and caused access violations during PIE teardown (see PR #144 I1).
 *
 * Rationale for FRunnable over AsyncTask:
 *   - Joinable: WaitForCompletion() enables deterministic cleanup in OnUnregister
 *   - Cancellable: Stop() + atomic flag enables cooperative shutdown
 *   - Lifecycle control: Component owns the thread, no fire-and-forget
 *   See: UE docs "FRunnable" — "If you need to control the lifetime of your thread, use FRunnable."
 *   Precedent: InferenceSession already uses this pattern for the synthesis path.
 *
 * Rationale for Async(TaskGraph) per-module dispatch:
 *   Uses Unreal's thread pool — no OS thread creation overhead.
 *   A typical character has multiple language modules + 1 character module, well within TaskGraph capacity.
 */
class FPreloadSession : public FRunnable
{
  public:
	/** Completion callback signature. Fired on the game thread when preload finishes or is cancelled. */
	using FOnComplete = TFunction<void(bool bSuccess, FString CharacterName, EThespeonModuleType ModuleType, EBackendType BackendType)>;

	FPreloadSession(
	    FString InCharacterName,
	    EThespeonModuleType InModuleType,
	    EBackendType InBackendType,
	    UInferenceWorkloadManager* InWorkloadMgr,
	    UModuleManager* InModuleMgr,
	    ULookupTableManager* InLookupMgr,
	    UManifestHandler* InManifest,
	    FOnComplete InOnComplete
	);

	virtual bool Init() override
	{
		return true;
	}

	/**
	 * Main preload execution. Dispatches per-module loads as parallel TaskGraph tasks
	 * and waits for all to complete with periodic cancellation checks.
	 * @return 1 on success, 0 on failure or cancellation.
	 */
	virtual uint32 Run() override;

	/** Requests cooperative cancellation. Thread-safe — called from game thread during teardown. */
	virtual void Stop() override;

	virtual void Exit() override
	{
		bFinished.store(true);
	}

	bool ShouldStop() const
	{
		return bStopRequested.load();
	}
	bool WasSuccessful() const
	{
		return bSuccess.load();
	}

	bool IsFinished() const
	{
		return bFinished.load();
	}

  private:
	FString CharacterName;
	EThespeonModuleType ModuleType;
	EBackendType BackendType;

	// Subsystem pointers — captured on game thread at construction time.
	// Valid for the lifetime of the preload because component OnUnregister (where we join)
	// precedes GameInstance::Shutdown (where subsystems are deinitialized).
	UInferenceWorkloadManager* WorkloadMgr;
	UModuleManager* ModuleMgr;
	ULookupTableManager* LookupMgr;
	UManifestHandler* ManifestHandler;

	FOnComplete OnComplete;
	std::atomic<bool> bStopRequested{false};
	std::atomic<bool> bSuccess{false};
	std::atomic<bool> bFinished{false};

	/** Fires OnComplete on the game thread. Captures all arguments by value. */
	void FireOnComplete(bool bResult);
};

} // namespace Inference
} // namespace Thespeon
