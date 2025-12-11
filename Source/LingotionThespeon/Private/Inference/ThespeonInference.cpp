// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "ThespeonInference.h"
#include "Core/PackManifestHandler.h"
#include "ModuleManager.h"
#include "Core/Module.h"
#include "../LanguagePack/LanguageModule.h"
#include "../LanguagePack/LookupTableManager.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "InferenceWorkload.h"
#include "../Inference/InferenceWorkloadManager.h"
#include "LanguagePack/TextPreprocessor.h"

// Execute callback on game thread, and not inference thread
void Thespeon::Inference::ThespeonInference::PostPacket(const Thespeon::Core::FAnyPacket& Packet, float BufferSeconds)
{
    AsyncTask(ENamedThreads::GameThread, [this, Packet, BufferSeconds]()
    {
        if (OnDataSynthesized.IsBound())
        {
            OnDataSynthesized.Execute(Packet, BufferSeconds);
        }
    });
}

void Thespeon::Inference::ThespeonInference::PostErrorPacket()
{
	Thespeon::Core::FPacketMetadata metadata;
	metadata.SessionID = SessionID;
	metadata.bPacketStatus = false;
	Thespeon::Core::FThespeonDataPacket<float> outputPacket;
	outputPacket.Metadata = metadata;
	
	Thespeon::Core::FAnyPacket Packet;
	Packet.Set<Thespeon::Core::FThespeonDataPacket<float>>(MoveTemp(outputPacket));
	PostPacket(Packet, InputConfig.BufferSeconds);
}

bool Thespeon::Inference::ThespeonInference::PhonemizeBatch(
	const TArray<FString>& Words,
	Thespeon::LanguagePack::LanguageModule* LangModule,
	UInferenceWorkloadManager* InferenceWorkloadManager,
	const FInferenceConfig& Config,
	TArray<TArray<int64>>& OutBatchPhonemeTokens
)
{
	if (!LangModule)
	{
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("PhonemizeBatch: LangModule is null"));
		return false;
	}

	const int32 BatchSize = Words.Num();
	if (BatchSize == 0)
	{
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("PhonemizeBatch: No words to phonemize"));
		return false;
	}

	// Encode all words to grapheme tokens
	TArray<TArray<int64>> BatchGraphemeTokens;
	BatchGraphemeTokens.Reserve(BatchSize);
	int32 MaxSrcLength = 0;

	for (const FString& Word : Words)
	{
		TArray<int64> GraphemeTokens = LangModule->EncodeGraphemes(Word);
		
		// Add SOS and EOS tokens
		GraphemeTokens.Insert(1, 0); // SOS token from grapheme vocab
		GraphemeTokens.Add(2);        // EOS token from grapheme vocab
		
		MaxSrcLength = FMath::Max(MaxSrcLength, GraphemeTokens.Num());
		BatchGraphemeTokens.Add(MoveTemp(GraphemeTokens));
	}

	// Pad all grapheme sequences to MaxSrcLength (padding token = 0)
	TArray<int64> FlatGraphemeTokens;
	FlatGraphemeTokens.Reserve(BatchSize * MaxSrcLength);
	for (TArray<int64>& Tokens : BatchGraphemeTokens)
	{
		while (Tokens.Num() < MaxSrcLength)
		{
			Tokens.Add(0); // Pad with 0
		}
		FlatGraphemeTokens.Append(Tokens);
	}

	const int32 StartToken = 1; // SOS token from phoneme vocab
	const int32 EndToken = 2;   // EOS token from phoneme vocab
	const int32 MaxPhonemeLength = 50; // Maximum autoregressive iterations

	auto* PhonemizerWorkload = InferenceWorkloadManager->GetWorkload(LangModule->GetInternalModelID(TEXT("phonemizer")));
	if (!PhonemizerWorkload)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("PhonemizeBatch: Failed to get phonemizer workload"));
		return false;
	}

	// Initialize batch state
	TArray<int64> CurrentTgt;
	TArray<int64> CurrentMask;
	TArray<int64> FinishedIndices;
	
	CurrentTgt.Init(StartToken, BatchSize);  // [B]
	CurrentMask.Init(1, BatchSize);          // [B]
	FinishedIndices.Init(0, BatchSize);      // [B] - all words still active
	
	OutBatchPhonemeTokens.SetNum(BatchSize);
	
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("PhonemizeBatch: Starting phonemization with %d words, max src length %d"), BatchSize, MaxSrcLength);
	
	// Autoregressive loop
	for (int32 Step = 0; Step < MaxPhonemeLength; ++Step)
	{
		const int32 CurrentTgtLength = CurrentTgt.Num() / BatchSize;
		
		// Prepare input tensors for this step
		TArray<FString> PhonemizerInputNames = { TEXT("src"), TEXT("tgt"), TEXT("mask_tensor"), TEXT("finished_indices") };
		TArray<UE::NNE::FTensorShape> PhonemizerInputShapes = {
			UE::NNE::FTensorShape::Make({ static_cast<uint32>(BatchSize), static_cast<uint32>(MaxSrcLength) }),
			UE::NNE::FTensorShape::Make({ static_cast<uint32>(BatchSize), static_cast<uint32>(CurrentTgtLength) }),
			UE::NNE::FTensorShape::Make({ static_cast<uint32>(BatchSize), static_cast<uint32>(CurrentTgtLength) }),
			UE::NNE::FTensorShape::Make({ static_cast<uint32>(BatchSize) })
		};
		
		// Set input tensors in pool
		TensorPool.SetTensor(PhonemizerInputNames[0], ModelIOData::MakeFromArray<int64>(PhonemizerInputShapes[0], FlatGraphemeTokens));
		TensorPool.SetTensor(PhonemizerInputNames[1], ModelIOData::MakeFromArray<int64>(PhonemizerInputShapes[1], CurrentTgt));
		TensorPool.SetTensor(PhonemizerInputNames[2], ModelIOData::MakeFromArray<int64>(PhonemizerInputShapes[2], CurrentMask));
		TensorPool.SetTensor(PhonemizerInputNames[3], ModelIOData::MakeFromArray<int64>(PhonemizerInputShapes[3], FinishedIndices));
		
		// Prepare output shapes - phonemizer outputs: new_tgt, new_mask, new_finished_indices, num_finished, next_word
		TArray<UE::NNE::FTensorShape> PhonemizerOutputShapes = {
			UE::NNE::FTensorShape::Make({ static_cast<uint32>(BatchSize), static_cast<uint32>(CurrentTgtLength + 1) }), // new_tgt
			UE::NNE::FTensorShape::Make({ static_cast<uint32>(BatchSize), static_cast<uint32>(CurrentTgtLength + 1) }), // new_mask
			UE::NNE::FTensorShape::Make({ static_cast<uint32>(BatchSize) }), // new_finished_indices
			UE::NNE::FTensorShape::Make({ 1 }), // num_finished (scalar)
			UE::NNE::FTensorShape::Make({ static_cast<uint32>(BatchSize) }) // next_word [B]
		};
		
		// Run inference
		PhonemizerWorkload->Infer(TensorPool, PhonemizerOutputShapes, Config, TEXT("Phonemizer"));
		
		// Get output tensors
		ModelIOData* NewTgtTensor = nullptr;
		ModelIOData* NewMaskTensor = nullptr;
		ModelIOData* NewFinishedIndicesTensor = nullptr;
		ModelIOData* NumFinishedTensor = nullptr;
		ModelIOData* NextWordTensor = nullptr;
		
		if (!TensorPool.TryGetTensor(TEXT("new_tgt"), NewTgtTensor))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("PhonemizeBatch: Failed to get phonemizer new_tgt tensor"));
			return false;
		}
		if (!TensorPool.TryGetTensor(TEXT("new_mask"), NewMaskTensor))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("PhonemizeBatch: Failed to get phonemizer new_mask tensor"));
			return false;
		}
		if (!TensorPool.TryGetTensor(TEXT("new_finished_indices"), NewFinishedIndicesTensor))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("PhonemizeBatch: Failed to get phonemizer new_finished_indices tensor"));
			return false;
		}
		if (!TensorPool.TryGetTensor(TEXT("num_finished"), NumFinishedTensor))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("PhonemizeBatch: Failed to get phonemizer num_finished tensor"));
			return false;
		}
		if (!TensorPool.TryGetTensor(TEXT("next_word"), NextWordTensor))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("PhonemizeBatch: Failed to get phonemizer next_word tensor"));
			return false;
		}
		
		// Check batch completion via num_finished
		int64 NumFinished = NumFinishedTensor->GetDataAsValue<int64>(0);
		
		if (NumFinished >= BatchSize)
		{
			LINGO_LOG(EVerbosityLevel::Debug, TEXT("PhonemizeBatch: Phonemization completed at step %d - all %d words finished"), Step, BatchSize);
			break;
		}
		
		// Extract batch outputs
		TArray<int64> NextTokens = NextWordTensor->GetDataAsArray<int64>(); // [B]
		
		// Add tokens to each word's phoneme sequence (skip SOS/EOS, exclude finished words)
		for (int32 b = 0; b < BatchSize; ++b)
		{
			if (FinishedIndices[b] == 0) // Word not finished
			{
				int64 Token = NextTokens[b];
				if (Token != StartToken && Token != EndToken)
				{
					OutBatchPhonemeTokens[b].Add(Token);
				}
			}
		}
		
		// Update state for next iteration
		CurrentTgt = NewTgtTensor->GetDataAsArray<int64>();
		CurrentMask = NewMaskTensor->GetDataAsArray<int64>();
		FinishedIndices = NewFinishedIndicesTensor->GetDataAsArray<int64>();
	}
	
	// Log results for each word
	for (int32 i = 0; i < BatchSize; ++i)
	{
		FString PhonemeText = LangModule->DecodePhonemes(OutBatchPhonemeTokens[i]);
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("PhonemizeBatch: Word %d ('%s'): %d phoneme tokens -> '%s'"), 
			i, *Words[i], OutBatchPhonemeTokens[i].Num(), *PhonemeText);
	}

	return true;
}

