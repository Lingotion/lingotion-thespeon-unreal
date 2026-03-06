// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "LanguageModule.h"
#include "Core/IO/RuntimeFileLoader.h" // Ensure this header is included for IO::RuntimeFileLoader'
#include "Serialization/JsonReader.h"
#include "Core/LingotionLogger.h"
#include "Serialization/JsonSerializer.h"

// Constructs a LanguageModule from a manifest entry. Loads the language JSON file from disk
// and initializes vocabularies (grapheme→ID, phoneme→ID, ID→phoneme) and the lookup table size.
Thespeon::Language::LanguageModule::LanguageModule(const Thespeon::Core::FModuleEntry& ModuleInfo)
    : Thespeon::Core::Module(ModuleInfo), ModuleLanguage(), lookupTableSize(0)
{
	// Load configuration from JSON - matching Unity pattern
	if (!ModuleInfo.JsonPath.IsEmpty())
	{
		FString JsonString =
		    Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsString(Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeFilePath(ModuleInfo.JsonPath));
		if (!JsonString.IsEmpty())
		{
			InitializeFromJSON(JsonString);
		}
		else
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to load module config from path: %s"), *ModuleInfo.JsonPath);
		}
	}
}

/**
 * Validates the JSON structure and type for a language module, then parses the configuration.
 */
bool Thespeon::Language::LanguageModule::InitializeFromJSON(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to deserialize JSON for module %s"), *ModuleID);
		return false;
	}

	// Validate type
	FString Type;
	if (!JsonObject->TryGetStringField(TEXT("type"), Type) || Type != TEXT("phonemizer"))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Config file path %s is not a valid language config file"), *JSONPath);
		return false;
	}

	// Get config files for MD5 mapping
	const TArray<TSharedPtr<FJsonValue>>* FileItemsArray = nullptr;
	if (!JsonObject->TryGetArrayField(TEXT("files"), FileItemsArray))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("No 'files' section found in language module config"));
		return false;
	}

	// Parse language specific JSON structure
	if (!ParseJSON(JsonObject, *FileItemsArray))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to parse JSON for module %s"), *ModuleID);
		return false;
	}

	return true;
}

// Parses the language JSON structure:
// 1. Populates InternalFileMappings from the "files" array (name → MD5 + extension)
// 2. Loads grapheme/phoneme vocabularies via LoadVocabularies
// 3. Reads "lookuptable_size" for the runtime lookup table capacity
bool Thespeon::Language::LanguageModule::ParseJSON(const TSharedPtr<FJsonObject>& JsonObject, const TArray<TSharedPtr<FJsonValue>>& ModuleFiles)
{

	// Load files to dict
	for (const auto& File : ModuleFiles)
	{
		TSharedPtr<FJsonObject> FileObj = File->AsObject();
		if (!FileObj.IsValid())
		{
			LINGO_LOG(EVerbosityLevel::Warning, TEXT("Invalid file object in module files array"));
			continue;
		}
		// Add to internal model mappings
		FString InternalName = FileObj->GetStringField(TEXT("name"));
		FString MD5 = FileObj->GetStringField(TEXT("md5"));
		FString Extension = FileObj->GetStringField(TEXT("extension"));
		if (!InternalFileMappings.Contains(InternalName))
		{
			Thespeon::Core::FModuleFile ModuleFile(MD5, Extension);
			InternalFileMappings.Add(InternalName, ModuleFile);
		}
	}

	// Load vocabularies
	LoadVocabularies(JsonObject);

	// Load lookup table size
	double LookupTableSizeValue = 0.0;
	if (JsonObject->TryGetNumberField(TEXT("lookuptable_size"), LookupTableSizeValue))
	{
		lookupTableSize = static_cast<int32>(LookupTableSizeValue);
	}

	return true;
}

