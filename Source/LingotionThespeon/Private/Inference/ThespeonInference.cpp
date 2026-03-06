// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "ThespeonInference.h"
#include "Core/ManifestHandler.h"
#include "ModuleManager.h"
#include "Core/Module.h"
#include "Core/LingotionLogger.h"
#include "Language/LanguageModule.h"
#include "Language/LookupTableManager.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "InferenceWorkload.h"
#include "InferenceWorkloadManager.h"
#include "Language/TextPreprocessor.h"
#include <fstream>
#include "meta_graph.pb.h"
#include "MetaGraphRunner.h"
#include "Utils/SubsystemUtils.h"

// Loads a protobuf-serialized MetaGraph from disk. Used to load the character's inference graph definition.
bool LoadModelFromDisk(const FString& Path, metaonnx::MetaGraph& OutGraph)
{
	std::string NativePath = TCHAR_TO_UTF8(*Path);

	std::ifstream Input(NativePath, std::ios::binary);
	if (!Input.is_open())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to open protobuf file: %s"), *Path);
		return false;
	}

	if (!OutGraph.ParseFromIstream(&Input))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to parse protobuf file: %s"), *Path);
		return false;
	}

	return true;
}

void Thespeon::Inference::ThespeonInference::PostErrorPacket()
{
	AsyncTask(
	    ENamedThreads::GameThread,
	    [this]()
	    {
		    Thespeon::Core::FThespeonDataPacket PacketToSend(Thespeon::Core::SynthCallbackType::CB_ERROR);

		    if (OnDataSynthesized.IsBound())
		    {
			    OnDataSynthesized.Execute(SessionID, PacketToSend);
		    }
	    }
	);
}

void Thespeon::Inference::ThespeonInference::PostCancelledPacket()
{
	AsyncTask(
	    ENamedThreads::GameThread,
	    [this]()
	    {
		    Thespeon::Core::FThespeonDataPacket PacketToSend(Thespeon::Core::SynthCallbackType::CB_ERROR);
		    // We should investigate some helper functions for this
		    Thespeon::Core::FPacketMetadataValue MetaVal;
		    MetaVal.Set<bool>(true);
		    PacketToSend.Metadata.Add(TEXT("was_cancelled"), MetaVal);

		    if (OnDataSynthesized.IsBound())
		    {
			    OnDataSynthesized.Execute(SessionID, PacketToSend);
		    }
	    }
	);
}

void Thespeon::Inference::ThespeonInference::Stop()
{
	// Call base class Stop first
	InferenceSession::Stop();

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("called for session: %s, Reason: %s"), *SessionID, LexToString(GetStopReason()));
}

void Thespeon::Inference::ThespeonInference::Exit()
{
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug, TEXT("called for session: %s, StopRequested: %s"), *SessionID, ShouldStop() ? TEXT("true") : TEXT("false")
	);

	// Call base class Exit
	InferenceSession::Exit();
}

