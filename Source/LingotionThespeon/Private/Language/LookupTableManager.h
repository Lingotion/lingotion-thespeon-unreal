// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RuntimeLookupTable.h"
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
	 * @return The RuntimeLookupTable associated with the MD5 identifier, or nullptr if not found.
	 */
	Thespeon::Language::RuntimeLookupTable* GetLookupTable(const FString& MD5);

	/** Clears all registered lookup tables for garbage collection. */
	void DisposeAndClear();

  private:
	inline static TWeakObjectPtr<ULookupTableManager> CachedInstance = nullptr;

	/** Map of MD5 identifiers to runtime lookup tables. */
	TMap<FString, TUniquePtr<Thespeon::Language::RuntimeLookupTable>> AvailableLookupTables;

	/** Checks if a lookup table with the given MD5 is already registered. */
	bool IsRegistered(const FString& MD5) const;
};