bool Thespeon::Inference::ThespeonInference::ConvertPhonemesToEncoderIDs(
	const TArray<TArray<int64>>& BatchPhonemeTokens,
	Thespeon::LanguagePack::LanguageModule* LangModule,
	Thespeon::ActorPack::ActorModule* ActorModule,
	TArray<TArray<int64>>& OutBatchEncoderTokens
)
{
	if (!LangModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ConvertPhonemesToEncoderIDs: LanguageModule is null"));
		return false;
	}
	
	if (!ActorModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ConvertPhonemesToEncoderIDs: ActorModule is null"));
		return false;
	}
	
	const int32 BatchSize = BatchPhonemeTokens.Num();
	OutBatchEncoderTokens.SetNum(BatchSize);
	
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("ConvertPhonemesToEncoderIDs: Converting %d phoneme sequences to encoder IDs"), BatchSize);
	
	for (int32 i = 0; i < BatchSize; ++i)
	{
		const TArray<int64>& PhonemeTokens = BatchPhonemeTokens[i];
		
		// Step 1: Decode phoneme IDs to phoneme strings using LanguageModule
		FString PhonemeString = LangModule->DecodePhonemes(PhonemeTokens);
		
		if (PhonemeString.IsEmpty())
		{
			LINGO_LOG(EVerbosityLevel::Debug, TEXT("ConvertPhonemesToEncoderIDs: Word %d decoded to empty string, skipping"), i);
			OutBatchEncoderTokens[i] = TArray<int64>();
			continue;
		}
		
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("ConvertPhonemesToEncoderIDs: Word %d - Phoneme IDs decoded to: '%s'"), i, *PhonemeString);
		
		// Step 2: Encode phoneme strings to encoder IDs using ActorModule
		TArray<int64> EncoderTokens = ActorModule->EncodePhonemes(PhonemeString);
		
		// Filter out sentinel values (-1) that represent phonemes not found in vocabulary
		TArray<int64> FilteredEncoderTokens;
		int32 SkippedCount = 0;
		
		for (int32 j = 0; j < EncoderTokens.Num(); ++j)
		{
			if (EncoderTokens[j] == -1)
			{
				SkippedCount++;
				LINGO_LOG(EVerbosityLevel::Debug, TEXT("ConvertPhonemesToEncoderIDs: Word %d, position %d - Phoneme not found in encoder vocabulary, skipping"), i, j);
			}
			else
			{
				FilteredEncoderTokens.Add(EncoderTokens[j]);
			}
		}
		
		if (SkippedCount > 0)
		{
			LINGO_LOG(EVerbosityLevel::Debug, TEXT("ConvertPhonemesToEncoderIDs: Word %d - Skipped %d/%d phonemes not found in encoder vocabulary"), 
				i, SkippedCount, EncoderTokens.Num());
		}
		
		OutBatchEncoderTokens[i] = FilteredEncoderTokens;
		
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("ConvertPhonemesToEncoderIDs: Word %d - Converted to %d encoder tokens"), i, FilteredEncoderTokens.Num());
	}
	
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("ConvertPhonemesToEncoderIDs: Successfully converted %d phoneme sequences to encoder IDs"), BatchSize);
	
	return true;
}

