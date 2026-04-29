// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "InferenceWorkload.h"
#include "InferenceWorkloadManager.h"
#include "Core/BackendType.h"
#include "Core/Module.h"

namespace Thespeon
{
namespace Inference
{

/**
 * Per-session RAII cache that acquires exclusive InferenceWorkload instances from
 * the workload pool on first access and releases them all on destruction.
 *
 * Create at the top of ExecuteInference(). Pass to MetaGraphRunner and PhonemizeBatch
 * instead of UInferenceWorkloadManager*. On scope exit, all workloads return to the pool.
 *
 * Thread Safety:
 *   This object is owned by a single inference session and is NOT shared between threads.
 *   Internal TMap access is single-threaded (per-session). The AcquireWorkload/ReleaseWorkload
 *   calls on the manager are thread-safe (internally locked).
 *
 *   Uses TWeakObjectPtr to guard against subsystem teardown during PIE shutdown:
 *   if the manager is destroyed while a session is still running, the destructor
 *   safely skips release (workloads are dropped with the subsystem anyway).
 */
class FSessionWorkloadCache
{
  public:
	FSessionWorkloadCache(UInferenceWorkloadManager* InManager, EBackendType InBackend) : Manager(InManager), Backend(InBackend) {}

	~FSessionWorkloadCache()
	{
		UInferenceWorkloadManager* Mgr = Manager.Get();
		if (!Mgr)
		{
			return; // Subsystem already destroyed (e.g. PIE shutdown) — workloads drop naturally
		}
		for (auto& [MD5, Entry] : AcquiredWorkloads)
		{
			if (Entry.Workload.IsValid())
			{
				Mgr->ReleaseWorkload(Entry.WorkloadID, MoveTemp(Entry.Workload));
			}
		}
		AcquiredWorkloads.Empty();
	}

	// Non-copyable, non-movable (RAII semantics)
	FSessionWorkloadCache(const FSessionWorkloadCache&) = delete;
	FSessionWorkloadCache& operator=(const FSessionWorkloadCache&) = delete;
	FSessionWorkloadCache(FSessionWorkloadCache&&) = delete;
	FSessionWorkloadCache& operator=(FSessionWorkloadCache&&) = delete;

	/** Gets an exclusive workload for the given model MD5.
	 *  First call per MD5: acquires from pool. Subsequent calls: returns cached instance.
	 *  @param MD5 The model MD5 hash.
	 *  @return An exclusive workload instance, or nullptr on failure. */
	TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe> GetWorkload(const FString& MD5)
	{
		if (FAcquiredEntry* Found = AcquiredWorkloads.Find(MD5))
		{
			return Found->Workload;
		}

		UInferenceWorkloadManager* Mgr = Manager.Get();
		if (!Mgr)
		{
			return nullptr;
		}

		// Resolve MD5 → WorkloadID once; AcquireWorkload resolves again internally,
		// but we store the resolved ID here for ReleaseWorkload in the destructor.
		FString WorkloadID;
		if (!Thespeon::Core::TryGetRuntimeWorkloadID(MD5, Backend, WorkloadID))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("FSessionWorkloadCache: WorkloadID resolution failed for '%s'"), *MD5);
			return nullptr;
		}

		TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe> Workload = Mgr->AcquireWorkload(MD5, Backend);
		if (Workload.IsValid())
		{
			AcquiredWorkloads.Add(MD5, {WorkloadID, Workload});
		}
		return Workload;
	}

  private:
	/** Weak reference to the workload manager — guards against subsystem teardown during PIE shutdown. */
	TWeakObjectPtr<UInferenceWorkloadManager> Manager;
	EBackendType Backend = EBackendType::None;

	/** Entry in the per-session cache: stores both the resolved WorkloadID (for release) and the workload. */
	struct FAcquiredEntry
	{
		FString WorkloadID;
		TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe> Workload;
	};

	/** Workloads acquired during this session, keyed by MD5. */
	TMap<FString, FAcquiredEntry> AcquiredWorkloads;
};

} // namespace Inference
} // namespace Thespeon