// Loads three vocabulary maps from the "vocabularies" JSON object:
// - grapheme_vocab: text characters → integer IDs (used for G2P model input)
// - phoneme_vocab: phoneme strings → integer IDs (used for G2P model output comparison)
// - phoneme_ivocab: integer IDs → phoneme strings (inverse map, used to decode G2P output)
void Thespeon::Language::LanguageModule::LoadVocabularies(const TSharedPtr<FJsonObject>& ModuleObj)
{
	// Load vocabularies - matching Unity structure
	const TSharedPtr<FJsonObject>* VocabulariesPtr = nullptr;
	if (ModuleObj->TryGetObjectField(TEXT("vocabularies"), VocabulariesPtr) && VocabulariesPtr)
	{
		for (const auto& VocabPair : (*VocabulariesPtr)->Values)
		{
			const FString& VocabName = VocabPair.Key;
			TSharedPtr<FJsonObject> VocabObj = VocabPair.Value->AsObject();

			if (!VocabObj.IsValid())
			{
				continue;
			}

			if (VocabName == TEXT("grapheme_vocab"))
			{
				for (const auto& Pair : VocabObj->Values)
				{
					double Value = 0.0;
					if (Pair.Value->TryGetNumber(Value))
					{
						GraphemeToID.Add(Pair.Key, static_cast<int64>(Value));
					}
				}
			}
			else if (VocabName == TEXT("phoneme_vocab"))
			{
				for (const auto& Pair : VocabObj->Values)
				{
					double Value = 0.0;
					if (Pair.Value->TryGetNumber(Value))
					{
						PhonemeToID.Add(Pair.Key, static_cast<int64>(Value));
					}
				}
			}
			else if (VocabName == TEXT("phoneme_ivocab"))
			{
				for (const auto& Pair : VocabObj->Values)
				{
					double KeyValue = 0.0;
					FString StringValue;
					if (FCString::IsNumeric(*Pair.Key) && Pair.Value->TryGetString(StringValue))
					{
						KeyValue = FCString::Atod(*Pair.Key);
						IDToPhoneme.Add(static_cast<int64>(KeyValue), StringValue);
					}
				}
			}
			else if (VocabName == TEXT("grapheme_ivocab"))
			{
				// Inverse grapheme vocabulary is not used in production but can be useful for debugging
				// Skip for now as Unity comments indicate it's not used
			}
			else
			{
				LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Unknown vocabulary %s encountered in Language Module. Ignoring"), *VocabName);
			}
		}
	}

	if (GraphemeToID.Num() == 0 || PhonemeToID.Num() == 0 || IDToPhoneme.Num() == 0)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Grapheme or phoneme vocabularies are not defined in the module"));
		return;
	}
}

TSet<FString> Thespeon::Language::LanguageModule::GetAllWorkloadMD5s() const
{
	TSet<FString> AllMD5s;

	// Collect MD5s from internal file mappings
	for (const auto& FilePair : InternalFileMappings)
	{
		if (FilePair.Key != TEXT("lookuptable")) // Exclude lookuptable from workload MD5s as it's not a runtime workload file
		{
			AllMD5s.Add(FilePair.Value.FileName);
		}
	}

	return AllMD5s;
}

// Encodes a text string into grapheme IDs character-by-character for G2P model input.
// Characters not found in GraphemeToID are silently skipped (filtered out).
TArray<int64> Thespeon::Language::LanguageModule::EncodeGraphemes(const FString& Graphemes)
{
	TArray<int64> EncodedGraphemes;

	// Encode character by character - matching Unity behavior
	for (int32 i = 0; i < Graphemes.Len(); ++i)
	{
		FString SingleChar = FString::Chr(Graphemes[i]);
		const int64* Key = GraphemeToID.Find(SingleChar);

		if (Key)
		{
			EncodedGraphemes.Add(*Key);
		}
		else
		{
			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug, TEXT("Lookup for grapheme '%s' not found in vocabulary. Character will be filtered out"), *SingleChar
			);
			// Skip this character - matching Unity behavior of filtering out
		}
	}

	return EncodedGraphemes;
}

// Decodes G2P model output IDs back into a phoneme string using IDToPhoneme (inverse vocabulary).
FString Thespeon::Language::LanguageModule::DecodePhonemes(const TArray<int64>& PhonemeIDs)
{
	FString Result;

	for (int64 PhonemeID : PhonemeIDs)
	{
		const FString* Phoneme = IDToPhoneme.Find(PhonemeID);
		if (Phoneme)
		{
			Result += *Phoneme;
		}
		else
		{
			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug, TEXT("Lookup for phoneme id '%lld' not found in vocabulary. Character will be filtered out"), PhonemeID
			);
			// Skip this ID - matching Unity behavior of filtering out
		}
	}

	return Result;
}