bool Thespeon::Inference::ThespeonInference::ProcessWordsWithLookup(
	const FString& Text,
	Thespeon::LanguagePack::LanguageModule* LangModule,
	Thespeon::ActorPack::ActorModule* ActorModule,
	UInferenceWorkloadManager* InferenceWorkloadManager,
	ULookupTableManager* LookupTableManager,
	const FInferenceConfig& Config,
	TArray<int64>& OutEncoderTokens
)
{
	if (!LangModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ProcessWordsWithLookup: LangModule is null"));
		return false;
	}
	
	if (!ActorModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ProcessWordsWithLookup: ActorModule is null"));
		return false;
	}
	
	if (!LookupTableManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ProcessWordsWithLookup: LookupTableManager is null"));
		return false;
	}
	
	// Step 1: Extract words from cleaned text
	FString CleanedText = FTextPreprocessor::CleanText(Text);
	TArray<FString> AllWords = FTextPreprocessor::ExtractWords(CleanedText);
	
	if (AllWords.Num() == 0)
	{
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("ProcessWordsWithLookup: No words extracted from text"));
		OutEncoderTokens.Empty();
		return true;
	}
	
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("ProcessWordsWithLookup: Extracted %d words from text"), AllWords.Num());
	
	// Step 2: Get lookup table for this language
	FString LookupTableMD5 = LangModule->GetLookupTableID();
	Thespeon::LanguagePack::RuntimeLookupTable* LookupTable = LookupTableManager->GetLookupTable(LookupTableMD5);
	
	if (!LookupTable)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ProcessWordsWithLookup: Failed to get lookup table for MD5: %s"), *LookupTableMD5);
		return false;
	}
	
	// Step 3: Separate known and unknown words
	// Using TMap to preserve language information for future multi-language support
	// Key: Language JSON representation (for future expansion)
	// Value: Array of unknown words for that language
	TMap<FLingotionLanguage, TArray<FString>> UnknownWordsByLanguage;
	TArray<FString> KnownPhonemes; // Phonemes from lookup table (parallel to AllWords)
	TArray<bool> IsWordKnown; // Track which words were found in lookup
	
	//FString LangKey = LangModule->ModuleLanguage.ToJson(); // Future-proof for multi-language
	TArray<FString>& UnknownWordsForLang = UnknownWordsByLanguage.FindOrAdd(LangModule->ModuleLanguage);
	
	KnownPhonemes.SetNum(AllWords.Num());
	IsWordKnown.Init(false, AllWords.Num());
	
	int32 LookupHits = 0;
	int32 LookupMisses = 0;
	
	for (int32 i = 0; i < AllWords.Num(); ++i)
	{
		FString Phonemes;
		if (LookupTable->TryGetValue(AllWords[i], Phonemes))
		{
			// Word found in lookup table
			KnownPhonemes[i] = Phonemes;
			IsWordKnown[i] = true;
			LookupHits++;
		}
		else
		{
			// Word not in lookup table - needs phonemization
			UnknownWordsForLang.Add(AllWords[i]);
			IsWordKnown[i] = false;
			LookupMisses++;
			LINGO_LOG(EVerbosityLevel::Debug, TEXT("ProcessWordsWithLookup: Word '%s' NOT in lookup - will phonemize"), *AllWords[i]);
		}
	}
	
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("ProcessWordsWithLookup: Lookup results - %d hits, %d misses (%.1f%% hit rate)"), 
		LookupHits, LookupMisses, AllWords.Num() > 0 ? (100.0f * LookupHits / AllWords.Num()) : 0.0f);
	
	// Step 4: Phonemize unknown words if any
	TArray<TArray<int64>> UnknownPhonemeTokens;
	if (LookupMisses > 0)
	{
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("ProcessWordsWithLookup: Phonemizing %d unknown words"), UnknownWordsForLang.Num());
		
		if (!PhonemizeBatch(UnknownWordsForLang, LangModule, InferenceWorkloadManager, Config, UnknownPhonemeTokens))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("ProcessWordsWithLookup: Failed to phonemize unknown words"));
			return false;
		}
		
		// Step 5: Cache phonemized results to dynamic lookup table
		for (int32 i = 0; i < UnknownWordsForLang.Num(); ++i)
		{
			FString PhonemeString = LangModule->DecodePhonemes(UnknownPhonemeTokens[i]);
			LookupTable->AddOrUpdateDynamicEntry(UnknownWordsForLang[i], PhonemeString);
			LINGO_LOG(EVerbosityLevel::Debug, TEXT("ProcessWordsWithLookup: Cached '%s' -> '%s' to dynamic lookup"), 
				*UnknownWordsForLang[i], *PhonemeString);
		}
	}
	
	// Step 6: Now all words are in lookup table - reconstruct full text with delimiters
	// Parse cleaned text character by character, extracting words and delimiters in order
	TArray<int64> CompleteEncoderSequence;
	int32 TextPos = 0;
	int32 WordIndex = 0;
	// Append SOS
	CompleteEncoderSequence.Append(ActorModule->EncodePhonemes(TEXT("⏩")));
	
	while (TextPos < CleanedText.Len())
	{
		// Check if we're at the start of a word
		bool bIsWordChar = FChar::IsAlpha(CleanedText[TextPos]) || CleanedText[TextPos] == TEXT('\'');
		
		if (bIsWordChar)
		{
			// Extract the word
			int32 WordStart = TextPos;
			while (TextPos < CleanedText.Len() && 
				   (FChar::IsAlpha(CleanedText[TextPos]) || CleanedText[TextPos] == TEXT('\'')))
			{
				TextPos++;
			}
			
			FString Word = CleanedText.Mid(WordStart, TextPos - WordStart);
			
			// Get phonemes from lookup table (should always exist now)
			FString Phonemes;
			if (LookupTable->TryGetValue(Word, Phonemes))
			{
				// Encode phonemes to encoder IDs via ActorModule
				TArray<int64> EncoderTokens = ActorModule->EncodePhonemes(Phonemes);
				CompleteEncoderSequence.Append(EncoderTokens);
				LINGO_LOG(EVerbosityLevel::Debug, TEXT("ProcessWordsWithLookup: Word '%s' -> phonemes '%s' -> %d tokens"), 
					*Word, *Phonemes, EncoderTokens.Num());
			}
			else
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("ProcessWordsWithLookup: Word '%s' not found in lookup after processing!"), *Word);
				return false;
			}
			
			WordIndex++;
		}
		else
		{
			// It's a delimiter (space, punctuation, etc.) - encode directly
			FString Delimiter = CleanedText.Mid(TextPos, 1);
			TArray<int64> DelimiterTokens = ActorModule->EncodePhonemes(Delimiter);
			
			if (DelimiterTokens.Num() > 0)
			{
				CompleteEncoderSequence.Append(DelimiterTokens);
				LINGO_LOG(EVerbosityLevel::Debug, TEXT("ProcessWordsWithLookup: Delimiter '%s' -> %d tokens"), 
					*Delimiter, DelimiterTokens.Num());
			}
			else
			{
				LINGO_LOG(EVerbosityLevel::Debug, TEXT("ProcessWordsWithLookup: Delimiter '%s' encoded to empty sequence, skipping"), *Delimiter);
			}
			
			TextPos++;
		}
	}
	// Append EOS
	CompleteEncoderSequence.Append(ActorModule->EncodePhonemes(TEXT("⏪")));
	
	// Step 7: Return complete flattened sequence
	OutEncoderTokens = MoveTemp(CompleteEncoderSequence);
	
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("ProcessWordsWithLookup: Successfully processed text -> %d total encoder tokens (from %d words)"), 
		OutEncoderTokens.Num(), AllWords.Num());
	
	return true;
}


