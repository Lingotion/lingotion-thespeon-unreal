// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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

bool UInferenceWorkloadManager::RegisterModuleWorkload(Thespeon::Core::Module* Module, const FInferenceConfig& Config)
{
    if(IsRegistered(Module))
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("InferenceWorkloadManager::RegisterModuleWorkload: Module '%s' already registered."), *Module->ModuleID);
        return true;
    }
    TSet<FString> currentMD5s;
    Workloads.GetKeys(currentMD5s);

    TMap<FString, TStrongObjectPtr<UNNEModelData>> Models;
    if(!Module->LoadModels(currentMD5s, Models))
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("InferenceWorkloadManager::RegisterModuleWorkload: Failed to load models for module '%s'."), *Module->ModuleID);
        return false;
    }
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("InferenceWorkloadManager::RegisterModuleWorkload: Loaded %d models for module '%s'."), Models.Num(), *Module->ModuleID);
    for(const auto& [md5, modelData] : Models)
    {
        Thespeon::Inference::InferenceWorkload workload(modelData, Config.BackendType);
        Workloads.Add(md5, workload);
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("InferenceWorkloadManager::RegisterModuleWorkload: Registered workload '%s' for module '%s'."), *md5, *Module->ModuleID);
    }
    return true;
}

bool UInferenceWorkloadManager::TryDeregisterModuleWorkloads(Thespeon::Core::Module* Module)
{
    if(!IsRegistered(Module))
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("InferenceWorkloadManager::TryDeregisterModuleWorkloads: Module '%s' not registered."), *Module->ModuleID);
        return false;
    }
    TSet<FString> workloadsToClear = UModuleManager::Get()->GetNonOverlappingModelMD5s(Module);
    for (const FString& workloadMd5 : workloadsToClear)
    {
        Workloads.Remove(workloadMd5);
    }

    return true;
}

bool UInferenceWorkloadManager::IsRegistered(Thespeon::Core::Module* Module)
{
    TArray<FString> keys;
    Workloads.GetKeys(keys);
    return Module->IsIncludedIn(TSet<FString>(keys));
}

// It is unclear if we need to have a workersInUse since it seems unreal allows for models to used by multiple threads. 
// But we might want to limit the number of concurrent inferences in config for frame performance reasons.
Thespeon::Inference::InferenceWorkload* UInferenceWorkloadManager::GetWorkload(FString MD5)
{
    if(!Workloads.Contains(MD5))
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("InferenceWorkloadManager::GetWorkload: Workload '%s' not found."), *MD5);
        return nullptr;
    }
    if(WorkloadsInUse.Contains(&Workloads[MD5]))
    {
        LINGO_LOG(EVerbosityLevel::Warning, TEXT("InferenceWorkloadManager::GetWorkload: Workload '%s' is currently in use."), *MD5);
        return nullptr;
    }
    // For now dont use this. Just block concurrent synths from the same thespeon component.
    // Calling concurrently from a second component will probably break as it will also use the same instances.
    // If threads make their own instances then blocking is never needed but we dont know how heavy instance creation is.
    // WorkloadsInUse.Add(&Workloads[MD5]); 
    return &Workloads[MD5];
}
