// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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
#include "InferenceWorkloadManager.generated.h"

/**
 * Game instance subsystem that owns and manages all inference workloads (loaded NNE model instances).
 *
 * Workloads are registered during character preloading and deregistered during unloading.
 * Each workload wraps a single ONNX model on a specific backend (CPU/GPU).
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
	    EBackendType BackendType
	); // deregisters all workloads in a module unless used by other module. Uses
	   // ModuleManager to figure out which are safe to remove. For Beta only do first bit.
	bool IsRegistered(Thespeon::Core::Module* Module, EBackendType BackendType); // checks if present in map.
	Thespeon::Inference::InferenceWorkload* GetWorkload(
	    FString MD5,
	    EBackendType BackendType
	); // gets workload by md5. Nullptr if not found or in use.
  private:
	TMap<FString, Thespeon::Inference::InferenceWorkload> Workloads; // map of name to workload.

	// set of workloads currently in use.
	// This is a temporary construction. We do not need to block calls in unreal - just create new runtime model instances.
	// But first we should investigate how performance heavy that operation is.
	TSet<Thespeon::Inference::InferenceWorkload*> WorkloadsInUse;
};
