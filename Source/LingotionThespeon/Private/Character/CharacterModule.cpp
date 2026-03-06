// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "CharacterModule.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "Core/LingotionLogger.h"
#include "Inference/ModuleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Constructs a CharacterModule from a manifest entry. Loads the character JSON file from disk
// and initializes all runtime data: phoneme-to-encoder-ID vocabulary, language module mappings,
// language keys for the encoder, and the character embedding key.
Thespeon::Character::CharacterModule::CharacterModule(const Thespeon::Core::FModuleEntry& ModuleInfo)
    : Thespeon::Core::Module(ModuleInfo), CharacterKey(-1) // Invalid key initially
{
	LanguageModuleIDs = TMap<FString, FString>();
	LangToLangKey = TMap<FLingotionLanguage, int64>();

	// Load character JSON file using RuntimeFileLoader
	if (!ModuleInfo.JsonPath.IsEmpty())
	{
		FString JsonString =
		    Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsString(Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeFilePath(ModuleInfo.JsonPath));
		if (!JsonString.IsEmpty())
		{
			if (!InitializeFromJSON(JsonString))
			{
				LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Failed to initialize from JSON for module %s"), *ModuleInfo.ModuleID);
				// Module will remain in invalid state (CharacterKey = -1)
			}
		}
		else
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to load module config from path: %s"), *ModuleInfo.JsonPath);
		}
	}
}

// Validates the JSON is a "lara" type character, then delegates to ParseCharacterJSON.
bool Thespeon::Character::CharacterModule::InitializeFromJSON(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Invalid JSON format for module config %s"), *ModuleID);
		return false;
	}

	// Validate type
	FString Type;
	if (!JsonObject->TryGetStringField(TEXT("type"), Type) || Type != TEXT("lara"))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Config file path %s is not a valid character config file"), *JSONPath);
		return false;
	}

	// Get config files for MD5 mapping
	const TArray<TSharedPtr<FJsonValue>>* FileItemsArray = nullptr;
	if (!JsonObject->TryGetArrayField(TEXT("files"), FileItemsArray))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("No 'files' section found in character config"));
		return false;
	}

	// Parse character specific JSON structure
	if (!ParseJSON(JsonObject, *FileItemsArray))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to parse JSON for module %s"), *ModuleID);
		return false;
	}

	return true;
}

// Parses the character JSON structure:
// 1. Builds PhonemeToEncoderKeys from "phonemes_table.symbol_to_id" (maps phoneme strings to encoder IDs)
// 2. Populates InternalFileMappings from the "files" array (name → MD5 + extension)
// 3. Delegates to LoadCharacterConfiguration for language and character key setup
bool Thespeon::Character::CharacterModule::ParseJSON(const TSharedPtr<FJsonObject>& JsonObject, const TArray<TSharedPtr<FJsonValue>>& ModuleFiles)
{

	// Load phoneme vocabularies - matching Unity structure
	const TSharedPtr<FJsonObject>* PhonemesTablePtr = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("phonemes_table"), PhonemesTablePtr) && PhonemesTablePtr)
	{
		const TSharedPtr<FJsonObject>* SymbolToIdPtr = nullptr;
		if ((*PhonemesTablePtr)->TryGetObjectField(TEXT("symbol_to_id"), SymbolToIdPtr) && SymbolToIdPtr)
		{
			for (const auto& Pair : (*SymbolToIdPtr)->Values)
			{
				double Value = 0.0;
				if (Pair.Value->TryGetNumber(Value))
				{
					PhonemeToEncoderKeys.Add(Pair.Key, static_cast<int64>(Value));
				}
			}
		}
	}

	if (PhonemeToEncoderKeys.Num() == 0)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Phoneme table is not defined in the module"));
		return false;
	}

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

	if (!LoadCharacterConfiguration(JsonObject))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to load character configuration for module %s"), *ModuleID);
		return false;
	}

	return true;
}