bool Thespeon::Inference::ThespeonInference::PhonemizeBatch(
    const TArray<FString>& Words,
    Thespeon::Language::LanguageModule* LangModule,
    Thespeon::Language::RuntimeLookupTable* LookupTable,
    UInferenceWorkloadManager* InferenceWorkloadManager,
    const FInferenceConfig& Config
)
{
	if (!LangModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("LangModule is null"));
		return false;
	}
	if (!LookupTable)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("LookupTable is null"));
		return false;
	}

	const int32 BatchSize = Words.Num();
	if (BatchSize == 0)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("No words to phonemize"));
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
		GraphemeTokens.Add(2);       // EOS token from grapheme vocab

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

	const int32 StartToken = 1;        // SOS token from phoneme vocab
	const int32 EndToken = 2;          // EOS token from phoneme vocab
	const int32 MaxPhonemeLength = 50; // Maximum autoregressive iterations

	auto* PhonemizerWorkload = InferenceWorkloadManager->GetWorkload(LangModule->GetInternalModelID(TEXT("phonemizer")), InputConfig.BackendType);
	if (!PhonemizerWorkload)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get phonemizer workload"));
		return false;
	}

	// Initialize batch state
	TArray<int64> CurrentTgt;
	TArray<int64> CurrentMask;
	TArray<int64> FinishedIndices;
	TArray<TArray<int64>> UnknownPhonemeTokens;
	CurrentTgt.Init(StartToken, BatchSize); // [B]
	CurrentMask.Init(1, BatchSize);         // [B]
	FinishedIndices.Init(0, BatchSize);     // [B] - all words still active

	UnknownPhonemeTokens.SetNum(BatchSize);

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Starting phonemization with %d words, max src length %d"), BatchSize, MaxSrcLength);

	// Autoregressive loop: iteratively predicts the next phoneme token for each word in the batch.
	// Continues until all words have produced an EOS token or MaxPhonemeLength is reached.
	for (int32 Step = 0; Step < MaxPhonemeLength; ++Step)
	{
		const int32 CurrentTgtLength = CurrentTgt.Num() / BatchSize;

		// Prepare input tensors for this step
		TArray<FString> PhonemizerInputNames = {TEXT("src"), TEXT("tgt"), TEXT("mask_tensor"), TEXT("finished_indices")};
		TArray<UE::NNE::FTensorShape> PhonemizerInputShapes = {
		    UE::NNE::FTensorShape::Make({static_cast<uint32>(BatchSize), static_cast<uint32>(MaxSrcLength)}),
		    UE::NNE::FTensorShape::Make({static_cast<uint32>(BatchSize), static_cast<uint32>(CurrentTgtLength)}),
		    UE::NNE::FTensorShape::Make({static_cast<uint32>(BatchSize), static_cast<uint32>(CurrentTgtLength)}),
		    UE::NNE::FTensorShape::Make({static_cast<uint32>(BatchSize)})
		};

		// Set input tensors in pool
		TensorPool.SetTensor(PhonemizerInputNames[0], ModelIOData::MakeFromArray<int64>(PhonemizerInputShapes[0], FlatGraphemeTokens));
		TensorPool.SetTensor(PhonemizerInputNames[1], ModelIOData::MakeFromArray<int64>(PhonemizerInputShapes[1], CurrentTgt));
		TensorPool.SetTensor(PhonemizerInputNames[2], ModelIOData::MakeFromArray<int64>(PhonemizerInputShapes[2], CurrentMask));
		TensorPool.SetTensor(PhonemizerInputNames[3], ModelIOData::MakeFromArray<int64>(PhonemizerInputShapes[3], FinishedIndices));

		// Prepare output shapes - phonemizer outputs: new_tgt, new_mask, new_finished_indices, num_finished, next_word
		TArray<UE::NNE::FTensorShape> PhonemizerOutputShapes = {
		    UE::NNE::FTensorShape::Make({static_cast<uint32>(BatchSize), static_cast<uint32>(CurrentTgtLength + 1)}), // new_tgt
		    UE::NNE::FTensorShape::Make({static_cast<uint32>(BatchSize), static_cast<uint32>(CurrentTgtLength + 1)}), // new_mask
		    UE::NNE::FTensorShape::Make({static_cast<uint32>(BatchSize)}),                                            // new_finished_indices
		    UE::NNE::FTensorShape::Make({1}),                                                                         // num_finished (scalar)
		    UE::NNE::FTensorShape::Make({static_cast<uint32>(BatchSize)})                                             // next_word [B]
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
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get phonemizer new_tgt tensor"));
			return false;
		}
		if (!TensorPool.TryGetTensor(TEXT("new_mask"), NewMaskTensor))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get phonemizer new_mask tensor"));
			return false;
		}
		if (!TensorPool.TryGetTensor(TEXT("new_finished_indices"), NewFinishedIndicesTensor))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get phonemizer new_finished_indices tensor"));
			return false;
		}
		if (!TensorPool.TryGetTensor(TEXT("num_finished"), NumFinishedTensor))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get phonemizer num_finished tensor"));
			return false;
		}
		if (!TensorPool.TryGetTensor(TEXT("next_word"), NextWordTensor))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get phonemizer next_word tensor"));
			return false;
		}

		// Check batch completion via num_finished
		int64 NumFinished = NumFinishedTensor->GetDataAsValue<int64>(0);

		if (NumFinished >= BatchSize)
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Phonemization completed at step %d - all %d words finished"), Step, BatchSize);
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
					UnknownPhonemeTokens[b].Add(Token);
				}
			}
		}

		// Update state for next iteration
		CurrentTgt = NewTgtTensor->GetDataAsArray<int64>();
		CurrentMask = NewMaskTensor->GetDataAsArray<int64>();
		FinishedIndices = NewFinishedIndicesTensor->GetDataAsArray<int64>();
	}

	// Update dynamic lookup table with newly phonemized words
	for (int32 i = 0; i < BatchSize; ++i)
	{
		FString PhonemeString = LangModule->DecodePhonemes(UnknownPhonemeTokens[i]);
		LookupTable->AddOrUpdateDynamicEntry(Words[i], PhonemeString);
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Cached '%s' -> '%s' to dynamic lookup"), *Words[i], *PhonemeString);
	}
	return true;
}

