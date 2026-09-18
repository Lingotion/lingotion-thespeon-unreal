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
		for (auto& [WorkloadID, Workload] : AcquiredWorkloads)
		{
			if (Workload.IsValid())
			{
				Mgr->ReleaseWorkload(WorkloadID, MoveTemp(Workload));
			}
		}
		AcquiredWorkloads.Empty();
	}

	// Non-copyable, non-movable (RAII semantics)
	FSessionWorkloadCache(const FSessionWorkloadCache&) = delete;
	FSessionWorkloadCache& operator=(const FSessionWorkloadCache&) = delete;
	FSessionWorkloadCache(FSessionWorkloadCache&&) = delete;
	FSessionWorkloadCache& operator=(FSessionWorkloadCache&&) = delete;

	/** Gets an exclusive workload for the given model MD5 on the given backend.
	 *  First call per workload: acquires from pool. Subsequent calls: returns the cached instance.
	 *  @param MD5 The model MD5 hash.
	 *  @param EffectiveBackend The backend this model must run on, as resolved by
	 *         Module::ResolveNodeBackend — a metagraph device pin may make it differ from the
	 *         session's requested backend. Pass EBackendType::None to use the session's backend.
	 *  @return An exclusive workload instance, or nullptr on failure. */
	TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe> GetWorkload(const FString& MD5, EBackendType EffectiveBackend = EBackendType::None)
	{
		const EBackendType ResolvedBackend = EffectiveBackend != EBackendType::None ? EffectiveBackend : Backend;

		// Keyed by WorkloadID rather than MD5: two nodes can share one MD5 while declaring different
		// devices, and those are separate pools holding separate instances.
		FString WorkloadID;
		if (!Thespeon::Core::TryGetRuntimeWorkloadID(MD5, ResolvedBackend, WorkloadID))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("FSessionWorkloadCache: WorkloadID resolution failed for '%s'"), *MD5);
			return nullptr;
		}

		if (TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe>* Found = AcquiredWorkloads.Find(WorkloadID))
		{
			return *Found;
		}

		UInferenceWorkloadManager* Mgr = Manager.Get();
		if (!Mgr)
		{
			return nullptr;
		}

		TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe> Workload = Mgr->AcquireWorkload(MD5, ResolvedBackend);
		if (Workload.IsValid())
		{
			AcquiredWorkloads.Add(MoveTemp(WorkloadID), Workload);
		}
		return Workload;
	}

  private:
	/** Weak reference to the workload manager — guards against subsystem teardown during PIE shutdown. */
	TWeakObjectPtr<UInferenceWorkloadManager> Manager;
	EBackendType Backend = EBackendType::None;

	/** Workloads acquired during this session, keyed by workload ID — which is also the key
	 *  ReleaseWorkload needs, and which distinguishes one MD5 pooled on two different backends. */
	TMap<FString, TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe>> AcquiredWorkloads;
};

} // namespace Inference
} // namespace Thespeon
