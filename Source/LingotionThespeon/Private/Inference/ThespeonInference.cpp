// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

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
#include "PreloadSession.h"
#include "Language/TextPreprocessor.h"
#include "Language/TextPreprocessingRules.h"
#include "Language/UnicodeTables.h"
#include <type_traits>
#include "Core/meta_graph.pb.h"
#include "MetaGraphRunner.h"
#include "SessionWorkloadCache.h"
#include "Inference/ThespeonEditorSignals.h"
#include "Async/Async.h"

void Thespeon::Inference::ThespeonInference::PostErrorPacket()
{
	TWeakPtr<std::atomic<bool>> WeakAlive(AliveToken);
	AsyncTask(
	    ENamedThreads::GameThread,
	    [this, WeakAlive]()
	    {
		    TSharedPtr<std::atomic<bool>> StrongAlive = WeakAlive.Pin();
		    if (!StrongAlive || !StrongAlive->load())
		    {
			    return;
		    }

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
	TWeakPtr<std::atomic<bool>> WeakAlive(AliveToken);
	AsyncTask(
	    ENamedThreads::GameThread,
	    [this, WeakAlive]()
	    {
		    TSharedPtr<std::atomic<bool>> StrongAlive = WeakAlive.Pin();
		    if (!StrongAlive || !StrongAlive->load())
		    {
			    return;
		    }

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
    Thespeon::Inference::FSessionWorkloadCache* WorkloadCache,
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
	LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Phonemizing batch of %d words"), BatchSize);

	const int32 StartToken = 1;
	const int32 EndToken = 2;

	// Encode all words to grapheme tokens
	TArray<TArray<int64>> BatchGraphemeTokens;
	BatchGraphemeTokens.Reserve(BatchSize);
	int32 MaxSrcLength = 0;

	for (const FString& Word : Words)
	{
		TArray<int64> GraphemeTokens = LangModule->EncodeGraphemes(Word);

		// Add SOS and EOS tokens
		GraphemeTokens.Insert(StartToken, 0);
		GraphemeTokens.Add(EndToken);

		MaxSrcLength = FMath::Max(MaxSrcLength, GraphemeTokens.Num());
		BatchGraphemeTokens.Add(MoveTemp(GraphemeTokens));
	}

	// Pad all grapheme sequences to MaxSrcLength (padding token = 0)
	TArray<int64> FlatGraphemeTokens;
	FlatGraphemeTokens.Reserve(BatchSize * MaxSrcLength);
	for (TArray<int64>& Tokens : BatchGraphemeTokens)
	{
		Tokens.SetNumZeroed(MaxSrcLength);
		FlatGraphemeTokens.Append(Tokens);
	}

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Starting phonemization with %d words, max src length %d"), BatchSize, MaxSrcLength);

	const UE::NNE::FTensorShape SrcShape = UE::NNE::FTensorShape::Make({static_cast<uint32>(BatchSize), static_cast<uint32>(MaxSrcLength)});
	TensorPool.SetTensor(TEXT("src"), ModelIOData::MakeFromArray<int64>(SrcShape, FlatGraphemeTokens));

	// Parsed once per module and shared by every session — the graph is read-only during Run().
	TSharedPtr<const metaonnx::MetaGraph, ESPMode::ThreadSafe> PhonemizerGraph = LangModule->GetMetaGraph();
	if (!PhonemizerGraph.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Could not get phonemizer metagraph for language module '%s'"), *LangModule->ModuleID);
		return false;
	}

	// The graph emits no external data, so eventual packets are logged and dropped
	FMetaGraphRunner PhonemizerRunner(
	    TensorPool,
	    LangModule,
	    WorkloadCache,
	    Config,
	    Thespeon::Inference::FPostPacketFn{[](const Thespeon::Core::FThespeonDataPacket& Packet) {
		    LINGO_LOG(
		        EVerbosityLevel::Debug, TEXT("Phonemizer graph emitted a packet of type %d; dropping it."), static_cast<int32>(Packet.CallbackType)
		    );
	    }},
	    [this]() { return ShouldStop(); }
	);

	if (!PhonemizerRunner.Run(*PhonemizerGraph))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Phonemizer metagraph failed to run, aborting phonemization."));
		return false;
	}

	const TCHAR* FinalTgtName = TEXT("tgt.in");
	const TCHAR* FinalFinishedName = TEXT("finished_indices.in");

	ModelIOData* FinalTgtTensor = nullptr;
	if (!TensorPool.TryGetTensor(FinalTgtName, FinalTgtTensor) || !FinalTgtTensor)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get final '%s' tensor from phonemizer graph"), FinalTgtName);
		return false;
	}
	TArray<int64> PhonemeIndices = FinalTgtTensor->GetDataAsArray<int64>();

	ModelIOData* FinishedIndicesTensor = nullptr;
	if (!TensorPool.TryGetTensor(FinalFinishedName, FinishedIndicesTensor) || !FinishedIndicesTensor)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get final '%s' tensor from phonemizer graph"), FinalFinishedName);
		return false;
	}
	TArray<int64> FinalFinishedIndices = FinishedIndicesTensor->GetDataAsArray<int64>();

	if (PhonemeIndices.Num() < BatchSize || FinalFinishedIndices.Num() < BatchSize)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Phonemizer graph returned undersized output for a batch of %d (tgt=%d, finished_indices=%d)"),
		    BatchSize,
		    PhonemeIndices.Num(),
		    FinalFinishedIndices.Num()
		);
		return false;
	}

	int32 Stride = PhonemeIndices.Num() / BatchSize;
	// Update dynamic lookup table with newly phonemized words
	for (int32 j = 0; j < BatchSize; ++j)
	{
		if (FinalFinishedIndices[j] <= 0)
		{
			LINGO_LOG(EVerbosityLevel::Warning, TEXT("Phonemizer did not produce result for word '%s'. Using word as-is."), *Words[j]);
			// Add the word itself as fallback to prevent downstream errors
			LookupTable->AddOrUpdateDynamicEntry(Words[j], Words[j]);
			continue;
		}

		int32 Start = j * Stride;
		int32 End = Start + static_cast<int32>(FinalFinishedIndices[j]);

		// Bounds check to prevent invalid range access
		if (End <= Start + 1 || End > Start + Stride)
		{
			LINGO_LOG(
			    EVerbosityLevel::Warning,
			    TEXT("Word was phonemized but with incorrect range for '%s' (start=%d, end=%d, stride=%d, length=%d). Using word as-is."),
			    *Words[j],
			    Start,
			    End,
			    Stride,
			    PhonemeIndices.Num()
			);
			// Add the word itself as fallback to prevent downstream errors
			LookupTable->AddOrUpdateDynamicEntry(Words[j], Words[j]);
			continue;
		}

		// Extract phoneme tokens for this word (skip SOS token at start+1, go to end)
		TArray<int64> WordPhonemeTokens;
		WordPhonemeTokens.Reserve(End - Start - 1);
		for (int32 k = Start + 1; k < End; ++k)
		{
			WordPhonemeTokens.Add(PhonemeIndices[k]);
		}

		FString PhonemeString = LangModule->DecodePhonemes(WordPhonemeTokens);
		LookupTable->AddOrUpdateDynamicEntry(Words[j], PhonemeString);
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Cached '%s' -> '%s' to dynamic lookup"), *Words[j], *PhonemeString);
	}
	return true;
}

