// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"

namespace Thespeon
{
namespace Language
{
/**
 * Runtime lookup table that manages both static (pre-loaded) and dynamic (runtime-added)
 * word-to-phoneme mappings. Checked before invoking the G2P neural model for faster resolution.
 *
 * Thread-safe: reads and writes to the dynamic table are protected by an internal FRWLock.
 */
class RuntimeLookupTable
{
  public:
	/**
	 * Initializes a new instance of the RuntimeLookupTable with a static lookup table.
	 * @param InStaticLookupTable A map containing static key-value pairs loaded from disk.
	 */
	RuntimeLookupTable(const TMap<FString, FString>& InStaticLookupTable);

	/**
	 * Tries to get the value associated with the specified key from the lookup tables.
	 * @param Key The key to look up.
	 * @param OutValue The value associated with the key if found.
	 * @return True if the key exists in either table, otherwise false.
	 */
	bool TryGetValue(const FString& Key, FString& OutValue) const;

	/**
	 * Checks if the specified key exists in either the static or dynamic lookup tables.
	 * @param Key The key to check for existence.
	 * @return True if the key exists in either table, otherwise false.
	 */
	bool ContainsKey(const FString& Key) const;

	/**
	 * Adds or updates an entry in the dynamic lookup table.
	 * If the key already exists, its value is updated; otherwise, a new entry is added.
	 * @param Key The key to add or update.
	 * @param Value The value to associate with the key.
	 */
	void AddOrUpdateDynamicEntry(const FString& Key, const FString& Value);

  private:
	TMap<FString, FString> StaticLookupTable;
	TMap<FString, FString> DynamicLookupTable;
	mutable FRWLock TableLock;
};
} // namespace Language
} // namespace Thespeon