// Wraps the grapheme ID sequence with start-of-string (<sos>) and end-of-string (<eos>) tokens.
// These boundary tokens are required by the G2P model for proper sequence recognition.
void Thespeon::Language::LanguageModule::InsertStringBoundaries(TArray<int64>& WordIDs)
{
	// Insert SOS and EOS tokens - matching Unity behavior
	const int64* SosID = GraphemeToID.Find(TEXT("<sos>"));
	const int64* EosID = GraphemeToID.Find(TEXT("<eos>"));

	if (SosID)
	{
		WordIDs.Insert(*SosID, 0);
	}

	if (EosID)
	{
		WordIDs.Add(*EosID);
	}
}

// Loads the word-to-phoneme lookup table from disk as a JSON file.
// The lookup table provides pre-computed phonemizations for common words, avoiding
// the slower G2P neural model inference. "license_terms" entries are filtered out.
TMap<FString, FString> Thespeon::Language::LanguageModule::GetLookupTable()
{
	TMap<FString, FString> Result;

	// Find the lookup table file in internal file mappings
	const Thespeon::Core::FModuleFile* LookupFile = InternalFileMappings.Find(TEXT("lookuptable"));
	if (!LookupFile)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Lookup table not found in internal file mappings for module %s"), *ModuleID);
		return Result;
	}

	// Construct the full path to the lookup table JSON file
	FString LookupTablePath = Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeFilePath(LookupFile->GetFullFileName());

	// Load the JSON content
	FString JsonContent = Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsString(LookupTablePath);
	if (JsonContent.IsEmpty())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to load lookup table from path: %s"), *LookupTablePath);
		return Result;
	}

	// Parse the JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to deserialize lookup table JSON for module %s"), *ModuleID);
		return Result;
	}

	// Convert JSON object to TMap, excluding license_terms
	for (const auto& Pair : JsonObject->Values)
	{
		if (Pair.Key.Equals(TEXT("license_terms"), ESearchCase::IgnoreCase))
		{
			continue; // Skip license terms
		}

		FString Value;
		if (Pair.Value->TryGetString(Value))
		{
			Result.Add(Pair.Key, Value);
		}
		else
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Could not convert value for key '%s' to string"), *Pair.Key);
		}
	}

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Successfully loaded %d entries from lookup table for module %s"), Result.Num(), *ModuleID);

	return Result;
}

// Checks whether all model files (excluding the lookuptable, which is not an ONNX model)
// are registered as workloads on the given backend. Used to determine if the module is fully loaded.
bool Thespeon::Language::LanguageModule::IsIncludedIn(const TSet<FString>& WorkloadIDs, EBackendType BackendType) const
{
	// Check if all files in this module (except lookuptable) are present in the provided MD5 set - matching Unity behavior
	for (const auto& FilePair : InternalFileMappings)
	{
		FString WorkloadID;
		if (!Thespeon::Core::TryGetRuntimeWorkloadID(FilePair.Value.FileName, BackendType, WorkloadID))
		{
			LINGO_LOG(
			    EVerbosityLevel::Error,
			    TEXT("Could not get WorkloadID for file md5 '%s' on backend '%s'."),
			    *FilePair.Value.FileName,
			    *UEnum::GetValueAsString(BackendType)
			);
		}
		if (FilePair.Key != TEXT("lookuptable") && !WorkloadIDs.Contains(WorkloadID))
		{
			return false;
		}
	}
	return true;
}

FString Thespeon::Language::LanguageModule::GetLookupTableID() const
{
	const Thespeon::Core::FModuleFile* LookupFile = InternalFileMappings.Find(TEXT("lookuptable"));
	if (!LookupFile)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Lookup table not found in internal file mappings for module %s"), *ModuleID);
		return FString();
	}
	return LookupFile->FileName;
}