bool Thespeon::Inference::ThespeonInference::PhonemizeSegment(
    const FLingotionInputSegment& Segment,
    const Thespeon::Language::FWordSplitter& Words,
    Thespeon::Character::CharacterModule* CharacterModule,
    Thespeon::Language::RuntimeLookupTable* LookupTable,
    TArray<int64>& OutSegmentTokens,
    TArray<int64>& GlobalIndices,
    int& TextLengthSoFar
)
{
	// Audio sample request markers are not word characters, so they are taken out before the words are found
	// (a marker inside a word would split it), and each is placed by where it was in the text.
	FString Text;
	Text.Reserve(Segment.Text.Len());
	TArray<int32> MarkerPositions;
	for (const TCHAR C : Segment.Text)
	{
		if (C == Thespeon::ControlCharacters::AudioSampleRequest)
		{
			MarkerPositions.Add(Text.Len());
		}
		else
		{
			Text.AppendChar(C);
		}
	}
	const TArray<Thespeon::Language::FWord> SegmentWords =
	    Words.Split(Text, [LookupTable](const FString& Word) { return LookupTable->ContainsKey(Word); });

	// The segment with its words replaced by their phonemes, for the debug log.
	FString Phonemized;
	// Marker positions are counted in encoder tokens, the unit of target_phoneme_indices.
	int32 SegmentTokenCount = 0;
	int32 MarkerIndex = 0;
	auto AddMarkersUpTo = [&](int32 Position)
	{
		while (MarkerIndex < MarkerPositions.Num() && MarkerPositions[MarkerIndex] <= Position)
		{
			GlobalIndices.Add(TextLengthSoFar + SegmentTokenCount);
			++MarkerIndex;
		}
	};

	int32 Position = 0;
	int32 WordIndex = 0;
	while (Position < Text.Len())
	{
		if (WordIndex < SegmentWords.Num() && SegmentWords[WordIndex].Index == Position)
		{
			const FString& Word = SegmentWords[WordIndex].Text;
			AddMarkersUpTo(Position);

			// Get phonemes from lookup table (should always exist now)
			FString Phonemes;
			if (!LookupTable->TryGetValue(Word, Phonemes))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Word '%s' not found in lookup after processing!"), *Word);
				return false;
			}
			// Encode phonemes to encoder IDs via CharacterModule
			TArray<int64> EncoderTokens = CharacterModule->EncodePhonemes(Phonemes);

			// A marker inside the word goes the same fraction of the way into its phonemes.
			const int32 WordEnd = Position + Word.Len();
			while (MarkerIndex < MarkerPositions.Num() && MarkerPositions[MarkerIndex] <= WordEnd)
			{
				const float Fraction = static_cast<float>(MarkerPositions[MarkerIndex] - Position) / static_cast<float>(Word.Len());
				GlobalIndices.Add(TextLengthSoFar + SegmentTokenCount + FMath::RoundToInt(Fraction * EncoderTokens.Num()));
				++MarkerIndex;
			}

			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Word '%s' -> phonemes '%s' (%d tokens)"), *Word, *Phonemes, EncoderTokens.Num());
			SegmentTokenCount += EncoderTokens.Num();
			OutSegmentTokens.Append(MoveTemp(EncoderTokens));
			Phonemized += Phonemes;
			Position = WordEnd;
			++WordIndex;
		}
		else
		{
			AddMarkersUpTo(Position);
			// It's a delimiter (space, punctuation, etc.) - encode directly, a whole code point at a time
			const int32 Length = Thespeon::Language::CodePoints::LengthAt(Text, Position);
			const FString Delimiter = Text.Mid(Position, Length);
			Phonemized += Delimiter;
			TArray<int64> DelimiterTokens = CharacterModule->EncodePhonemes(Delimiter);
			if (DelimiterTokens.Num() > 0)
			{
				LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Delimiter '%s' -> %d tokens"), *Delimiter, DelimiterTokens.Num());
				SegmentTokenCount += DelimiterTokens.Num();
				OutSegmentTokens.Append(MoveTemp(DelimiterTokens));
			}
			Position += Length;
		}
	}
	AddMarkersUpTo(MAX_int32);
	TextLengthSoFar += SegmentTokenCount;
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Phonemized segment: %s"), *Phonemized);
	return true;
}

