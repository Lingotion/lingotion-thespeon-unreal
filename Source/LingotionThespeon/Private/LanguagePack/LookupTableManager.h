// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RuntimeLookupTable.h"
#include "LookupTableManager.generated.h"

namespace Thespeon
{
    namespace LanguagePack
    {
        class LanguageModule;
    }
}

/// @brief Singleton that manages and registers runtime lookup tables for language modules.
/// Similar to ModuleManager but specifically for lookup tables.
UCLASS()
class ULookupTableManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    static ULookupTableManager* Get();
    
    /// @brief Registers a language module's lookup table if not already registered.
    /// @param Module The language module to register lookup table for
    void RegisterLookupTable(Thespeon::LanguagePack::LanguageModule* Module);
    
    /// @brief Deregisters a language module's lookup table by its MD5 identifier.
    /// @param Module The language module to deregister lookup table for
    bool TryDeregisterTable(Thespeon::LanguagePack::LanguageModule*);
    
    /// @brief Retrieves a registered lookup table by its MD5 identifier.
    /// @param MD5 The MD5 identifier of the lookup table
    /// @return The RuntimeLookupTable associated with the MD5 identifier, or nullptr if not found
    Thespeon::LanguagePack::RuntimeLookupTable* GetLookupTable(const FString& MD5);
    
    /// @brief Clears all registered lookup tables for garbage collection.
    void DisposeAndClear();
    
private:
    inline static TWeakObjectPtr<ULookupTableManager> CachedInstance = nullptr;
    
    /// @brief Map of MD5 identifiers to runtime lookup tables
    TMap<FString, TUniquePtr<Thespeon::LanguagePack::RuntimeLookupTable>> AvailableLookupTables;
    
    /// @brief Checks if a lookup table with the given MD5 is already registered
    bool IsRegistered(const FString& MD5) const;
};