bool Thespeon::Inference::ThespeonInference::PhonemizeSegment(
    FLingotionInputSegment Segment,
    Thespeon::Character::CharacterModule* CharacterModule,
    Thespeon::Language::RuntimeLookupTable* LookupTable,
    TArray<int64>& OutSegmentTokens,
    TArray<int64>& GlobalIndices,
    int& TextLengthSoFar
)
{
	FString CleanedText = Segment.Text;
	int32 TextPos = 0;
	int32 OutIndexPos = 0;
	while (TextPos < CleanedText.Len())
	{
		// Check if we're at the start of a word
		bool bIsWordChar = FChar::IsAlpha(CleanedText[TextPos]) || CleanedText[TextPos] == TEXT('\'');

		if (bIsWordChar)
		{
			// Extract the word
			int32 WordStart = TextPos;
			int LocalWordIndex = 0;
			int MarkerFoundAt = -1;
			int CurrentMarkerIndex = 0;
			while (TextPos < CleanedText.Len() && (FChar::IsAlpha(CleanedText[TextPos]) || CleanedText[TextPos] == TEXT('\'') ||
			                                       CleanedText[TextPos] == Thespeon::ControlCharacters::AudioSampleRequest))
			{
				if (CleanedText[TextPos] == Thespeon::ControlCharacters::AudioSampleRequest) // record and remove
				{
					CurrentMarkerIndex = OutIndexPos + TextLengthSoFar - LocalWordIndex;
					MarkerFoundAt = LocalWordIndex;
					CleanedText.RemoveAt(TextPos);
				}
				else
				{
					TextPos++;
					OutIndexPos++;
				}
				LocalWordIndex++;
			}

			FString Word = CleanedText.Mid(WordStart, TextPos - WordStart);

			// Get phonemes from lookup table (should always exist now)
			FString Phonemes;
			if (!LookupTable->TryGetValue(Word, Phonemes))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Word '%s' not found in lookup after processing!"), *Word);
				return false;
			}
			float WordLengtheningFactor = static_cast<float>(Phonemes.Len()) / static_cast<float>(Word.Len());
			if (MarkerFoundAt != -1)
			{
				GlobalIndices.Add(CurrentMarkerIndex + FMath::RoundToInt(MarkerFoundAt * WordLengtheningFactor));
			}
			// Encode phonemes to encoder IDs via CharacterModule
			TArray<int64> EncoderTokens = CharacterModule->EncodePhonemes(Phonemes);
			OutSegmentTokens.Append(EncoderTokens);
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Word '%s' -> phonemes '%s' (%d tokens)"), *Word, *Phonemes, EncoderTokens.Num());
			OutIndexPos += Phonemes.Len() - Word.Len(); // adjust for phoneme length difference
		}
		else
		{
			if (CleanedText[TextPos] == Thespeon::ControlCharacters::AudioSampleRequest)
			{
				GlobalIndices.Add(OutIndexPos + TextLengthSoFar);
				CleanedText.RemoveAt(TextPos);
			}
			else
			{
				// It's a delimiter (space, punctuation, etc.) - encode directly
				FString Delimiter = FString::Chr(CleanedText[TextPos]);
				TArray<int64> DelimiterTokens = CharacterModule->EncodePhonemes(Delimiter);

				if (DelimiterTokens.Num() > 0)
				{
					OutSegmentTokens.Append(DelimiterTokens);
					LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Delimiter '%s' -> %d tokens"), *Delimiter, DelimiterTokens.Num());
				}
				TextPos++;
				OutIndexPos++;
			}
		}
	}
	TextLengthSoFar += OutIndexPos;
	return true;
}

TArray<FString> Thespeon::Inference::ThespeonInference::GetUnknownWords(
    const FString& Text, Thespeon::Language::LanguageModule* LangModule, Thespeon::Language::RuntimeLookupTable* LookupTable
)
{
	TArray<FString> OutUnknownWords;
	TArray<FString> AllWords = FTextPreprocessor::ExtractWords(Text);
	if (AllWords.Num() == 0)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("No words extracted from text"));
		return TArray<FString>();
	}

	for (const FString& Word : AllWords)
	{
		FString Phonemes;
		if (!LookupTable->TryGetValue(Word, Phonemes))
		{
			OutUnknownWords.Add(Word);
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Word '%s' NOT in lookup - will phonemize"), *Word);
		}
	}
	return OutUnknownWords;
}

