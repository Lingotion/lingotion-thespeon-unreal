// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "SessionTensorPool.h"
#include "InferenceSession.h"
#include "Engine/InferenceConfig.h"
#include "Core/ThespeonDataPacket.h"

// Forward declarations
namespace Thespeon
{
	namespace LanguagePack
	{
		class LanguageModule;
	}
	namespace ActorPack
	{
		class ActorModule;
	}
}

class UInferenceWorkloadManager;
class ULookupTableManager;

namespace Thespeon
{
	namespace Inference
    {

		// abstract class whose implementation handles a complete inference chain from input to output. 
		// Runs several workloads according to the particular implementation's inference scheme.
		class ThespeonInference: public InferenceSession
		{
		public:
			ThespeonInference(const FLingotionModelInput& InInput, const FInferenceConfig InConfig, const FString& InSessionID)
			: InferenceSession(InInput, InConfig, InSessionID)
			{}
			uint32 Run() override;
			static bool TryPreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, const FInferenceConfig& Config);
			static bool TryUnloadCharacter(const FString& CharacterName, const EThespeonModuleType& ModuleType);
		protected:
			
			void PostPacket(const Thespeon::Core::FAnyPacket& Packet, float BufferSeconds);
			void PostErrorPacket();

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
				Thespeon::LanguagePack::LanguageModule* LangModule,
				UInferenceWorkloadManager* InferenceWorkloadManager,
				const FInferenceConfig& Config,
				TArray<TArray<int64>>& OutBatchPhonemeTokens
			);

			/**
			 * Converts phoneme IDs from the phonemizer output to encoder-compliant IDs.
			 * 
			 * Step 1: Decodes phoneme IDs to phoneme strings using LanguageModule
			 * Step 2: Encodes phoneme strings to encoder IDs using ActorModule
			 * 
			 * @param BatchPhonemeTokens - Input array of phoneme token sequences (from phonemizer)
			 * @param LangModule - Language module for decoding phoneme IDs to strings
			 * @param ActorModule - Actor module for encoding phoneme strings to encoder IDs
			 * @param OutBatchEncoderTokens - Output array of encoder-compliant token sequences
			 * @return true if conversion succeeded, false otherwise
			 */
			bool ConvertPhonemesToEncoderIDs(
				const TArray<TArray<int64>>& BatchPhonemeTokens,
				Thespeon::LanguagePack::LanguageModule* LangModule,
				Thespeon::ActorPack::ActorModule* ActorModule,
				TArray<TArray<int64>>& OutBatchEncoderTokens
			);

			/**
			 * Processes words with lookup table integration and phonemizer fallback.
			 * 
			 * Pipeline:
			 * 1. Extracts words from input text
			 * 2. Checks lookup table for known words
			 * 3. Batches unknown words by language
			 * 4. Phonemizes unknown words only
			 * 5. Caches phonemized results to dynamic lookup table
			 * 6. Returns encoder tokens for all words
			 * 
			 * @param Text - Input text to process
			 * @param LangModule - Language module containing phonemizer and lookup table
			 * @param ActorModule - Actor module for encoder vocabulary
			 * @param InferenceWorkloadManager - Manager to access phonemizer workload
			 * @param LookupTableManager - Manager to access and update lookup tables
			 * @param Config - Inference configuration
			 * @param OutBatchEncoderTokens - Output array of encoder token sequences for all words
			 * @return true if processing succeeded, false otherwise
			 */
		bool ProcessWordsWithLookup(
			const FString& Text,
			Thespeon::LanguagePack::LanguageModule* LangModule,
			Thespeon::ActorPack::ActorModule* ActorModule,
			UInferenceWorkloadManager* InferenceWorkloadManager,
			ULookupTableManager* LookupTableManager,
			const FInferenceConfig& Config,
			TArray<int64>& OutEncoderTokens
		);
		};

	}
}
