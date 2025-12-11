// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/Module.h"
#include "../ActorPack/ActorModule.h"
#include "Core/LingotionLogger.h"
#include "ModuleManager.generated.h"



/// @brief Singleton which handles and creates all Modules. To be called by preload and unload. Used by WorkloadManager to figure out which workloads to deregister during unload.
UCLASS()
class UModuleManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    static UModuleManager* Get();
    
    template<typename T>
    void RegisterModule(Thespeon::Core::FModuleEntry ModuleEntry)
    {
        static_assert(std::is_base_of_v<Thespeon::Core::Module, T>, "T must derive from Module");
        if (!Modules.Contains(ModuleEntry.ModuleID))
        {
            LINGO_LOG(EVerbosityLevel::Debug, TEXT("ModuleManager::RegisterModule: Registering module '%s'."), *ModuleEntry.ModuleID);
            Modules.Add(ModuleEntry.ModuleID, MakeUnique<T>(ModuleEntry)); // assumes T has ctor(TModuleEntry)
        }
    }
    bool TryDeregisterModule(FString ModuleID); //deregisters all workloads in a module unless used by other module.
    bool IsRegistered(FString ModuleID); //checks if present in map.
    template<typename T>
    T* GetModule(Thespeon::Core::FModuleEntry ModuleEntry, bool ShouldCreate = true)
    {
        static_assert(std::is_base_of_v<Thespeon::Core::Module, T>, "T must derive from Module");
        
        if(!Modules.Contains(ModuleEntry.ModuleID))
        {
            if(!ShouldCreate)
                return nullptr;
            LINGO_LOG(EVerbosityLevel::Debug, TEXT("ModuleManager::GetModule: Module '%s' not found. Creating "), *ModuleEntry.ModuleID);
            RegisterModule<T>(ModuleEntry);
        }
        return static_cast<T*>(Modules[ModuleEntry.ModuleID].Get());
    }
    // Resource overlap detection - now uses safe dynamic_cast instead of enum + static_cast
    TSet<FString> GetNonOverlappingModelMD5s(Thespeon::Core::Module* Module); // returns file md5s that are not in any other module (safe to remove)
    TSet<FString> GetNonOverlappingModelLangModules(Thespeon::ActorPack::ActorModule* Module); // finds language modules used by argument module that are not used by any other module. (safe to remove)
private:
    inline static TWeakObjectPtr<UModuleManager> CachedInstance = nullptr;
    TMap<FString, TUniquePtr<Thespeon::Core::Module>> Modules; // map of ModuleID to Module.

};