// Main inference pipeline. Runs on a background thread (FRunnable::Run).
// Pipeline: preload models -> validate input -> collect unknown words across all segments ->
// batch-phonemize unknowns per language -> encode all segments to encoder tokens ->
// build input tensors -> load and execute the MetaGraph (which handles encoder/decoder/vocoder).
// Cooperative cancellation is checked at key points via ShouldStop().
uint32 Thespeon::Inference::ThespeonInference::Run()
{
	// Early exit if stop was requested before we even started
	if (ShouldStop())
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Stop requested before execution started for session: %s"), *SessionID);
		PostCancelledPacket();
		return false;
	}

	// Preload character to ensure models are loaded
	const EThespeonModuleType EffectiveModuleType = Input.ModuleType != EThespeonModuleType::None ? Input.ModuleType : InputConfig.ModuleType;
	if (!TryPreloadCharacter(Input.CharacterName, EffectiveModuleType, InputConfig.BackendType))
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Failed to preload character: %s, ModuleType: %s. Please ensure you have imported this character."),
		    *Input.CharacterName,
		    *UEnum::GetValueAsString(Input.ModuleType)
		);
		PostErrorPacket();
		return false;
	}
	if (!Input.ValidateAndPopulate(InputConfig.ModuleType, InputConfig.FallbackLanguage, InputConfig.FallbackEmotion))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Input validation failed. See earlier logs for details."));
		PostErrorPacket();
		return false;
	}
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug, TEXT("ThespeonInference.Run called with SessionID: %s and validated input:\n%s"), *SessionID, *Input.ToJson()
	);

	// Check for stop after preloading (which can take time)
	if (ShouldStop())
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Stop requested after preload for session: %s"), *SessionID);
		PostCancelledPacket();
		return false;
	}

	auto* Manifest = UManifestHandler::Get();
	if (!Manifest)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ManifestHandler instance is null"));
		PostErrorPacket();
		return false;
	}
	Thespeon::Core::FModuleEntry Entry = Manifest->GetCharacterModuleEntry(Input.CharacterName, Input.ModuleType);

	auto* ModuleManager = UModuleManager::Get();
	if (!ModuleManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ModuleManager instance is null"));
		PostErrorPacket();
		return false;
	}

	UInferenceWorkloadManager* InferenceWorkloadManager = nullptr;
	ULookupTableManager* LookupTableManager = nullptr;
	if (!FLingotionThespeonSubsystemUtils::GetSubsystems(InferenceWorkloadManager, LookupTableManager))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get subsystems from GameInstance."));
		PostErrorPacket();
		return false;
	}

	if (!InferenceWorkloadManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("InferenceWorkloadManager subsystem not found in GameInstance."));
		PostErrorPacket();
		return false;
	}
	if (!LookupTableManager)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("LookupTableManager subsystem not found in GameInstance."));
		PostErrorPacket();
		return false;
	}

	using namespace Thespeon::Inference;

	Thespeon::Character::CharacterModule* CharacterModule = ModuleManager->GetModule<Thespeon::Character::CharacterModule>(Entry);

	if (!CharacterModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get CharacterModule for character: %s"), *Input.CharacterName);
		PostErrorPacket();
		return false;
	}
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug,
	    TEXT("Got CharacterModule: %s, %s, %s"),
	    *(CharacterModule->ModuleID),
	    *(CharacterModule->JSONPath),
	    *(CharacterModule->Version.ToString())
	);

	// Phase 1: Collect unknown words across all segments, grouped by language.
	// Each language gets its own batch for the phonemizer model.
	bool bIsFirstSegment = true;
	EEmotion lastEmotion = EEmotion::None;
	int64 lastLanguageKey = -1;
	TMap<FString, TArray<FString>> UnknownWordsByISO;
	TMap<FString, Thespeon::Language::LanguageModule*> LanguageModulesCache;
	TMap<FString, Thespeon::Language::RuntimeLookupTable*> LookupTablesCache;
	for (const FLingotionInputSegment& Segment : Input.Segments)
	{
		if (Segment.bIsCustomPronounced)
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Skipping custom pronounced segment: %s"), *Segment.Text);
			continue; // Skip custom pronounced segments
		}
		FString TargetISO639_2 = Segment.Language.ISO639_2;
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Processing segment with target language: %s"), *TargetISO639_2);
		Thespeon::Language::LanguageModule* LangModule = nullptr;
		if (!LanguageModulesCache.Contains(TargetISO639_2)) // Cache LM per language
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("LanguageModule for ISO %s not in cache, loading..."), *TargetISO639_2);
			FString* LangModuleID = CharacterModule->LanguageModuleIDs.Find(TargetISO639_2);
			if (!LangModuleID)
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Character module has no language: %s"), *TargetISO639_2);
				PostErrorPacket();
				return false;
			}
			Thespeon::Core::FModuleEntry LangEntry = Manifest->GetLanguageModuleEntry(*LangModuleID);
			LangModule = ModuleManager->GetModule<Thespeon::Language::LanguageModule>(LangEntry);
			if (!LangModule)
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get LanguageModule"));
				PostErrorPacket();
				return false;
			}
			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug,
			    TEXT("Got LanguageModule: %s, %s, %s"),
			    *(LangModule->ModuleID),
			    *(LangModule->JSONPath),
			    *(LangModule->Version.ToString())
			);
			LanguageModulesCache.Add(TargetISO639_2, LangModule);
		}

		LangModule = LanguageModulesCache[TargetISO639_2];
		if (!LookupTablesCache.Contains(TargetISO639_2)) // Cache LM per language
		{
			FString LookupTableMD5 = LangModule->GetLookupTableID();
			Thespeon::Language::RuntimeLookupTable* LookupTable = LookupTableManager->GetLookupTable(LookupTableMD5);
			if (!LookupTable)
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get lookup table for MD5: %s"), *LookupTableMD5);
				PostErrorPacket();
				return false;
			}
			LookupTablesCache.Add(TargetISO639_2, LookupTable);
		}
		FString StrippedText = Segment.Text.Replace(*FString(1, &Thespeon::ControlCharacters::AudioSampleRequest), TEXT(""), ESearchCase::IgnoreCase);
		TArray<FString> UnknownWords = GetUnknownWords(StrippedText, LangModule, LookupTablesCache[TargetISO639_2]);
		if (UnknownWords.Num() > 0)
		{
			UnknownWordsByISO.FindOrAdd(TargetISO639_2).Append(MoveTemp(UnknownWords));
		}
	}

	// Phase 2: Run batched phonemization per language. Results are cached in lookup tables.
	for (const TPair<FString, TArray<FString>>& Pair : UnknownWordsByISO)
	{
		const FString& ISO639_2 = Pair.Key;
		const TArray<FString>& WordsToPhonemize = Pair.Value;
		PhonemizeBatch(WordsToPhonemize, LanguageModulesCache[ISO639_2], LookupTablesCache[ISO639_2], InferenceWorkloadManager, InputConfig);
	}

	// Phase 3: Build the final encoder token sequence from all segments.
	// SOS token (⏩) marks the start, EOS token (⏪) marks the end.
	// Each segment contributes encoded phoneme tokens plus per-token emotion and language IDs.
	TArray<int64> EncodedTxt;
	EncodedTxt.Append(CharacterModule->EncodePhonemes(TEXT("⏩")));
	TArray<int64> emotions;
	TArray<int64> languages;

	// Process segments to final encoder token sequence
	int TextLengthSoFar = 0;
	TArray<int64> AudioSampleRequestGlobalIndices;
	for (const FLingotionInputSegment& Segment : Input.Segments)
	{
		TArray<int64> SegmentTxtEncoded;
		if (Segment.bIsCustomPronounced)
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Encoding custom pronounced segment: %s"), *Segment.Text);
			// get indices of all audio sample requests in segment and remove them from text
			int32 TextPos = 0;
			FString SegmentTextCleaned = Segment.Text;
			while (TextPos < SegmentTextCleaned.Len())
			{
				if (SegmentTextCleaned[TextPos] == TEXT("'")[0]) // unreal has switched for some reason...
				{
					SegmentTextCleaned[TextPos] = TEXT("ˈ")[0]; // this is the IPA emphasis mark we use in lookuptable
				}
				if (SegmentTextCleaned[TextPos] == Thespeon::ControlCharacters::AudioSampleRequest)
				{
					AudioSampleRequestGlobalIndices.Add(TextLengthSoFar + TextPos);
					SegmentTextCleaned.RemoveAt(TextPos);
				}
				else
				{
					TextPos++;
				}
			}

			SegmentTxtEncoded = CharacterModule->EncodePhonemes(SegmentTextCleaned);
			TextLengthSoFar += SegmentTxtEncoded.Num();
		}
		else
		{
			FString TargetISO639_2 = Segment.Language.ISO639_2;
			Thespeon::Language::LanguageModule* LangModule = LanguageModulesCache[TargetISO639_2];
			if (!PhonemizeSegment(
			        Segment,
			        CharacterModule,
			        LookupTableManager->GetLookupTable(LangModule->GetLookupTableID()),
			        SegmentTxtEncoded,
			        AudioSampleRequestGlobalIndices,
			        TextLengthSoFar
			    ))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to process words with lookup table pipeline"));
				PostErrorPacket();
				return false;
			}
		}

		int64 LangKey = CharacterModule->GetLangKey(Segment.Language);
		if (LangKey == -1)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Character module does not have a valid LangKey for language: %s"), *Segment.Language.ToJson());
			PostErrorPacket();
			return false;
		}
		EncodedTxt.Append(SegmentTxtEncoded);
		TArray<int64> SegmentEmotions;
		TArray<int64> SegmentLanguages;
		int len = SegmentTxtEncoded.Num() + (bIsFirstSegment ? 1 : 0); // Account for SOS token in first segment
		bIsFirstSegment = false;
		SegmentEmotions.Init(static_cast<int64>(Segment.Emotion), len);
		SegmentLanguages.Init(LangKey, len);
		emotions.Append(SegmentEmotions);
		languages.Append(SegmentLanguages);
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug,
		    TEXT("Segment processed: %d encoder tokens, Emotion: %s, Language: %s"),
		    SegmentTxtEncoded.Num(),
		    *UEnum::GetValueAsString(Segment.Emotion),
		    *Segment.Language.ToJson()
		);
		lastEmotion = Segment.Emotion;
		lastLanguageKey = LangKey;
	}

	// EOS token
	EncodedTxt.Append(CharacterModule->EncodePhonemes(TEXT("⏪")));
	emotions.Add(static_cast<int64>(lastEmotion));
	languages.Add(lastLanguageKey);

	// Log complete encoder sequence
	FString TokensStr;
	for (int64 Token : EncodedTxt)
	{
		TokensStr += FString::Printf(TEXT("%lld "), Token);
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Complete encoder sequence (%d tokens): [%s]"), EncodedTxt.Num(), *TokensStr);

	TArray<int64> actors;
	TArray<float> speed;
	TArray<float> loudness;
	uint32 txt_len = EncodedTxt.Num();
	actors.Init(1, 1);
	speed.Init(1.0f, txt_len);
	loudness.Init(1.0f, txt_len);
	TArray<int64> text_lengths;
	text_lengths.Init(txt_len, 1);

	TArray<UE::NNE::FTensorShape> InputTensorShapes = {
	    UE::NNE::FTensorShape::Make({1, txt_len}),
	    UE::NNE::FTensorShape::Make({1, txt_len}),
	    UE::NNE::FTensorShape::Make({1}),
	};

	TArray<UE::NNE::FTensorShape> encoderOutputShapes = {
	    UE::NNE::FTensorShape::Make({1, 96, 2 * txt_len}),
	    UE::NNE::FTensorShape::Make({1, 1, 2 * txt_len}),
	    UE::NNE::FTensorShape::Make({1, 1, 2 * txt_len}),
	    UE::NNE::FTensorShape::Make({1, 2 * txt_len}),
	    UE::NNE::FTensorShape::Make({1, 1}),
	    UE::NNE::FTensorShape::Make({1, txt_len}),
	    UE::NNE::FTensorShape::Make({1}),
	};

	TArray<FString> InputNames = {TEXT("phoneme_keys"), TEXT("emotions"), TEXT("text_lengths")};

	// Set input tensors in pool
	TensorPool.SetTensor(InputNames[0], ModelIOData::MakeFromArray<int64>(InputTensorShapes[0], EncodedTxt));
	TensorPool.SetTensor(InputNames[1], ModelIOData::MakeFromArray<int64>(InputTensorShapes[1], emotions));
	TensorPool.SetTensor(InputNames[2], ModelIOData::MakeFromArray<int64>(InputTensorShapes[2], text_lengths));
	uint32 NumSampleRequestIndices = AudioSampleRequestGlobalIndices.Num();
	TensorPool.SetTensor(
	    TEXT("target_phoneme_indices"),
	    ModelIOData::MakeFromArray<int64>(UE::NNE::FTensorShape::Make({NumSampleRequestIndices}), AudioSampleRequestGlobalIndices)
	);

	// TODO: load the meta graph file detailed in the .json file instead
	metaonnx::MetaGraph graph;
	const Thespeon::Core::FModuleFile* MetagraphFile = CharacterModule->InternalFileMappings.Find(TEXT("metagraph"));
	if (!MetagraphFile)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Metagraph file not found in module mappings"));
		PostErrorPacket();
		return false;
	}
	FString MetagraphFileName = MetagraphFile->GetFullFileName();
	const bool bLoadedMetagraph = LoadModelFromDisk(Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeFilePath(MetagraphFileName), graph);
	if (!bLoadedMetagraph)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to load metagraph from disk: %s"), *MetagraphFileName);
		PostErrorPacket();
		return false;
	}

	TWeakPtr<TAtomic<bool>> WeakAlive(AliveToken);
	FMetaGraphRunner runner(
	    TensorPool,
	    CharacterModule,
	    InputConfig,
	    Thespeon::Inference::FPostPacketFn{[this, WeakAlive](const Thespeon::Core::FThespeonDataPacket& PacketToSend)
	                                       {
		                                       AsyncTask(
		                                           ENamedThreads::GameThread,
		                                           [this, WeakAlive, PacketToSend]()
		                                           {
			                                           // Pin the weak pointer - fails if InferenceSession was destroyed
			                                           TSharedPtr<TAtomic<bool>> StrongAlive = WeakAlive.Pin();
			                                           if (!StrongAlive || !StrongAlive->Load())
			                                           {
				                                           return; // Object destroyed during shutdown, bail safely
			                                           }

			                                           if (OnDataSynthesized.IsBound())
			                                           {
				                                           OnDataSynthesized.Execute(SessionID, PacketToSend);
			                                           }
		                                           }
		                                       );
	                                       }},
	    [this, WeakAlive]()
	    {
		    TSharedPtr<TAtomic<bool>> StrongAlive = WeakAlive.Pin();
		    // Treat a destroyed owner as "stop requested"
		    return !StrongAlive || !StrongAlive->Load() || ShouldStop();
	    }

	);
	TArray<float> outvals;
	return runner.Run(graph, outvals);
}

