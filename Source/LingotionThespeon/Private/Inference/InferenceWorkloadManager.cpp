// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "InferenceWorkloadManager.h"
#include "ModuleManager.h"

void UInferenceWorkloadManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UInferenceWorkloadManager::Deinitialize()
{
	// Cleanup of all workloads... Do Later.
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
	Workloads.GetKeys(CurrentWorkloadIDs);

	TMap<FString, TStrongObjectPtr<UNNEModelData>> Models;
	if (!Module->LoadModels(CurrentWorkloadIDs, BackendType, Models))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to load models for module '%s'."), *Module->ModuleID);
		return false;
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Loaded %d models for module '%s'."), Models.Num(), *Module->ModuleID);
	for (const auto& [WorkloadID, ModelData] : Models)
	{
		Thespeon::Inference::InferenceWorkload Workload(ModelData, BackendType);
		Workloads.Add(WorkloadID, Workload);
		LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Registered workload '%s' for module '%s'."), *WorkloadID, *Module->ModuleID);
	}
	Module->LoadedBackends.Add(BackendType);
	return true;
}

// Removes all workloads for a module on the specified backend (or all backends if None).
// Consults ModuleManager to determine which workload IDs are safe to remove (accounting for
// shared models between modules), then clears the backend from the module's LoadedBackends set.
bool UInferenceWorkloadManager::TryDeregisterModuleWorkloads(Thespeon::Core::Module* Module, EBackendType BackendType)
{
	if (!IsRegistered(Module, BackendType))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Module '%s' not registered."), *Module->ModuleID);
		return true;
	}
	TSet<FString> workloadsToClear = UModuleManager::Get()->GetWorkloadIDsToRemove(Module, BackendType);
	for (const FString& workloadMd5 : workloadsToClear)
	{
		Workloads.Remove(workloadMd5);
	}
	if (BackendType == EBackendType::None)
	{
		Module->LoadedBackends.Empty();
	}
	else
	{
		Module->LoadedBackends.Remove(BackendType);
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
	Workloads.GetKeys(keys);
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

// Looks up and returns a pointer to the InferenceWorkload for the given model MD5 and backend.
// Returns nullptr if not found or if the workload is currently in use (concurrency guard).
// Note: The WorkloadsInUse tracking is currently disabled — concurrent access is instead
// blocked at the ThespeonComponent level (one synthesis at a time per component).
Thespeon::Inference::InferenceWorkload* UInferenceWorkloadManager::GetWorkload(FString MD5, EBackendType BackendType)
{
	FString WorkloadID;
	if (!Thespeon::Core::TryGetRuntimeWorkloadID(MD5, BackendType, WorkloadID) || !Workloads.Contains(WorkloadID))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Workload '%s' not found. Character has not been properly loaded"), *WorkloadID);
		return nullptr;
	}
	if (WorkloadsInUse.Contains(&Workloads[WorkloadID]))
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Workload '%s' is currently in use."), *WorkloadID);
		return nullptr;
	}
	// For now dont use this. Just block concurrent synths from the same thespeon component.
	// Calling concurrently from a second component will probably break as it will also use the same instances.
	// If threads make their own instances then blocking is never needed but we dont know how heavy instance creation is.
	// WorkloadsInUse.Add(&Workloads[WorkloadID]);
	return &Workloads[WorkloadID];
}