uint32 Thespeon::Inference::ThespeonInference::Run()
{
	// Get the FString character name that is a UProperty from input
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("ThespeonInference.Infer called with CharacterName: %s, ModuleType: %s, SessionID: %s"), *Input.CharacterName, *UEnum::GetValueAsString(Input.ModuleType), *SessionID);
	// Preload character to ensure models are loaded
	if(!TryPreloadCharacter(Input.CharacterName, Input.ModuleType, InputConfig))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to preload character: %s, ModuleType: %s"), *Input.CharacterName, *UEnum::GetValueAsString(Input.ModuleType));
		PostErrorPacket();
		return false;
	}

	auto* Manifest = UPackManifestHandler::Get();
	if (!Manifest)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("PackManifestHandler instance is null"));
		PostErrorPacket();
		return false;
	}
	Thespeon::Core::FModuleEntry Entry = Manifest->GetActorPackModuleEntry(Input.CharacterName, Input.ModuleType);

	auto* ModuleManager = UModuleManager::Get();
	if (!ModuleManager)
	{
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("ModuleManager instance is null"));
		PostErrorPacket();
		return false;
	}

	UInferenceWorkloadManager* InferenceWorkloadManager = nullptr;
	ULookupTableManager* LookupTableManager = nullptr;

	if (GEngine)
	{
		//Stupid way to get GI subsystem. This returns the first GI it finds not necessarily the right one.
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.OwningGameInstance)
			{
				InferenceWorkloadManager = C.OwningGameInstance->GetSubsystem<UInferenceWorkloadManager>();
				LookupTableManager = C.OwningGameInstance->GetSubsystem<ULookupTableManager>();
				break;
			}
		}
	}

	if(!InferenceWorkloadManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("InferenceWorkloadManager subsystem not found in GameInstance."));
		PostErrorPacket();
		return false;
	}
	if(!LookupTableManager)
	{
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("LookupTableManager subsystem not found in GameInstance."));
		PostErrorPacket();
		return false;
	}

	using namespace Thespeon::Inference;

	Thespeon::ActorPack::ActorModule* CharacterModule = ModuleManager->GetModule<Thespeon::ActorPack::ActorModule>(Entry);

	if(!CharacterModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get ActorModule for character: %s"), *Input.CharacterName);
		PostErrorPacket();
		return false;
	}
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Got ActorModule: %s, %s, %s"), *(CharacterModule->ModuleID), *(CharacterModule->JSONPath), *(CharacterModule->Version.ToString()));

	// ---------- Start: Testing phonemizer ----------
	FString TargetLang = Input.DefaultLanguage.ISO639_2;
	if(TargetLang == TEXT("NOLANG"))
	{
		TargetLang = Input.Segments[0].Language.ISO639_2;
	}
	Thespeon::LanguagePack::LanguageModule* LangModule = nullptr;
	FString* LangModuleID = CharacterModule->LanguageModuleIDs.Find(TargetLang);
	if(!LangModuleID)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Character module has no language: %s"), *TargetLang);
		PostErrorPacket();
		return false;
	}
	Thespeon::Core::FModuleEntry LangEntry = Manifest->GetLanguagePackModuleEntry(*LangModuleID);

	if (LangEntry.ModuleID.IsEmpty())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("No language module entry found for ModuleName: %s"), **LangModuleID);
		PostErrorPacket();
		return false;
	}
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Got Language Entry: %s, %s, %s"), *LangEntry.ModuleID, *LangEntry.JsonPath, *LangEntry.Version.ToString());

	LangModule = ModuleManager->GetModule<Thespeon::LanguagePack::LanguageModule>(LangEntry);
	
	if (!LangModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get LanguageModule"));
		PostErrorPacket();
		return false;
	}
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Got LanguageModule: %s, %s, %s"), *(LangModule->ModuleID), *(LangModule->JSONPath), *(LangModule->Version.ToString()));
	

	// Process words using integrated lookup + phonemizer pipeline
	TArray<int64> txt;
	if (!ProcessWordsWithLookup(Input.Segments[0].Text, LangModule, CharacterModule, InferenceWorkloadManager, LookupTableManager, InputConfig, txt))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to process words with lookup table pipeline"));
		PostErrorPacket();
		return false;
	}

	// Log complete encoder sequence
	if (txt.Num() > 0)
	{
		FString TokensStr;
		for (int64 Token : txt)
		{
			TokensStr += FString::Printf(TEXT("%lld "), Token);
		}
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("Complete encoder sequence (%d tokens): [%s]"), txt.Num(), *TokensStr);
	}

	// Validate encoder tokens
	if (txt.Num() == 0)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ProcessWordsWithLookup returned empty encoder tokens"));
		PostErrorPacket();
		return false;
	}
	
	uint32 txt_len = txt.Num();
	EEmotion EmotionToUse = Input.Segments[0].Emotion != EEmotion::None
		? Input.Segments[0].Emotion
		: (Input.DefaultEmotion != EEmotion::None
			? Input.DefaultEmotion
			: InputConfig.FallbackEmotion);
	FLingotionLanguage LangToTry = Input.Segments[0].Language.IsUndefined()
		? (Input.DefaultLanguage.IsUndefined()
			? InputConfig.FallbackLanguage
			: Input.DefaultLanguage)
		: Input.Segments[0].Language;
	TArray<FLingotionLanguage> CandidateLanguages = CharacterModule->GetSupportedLanguages();
	if(CandidateLanguages.Num() == 0)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Character module has no supported languages"));
		PostErrorPacket();
		return false;
	}
	FLingotionLanguage LangToUse = Thespeon::Core::BestLanguageMatch(CandidateLanguages, LangToTry);
	if(LangToUse.IsUndefined())
	{
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("No matching language found in character module, defaulting to first supported language"));
		LangToUse = CandidateLanguages[0];
	}
	int64 LangKey = CharacterModule->GetLangKey(LangToUse);
	if(LangKey == -1)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Character module does not have a valid LangKey for language: %s"), *LangToUse.ToJson());
		PostErrorPacket();
		return false;
	}
	// Initialize encoder input tensors based on actual text length
	TArray<int64> emotions;
	TArray<int64> actors;
	TArray<int64> languages;
	TArray<float> speed;
	TArray<float> loudness;
	emotions.Init(static_cast<int64>(EmotionToUse), txt_len);
	actors.Init(1, 1);
	languages.Init(LangKey, txt_len);
	speed.Init(1.0f, txt_len);
	loudness.Init(1.0f, txt_len);

	TArray<UE::NNE::FTensorShape> InputTensorShapes = { 
		UE::NNE::FTensorShape::Make({ 1, txt_len }), 
		UE::NNE::FTensorShape::Make({ 1, txt_len }),
		UE::NNE::FTensorShape::Make({ 1, 1 }), 
		UE::NNE::FTensorShape::Make({ 1, txt_len }), 
		UE::NNE::FTensorShape::Make({ 1, txt_len }), 
		UE::NNE::FTensorShape::Make({ 1, txt_len }), 
	};
	
	TArray<UE::NNE::FTensorShape> encoderOutputShapes = {
		UE::NNE::FTensorShape::Make({ 1, 96, 2 * txt_len }),
		UE::NNE::FTensorShape::Make({ 1, 1, 2 * txt_len }),
		UE::NNE::FTensorShape::Make({ 1, 1, 2 * txt_len }),
		UE::NNE::FTensorShape::Make({ 1, 2 * txt_len }),
		UE::NNE::FTensorShape::Make({ 1, 1 }),
		UE::NNE::FTensorShape::Make({ 1, txt_len }),
		UE::NNE::FTensorShape::Make({ 1 }),
	};
	
	TArray<FString> InputNames = { TEXT("txt"), TEXT("emotions"), TEXT("actors.1"), TEXT("languages.1"), TEXT("speed"), TEXT("loudness.1") };
	
	// Set input tensors in pool
	TensorPool.SetTensor(InputNames[0], ModelIOData::MakeFromArray<int64>(InputTensorShapes[0], txt));
	TensorPool.SetTensor(InputNames[1], ModelIOData::MakeFromArray<int64>(InputTensorShapes[1], emotions));
	TensorPool.SetTensor(InputNames[2], ModelIOData::MakeFromArray<int64>(InputTensorShapes[2], actors));
	TensorPool.SetTensor(InputNames[3], ModelIOData::MakeFromArray<int64>(InputTensorShapes[3], languages));
	TensorPool.SetTensor(InputNames[4], ModelIOData::MakeFromArray<float>(InputTensorShapes[4], speed));
	TensorPool.SetTensor(InputNames[5], ModelIOData::MakeFromArray<float>(InputTensorShapes[5], loudness));

	// Create InferenceWorkload for the encoder
	Thespeon::Inference::InferenceWorkload* EncoderNAWorkload = InferenceWorkloadManager->GetWorkload(CharacterModule->GetInternalModelID(TEXT("encoder_noalign")));
	if(!EncoderNAWorkload)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get encoder_noalign workload"));
		PostErrorPacket();
		return false;
	}
	

	EncoderNAWorkload->Infer(TensorPool, encoderOutputShapes, InputConfig, TEXT("Encoder Noalign"));
	
	ModelIOData* encoderAlignment;
	check(TensorPool.TryGetTensor(TEXT("aligned_length"), encoderAlignment));
	uint32 aligned_length = encoderAlignment->GetDataAsValue<uint32>(0);
	TArray<UE::NNE::FTensorShape> alignedOutputShapes = {
		UE::NNE::FTensorShape::Make({ 1, 96,  aligned_length}), // mel_aligned
		UE::NNE::FTensorShape::Make({ 1, 1, aligned_length}), // mel_mask
		UE::NNE::FTensorShape::Make({ 1 }), // mel_max_length
		UE::NNE::FTensorShape::Make({ 1, 1 }), // actors
		UE::NNE::FTensorShape::Make({ 1, txt_len }), // languages
		UE::NNE::FTensorShape::Make({ 1, 1, 512 * aligned_length}), // loudness_scale_audio
		UE::NNE::FTensorShape::Make({ 1, 1, 2 * txt_len }), // alignment
	};

	Thespeon::Inference::InferenceWorkload* EncoderAlignerWorkload = InferenceWorkloadManager->GetWorkload(CharacterModule->GetInternalModelID(TEXT("encoder_aligner")));
	if(!EncoderAlignerWorkload)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get encoder_aligner workload"));
		PostErrorPacket();
		return false;
	}
	EncoderAlignerWorkload->Infer(TensorPool, alignedOutputShapes, InputConfig, TEXT("Encoder Aligner"));

	// Decoder preprocess, takes mel_aligned and mel_max_length as inputs
	check(TensorPool.TryRenameTensor(TEXT("mel_aligned"), TEXT("encoder_mel")));

	check(TensorPool.TryRenameTensor(TEXT("mel_max_length"), TEXT("encoder_mel_max_length")));

	ModelIOData* mel_aligned;
	check(TensorPool.TryGetTensor(TEXT("encoder_mel"), mel_aligned));

	
	TArray<UE::NNE::FTensorShape> DecoderPreOutputShapes = {
		mel_aligned->GetTensorShape(), // z
		UE::NNE::FTensorShape::Make({ 1 }), // nbr_of_chunks
		UE::NNE::FTensorShape::Make({ 1, 96, 16}), // decoded_mel_overlap
		UE::NNE::FTensorShape::Make({ 1 }), // remainder
		UE::NNE::FTensorShape::Make({ 1 }), // trim_length
	};

	Thespeon::Inference::InferenceWorkload* DecoderPreWorkload = InferenceWorkloadManager->GetWorkload(CharacterModule->GetInternalModelID(TEXT("decoder_preprocess")));
	if(!DecoderPreWorkload)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get decoder_preprocess workload"));
		PostErrorPacket();
		return false;
	}
	DecoderPreWorkload->Infer(TensorPool, DecoderPreOutputShapes, InputConfig, TEXT("Decoder Preprocess"));

	ModelIOData* nbr_of_chunks;
	check(TensorPool.TryGetTensor(TEXT("nbr_of_chunks"), nbr_of_chunks));
	int32 NumberOfChunks = nbr_of_chunks->GetDataAsValue<int32>(0);
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("nbr chunks %d"), NumberOfChunks);
	
	Thespeon::Inference::InferenceWorkload* DecoderChunkedWorkload = InferenceWorkloadManager->GetWorkload(CharacterModule->GetInternalModelID(TEXT("decoder_chunked")));
	if(!DecoderChunkedWorkload)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get decoder_chunked workload"));
		PostErrorPacket();
		return false;
	}

	Thespeon::Inference::InferenceWorkload* VocoderFirstWorkload = InferenceWorkloadManager->GetWorkload(CharacterModule->GetInternalModelID(TEXT("vocoder_first")));
	if(!VocoderFirstWorkload)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get vocoder_first workload"));
		PostErrorPacket();
		return false;
	}
	Thespeon::Inference::InferenceWorkload* VocoderMiddleWorkload = InferenceWorkloadManager->GetWorkload(CharacterModule->GetInternalModelID(TEXT("vocoder_middle")));
	if(!VocoderMiddleWorkload)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get vocoder_middle workload"));
		PostErrorPacket();
		return false;
	}
	Thespeon::Inference::InferenceWorkload* DecoderPostWorkload = InferenceWorkloadManager->GetWorkload(CharacterModule->GetInternalModelID(TEXT("decoder_postprocess")));
	if(!DecoderPostWorkload)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get decoder_postprocess workload"));
		PostErrorPacket();
		return false;
	}
	Thespeon::Inference::InferenceWorkload* vocoderLastWorkload = InferenceWorkloadManager->GetWorkload(CharacterModule->GetInternalModelID(TEXT("vocoder_last")));
	if(!vocoderLastWorkload)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get vocoder_last workload"));
		PostErrorPacket();
		return false;
	}

	TArray<int64> ChunkLengthContainer;
	TArray<int64> currentChunkIndexContainer;
	TArray<float> BoundaryCloneAlphaContainer;
	ChunkLengthContainer.Init(48, 1);
	currentChunkIndexContainer.Init(0, 1);
	BoundaryCloneAlphaContainer.Init(1.0, 1);
	TensorPool.SetTensor(TEXT("chunk_length"), ModelIOData::MakeFromArray<int64>(UE::NNE::FTensorShape::Make({ 1 }), ChunkLengthContainer));
	TensorPool.SetTensor(TEXT("chunk_index"), ModelIOData::Make(UE::NNE::FTensorShape::Make({ 1 }), ENNETensorDataType::Int64));
	TensorPool.SetTensor(TEXT("boundary_clone_alpha"), ModelIOData::MakeFromArray<float>(UE::NNE::FTensorShape::Make({ 1 }), BoundaryCloneAlphaContainer));

	TArray<UE::NNE::FTensorShape> DecoderChunkedOutputs = {
		UE::NNE::FTensorShape::Make({ 1, 96, 32}), // decoded_mel_chunk
		UE::NNE::FTensorShape::Make({ 1 }), // decoded_mel_chunk_length
		UE::NNE::FTensorShape::Make({ 1, 96, 16}), // decoded_mel_overlap
		UE::NNE::FTensorShape::Make({ 1, 1 }), // actors
	};

	ModelIOData* currentChunkTensor = nullptr;
	for (int32 chunk = 0; chunk < NumberOfChunks; ++chunk)
	{
		// update current index tensor
		currentChunkIndexContainer[0] = chunk;
		check(TensorPool.TryGetTensor(TEXT("chunk_index"), currentChunkTensor));
		currentChunkTensor->TrySetValueFromArray<int64>(currentChunkIndexContainer);

		// swap overlap
		check(TensorPool.TryRenameTensor(TEXT("decoded_mel_overlap"), TEXT("prev_decoded_overlap")));
		//first chunk
		if(chunk == 0)
		{
			check(TensorPool.TryRenameTensor(TEXT("mel_mask"), TEXT("encoder_mel_mask")));
			DecoderChunkedWorkload->Infer(TensorPool, DecoderChunkedOutputs, InputConfig, TEXT("Decoder Chunked"));
			
			check(TensorPool.TryRenameTensor(TEXT("loudness_scale_audio"), TEXT("loudness_scale_audio.1")));
			TConstArrayView<UE::NNE::FTensorShape> trueVocoderFirstShapes = VocoderFirstWorkload->MockInfer(TensorPool, InputConfig, TEXT("Vocoder first mock"));
			VocoderFirstWorkload->Infer(TensorPool, trueVocoderFirstShapes, InputConfig, TEXT("Vocoder first"));
			TConstArrayView<UE::NNE::FTensorDesc> descs = VocoderFirstWorkload->GetOutputTensorDesc();
			// rename rest tensors to 'rest.1' for next infer 
			for (int i = 1; i < descs.Num(); i++)
			{
				check(TensorPool.TryRenameTensor(descs[i].GetName(), descs[i].GetName() + ".1"));
			}

		} else if(chunk == NumberOfChunks - 1)
		{
			// get encoder mel max length 
			ModelIOData* mel_mask;
			check(TensorPool.TryGetTensor(TEXT("encoder_mel_mask"), mel_mask));
			ModelIOData* remain;
			check(TensorPool.TryGetTensor(TEXT("remainder"), remain));
			uint64 val = remain->GetDataAsValue<uint64>(0);
			uint32 res = mel_mask->GetTensorShape().GetData()[2] % 32;
			uint32 dec_size = 32;
			if(val == 0 && res != 0)
			{
				dec_size = res;
			}
			const uint32 final_dec_size = dec_size;
			DecoderChunkedOutputs = {
				UE::NNE::FTensorShape::Make({ 1, 96, final_dec_size}), // decoded_mel_chunk
				UE::NNE::FTensorShape::Make({ 1 }), // decoded_mel_chunk_length
				UE::NNE::FTensorShape::Make({ 1, 96, 16}), // decoded_mel_overlap
				UE::NNE::FTensorShape::Make({ 1, 1 }), // actors
			};
			DecoderChunkedWorkload->Infer(TensorPool, DecoderChunkedOutputs, InputConfig, TEXT("Decoder Chunked"));
			
			TConstArrayView<UE::NNE::FTensorShape> trueDecoderPostShapes = DecoderPostWorkload->MockInfer(TensorPool, InputConfig, TEXT("Decoder Post mockd"));
			DecoderPostWorkload->Infer(TensorPool, trueDecoderPostShapes, InputConfig, TEXT("Decoder Post"));
			TConstArrayView<UE::NNE::FTensorDesc> descs = VocoderMiddleWorkload->GetOutputTensorDesc();
			TConstArrayView<UE::NNE::FTensorShape> trueVocoderLastShapes = vocoderLastWorkload->MockInfer(TensorPool, InputConfig, TEXT("Vocoder last mockd"));
			vocoderLastWorkload->Infer(TensorPool, trueVocoderLastShapes, InputConfig, TEXT("Vocoder last"));
		} else
		{
			DecoderChunkedWorkload->Infer(TensorPool, DecoderChunkedOutputs, InputConfig, TEXT("Decoder Chunked"));
			TConstArrayView<UE::NNE::FTensorShape> trueVocoderMidShapes = VocoderMiddleWorkload->MockInfer(TensorPool, InputConfig, TEXT("Vocoder mid mock"));
			VocoderMiddleWorkload->Infer(TensorPool, trueVocoderMidShapes, InputConfig, TEXT("Vocoder mid"));
			TConstArrayView<UE::NNE::FTensorDesc> descs = VocoderMiddleWorkload->GetOutputTensorDesc();
			// rename rest tensors to 'rest.1' for next infer 
			if(chunk != NumberOfChunks - 2)
			{
				for (int i = 1; i < descs.Num(); i++)
				{
					check(TensorPool.TryRenameTensor(descs[i].GetName(), descs[i].GetName() + ".1"));
				}
			}
		}

		ModelIOData* AudioData;
		check(TensorPool.TryGetTensor(TEXT("vocoder_audio"), AudioData));
		Thespeon::Core::FPacketMetadata metadata(SessionID, Input.CharacterName, Input.ModuleType, true);
		Thespeon::Core::FThespeonDataPacket<float> outputPacket;
		outputPacket.Data = AudioData->GetDataAsArray<float>();
		outputPacket.bIsFinalPacket = chunk == NumberOfChunks - 1;
		outputPacket.Metadata = metadata;
		
		Thespeon::Core::FAnyPacket Packet;
		Packet.Set<Thespeon::Core::FThespeonDataPacket<float>>(MoveTemp(outputPacket));
		PostPacket(Packet, InputConfig.BufferSeconds);
	}
	
	return true;
}