// Static method: Loads all ONNX models for a character and its supported languages into the workload manager.
// Steps: 1) Find character in manifest, 2) Get/create CharacterModule, 3) For each supported language:
//   load LanguageModule, register its workloads, register its lookup table, 4) Register character workloads.
bool Thespeon::Inference::ThespeonInference::TryPreloadCharacter(
    FString CharacterName, EThespeonModuleType ModuleType, const EBackendType BackendType
)
{
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug, TEXT("called with CharacterName: %s, ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType)
	);
	auto* Manifest = UManifestHandler::Get();
	if (!Manifest)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ManifestHandler instance is null"));
		return false;
	}
	Thespeon::Core::FModuleEntry Entry = Manifest->GetCharacterModuleEntry(CharacterName, ModuleType);
	if (Entry.ModuleID.IsEmpty())
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Character '%s' of module type '%s' has not been imported and cannot be preloaded. "
		         "Check the Lingotion Thespeon Info window (Window > Lingotion Thespeon Info) to see your imported character modules."),
		    *CharacterName,
		    *UEnum::GetValueAsString(ModuleType)
		);
		return false;
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Got Character Entry: %s, %s, %s"), *Entry.ModuleID, *Entry.JsonPath, *Entry.Version.ToString());

	// 2. If result, Get Module from ModuleManager
	auto* ModuleManager = UModuleManager::Get();
	if (!ModuleManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ModuleManager instance is null"));
		return false;
	}

	// 3. Get InferenceWorkloadManager from GameInstanceSubsystem
	UInferenceWorkloadManager* InferenceWorkloadManager = FLingotionThespeonSubsystemUtils::GetInferenceWorkloadManager();
	if (!InferenceWorkloadManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("InferenceWorkloadManager subsystem not found in GameInstance."));
		return false;
	}

	// 4. If result, Get CharacterModule from ModuleManager
	Thespeon::Character::CharacterModule* CharacterModule = ModuleManager->GetModule<Thespeon::Character::CharacterModule>(Entry);
	if (!CharacterModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get CharacterModule"));
		return false; // or handle error appropriately
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Got Character Module: %s"), *CharacterModule->ModuleID);

	// 5. For each necessary language module find in manifest handler
	int foundLanguageModules = 0;
	for (const auto& kvp : CharacterModule->LanguageModuleIDs)
	{
		// If result, for each necessary language module find in manifest handler
		Thespeon::Core::FModuleEntry LangEntry = Manifest->GetLanguageModuleEntry(kvp.Value);
		if (LangEntry.ModuleID.IsEmpty())
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("No language module entry found for ModuleName: %s"), *kvp.Value);
			continue;
		}
		foundLanguageModules++;
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug, TEXT("Got Language Entry: %s, %s, %s"), *LangEntry.ModuleID, *LangEntry.JsonPath, *LangEntry.Version.ToString()
		);
		// 6. If result, register language module in InferenceWorkloadManager
		Thespeon::Language::LanguageModule* LangModule = ModuleManager->GetModule<Thespeon::Language::LanguageModule>(LangEntry);
		if (!LangModule)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get LanguageModule"));
			return false;
		}
		LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Registering workload for Language Module: %s"), *(LangModule->ModuleID));
		if (!InferenceWorkloadManager->RegisterModuleWorkload(LangModule, BackendType))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to register LanguageModule workload: %s"), *(LangModule->ModuleID));
			return false;
		}

		// 7. Register lookup table in LookupTableManager
		ULookupTableManager* LookupTableManager = FLingotionThespeonSubsystemUtils::GetLookupTableManager();
		if (LookupTableManager)
		{
			LookupTableManager->RegisterLookupTable(LangModule);
			LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Successfully registered lookup table for language module: %s"), *(LangModule->ModuleID));
		}
		else
		{
			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug, TEXT("LookupTableManager subsystem not found in GameInstance. Lookup tables will not be available.")
			);
		}
	}
	if (foundLanguageModules == 0)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error, TEXT("None of the supported languages for character module was imported. Make sure to import at least one.")
		);
		return false;
	}

	// Register character module in InferenceWorkloadManager when all associated language modules are registered.
	if (!InferenceWorkloadManager->RegisterModuleWorkload(CharacterModule, BackendType))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to register CharacterModule workload: %s"), *(CharacterModule->ModuleID));
		return false;
	}
	return true;
}

