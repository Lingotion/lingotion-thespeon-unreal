// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Inference/ThespeonEditorSignals.h"

class FJsonObject;

/**
 * Editor-owned persistent store for data cache.
 *
 * JSON file cache under the project's Saved/ directory, and hands a rounded snapshot to the
 * license verifier. On a successful verification the sent snapshot is subtracted back out, so
 * synths that land during the network round-trip are preserved.
 *
 * All access is expected on the game thread; a static critical section still guards
 * file access as insurance against any future off-thread caller.
 */
class LINGOTIONTHESPEONEDITOR_API FEditorDataCache
{
  public:
	using FSynthData = Thespeon::Inference::FThespeonDataCache;

	/** Absolute path to the cache file: <ProjectSaved>/Lingotion/Thespeon.datacache. */
	static FString GetCacheFilePath();

	/** Additively merges a synthesis's data into the persisted cache. */
	static void AddToCache(const FSynthData& Data);

	/** Returns the current cache contents (used to build and later subtract the payload). */
	static FSynthData Snapshot();

	/** Builds the JSON object embedded in the verify payload (raw counts, no rounding). */
	static TSharedPtr<FJsonObject> ToJsonObject(const FSynthData& Data);

	/**
	 * Subtracts an already-sent snapshot from the persisted cache and writes the remainder back
	 * (deleting the file when nothing remains). Preserves counts accumulated after the snapshot.
	 */
	static void SubtractAndPersist(const FSynthData& Sent);

  private:
	static FSynthData LoadFromFile();
	static void SaveToFile(const FSynthData& Data);
	static FCriticalSection& GetLock();
};