TArray<FString> Thespeon::Inference::ThespeonInference::GetUnknownWords(
    const FString& Text, const Thespeon::Language::FWordSplitter& Words, Thespeon::Language::RuntimeLookupTable* LookupTable
)
{
	TArray<FString> OutUnknownWords;
	for (const Thespeon::Language::FWord& Word :
	     Words.Split(Text, [LookupTable](const FString& Candidate) { return LookupTable->ContainsKey(Candidate); }))
	{
		if (!LookupTable->ContainsKey(Word.Text))
		{
			OutUnknownWords.AddUnique(Word.Text);
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Word '%s' NOT in lookup - will phonemize"), *Word.Text);
		}
	}
	return OutUnknownWords;
}

bool Thespeon::Inference::ThespeonInference::ExecuteInference()
{
	// Early exit if stop was requested before we even started.
	// Return true so Run() routes to PostCancelledPacket(), not PostErrorPacket().
	if (ShouldStop())
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Stop requested before execution started for session: %s"), *SessionID);
		return true;
	}

	// Preload character to ensure models are loaded
	const EThespeonModuleType EffectiveModuleType = Input.ModuleType != EThespeonModuleType::None ? Input.ModuleType : InputConfig.ModuleType;
	// High priority: any model loads triggered by an inline synth-time preload must jump ahead of
	// background preload requests already queued in FStreamableManager (value matches
	// FStreamableManager::AsyncLoadHighPriority).
	constexpr int32 SynthLoadPriority = 100;
	Thespeon::Inference::FPreloadSession preload = Thespeon::Inference::FPreloadSession(
	    Input.CharacterName,
	    EffectiveModuleType,
	    InputConfig.BackendType,
	    InputConfig.bForceRequestedBackend,
	    InferenceWorkloadManager,
	    ModuleManager,
	    LookupTableManager,
	    ManifestHandler,
	    nullptr,
	    SynthLoadPriority
	);
	preload.Init();
	preload.Run();
	if (!preload.WasSuccessful())
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Failed to preload character: %s, ModuleType: %s. Please ensure you have imported this character."),
		    *Input.CharacterName,
		    *UEnum::GetValueAsString(Input.ModuleType)
		);
		return false;
	}
	// Keypoints are populated after text preprocessing, below, since it can split segments and change their length.
	if (!Input.Validate(InputConfig.ModuleType, InputConfig.FallbackLanguage, InputConfig.FallbackEmotion))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Input validation failed. See earlier logs for details."));
		return false;
	}
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug, TEXT("ThespeonInference.Run called with SessionID: %s and validated input:\n%s"), *SessionID, *Input.ToJson()
	);

	// Check for stop after preloading (which can take time).
	// Return true so Run() routes to PostCancelledPacket(), not PostErrorPacket().
	if (ShouldStop())
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Stop requested after preload for session: %s"), *SessionID);
		return true;
	}

	// Validate subsystem pointers (captured on game thread at construction)
	if (!ManifestHandler || !ModuleManager || !InferenceWorkloadManager || !LookupTableManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("One or more subsystem pointers are null. Session was not properly constructed."));
		return false;
	}

	// Session-scoped workload cache: acquires exclusive workload instances from the pool on first
	// access per model, caches them for the session duration, and releases all back to the pool
	// when this scope exits (RAII). This ensures each concurrent session gets its own ModelInstance.
	Thespeon::Inference::FSessionWorkloadCache WorkloadCache(InferenceWorkloadManager, InputConfig.BackendType);

	Thespeon::Core::FModuleEntry Entry = ManifestHandler->GetCharacterModuleEntry(Input.CharacterName, Input.ModuleType);

	using namespace Thespeon::Inference;

	// TSharedPtr keeps module alive for this scope; raw pointer used for downstream API compatibility
	TSharedPtr<Thespeon::Character::CharacterModule, ESPMode::ThreadSafe> CharacterModulePtr =
	    ModuleManager->GetModule<Thespeon::Character::CharacterModule>(Entry);

	if (!CharacterModulePtr)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get CharacterModule for character: %s"), *Input.CharacterName);
		return false;
	}
	Thespeon::Character::CharacterModule* CharacterModule = CharacterModulePtr.Get();
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug,
	    TEXT("Got CharacterModule: %s, %s, %s"),
	    *(CharacterModule->ModuleID),
	    *(CharacterModule->JSONPath),
	    *(CharacterModule->Version.ToString())
	);

	// Language modules and lookup tables, resolved once per language for this session.
	TMap<FString, TSharedPtr<Thespeon::Language::LanguageModule, ESPMode::ThreadSafe>> LanguageModules;
	TMap<FString, TSharedPtr<Thespeon::Language::RuntimeLookupTable, ESPMode::ThreadSafe>> LookupTables;
	auto ResolveLanguageModule = [&](const FString& TargetISO639_2) -> Thespeon::Language::LanguageModule*
	{
		if (const TSharedPtr<Thespeon::Language::LanguageModule, ESPMode::ThreadSafe>* Cached = LanguageModules.Find(TargetISO639_2))
		{
			return Cached->Get();
		}
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("LanguageModule for ISO %s not in cache, loading..."), *TargetISO639_2);
		FString* LangModuleID = CharacterModule->LanguageModuleIDs.Find(TargetISO639_2);
		if (!LangModuleID)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Character module has no language: %s"), *TargetISO639_2);
			return nullptr;
		}
		Thespeon::Core::FModuleEntry LangEntry = ManifestHandler->GetLanguageModuleEntry(*LangModuleID);
		TSharedPtr<Thespeon::Language::LanguageModule, ESPMode::ThreadSafe> LangModulePtr =
		    ModuleManager->GetModule<Thespeon::Language::LanguageModule>(LangEntry);
		if (!LangModulePtr)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get LanguageModule"));
			return nullptr;
		}
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug,
		    TEXT("Got LanguageModule: %s, %s, %s"),
		    *(LangModulePtr->ModuleID),
		    *(LangModulePtr->JSONPath),
		    *(LangModulePtr->Version.ToString())
		);
		return LanguageModules.Add(TargetISO639_2, LangModulePtr).Get();
	};

	// Text preprocessing, by the rules of each segment's language pack. There are no built-in fallback rules.
	const bool bPreprocessed = FTextPreprocessor::PreprocessSegments(
	    Input.Segments,
	    [&ResolveLanguageModule](const FLingotionLanguage& Language) -> const Thespeon::Language::FTextPreprocessingRules*
	    {
		    Thespeon::Language::LanguageModule* LangModule = ResolveLanguageModule(Language.ISO639_2);
		    if (!LangModule)
		    {
			    return nullptr;
		    }
		    const Thespeon::Language::FTextPreprocessingRules* Rules = LangModule->GetTextPreprocessingRules().Get();
		    if (!Rules)
		    {
			    LINGO_LOG(
			        EVerbosityLevel::Error,
			        TEXT("Language module %s does not contain valid text preprocessing rules. Cannot preprocess text. Please re-import your "
			             "language pack."),
			        *LangModule->ModuleID
			    );
		    }
		    return Rules;
	    },
	    Input.DefaultEmotion
	);
	if (!bPreprocessed)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Text preprocessing failed. See earlier logs for details."));
		return false;
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Preprocessed input:\n%s"), *Input.ToJson());

