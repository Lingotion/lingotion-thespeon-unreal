// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "NNE.h"
#include "NNERuntimeCPU.h"
#include "NNEModelData.h"
#include "NNETypes.h"
#include "Containers/Map.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/Module.h"
#include "../Inference/InferenceWorkload.h"
#include "Engine/InferenceConfig.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InferenceWorkloadManager.generated.h"

/// @brief Singleton which handles and creates all workloads. To be called by preload and unload. 

/// OBS! General notes on our Managers: They should probably all be UGameSubsystems or UGameInstanceSubsystems instead of these UObject with pointer singleton instance and lazy init.
UCLASS()
class UInferenceWorkloadManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public: 
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    bool RegisterModuleWorkload(Thespeon::Core::Module* Module, const FInferenceConfig& Config); // loads each onnx in module and from its ModelData creates an InferenceWorkload.
    bool TryDeregisterModuleWorkloads(Thespeon::Core::Module* Module); //deregisters all workloads in a module unless used by other module. Uses ModuleManager to figure out which are safe to remove. For Beta only do first bit.
    bool IsRegistered(Thespeon::Core::Module* Module); //checks if present in map.
    Thespeon::Inference::InferenceWorkload* GetWorkload(FString MD5); // gets workload by md5. Nullptr if not found or in use.
    private:
    TMap<FString, Thespeon::Inference::InferenceWorkload> Workloads; // map of name to workload.
    
    // set of workloads currently in use. 
    // This is a temporary construction. We do not need to block calls in unreal - just create new runtime model instances. 
    // But first we should investigate how performance heavy that operation is.
    TSet<Thespeon::Inference::InferenceWorkload*> WorkloadsInUse; 
};
