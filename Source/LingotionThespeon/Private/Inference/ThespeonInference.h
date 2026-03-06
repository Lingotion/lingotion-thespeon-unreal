// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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

namespace Thespeon
{
namespace Inference
{
/**
 * Concrete inference session that orchestrates the full TTS pipeline.
 *
 * Handles phonemization, encoder inference, and vocoder inference using
 * a MetaGraphRunner. Runs on a background thread via FRunnable.
 */
class ThespeonInference : public InferenceSession
{
  public:
	ThespeonInference(const FLingotionModelInput& InInput, const FInferenceConfig InConfig, const FString& InSessionID)
	    : InferenceSession(InInput, InConfig, InSessionID)
	{
	}

	/** @brief Executes the full inference pipeline on the background thread.
	 *  @return Exit code (0 on success). */
	uint32 Run() override;

	/** @brief Requests the session to stop execution. */
	void Stop() override;

	/** @brief Called after Run() completes for cleanup. */
	void Exit() override;

	/** @brief Attempts to preload the models for a character and module type into memory.
	 *  @param CharacterName The name of the character to preload.
	 *  @param ModuleType The module type (quality tier) to preload.
	 *  @param BackendType The backend whose models should be preloaded.
	 *  @return True if preloading succeeded. */
	static bool TryPreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, EBackendType BackendType);

	/** @brief Attempts to unload the models for a character and module type from memory.
	 *  @param CharacterName The name of the character to unload.
	 *  @param ModuleType The module type to unload.
	 *  @param BackendType The backend whose models should be unloaded.
	 *  @return True if unloading succeeded. */

	static bool TryUnloadCharacter(const FString& CharacterName, const EThespeonModuleType& ModuleType, EBackendType BackendType);

  private:
	void PostErrorPacket();
	void PostCancelledPacket();

	/**
	 * Phonemizes a batch of words using the language module's phonemizer model.
	 *
	 * @param Words - Array of words to phonemize
	 * @param LangModule - Language module containing phonemizer model and vocab
	 * @param InferenceWorkloadManager - Manager to access phonemizer workload
	 * @param Config - Inference configuration
	 * @param OutBatchPhonemeTokens - Output array of phoneme token sequences for each word
	 * @return true if phonemization succeeded, false otherwise
	 */
	bool PhonemizeBatch(
	    const TArray<FString>& Words,
	    Thespeon::Language::LanguageModule* LangModule,
	    Thespeon::Language::RuntimeLookupTable* LookupTable,
	    UInferenceWorkloadManager* InferenceWorkloadManager,
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
};
} // namespace Inference
} // namespace Thespeon
