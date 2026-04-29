// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "InferenceWorkloadManager.h"
#include "ModuleManager.h"

void UInferenceWorkloadManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UInferenceWorkloadManager::Deinitialize()
{
	FWriteScopeLock WriteLock(WorkloadsLock);
	WorkloadPools.Empty();
	Super::Deinitialize();
}

// Registers all ONNX models from a module as inference workloads on the specified backend.
// Skips if the module is already registered. Loads models via Module::LoadModels (which
// dispatches to the game thread), then wraps each in an InferenceWorkload and stores it.
bool UInferenceWorkloadManager::RegisterModuleWorkload(Thespeon::Core::Module* Module, EBackendType BackendType)
{
	if (IsRegistered(Module, BackendType))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Module '%s' already registered."), *Module->ModuleID);
		return true;
	}
	TSet<FString> CurrentWorkloadIDs;
	{
		FReadScopeLock ReadLock(WorkloadsLock);
		WorkloadPools.GetKeys(CurrentWorkloadIDs);
	}

	TMap<FString, TStrongObjectPtr<UNNEModelData>> Models;
	if (!Module->LoadModels(CurrentWorkloadIDs, BackendType, Models))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to load models for module '%s'."), *Module->ModuleID);
		return false;
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Loaded %d models for module '%s'."), Models.Num(), *Module->ModuleID);
	{
		FWriteScopeLock WriteLock(WorkloadsLock);
		for (const auto& [WorkloadID, ModelData] : Models)
		{
			auto NewWorkload = MakeShared<Thespeon::Inference::InferenceWorkload, ESPMode::ThreadSafe>(ModelData, BackendType);
			FWorkloadPool& Pool = WorkloadPools.FindOrAdd(WorkloadID);
			Pool.Prototype = NewWorkload;
			Pool.Available.Add(NewWorkload);
			LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Registered workload '%s' for module '%s'."), *WorkloadID, *Module->ModuleID);
		}
		// LoadedBackends must be updated under the same write lock as the WorkloadPools map.
		// With parallel per-module preload (TUNR-134: within a single PreloadCharacter call,
		// language modules load in parallel via TaskGraph), multiple threads may call
		// RegisterModuleWorkload concurrently — this prevents a data race on TSet::Add.
		// Note: IsRegistered() with BackendType::None reads LoadedBackends WITHOUT lock
		// protection; this is safe because that path is only called from the game thread
		// (deregistration via TryUnloadCharacter).
		Module->LoadedBackends.Add(BackendType);
	}
	return true;
}

// Removes all workloads for a module on the specified backend (or all backends if None).
// Consults ModuleManager to determine which workload IDs are safe to remove (accounting for
// shared models between modules), then clears the backend from the module's LoadedBackends set.
bool UInferenceWorkloadManager::TryDeregisterModuleWorkloads(
    Thespeon::Core::Module* Module, EBackendType BackendType, UModuleManager* InModuleManager
)
{
	if (!IsRegistered(Module, BackendType))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Module '%s' not registered."), *Module->ModuleID);
		return true;
	}
	TSet<FString> workloadsToClear = InModuleManager->GetWorkloadIDsToRemove(Module, BackendType);
	{
		FWriteScopeLock WriteLock(WorkloadsLock);
		for (const FString& workloadMd5 : workloadsToClear)
		{
			// Removing the pool entry clears the Prototype and Available array.
			// Any in-flight sessions that already acquired workloads still hold valid
			// TSharedPtr references — those instances survive until the session ends.
			WorkloadPools.Remove(workloadMd5);
		}
		// LoadedBackends modification under write lock for consistency with
		// RegisterModuleWorkload (which adds under the same lock). Currently
		// deregistration is game-thread-only, but this prevents future races
		// if async deregistration is added.
		if (BackendType == EBackendType::None)
		{
			Module->LoadedBackends.Empty();
		}
		else
		{
			Module->LoadedBackends.Remove(BackendType);
		}
	}
	FString BackendStr = BackendType == EBackendType::None ? TEXT("all backends") : UEnum::GetValueAsString(BackendType);
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Info,
	    TEXT("Deregistered %d workloads for module '%s' on backend '%s'."),
	    workloadsToClear.Num(),
	    *Module->ModuleID,
	    *BackendStr
	);
	return true;
}

/**
 * @brief Checks if a module's workloads are registered with the given backend. If BackendType is None, checks if registered for any backend.
 * @param Module - The module to check
 * @param BackendType - The backend type to check
 */