// Loads character-specific configuration from JSON:
// 1. "phonemizer_setup.modules" → LanguageModuleIDs (iso639_2 → base_module_id mapping)
// 2. "languages" array → LangToLangKey (FLingotionLanguage → encoder language key)
// 3. "character.characterkey" → CharacterKey (encoder embedding index, CRITICAL for synthesis)
bool Thespeon::Character::CharacterModule::LoadCharacterConfiguration(const TSharedPtr<FJsonObject>& ModuleObj)
{
	// Load phonemizer setup for language module mappings - matching Unity structure
	const TSharedPtr<FJsonObject>* PhonemizerSetupPtr = nullptr;
	if (ModuleObj->TryGetObjectField(TEXT("phonemizer_setup"), PhonemizerSetupPtr) && PhonemizerSetupPtr)
	{
		const TArray<TSharedPtr<FJsonValue>>* ModulesPtr = nullptr;
		if (!(*PhonemizerSetupPtr)->TryGetArrayField(TEXT("modules"), ModulesPtr) || !ModulesPtr)
		{
			// No modules found, continue without phonemizer setup
		}
		else
		{
			for (const auto& PhonemizerObject : *ModulesPtr)
			{

				TSharedPtr<FJsonObject> LangModuleObj = PhonemizerObject->AsObject();

				FString ModuleId;
				if (!LangModuleObj->TryGetStringField(TEXT("base_module_id"), ModuleId))
				{
					continue;
				}

				// Create language key from ModuleLanguage (simplified for now)
				FString Iso639_2;
				if (!LangModuleObj->TryGetStringField(TEXT("iso639_2"), Iso639_2))
				{
					continue;
				}

				LanguageModuleIDs.Add(Iso639_2, ModuleId);
			}
		}
	}

	// Load language options
	const TArray<TSharedPtr<FJsonValue>>* LanguagesArray = nullptr;
	if (ModuleObj->TryGetArrayField(TEXT("languages"), LanguagesArray))
	{
		for (const auto& LangValue : *LanguagesArray)
		{
			if (!LangValue->AsObject().IsValid())
			{
				continue;
			}

			TSharedPtr<FJsonObject> LangObj = LangValue->AsObject();

			FString Iso639_2;
			FString Iso639_3;
			FString Glottocode;
			FString Iso3166_1;
			FString Iso3166_2;
			FString CustomDialect;
			double LanguageKey = -1.0;

			if (!LangObj->TryGetStringField(TEXT("iso639_2"), Iso639_2))
			{
				continue;
			}

			if (!LangObj->TryGetNumberField(TEXT("languagekey"), LanguageKey) || LanguageKey == -1.0)
			{
				continue;
			}

			// Extract additional fields if available
			LangObj->TryGetStringField(TEXT("iso639_3"), Iso639_3);
			LangObj->TryGetStringField(TEXT("glottocode"), Glottocode);
			LangObj->TryGetStringField(TEXT("iso3166_1"), Iso3166_1);
			LangObj->TryGetStringField(TEXT("iso3166_2"), Iso3166_2);
			LangObj->TryGetStringField(TEXT("customdialect"), CustomDialect);

			// Create ULingotionLanguage from JSON data and use its serialization as key
			FLingotionLanguage ResultLang(Iso639_2, Iso639_3, Glottocode, Iso3166_1, Iso3166_2, CustomDialect);

			// Use JSON serialization as key for proper value-based lookup
			LangToLangKey.Add(ResultLang, static_cast<int64>(LanguageKey));
		}
	}

	// Load character key - matching Unity structure (CRITICAL - must succeed)
	const TSharedPtr<FJsonObject>* CharacterObj = nullptr;
	if (!ModuleObj->TryGetObjectField(TEXT("character"), CharacterObj) || !CharacterObj)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Character options not found in the module configuration"));
		return false;
	}

	double CharacterKeyValue = -1.0;
	if (!(*CharacterObj)->TryGetNumberField(TEXT("characterkey"), CharacterKeyValue) || CharacterKeyValue == -1.0)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Character key not found in the module configuration"));
		return false;
	}

	CharacterKey = static_cast<int64>(CharacterKeyValue);

	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug,
	    TEXT("CharacterModule %s initialized: CharacterKey=%lld, Languages=%d, Phonemes=%d"),
	    *ModuleID,
	    CharacterKey,
	    LanguageModuleIDs.Num(),
	    PhonemeToEncoderKeys.Num()
	);

	return true;
}

