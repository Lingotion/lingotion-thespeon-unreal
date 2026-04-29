// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "SessionTensorPool.h"
#include "InferenceSession.h"
#include "Engine/InferenceConfig.h"
#include "Core/ThespeonDataPacket.h"
#include "Core/BackendType.h"

// Forward declarations
namespace Thespeon
{
namespace Language
{
class LanguageModule;
class RuntimeLookupTable;
} // namespace Language
namespace Character
{
class CharacterModule;
}
} // namespace Thespeon

class UInferenceWorkloadManager;
class ULookupTableManager;
class UModuleManager;
class UManifestHandler;

namespace Thespeon
{
namespace Inference
{
class FSessionWorkloadCache;
/**
 * Concrete inference session that orchestrates the full TTS pipeline.
 *
 * Handles phonemization, encoder inference, and vocoder inference using
 * a MetaGraphRunner. Runs on a background thread via FRunnable.
 *
 * Subsystem pointers must be captured on the game thread and passed to
 * the constructor before the session is started on a background thread.
 */
class ThespeonInference : public InferenceSession
{
  public:
	ThespeonInference(
	    const FLingotionModelInput& InInput,
	    const FInferenceConfig InConfig,
	    const FString& InSessionID,
	    UInferenceWorkloadManager* InWorkloadManager,
	    UModuleManager* InModuleManager,
	    ULookupTableManager* InLookupTableManager,
	    UManifestHandler* InManifestHandler
	)
	    : InferenceSession(InInput, InConfig, InSessionID)
	    , InferenceWorkloadManager(InWorkloadManager)
	    , ModuleManager(InModuleManager)
	    , LookupTableManager(InLookupTableManager)
	    , ManifestHandler(InManifestHandler)
	{
	}

	/** @brief Executes the full inference pipeline on the background thread.
	 *  @return Exit code (0 on success). */
	uint32 Run() override;

	/** @brief Requests the session to stop execution. */
	void Stop() override;

	/** @brief Called after Run() completes for cleanup. */
	void Exit() override;

	/** @brief Attempts to unload the models for a character and module type from memory.
	 *  Subsystem pointers must be captured on the game thread by the caller.
	 *  @return True if unloading succeeded. */
	static bool TryUnloadCharacter(
	    const FString& CharacterName,
	    const EThespeonModuleType& ModuleType,
	    EBackendType BackendType,
	    UInferenceWorkloadManager* InferenceWorkloadManager,
	    UModuleManager* ModuleManager,
	    ULookupTableManager* LookupTableManager,
	    UManifestHandler* ManifestHandler
	);

  private:
	/**
	 * Posts an error packet to the packet callback.
	 */
	void PostErrorPacket();
	/**
	 * Posts a cancellation packet to the packet callback.
	 */
	void PostCancelledPacket();
	/**
	 * Executes the inference steps.
	 * @return true if inference suceeded, otherwise false
	 */
	bool ExecuteInference();

	/**
	 * Phonemizes a batch of words using the language module's phonemizer model.
	 *
	 * @param Words - Array of words to phonemize
	 * @param LangModule - Language module containing phonemizer model and vocab
	 * @param LookupTable - Runtime lookup table for caching phoneme results
	 * @param WorkloadCache - Session workload cache for acquiring exclusive phonemizer workload
	 * @param Config - Inference configuration
	 * @return true if phonemization succeeded, false otherwise
	 */
	bool PhonemizeBatch(
	    const TArray<FString>& Words,
	    Thespeon::Language::LanguageModule* LangModule,
	    Thespeon::Language::RuntimeLookupTable* LookupTable,
	    Thespeon::Inference::FSessionWorkloadCache* WorkloadCache,
	    const FInferenceConfig& Config
	);
	/**
	 * Phonemizes a single input segment, turning each word into phonemes and encoding them into encoder tokens.
	 *
	 * @param Segment - Input text segment to phonemize
	 * @param CharacterModule - Character module for phoneme encoding table
	 * @param LookupTable - Runtime lookup table for grapheme-to-phoneme mapping
	 * @param OutSegmentTokens - Output array of encoded phoneme tokens for the segment
	 * @return true if phonemization succeeded, false otherwise
	 */
	bool PhonemizeSegment(
	    FLingotionInputSegment Segment,
	    Thespeon::Character::CharacterModule* CharacterModule,
	    Thespeon::Language::RuntimeLookupTable* LookupTable,
	    TArray<int64>& SegmentTokens,
	    TArray<int64>& GlobalIndices,
	    int& TextLengthSoFar
	);
	/**
	 * Identifies words in the input text that are not present in the provided lookup table and returns them.
	 * Text is meant to be a single segment's text and this of a language encompassed by LangModule.
	 *
	 * @param Text - Input text segment to analyze
	 * @param LangModule - Language module for phoneme encoding table
	 * @param LookupTable - Runtime lookup table for checking word existence.
	 * @return Array of unknown words not found in the lookup table
	 */
	TArray<FString>
	GetUnknownWords(const FString& Text, Thespeon::Language::LanguageModule* LangModule, Thespeon::Language::RuntimeLookupTable* LookupTable);

	/** Subsystem pointers captured on game thread at construction time */
	UInferenceWorkloadManager* InferenceWorkloadManager = nullptr;
	UModuleManager* ModuleManager = nullptr;
	ULookupTableManager* LookupTableManager = nullptr;
	UManifestHandler* ManifestHandler = nullptr;
};
} // namespace Inference
} // namespace Thespeon
