// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "NNE.h"
#include "NNERuntimeCPU.h"
#include "NNEModelData.h"
#include "NNETypes.h"
#include "Containers/Map.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/Module.h"
#include "Inference/InferenceWorkload.h"
#include "Engine/InferenceConfig.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Core/BackendType.h"
#include "HAL/CriticalSection.h"
class UModuleManager;
#include "InferenceWorkloadManager.generated.h"

/**
 * Game instance subsystem that owns and manages all inference workloads (loaded NNE model instances).
 *
 * Workloads are registered during character preloading and deregistered during unloading.
 * Each workload wraps a single ONNX model on a specific backend (CPU/GPU).
 *
 * Supports concurrent inference via workload pooling: each session acquires an exclusive
 * InferenceWorkload instance via AcquireWorkload() and returns it via ReleaseWorkload().
 * When the pool is empty, a new model instance is created from the stored IModel.
 */
UCLASS()
class UInferenceWorkloadManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
  public:
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
	/**
	 * @brief Loads models and creates InferenceWorkload instances for each on the given backend for the given module.
	 * @param Module - The module indicating models to load
	 * @param BackendType - The backend type to load workload on
	 */
	bool RegisterModuleWorkload(
	    Thespeon::Core::Module* Module,
	    EBackendType BackendType
	); // loads each onnx in module and from its ModelData creates an InferenceWorkload.
	bool TryDeregisterModuleWorkloads(
	    Thespeon::Core::Module* Module,
	    EBackendType BackendType,
	    UModuleManager* InModuleManager
	); // deregisters all workloads in a module unless used by other module. Uses
	   // ModuleManager to figure out which are safe to remove. For Beta only do first bit.
	bool IsRegistered(Thespeon::Core::Module* Module, EBackendType BackendType); // checks if present in map.

	/** @brief Acquires an exclusive InferenceWorkload for the given model.
	 *  Returns a pooled instance if available, otherwise creates a new one from the stored IModel.
	 *  The caller MUST call ReleaseWorkload() when done (or use FSessionWorkloadCache for RAII).
	 *  @param MD5 The model MD5 hash.
	 *  @param BackendType The backend type.
	 *  @return An exclusive workload instance, or nullptr on failure. */
	TSharedPtr<Thespeon::Inference::InferenceWorkload, ESPMode::ThreadSafe> AcquireWorkload(const FString& MD5, EBackendType BackendType);

	/** @brief Returns a previously acquired workload to the pool for reuse by future sessions.
	 *  If the pool was removed (deregistered), the workload is simply dropped.
	 *  If the pool is at capacity (MaxPooledInstancesPerModel), the workload is dropped to bound memory.
	 *  @param WorkloadID The workload ID (as resolved by TryGetRuntimeWorkloadID).
	 *  @param Workload The workload to return (moved in). */
	void ReleaseWorkload(const FString& WorkloadID, TSharedPtr<Thespeon::Inference::InferenceWorkload, ESPMode::ThreadSafe>&& Workload);

  private:
	/** Per-model workload pool. */
	struct FWorkloadPool
	{
		/** The original workload from registration. Serves as an immortal template:
		 *  its stored IModel (CPU/GPU) is used to create new instances when the pool is empty.
		 *  The Prototype is also placed in Available during registration as the first lendable instance.
		 *  After that first acquisition, Prototype stays alive solely as the source model reference —
		 *  it is never re-lent, even if Available becomes empty. */
		TSharedPtr<Thespeon::Inference::InferenceWorkload, ESPMode::ThreadSafe> Prototype;

		/** Stack of available (idle) workload instances ready for acquisition.
		 *  Capped at MaxPooledInstancesPerModel to bound memory usage. */
		TArray<TSharedPtr<Thespeon::Inference::InferenceWorkload, ESPMode::ThreadSafe>> Available;

		/** Maximum number of idle instances to retain per model. Beyond this, released instances are dropped. */
		static constexpr int32 MaxPooledInstancesPerModel = 4;
	};

	TMap<FString, FWorkloadPool> WorkloadPools;
	mutable FRWLock WorkloadsLock;
};