bool Thespeon::Inference::ThespeonInference::TryPreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, const FInferenceConfig& Config)
{
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("ThespeonInference.TryPreloadCharacter called with CharacterName: %s, ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType));
	//Do steps and check that each was successful to continue. Unreal does not really allow for exceptions.
	//1. Try to find character + type in manifest handler
	auto* Manifest = UPackManifestHandler::Get();
	if (!Manifest)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("PackManifestHandler instance is null"));
		return false;
	}
	Thespeon::Core::FModuleEntry Entry = Manifest->GetActorPackModuleEntry(CharacterName, ModuleType);
	if (Entry.ModuleID.IsEmpty())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("No module entry found for CharacterName: %s, ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType));
		return false;
	}
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Got Actor Entry: %s, %s, %s"), *Entry.ModuleID, *Entry.JsonPath, *Entry.Version.ToString());

	//2. If result, Get Module from ModuleManager
	auto* ModuleManager = UModuleManager::Get();
	if (!ModuleManager)
	{
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("ModuleManager instance is null"));
		return false;
	}

	//3. Get InferenceWorkloadManager from GameInstanceSubsystem
	UInferenceWorkloadManager* InferenceWorkloadManager = nullptr;
	if (GEngine)
	{
		//Stupid way to get GI subsystem. This returns the first GI it finds not necessarily the right one.
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.OwningGameInstance)
			{
				InferenceWorkloadManager = C.OwningGameInstance->GetSubsystem<UInferenceWorkloadManager>();
				break;
			}
		}
	}
	if(!InferenceWorkloadManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("InferenceWorkloadManager subsystem not found in GameInstance."));
		return false;
	}

	// 4. If result, Get ActorModule from ModuleManager
	Thespeon::ActorPack::ActorModule* ActorModule = ModuleManager->GetModule<Thespeon::ActorPack::ActorModule>(Entry);
	if (!ActorModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get ActorModule"));
		return false; // or handle error appropriately
	}
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Got Actor Module: %s"), *ActorModule->ModuleID);


	// 5. For each necessary language module find in manifest handler
	for(const auto& kvp: ActorModule->LanguageModuleIDs)
	{
		// If result, for each necessary language module find in manifest handler
		Thespeon::Core::FModuleEntry LangEntry = Manifest->GetLanguagePackModuleEntry(kvp.Value);
		if (LangEntry.ModuleID.IsEmpty())
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("No language module entry found for ModuleName: %s"), *kvp.Value);
			return false;
		}
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("Got Language Entry: %s, %s, %s"), *LangEntry.ModuleID, *LangEntry.JsonPath, *LangEntry.Version.ToString());
		//6. If result, register language module in InferenceWorkloadManager
		Thespeon::LanguagePack::LanguageModule* LangModule = ModuleManager->GetModule<Thespeon::LanguagePack::LanguageModule>(LangEntry);
		if (!LangModule)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get LanguageModule"));
			return false;
		}
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("Registering workload for Language Module: %s"), *(LangModule->ModuleID));
		if(!InferenceWorkloadManager->RegisterModuleWorkload(LangModule, Config))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to register LanguageModule workload: %s"), *(LangModule->ModuleID));
			return false;
		}
		
		//7. Register lookup table in LookupTableManager
		ULookupTableManager* LookupTableManager = nullptr;
		if (GEngine)
		{
			for (const FWorldContext& C : GEngine->GetWorldContexts())
			{
				if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.OwningGameInstance)
				{
					LookupTableManager = C.OwningGameInstance->GetSubsystem<ULookupTableManager>();
					break;
				}
			}
		}
		if (LookupTableManager)
		{
			LookupTableManager->RegisterLookupTable(LangModule);
			LINGO_LOG(EVerbosityLevel::Debug, TEXT("Successfully registered lookup table for language module: %s"), *(LangModule->ModuleID));
		}
		else
		{
			LINGO_LOG(EVerbosityLevel::Debug, TEXT("LookupTableManager subsystem not found in GameInstance. Lookup tables will not be available."));
		}
	}

	// Register actor module in InferenceWorkloadManager when all associated language modules are registered.
	if(!InferenceWorkloadManager->RegisterModuleWorkload(ActorModule, Config))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to register ActorModule workload: %s"), *(ActorModule->ModuleID));
		return false;
	}
	return true;
	
}