#if WITH_EDITOR
	// Editor-only data caching. Delegates are not thread-safe so we broadcast to the game thread.
	if (SessionID != TEXT("LINGOTION_WARMUP"))
	{
		Thespeon::Inference::FThespeonDataCache DataCache;
		TMap<FString, double>& ModuleCounts = DataCache.Data.Add(CharacterModule->ModuleID);
		ModuleCounts.Add(TEXT("nbrSynths"), 1.0);

		const UEnum* EmotionEnum = StaticEnum<EEmotion>();
		for (const FLingotionInputSegment& Segment : Input.Segments)
		{
			const double L = static_cast<double>(Segment.Text.Len());
			if (L <= 0.0)
			{
				continue;
			}

			// Distinct emotions present across the segment's start/end blend keypoints.
			TSet<EEmotion> SegmentEmotions;
			for (const TMap<EEmotion, float>* Blend : {&Segment.StartEmotion, &Segment.EndEmotion})
			{
				for (const TPair<EEmotion, float>& Pair : *Blend)
				{
					if (Pair.Value > 0.0f)
					{
						SegmentEmotions.Add(Pair.Key);
					}
				}
			}

			// Per-emotion character integral over the linearly-interpolated blend: L*(startW+endW)/2.
			// Summed over all emotions this equals L (each keypoint's weights sum to 1).
			for (const EEmotion Emotion : SegmentEmotions)
			{
				const double Contribution = L * (Segment.StartEmotion.FindRef(Emotion) + Segment.EndEmotion.FindRef(Emotion)) / 2.0;
				if (Contribution <= 0.0)
				{
					continue;
				}
				const FString EmotionName =
				    EmotionEnum ? EmotionEnum->GetNameStringByValue(static_cast<int64>(Emotion)) : FString::FromInt(static_cast<int32>(Emotion));
				ModuleCounts.FindOrAdd(EmotionName.ToLower()) += Contribution;
			}

			// Blend-cardinality histogram: chars synthesized with 1 / 2 / 3+ simultaneous emotions.
			const int32 Cardinality = SegmentEmotions.Num();
			if (Cardinality >= 1)
			{
				const TCHAR* BlendKey = Cardinality == 1 ? TEXT("blend1") : (Cardinality == 2 ? TEXT("blend2") : TEXT("blend3plus"));
				ModuleCounts.FindOrAdd(BlendKey) += L;
			}
		}

		AsyncTask(
		    ENamedThreads::GameThread,
		    [DataCache = MoveTemp(DataCache)]() { Thespeon::Inference::FThespeonEditorSignals::OnSynthesisDataSignal.Broadcast(DataCache); }
		);
	}
