// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "LookupTableManager.h"
#include "LanguageModule.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Core/LingotionLogger.h"
#include "Engine/World.h"

void ULookupTableManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("LookupTableManager initialized."));
}

void ULookupTableManager::Deinitialize()
{
    DisposeAndClear();
    Super::Deinitialize();
}

ULookupTableManager* ULookupTableManager::Get()
{
    // Only ever do this on game thread
    check(IsInGameThread());

    // If we have a cached instance, make sure its GI still matches a live context
    if (ULookupTableManager* S = CachedInstance.Get())
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
        // GI changed or went away -> drop cache
        CachedInstance = nullptr;
    }

    // Find a suitable GI and cache a fresh instance
    if (GEngine)
    {
        for (const FWorldContext& C : GEngine->GetWorldContexts())
        {
            if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.OwningGameInstance)
            {
                CachedInstance = C.OwningGameInstance->GetSubsystem<ULookupTableManager>();
                if (!CachedInstance.IsValid())
                {
                    LINGO_LOG(EVerbosityLevel::Warning, TEXT("LookupTableManager subsystem not found in GameInstance."));
                    return nullptr;
                }
                return CachedInstance.Get();
            }
        }
    }

    return nullptr;
}

void ULookupTableManager::RegisterLookupTable(Thespeon::LanguagePack::LanguageModule* Module)
{
    if (!Module)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("LookupTableManager::RegisterLookupTable - Module is null"));
        return;
    }

    FString MD5 = Module->GetLookupTableID();

    if (IsRegistered(MD5))
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("LookupTableManager::RegisterLookupTable - Lookup table with MD5 '%s' already registered."), *MD5);
        return;
    }

    // Load the lookup table from the module
    TMap<FString, FString> LookupTableData = Module->GetLookupTable();
    
    // Create a new RuntimeLookupTable with the loaded data
    TUniquePtr<Thespeon::LanguagePack::RuntimeLookupTable> LookupTable = 
        MakeUnique<Thespeon::LanguagePack::RuntimeLookupTable>(LookupTableData);
    
    AvailableLookupTables.Add(MD5, MoveTemp(LookupTable));
    
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("LookupTableManager::RegisterLookupTable - Successfully registered lookup table with MD5 '%s' containing %d entries."), 
        *MD5, LookupTableData.Num());
}

bool ULookupTableManager::TryDeregisterTable(Thespeon::LanguagePack::LanguageModule* Module)
{

    FString MD5 = Module->GetLookupTableID();

    if (!IsRegistered(MD5))
    {
        LINGO_LOG(EVerbosityLevel::Warning, TEXT("LookupTableManager::DeregisterTable - Lookup table with MD5 '%s' not registered."), *MD5);
        return false;
    }

    AvailableLookupTables.Remove(MD5);
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("LookupTableManager::DeregisterTable - Successfully deregistered lookup table with MD5 '%s'."), *MD5);
    return true;
}

Thespeon::LanguagePack::RuntimeLookupTable* ULookupTableManager::GetLookupTable(const FString& MD5)
{
    TUniquePtr<Thespeon::LanguagePack::RuntimeLookupTable>* LookupTablePtr = AvailableLookupTables.Find(MD5);
    
    if (LookupTablePtr && LookupTablePtr->IsValid())
    {
        return LookupTablePtr->Get();
    }
    else
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("LookupTableManager::GetLookupTable - Lookup table with MD5: %s not found."), *MD5);
        return nullptr;
    }
}

void ULookupTableManager::DisposeAndClear()
{
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("LookupTableManager::DisposeAndClear - Clearing %d lookup tables."), AvailableLookupTables.Num());
    AvailableLookupTables.Empty();
}

bool ULookupTableManager::IsRegistered(const FString& MD5) const
{
    return AvailableLookupTables.Contains(MD5);
}