bool Thespeon::Inference::ThespeonInference::TryUnloadCharacter(const FString& CharacterName, const EThespeonModuleType& ModuleType)
{
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("ThespeonInference.TryUnloadCharacter called with CharacterName: %s, ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType));
	//Do steps and check that each was successful to continue. Unreal does not really allow for exceptions.
	//1. Try to find character + type in manifest handler
	auto* Manifest = UPackManifestHandler::Get();
	if (!Manifest)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("PackManifestHandler instance is null"));
		return false;
	}
	Thespeon::Core::FModuleEntry Entry = Manifest->GetActorPackModuleEntry(CharacterName, ModuleType);
	if (Entry.ModuleID.IsEmpty())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("No module entry found for CharacterName: %s, ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType));
		return false;
	}
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Got Actor Entry: %s, %s, %s"), *Entry.ModuleID, *Entry.JsonPath, *Entry.Version.ToString());

	//2. If result, Get Module from ModuleManager
	auto* ModuleManager = UModuleManager::Get();
	if (!ModuleManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ModuleManager instance is null"));
		return false;
	}

	//3. Get InferenceWorkloadManager from GameInstanceSubsystem
	UInferenceWorkloadManager* InferenceWorkloadManager = nullptr;
	if (GEngine)
	{
		//Stupid way to get GI subsystem. This returns the first GI it finds not necessarily the right one.
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.OwningGameInstance)
			{
				InferenceWorkloadManager = C.OwningGameInstance->GetSubsystem<UInferenceWorkloadManager>();
				break;
			}
		}
	}
	if(!InferenceWorkloadManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("InferenceWorkloadManager subsystem not found in GameInstance."));
		return false;
	}

	// 4. If result, Get ActorModule from ModuleManager
	Thespeon::ActorPack::ActorModule* ActorModule = ModuleManager->GetModule<Thespeon::ActorPack::ActorModule>(Entry, false);
	if (!ActorModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get ActorModule"));
		return false; // or handle error appropriately
	}
	LINGO_LOG(EVerbosityLevel::Debug, TEXT("Got Actor Module: %s"), *ActorModule->ModuleID);


	// 5. Remove all language modules for actor module
	ULookupTableManager* LookupTableManager = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.OwningGameInstance)
			{
				LookupTableManager = C.OwningGameInstance->GetSubsystem<ULookupTableManager>();
				break;
			}
		}
	}

	if(!LookupTableManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("LookupTableManager subsystem not found in GameInstance."));
		return false;
	}

	TSet<FString> langModulesToRemove = ModuleManager->GetNonOverlappingModelLangModules(ActorModule);
	for (const FString& langModule : langModulesToRemove)
	{
		Entry = Manifest->GetLanguagePackModuleEntry(langModule);
		if (Entry.ModuleID.IsEmpty())
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("No module entry found for module: %s"), *langModule);
			return false;
		}

		Thespeon::LanguagePack::LanguageModule* currentLangModule = ModuleManager->GetModule<Thespeon::LanguagePack::LanguageModule>(Entry, false);
		if (!currentLangModule)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get Language module %s"), *Entry.ModuleID);
			return false;
		}

		if(!InferenceWorkloadManager->TryDeregisterModuleWorkloads(currentLangModule))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to deregister workloads of module %s"), *Entry.ModuleID);
			return false;
		}
		
		if(!LookupTableManager->TryDeregisterTable(currentLangModule))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to deregister tables of module %s"), *Entry.ModuleID);
			return false;
		}

		if(!ModuleManager->TryDeregisterModule(langModule))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to deregister module %s"), *Entry.ModuleID);
			return false;
		}
	}

	// 6. Remove actor module itself
	if(!InferenceWorkloadManager->TryDeregisterModuleWorkloads(ActorModule))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to deregister workloads for CharacterName: %s, ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType));
		return false;
	}
	
	if(!ModuleManager->TryDeregisterModule(ActorModule->ModuleID))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to deregister module %s"), *(ActorModule->ModuleID));
		return false;
	}
	
	return true;
}