#endif // WITH_EDITOR

	// Phase 1: Collect unknown words across all segments, grouped by language.
	// Each language gets its own batch for the phonemizer model.
	bool bIsFirstSegment = true;
	int64 lastLanguageKey = -1;
	TMap<FString, TArray<FString>> UnknownWordsByISO;
	for (const FLingotionInputSegment& Segment : Input.Segments)
	{
		if (Segment.bIsCustomPronounced)
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Skipping custom pronounced segment: %s"), *Segment.Text);
			continue; // Skip custom pronounced segments
		}
		FString TargetISO639_2 = Segment.Language.ISO639_2;
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Processing segment with target language: %s"), *TargetISO639_2);
		// Preprocessing resolved every natural language segment's module and checked it has rules.
		Thespeon::Language::LanguageModule* LangModule = ResolveLanguageModule(TargetISO639_2);
		if (!LangModule)
		{
			return false;
		}
		if (!LookupTables.Contains(TargetISO639_2)) // Cache LT per language
		{
			FString LookupTableMD5 = LangModule->GetLookupTableID();
			TSharedPtr<Thespeon::Language::RuntimeLookupTable, ESPMode::ThreadSafe> LookupTablePtr =
			    LookupTableManager->GetLookupTable(LookupTableMD5);
			if (!LookupTablePtr)
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get lookup table for MD5: %s"), *LookupTableMD5);
				return false;
			}
			LookupTables.Add(TargetISO639_2, LookupTablePtr);
		}
		FString StrippedText =
		    Segment.Text.Replace(*FString::Chr(Thespeon::ControlCharacters::AudioSampleRequest), TEXT(""), ESearchCase::CaseSensitive);
		TArray<FString> UnknownWords =
		    GetUnknownWords(StrippedText, LangModule->GetTextPreprocessingRules()->GetWords(), LookupTables[TargetISO639_2].Get());
		TArray<FString>& LanguageUnknownWords = UnknownWordsByISO.FindOrAdd(TargetISO639_2);
		for (FString& Word : UnknownWords)
		{
			LanguageUnknownWords.AddUnique(MoveTemp(Word));
		}
	}

	// Phase 2: Run batched phonemization per language. Results are cached in lookup tables.
	for (const TPair<FString, TArray<FString>>& Pair : UnknownWordsByISO)
	{
		if (Pair.Value.Num() == 0)
		{
			continue;
		}
		const FString& ISO639_2 = Pair.Key;
		const TArray<FString>& WordsToPhonemize = Pair.Value;
		PhonemizeBatch(WordsToPhonemize, LanguageModules[ISO639_2].Get(), LookupTables[ISO639_2].Get(), &WorkloadCache, InputConfig);
	}

	// Phase 3: Build the final encoder token sequence from all segments.
	// SOS token (⏩) marks the start, EOS token (⏪) marks the end.
	// Each segment contributes encoded phoneme tokens plus per-token emotion and language IDs.
	TArray<int64> EncodedTxt;
	EncodedTxt.Append(CharacterModule->EncodePhonemes(TEXT("⏩")));
	TArray<int64> languages;

	// Emotion keypoints: collect distinct emotions used across all segments' Start/End blends
	// (first-appearance order). Each becomes a row in the k×N emotions / emotion_blending tensors.
	// Assumes text preprocessing has already sanitized the blends (no EEmotion::None, weights sum to 1).
	TArray<EEmotion> DistinctEmotions;
	TMap<EEmotion, int32> EmotionRow;
	for (const FLingotionInputSegment& Segment : Input.Segments)
	{
		for (const TMap<EEmotion, float>* Blend : {&Segment.StartEmotion, &Segment.EndEmotion})
		{
			for (const TPair<EEmotion, float>& Pair : *Blend)
			{
				if (!EmotionRow.Contains(Pair.Key))
				{
					EmotionRow.Add(Pair.Key, DistinctEmotions.Num());
					DistinctEmotions.Add(Pair.Key);
				}
			}
		}
	}
	const int32 k = DistinctEmotions.Num();
	auto MapToVector = [&](const TMap<EEmotion, float>& M) -> TArray<float>
	{
		TArray<float> V;
		V.Init(0.0f, k);
		for (const TPair<EEmotion, float>& Pair : M)
		{
			const int32 Row = EmotionRow.FindChecked(Pair.Key);
			V[Row] = Pair.Value;
		}
		return V;
	};

	// Per-segment token span + start/end keypoints, recorded during the loop below.
	struct FSegmentControlSpan
	{
		int32 TokenCount;
		TArray<float> StartVec;
		TArray<float> EndVec;
		float StartSpeed;
		float EndSpeed;
		float StartLoudness;
		float EndLoudness;
	};
	TArray<FSegmentControlSpan> ControlSpans;
	ControlSpans.Reserve(Input.Segments.Num());

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
			Thespeon::Language::LanguageModule* LangModule = LanguageModules[TargetISO639_2].Get();
			if (!PhonemizeSegment(
			        Segment,
			        LangModule->GetTextPreprocessingRules()->GetWords(),
			        CharacterModule,
			        LookupTables[TargetISO639_2].Get(),
			        SegmentTxtEncoded,
			        AudioSampleRequestGlobalIndices,
			        TextLengthSoFar
			    ))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to process words with lookup table pipeline"));
				return false;
			}
		}

		int64 LangKey = CharacterModule->GetLangKey(Segment.Language);
		if (LangKey == -1)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Character module does not have a valid LangKey for language: %s"), *Segment.Language.ToJson());
			return false;
		}
		EncodedTxt.Append(SegmentTxtEncoded);
		TArray<int64> SegmentLanguages;
		int len = SegmentTxtEncoded.Num() + (bIsFirstSegment ? 1 : 0); // Account for SOS token in first segment
		bIsFirstSegment = false;
		SegmentLanguages.Init(LangKey, len);
		languages.Append(SegmentLanguages);
		ControlSpans.Add(
		    {SegmentTxtEncoded.Num(),
		     MapToVector(Segment.StartEmotion),
		     MapToVector(Segment.EndEmotion),
		     Segment.StartSpeed,
		     Segment.EndSpeed,
		     Segment.StartLoudness,
		     Segment.EndLoudness}
		);
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug, TEXT("Segment processed: %d encoder tokens, Language: %s"), SegmentTxtEncoded.Num(), *Segment.Language.ToJson()
		);
		lastLanguageKey = LangKey;
	}

	// EOS token
	EncodedTxt.Append(CharacterModule->EncodePhonemes(TEXT("⏪")));
	languages.Add(lastLanguageKey);

	// Log complete encoder sequence
	FString TokensStr;
	for (int64 Token : EncodedTxt)
	{
		TokensStr += FString::Printf(TEXT("%lld "), Token);
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Complete encoder sequence (%d tokens): [%s]"), EncodedTxt.Num(), *TokensStr);

	TArray<float> speed;
	TArray<float> loudness;
	uint32 txt_len = EncodedTxt.Num();
	TArray<int64> text_lengths;
	text_lengths.Init(txt_len, 1);

	// Build scalar control curves over the same token layout as emotion:
	// SOS, encoded segment tokens, EOS. Speed follows the encoder's doubled
	// sequence, so each token's interpolated speed is repeated twice.
	const int32 N = static_cast<int32>(txt_len);
	speed.Init(1.0f, 2 * N);
	loudness.Init(1.0f, N);
	auto WriteControlSample = [&speed, &loudness](const int32 TokenIndex, const float SpeedValue, const float LoudnessValue)
	{
		speed[2 * TokenIndex] = SpeedValue;
		speed[2 * TokenIndex + 1] = SpeedValue;
		loudness[TokenIndex] = LoudnessValue;
	};

	// Build the scalar controls and k×N emotion tensors from keypoints.
	// Token layout: SOS, tokens in order, EOS.
	// emotion_blending[:,c] = proportion vector at token c sum=1.0, lerp between keypoints.
	// emotions[:,c] = constant list of the k distinct emotion IDs. Both row-major for shape {k, txt_len}.
	TArray<float> Blending;
	Blending.Init(0.0f, k * N);
	if (ControlSpans.Num() > 0)
	{
		// SOS = first segment's start keypoints
		WriteControlSample(0, ControlSpans[0].StartSpeed, ControlSpans[0].StartLoudness);
		for (int32 r = 0; r < k; ++r)
		{
			Blending[r * N + 0] = ControlSpans[0].StartVec[r];
		}
		int32 Cursor = 1; // token index 0 is SOS
		for (const FSegmentControlSpan& Span : ControlSpans)
		{
			const int32 L = Span.TokenCount;
			for (int32 j = 0; j < L; ++j)
			{
				const float t = (L == 1) ? 0.5f : (static_cast<float>(j) / static_cast<float>(L - 1));
				WriteControlSample(Cursor + j, FMath::Lerp(Span.StartSpeed, Span.EndSpeed, t), FMath::Lerp(Span.StartLoudness, Span.EndLoudness, t));
				for (int32 r = 0; r < k; ++r)
				{
					Blending[r * N + (Cursor + j)] = Span.StartVec[r] + (Span.EndVec[r] - Span.StartVec[r]) * t;
				}
			}
			Cursor += L;
		}
		// EOS = last segment's end keypoints
		WriteControlSample(N - 1, ControlSpans.Last().EndSpeed, ControlSpans.Last().EndLoudness);
		for (int32 r = 0; r < k; ++r)
		{
			Blending[r * N + (N - 1)] = ControlSpans.Last().EndVec[r];
		}
	}

	TArray<int64> EmotionIds;
	EmotionIds.SetNumUninitialized(k * N);
	for (int32 r = 0; r < k; ++r)
	{
		const int64 Id = static_cast<int64>(DistinctEmotions[r]);
		for (int32 c = 0; c < N; ++c)
		{
			EmotionIds[r * N + c] = Id;
		}
	}

	// Debug: dump the distinct emotion set and the first few interpolated blend columns.
	FString EmoStr;
	for (int32 r = 0; r < k; ++r)
	{
		EmoStr += FString::Printf(TEXT("%s(%lld) "), *UEnum::GetValueAsString(DistinctEmotions[r]), static_cast<int64>(DistinctEmotions[r]));
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Emotion tensors: k=%d, N=%d, emotions=[%s]"), k, N, *EmoStr);
	for (int32 c = 0; c < FMath::Min(N, 10); ++c)
	{
		FString ColStr;
		for (int32 r = 0; r < k; ++r)
		{
			ColStr += FString::Printf(TEXT("%.3f "), Blending[r * N + c]);
		}
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("  emotion_blending[:,%d] = [%s]"), c, *ColStr);
	}

	const uint32 NumEmotions = static_cast<uint32>(k);
	struct FInputTensor
	{
		FString Name;
		UE::NNE::FTensorShape Shape;
		ModelIOData Data;
	};

	auto MakeInputTensor = [](const TCHAR* Name, const UE::NNE::FTensorShape& Shape, const auto& Values)
	{
		using ElementType = typename std::decay_t<decltype(Values)>::ElementType;
		return FInputTensor{Name, Shape, ModelIOData::MakeFromArray<ElementType>(Shape, Values)};
	};
	int64 CharacterKey = CharacterModule->GetCharacterKey();
	if (CharacterKey == -1)
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Character module does not have a valid CharacterKey"));
		CharacterKey = 1; // Fallback to 1 to avoid invalid tensor
	}
	const uint32 NumSampleRequestIndices = AudioSampleRequestGlobalIndices.Num();
	TArray<FInputTensor> InputTensors = {
	    MakeInputTensor(TEXT("phoneme_keys"), UE::NNE::FTensorShape::Make({1, txt_len}), EncodedTxt),
	    MakeInputTensor(TEXT("emotions"), UE::NNE::FTensorShape::Make({1, NumEmotions, txt_len}), EmotionIds),
	    MakeInputTensor(TEXT("emotions_blending"), UE::NNE::FTensorShape::Make({1, NumEmotions, txt_len}), Blending),
	    MakeInputTensor(TEXT("actors"), UE::NNE::FTensorShape::Make({1, 1}), TArray<int64>{CharacterKey}),
	    MakeInputTensor(TEXT("languages"), UE::NNE::FTensorShape::Make({1, txt_len}), languages),
	    MakeInputTensor(TEXT("text_lengths"), UE::NNE::FTensorShape::Make({1}), text_lengths),
	    MakeInputTensor(TEXT("speed"), UE::NNE::FTensorShape::Make({1, 2 * txt_len}), speed),
	    MakeInputTensor(TEXT("loudness"), UE::NNE::FTensorShape::Make({1, 1, txt_len}), loudness),
	    MakeInputTensor(TEXT("target_phoneme_indices"), UE::NNE::FTensorShape::Make({NumSampleRequestIndices}), AudioSampleRequestGlobalIndices),
	};

	for (FInputTensor& InputTensor : InputTensors)
	{
		TensorPool.SetTensor(InputTensor.Name, MoveTemp(InputTensor.Data));
	}

	// Parsed once per module and shared by every session — the graph is read-only during Run().
	TSharedPtr<const metaonnx::MetaGraph, ESPMode::ThreadSafe> graph = CharacterModule->GetMetaGraph();
	if (!graph.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Could not get metagraph for character module '%s'"), *CharacterModule->ModuleID);
		return false;
	}

	TWeakPtr<std::atomic<bool>> WeakAlive(AliveToken);
	FMetaGraphRunner runner(
	    TensorPool,
	    CharacterModule,
	    &WorkloadCache,
	    InputConfig,
	    Thespeon::Inference::FPostPacketFn{[this, WeakAlive](const Thespeon::Core::FThespeonDataPacket& PacketToSend)
	                                       {
		                                       AsyncTask(
		                                           ENamedThreads::GameThread,
		                                           [this, WeakAlive, PacketToSend]()
		                                           {
			                                           // Pin the weak pointer - fails if InferenceSession was destroyed
			                                           TSharedPtr<std::atomic<bool>> StrongAlive = WeakAlive.Pin();
			                                           if (!StrongAlive || !StrongAlive->load())
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
		    TSharedPtr<std::atomic<bool>> StrongAlive = WeakAlive.Pin();
		    // Treat a destroyed owner as "stop requested"
		    return !StrongAlive || !StrongAlive->load() || ShouldStop();
	    }

	);
	if (!runner.Run(*graph))
	{
		// If the runner failed due to cancellation, return true so Run() routes
		// to PostCancelledPacket() instead of PostErrorPacket().
		return ShouldStop();
	}
	return true;
}

// Main inference pipeline. Runs on a background thread (FRunnable::Run).
// Pipeline: preload models -> validate input -> collect unknown words across all segments ->
// batch-phonemize unknowns per language -> encode all segments to encoder tokens ->
// build input tensors -> load and execute the MetaGraph (which handles encoder/decoder/vocoder).
// Cooperative cancellation is checked at key points via ShouldStop().
uint32 Thespeon::Inference::ThespeonInference::Run()
{
	bool RunStatus = Thespeon::Inference::ThespeonInference::ExecuteInference();
	if (!RunStatus)
	{
		PostErrorPacket();
	}
	else if (ShouldStop())
	{
		PostCancelledPacket();
	}
	// Convert boolean to error code (0 is success, else fail)
	return RunStatus ? 0u : 1u;
}

// Static method: Unloads a character's ONNX models and its non-shared language modules from the workload manager.
// Only unloads language modules that are not used by any other loaded character.
bool Thespeon::Inference::ThespeonInference::TryUnloadCharacter(
    const FString& CharacterName,
    const EThespeonModuleType& ModuleType,
    EBackendType BackendType,
    UInferenceWorkloadManager* InferenceWorkloadManager,
    UModuleManager* ModuleManager,
    ULookupTableManager* LookupTableManager,
    UManifestHandler* Manifest
)
{
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug, TEXT("called with CharacterName: %s, ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType)
	);
	if (!Manifest || !ModuleManager || !InferenceWorkloadManager || !LookupTableManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("One or more subsystem pointers are null"));
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

	// 2. If result, Get CharacterModule from ModuleManager
	// TSharedPtr keeps module alive for this scope; raw pointer used for downstream API compatibility
	TSharedPtr<Thespeon::Character::CharacterModule, ESPMode::ThreadSafe> CharacterModulePtr =
	    ModuleManager->GetModule<Thespeon::Character::CharacterModule>(Entry, false);
	if (!CharacterModulePtr)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get CharacterModule"));
		return false;
	}
	Thespeon::Character::CharacterModule* CharacterModule = CharacterModulePtr.Get();
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Got Character Module: %s"), *CharacterModule->ModuleID);

	// 3. Remove all language modules for character module
	TSet<FString> langModulesToRemove = ModuleManager->GetNonOverlappingModelLangModules(CharacterModule);
	for (const FString& langModule : langModulesToRemove)
	{
		Entry = Manifest->GetLanguageModuleEntry(langModule);
		if (Entry.ModuleID.IsEmpty())
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("No module entry found for module: %s"), *langModule);
			return false;
		}

		TSharedPtr<Thespeon::Language::LanguageModule, ESPMode::ThreadSafe> currentLangModulePtr =
		    ModuleManager->GetModule<Thespeon::Language::LanguageModule>(Entry, false);
		if (!currentLangModulePtr)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get Language module %s"), *Entry.ModuleID);
			return false;
		}

		Thespeon::Language::LanguageModule* currentLangModule = currentLangModulePtr.Get();
		if (!InferenceWorkloadManager->TryDeregisterModuleWorkloads(currentLangModule, BackendType, ModuleManager))
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

	// 4. Remove character module itself
	if (!InferenceWorkloadManager->TryDeregisterModuleWorkloads(CharacterModule, BackendType, ModuleManager))
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