// Static method: Unloads a character's ONNX models and its non-shared language modules from the workload manager.
// Only unloads language modules that are not used by any other loaded character.
bool Thespeon::Inference::ThespeonInference::TryUnloadCharacter(
    const FString& CharacterName, const EThespeonModuleType& ModuleType, EBackendType BackendType
)
{
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug, TEXT("called with CharacterName: %s, ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType)
	);
	auto* Manifest = UManifestHandler::Get();
	if (!Manifest)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ManifestHandler instance is null"));
		return false;
	}
	Thespeon::Core::FModuleEntry Entry = Manifest->GetCharacterModuleEntry(CharacterName, ModuleType);
	if (Entry.ModuleID.IsEmpty())
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Character '%s' of module type '%s' has not been imported nor loaded and cannot be unloaded. "
		         "Check the Lingotion Thespeon Info window (Window > Lingotion Thespeon Info) to see your imported character modules."),
		    *CharacterName,
		    *UEnum::GetValueAsString(ModuleType)
		);
		return false;
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Got Character Entry: %s, %s, %s"), *Entry.ModuleID, *Entry.JsonPath, *Entry.Version.ToString());

	// 2. If result, Get Module from ModuleManager
	auto* ModuleManager = UModuleManager::Get();
	if (!ModuleManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ModuleManager instance is null"));
		return false;
	}

	// 3. Get InferenceWorkloadManager from GameInstanceSubsystem
	UInferenceWorkloadManager* InferenceWorkloadManager = FLingotionThespeonSubsystemUtils::GetInferenceWorkloadManager();
	if (!InferenceWorkloadManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("InferenceWorkloadManager subsystem not found in GameInstance."));
		return false;
	}

	// 4. If result, Get CharacterModule from ModuleManager
	Thespeon::Character::CharacterModule* CharacterModule = ModuleManager->GetModule<Thespeon::Character::CharacterModule>(Entry, false);
	if (!CharacterModule)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get CharacterModule"));
		return false; // or handle error appropriately
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Got Character Module: %s"), *CharacterModule->ModuleID);

	// 5. Remove all language modules for character module
	ULookupTableManager* LookupTableManager = FLingotionThespeonSubsystemUtils::GetLookupTableManager();
	if (!LookupTableManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("LookupTableManager subsystem not found in GameInstance."));
		return false;
	}

	TSet<FString> langModulesToRemove = ModuleManager->GetNonOverlappingModelLangModules(CharacterModule);
	for (const FString& langModule : langModulesToRemove)
	{
		Entry = Manifest->GetLanguageModuleEntry(langModule);
		if (Entry.ModuleID.IsEmpty())
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("No module entry found for module: %s"), *langModule);
			return false;
		}

		Thespeon::Language::LanguageModule* currentLangModule = ModuleManager->GetModule<Thespeon::Language::LanguageModule>(Entry, false);
		if (!currentLangModule)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get Language module %s"), *Entry.ModuleID);
			return false;
		}

		if (!InferenceWorkloadManager->TryDeregisterModuleWorkloads(currentLangModule, BackendType))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to deregister workloads of module %s"), *Entry.ModuleID);
			return false;
		}
		if (!LookupTableManager->TryDeregisterTable(currentLangModule))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to deregister tables of module %s"), *Entry.ModuleID);
			return false;
		}

		if (!ModuleManager->TryDeregisterModule(langModule))
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Did not deregister language module %s"), *Entry.ModuleID);
		}
	}

	// 6. Remove character module itself
	if (!InferenceWorkloadManager->TryDeregisterModuleWorkloads(CharacterModule, BackendType))
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Failed to deregister workloads for CharacterName: %s, ModuleType: %s"),
		    *CharacterName,
		    *UEnum::GetValueAsString(ModuleType)
		);
		return false;
	}
	if (!ModuleManager->TryDeregisterModule(CharacterModule->ModuleID))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Did not deregister character module %s"), *(CharacterModule->ModuleID));
	}

	return true;
}