// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "ModuleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UModuleManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // your init...
}

void UModuleManager::Deinitialize()
{
    // your shutdown...
    Super::Deinitialize();
}

// We need to discuss how we want to do with these singletons. UGameInstanceSubsystem requires a context to be passed all the way down here.
// This Getter is not safe as it just returns the first it finds. Having a cached instance is unncessary since getting it from GI is not expensive given a context.
UModuleManager* UModuleManager::Get()
{

    // If we have a cached instance, make sure its GI still matches a live context
    if (UModuleManager* S = CachedInstance.Get())
    {
        if (GEngine)
        {
            for (const FWorldContext& C : GEngine->GetWorldContexts())
            {
                if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.OwningGameInstance)
                {
                    if (S->GetGameInstance() == C.OwningGameInstance)
                    {
                        return S; // still valid for current GI
                    }
                }
            }
        }
        // GI changed or went away → drop cache
        CachedInstance = nullptr;
    }

    // Find a suitable GI and cache fresh
    if (GEngine)
    {
        for (const FWorldContext& C : GEngine->GetWorldContexts())
        {
            if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.OwningGameInstance)
            {
                CachedInstance = C.OwningGameInstance->GetSubsystem<UModuleManager>();
                if(!CachedInstance.IsValid())
                {
                    LINGO_LOG(EVerbosityLevel::Error, TEXT("ModuleManager subsystem not found in GameInstance."));
                    return nullptr;
                }
                return CachedInstance.Get();
            }
        }
    }

    return nullptr;
}

bool UModuleManager::TryDeregisterModule(FString ModuleID)
{
    if(!IsRegistered(ModuleID))
        return false;
    Modules.Remove(ModuleID);
    return true;
}

bool UModuleManager::IsRegistered(FString ModuleID)
{
    return Modules.Contains(ModuleID);
}

TSet<FString> UModuleManager::GetNonOverlappingModelMD5s(Thespeon::Core::Module* Module)
{
    TSet<FString> moduleMd5s = Module->GetAllFileMD5s();
    TSet<FString> otherMd5s;

    for (const auto& loadedModule : Modules)
    {
        if(loadedModule.Value->ModuleID == Module->ModuleID || loadedModule.Value->GetModuleType() != Module->GetModuleType())
            continue;
     
        otherMd5s.Append(loadedModule.Value->GetAllFileMD5s());
        
    }
    
    return moduleMd5s.Difference(otherMd5s);
}

TSet<FString> UModuleManager::GetNonOverlappingModelLangModules(Thespeon::ActorPack::ActorModule* Module)
{
    TArray<FString> langModuleIDs;
    Module->LanguageModuleIDs.GenerateValueArray(langModuleIDs);

    TSet<FString> moduleMd5s(langModuleIDs);
    TSet<FString> otherMd5s;

    for (const auto& loadedModule : Modules)
    {
        if(loadedModule.Value->ModuleID == Module->ModuleID || loadedModule.Value->GetModuleType() != Module->GetModuleType())
            continue;
     
        TArray<FString> currentLangModuleIDs;
        static_cast<Thespeon::ActorPack::ActorModule*>(loadedModule.Value.Get())->LanguageModuleIDs.GenerateValueArray(currentLangModuleIDs);
        otherMd5s.Append(currentLangModuleIDs);
        
    }
    
    return moduleMd5s.Difference(otherMd5s);
}