// Gets all MD5s of the files in this module
TSet<FString> Thespeon::Character::CharacterModule::GetAllWorkloadMD5s() const
{
	TSet<FString> AllFileNames;

	// Collect MD5s from internal file mappings
	for (const auto& FilePair : InternalFileMappings)
	{
		if (FilePair.Key != TEXT("metagraph")) // Exclude metagraph from workload MD5s as it's not a runtime workload file
		{
			AllFileNames.Add(FilePair.Value.FileName);
		}
	}

	return AllFileNames;
}

// Encodes a phoneme string into encoder IDs using PhonemeToEncoderKeys.
// First attempts to match the entire string as a single vocabulary entry (e.g., multi-char phonemes).
// Falls back to character-by-character encoding if no full-string match is found.
// Unknown phonemes are skipped with a warning.
TArray<int64> Thespeon::Character::CharacterModule::EncodePhonemes(const FString& Phonemes)
{
	TArray<int64> EncodedPhonemes;

	// First try to find the entire phonemes string as single entry
	const int64* FullStringKey = PhonemeToEncoderKeys.Find(Phonemes);
	if (FullStringKey)
	{
		EncodedPhonemes.Add(*FullStringKey);
		return EncodedPhonemes;
	}

	// If not found as full string, encode character by character
	for (int32 i = 0; i < Phonemes.Len(); ++i)
	{
		FString SingleChar = FString::Chr(Phonemes[i]);
		const int64* EncoderKey = PhonemeToEncoderKeys.Find(SingleChar);

		if (EncoderKey)
		{
			EncodedPhonemes.Add(*EncoderKey);
		}
		else
		{
			LINGO_LOG(EVerbosityLevel::Warning, TEXT("Phoneme '%s' not found in vocabulary, skipping"), *SingleChar);
		}
	}

	return EncodedPhonemes;
}

// Gets the language key for a given language
int64 Thespeon::Character::CharacterModule::GetLangKey(const FLingotionLanguage& Language)
{
	if (Language.IsUndefined())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Language parameter is uninitialized"));
		return -1;
	}
	FString LangName = Language.Name;
	const int64* LanguageKey = LangToLangKey.Find(Language);
	if (LanguageKey && *LanguageKey != -1)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Language '%s' has key %lld"), *LangName, *LanguageKey);
		return *LanguageKey;
	}
	FString iso639_2 = Language.ISO639_2;
	LINGO_LOG(EVerbosityLevel::Error, TEXT("Language key for '%s' not found in character module config"), *Language.ToJson());
	return -1;
}

// Gets the character key
int64 Thespeon::Character::CharacterModule::GetCharacterKey() const
{
	if (CharacterKey == -1)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("No character key found in the character module"));
	}
	return CharacterKey;
}

// Checks whether all model files in this character module are registered as workloads
// on the given backend. Used to determine if the module is fully loaded.
bool Thespeon::Character::CharacterModule::IsIncludedIn(const TSet<FString>& WorkloadIDs, EBackendType BackendType) const
{
	// Check if all potential workloads on the given backend in this module are present in the provided ID set
	for (const auto& FilePair : InternalFileMappings)
	{
		FString WorkloadID;
		if (!Thespeon::Core::TryGetRuntimeWorkloadID(FilePair.Value.FileName, BackendType, WorkloadID))
		{
			LINGO_LOG(
			    EVerbosityLevel::Error,
			    TEXT("Could not get WorkloadID for file MD5 '%s' on backend '%s'."),
			    *FilePair.Value.FileName,
			    *UEnum::GetValueAsString(BackendType)
			);
		}
		if (FilePair.Key != TEXT("metagraph") && !WorkloadIDs.Contains(WorkloadID))
		{
			return false;
		}
		// --- IGNORE ---
	}
	return true;
}

TArray<FLingotionLanguage> Thespeon::Character::CharacterModule::GetSupportedLanguages() const
{
	TArray<FLingotionLanguage> SupportedLanguages;
	LangToLangKey.GetKeys(SupportedLanguages);
	return SupportedLanguages;
}
