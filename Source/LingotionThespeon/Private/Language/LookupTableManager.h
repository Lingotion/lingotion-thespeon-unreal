// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RuntimeLookupTable.h"
#include "HAL/CriticalSection.h"
#include "LookupTableManager.generated.h"

namespace Thespeon
{
namespace Language
{
class LanguageModule;
}
} // namespace Thespeon

/**
 * Game instance subsystem that manages runtime lookup tables for language modules.
 *
 * Each language module's word-to-phoneme lookup table is registered here during preloading
 * and retrieved during inference for fast common-word phonemization.
 */
UCLASS()
class ULookupTableManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
  public:
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
	static ULookupTableManager* Get();

	/**
	 * Registers a language module's lookup table if not already registered.
	 * @param Module The language module to register lookup table for.
	 */
	void RegisterLookupTable(Thespeon::Language::LanguageModule* Module);

	/**
	 * Deregisters a language module's lookup table by its MD5 identifier.
	 * @param Module The language module to deregister lookup table for.
	 */
	bool TryDeregisterTable(Thespeon::Language::LanguageModule*);

	/**
	 * Retrieves a registered lookup table by its MD5 identifier.
	 * @param MD5 The MD5 identifier of the lookup table.
	 * @return Shared pointer to the RuntimeLookupTable, or nullptr if not found.
	 */
	TSharedPtr<Thespeon::Language::RuntimeLookupTable, ESPMode::ThreadSafe> GetLookupTable(const FString& MD5);

	/** Clears all registered lookup tables for garbage collection. */
	void DisposeAndClear();

  private:
	/** Map of MD5 identifiers to runtime lookup tables. */
	TMap<FString, TSharedPtr<Thespeon::Language::RuntimeLookupTable, ESPMode::ThreadSafe>> AvailableLookupTables;

	/** Checks if a lookup table with the given MD5 is already registered. */
	bool IsRegistered(const FString& MD5) const;

	mutable FRWLock LookupTablesLock;
};