bool UInferenceWorkloadManager::IsRegistered(Thespeon::Core::Module* Module, EBackendType BackendType)
{
	TArray<FString> keys;
	{
		FReadScopeLock ReadLock(WorkloadsLock);
		WorkloadPools.GetKeys(keys);
	}
	if (BackendType == EBackendType::None)
	{
		// Check if all workloads for any backend is registered
		for (EBackendType backend : Module->LoadedBackends)
		{
			if (Module->IsIncludedIn(TSet<FString>(keys), backend))
			{
				return true;
			}
		}
		return false;
	}
	return Module->IsIncludedIn(TSet<FString>(keys), BackendType);
}

// Acquires an exclusive workload instance from the pool.
// If the pool has an available instance, it is popped and returned.
// If empty, a new instance is created from the prototype's stored IModel (no game thread needed).
// Note: CreateFromModel() is called OUTSIDE the lock to avoid blocking other sessions during
// expensive ONNX instance creation.
TSharedPtr<Thespeon::Inference::InferenceWorkload, ESPMode::ThreadSafe>
UInferenceWorkloadManager::AcquireWorkload(const FString& MD5, EBackendType BackendType)
{
	FString WorkloadID;
	if (!Thespeon::Core::TryGetRuntimeWorkloadID(MD5, BackendType, WorkloadID))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("AcquireWorkload: WorkloadID resolution failed for '%s'"), *MD5);
		return nullptr;
	}

	// Phase 1: Try to pop from pool under lock, or capture source model data for creation outside lock
	TSharedPtr<UE::NNE::IModelCPU> SourceCPU;
	TSharedPtr<UE::NNE::IModelGPU> SourceGPU;
	EBackendType SourceBackend = EBackendType::None;
	{
		FWriteScopeLock WriteLock(WorkloadsLock);
		FWorkloadPool* Pool = WorkloadPools.Find(WorkloadID);
		if (!Pool || !Pool->Prototype.IsValid())
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("AcquireWorkload: No pool for '%s'. Character has not been properly loaded"), *WorkloadID);
			return nullptr;
		}

		if (Pool->Available.Num() > 0)
		{
			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug,
			    TEXT("AcquireWorkload: Returning pooled instance for '%s' (%d remaining)"),
			    *WorkloadID,
			    Pool->Available.Num() - 1
			);
			return Pool->Available.Pop();
		}

		// Capture source model refs for creation outside lock
		SourceCPU = Pool->Prototype->GetSourceModelCPU();
		SourceGPU = Pool->Prototype->GetSourceModelGPU();
		SourceBackend = Pool->Prototype->GetBackendType();
	}

	// Phase 2: Lock released — create instance without blocking other sessions
	LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("AcquireWorkload: Pool empty for '%s', creating new instance from source model"), *WorkloadID);
	return Thespeon::Inference::InferenceWorkload::CreateFromModel(SourceCPU, SourceGPU, SourceBackend);
}

// Returns a previously acquired workload to the pool for reuse.
// If the pool was removed (e.g. character unloaded), the workload TSharedPtr simply goes out of scope.
// If the pool is at capacity (MaxPooledInstancesPerModel), the instance is dropped to bound memory.
void UInferenceWorkloadManager::ReleaseWorkload(
    const FString& WorkloadID, TSharedPtr<Thespeon::Inference::InferenceWorkload, ESPMode::ThreadSafe>&& Workload
)
{
	if (!Workload.IsValid())
	{
		return;
	}

	FWriteScopeLock WriteLock(WorkloadsLock);
	FWorkloadPool* Pool = WorkloadPools.Find(WorkloadID);
	if (Pool)
	{
		if (Pool->Available.Num() < FWorkloadPool::MaxPooledInstancesPerModel)
		{
			Pool->Available.Add(MoveTemp(Workload));
			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug, TEXT("ReleaseWorkload: Returned instance to pool '%s' (%d available)"), *WorkloadID, Pool->Available.Num()
			);
		}
		else
		{
			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug, TEXT("ReleaseWorkload: Pool '%s' at capacity (%d), dropping instance"), *WorkloadID, Pool->Available.Num()
			);
			// Workload TSharedPtr goes out of scope — instance destroyed
		}
	}
	else
	{
		// Pool was removed (character unloaded) — workload will be destroyed when TSharedPtr drops
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("ReleaseWorkload: Pool '%s' no longer exists, workload will be destroyed"), *WorkloadID);
	}
}