// Static method: Reports whether a character is fully loaded on a backend.
//
// Mirrors every effect of FPreloadSession::Run, which is also what synthesis runs to load on demand:
// the character module workload, plus workload and lookup table for each imported language module.
// The preload tasks run independently, so checking the character module alone would report true
// while language tasks are still running, or after one of them failed.
// Lookup tables are checked last per language because the preload registers them last.
// A preload cancelled after all registrations finished still reports failure, yet this returns true;
// that is accurate, since the data is in fact loaded.
bool Thespeon::Inference::ThespeonInference::IsLoaded(
    const FString& CharacterName,
    const EThespeonModuleType& ModuleType,
    EBackendType BackendType,
    UInferenceWorkloadManager* InferenceWorkloadManager,
    UModuleManager* ModuleManager,
    ULookupTableManager* LookupTableManager,
    UManifestHandler* Manifest
)
{
	if (!Manifest || !ModuleManager || !InferenceWorkloadManager || !LookupTableManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("One or more subsystem pointers are null"));
		return false;
	}

	Thespeon::Core::FModuleEntry Entry = Manifest->GetCharacterModuleEntry(CharacterName, ModuleType);
	if (Entry.ModuleID.IsEmpty())
	{
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug,
		    TEXT("Character '%s' of module type '%s' is not imported, so it cannot be loaded."),
		    *CharacterName,
		    *UEnum::GetValueAsString(ModuleType)
		);
		return false;
	}

	// Checked before GetModule because GetModule with ShouldCreate=false logs at Error on a miss,
	// and "not loaded" is an expected answer here rather than a fault.
	if (!ModuleManager->IsRegistered(Entry.ModuleID))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Module '%s' is not registered."), *Entry.ModuleID);
		return false;
	}

	TSharedPtr<Thespeon::Character::CharacterModule, ESPMode::ThreadSafe> CharacterModulePtr =
	    ModuleManager->GetModule<Thespeon::Character::CharacterModule>(Entry, false);
	if (!CharacterModulePtr)
	{
		// Only reachable if the module was deregistered between the check above and this lookup.
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Module '%s' disappeared during lookup."), *Entry.ModuleID);
		return false;
	}

	if (!InferenceWorkloadManager->IsRegistered(CharacterModulePtr.Get(), BackendType))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Module '%s' has no workloads registered on this backend."), *Entry.ModuleID);
		return false;
	}

	// Same language set the preload iterates: LanguageModuleIDs already has the language fallback applied,
	// and entries that are not imported are skipped there too.
	int32 ImportedLanguageCount = 0;
	for (const auto& Kvp : CharacterModulePtr->LanguageModuleIDs)
	{
		Thespeon::Core::FModuleEntry LangEntry = Manifest->GetLanguageModuleEntry(Kvp.Value);
		if (LangEntry.ModuleID.IsEmpty())
		{
			continue;
		}
		++ImportedLanguageCount;

		if (!ModuleManager->IsRegistered(LangEntry.ModuleID))
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Language module '%s' is not registered."), *LangEntry.ModuleID);
			return false;
		}

		TSharedPtr<Thespeon::Language::LanguageModule, ESPMode::ThreadSafe> LangModulePtr =
		    ModuleManager->GetModule<Thespeon::Language::LanguageModule>(LangEntry, false);
		if (!LangModulePtr)
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Language module '%s' disappeared during lookup."), *LangEntry.ModuleID);
			return false;
		}

		if (!InferenceWorkloadManager->IsRegistered(LangModulePtr.Get(), BackendType))
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Language module '%s' has no workloads registered on this backend."), *LangEntry.ModuleID);
			return false;
		}

		if (!LookupTableManager->IsTableRegistered(LangModulePtr.Get()))
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Lookup table for language module '%s' is not registered."), *LangEntry.ModuleID);
			return false;
		}
	}

	if (ImportedLanguageCount == 0)
	{
		// The preload fails in this case, so the character is never usable for synthesis.
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("None of the languages of module '%s' are imported."), *Entry.ModuleID);
		return false;
	}

	return true;
}
