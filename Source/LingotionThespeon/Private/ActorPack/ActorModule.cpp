// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "ActorModule.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "Core/LingotionLogger.h"
#include "../Inference/ModuleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"


Thespeon::ActorPack::ActorModule::ActorModule(const Thespeon::Core::FModuleEntry& ModuleInfo)
    : Thespeon::Core::Module(ModuleInfo)
    , ChunkLength(48) // Default chunk length - will be overridden from JSON
    , ActorKey(-1) // Invalid key initially
{
    LanguageModuleIDs = TMap<FString, FString>();
    LangToLangKey = TMap<FLingotionLanguage, int64>();
    // Runtime file loader to load Actor pack json and extract relevant info.
    // Populate LanguageModuleIDs, LangToLangKey, ActorKey, PhonemeToEncoderKeys based on the loaded data.   
    
    // Load actor pack JSON using RuntimeFileLoader
    if (!ModuleInfo.JsonPath.IsEmpty())
    {
        FString JsonString = Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsString(
            Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeFilePath(ModuleInfo.JsonPath));
        if (!JsonString.IsEmpty())
        {
            if (!InitializeFromJSON(JsonString))
            {
                LINGO_LOG(EVerbosityLevel::Debug, TEXT("ActorModule: Failed to initialize from JSON for module %s"), *ModuleInfo.ModuleID);
                // Module will remain in invalid state (ActorKey = -1)
            }
        }
        else
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("ActorModule: Failed to load JSON from path: %s"), *ModuleInfo.JsonPath);
        }
    }
}

bool Thespeon::ActorPack::ActorModule::InitializeFromJSON(const FString& JsonString)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("ActorModule: Failed to parse JSON for module %s"), *ModuleID);
        return false;
    }
    
    // Validate pack type - matching Unity validation
    FString PackType;
    if (!JsonObject->TryGetStringField(TEXT("type"), PackType) || PackType != TEXT("ACTORPACK"))
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Config file path %s is not a valid actorpack config file"), *JSONPath);
        return false;
    }
    
    // Get config files for MD5 mapping
    const TSharedPtr<FJsonObject>* ConfigFilesPtr = nullptr;
    if (!JsonObject->TryGetObjectField(TEXT("files"), ConfigFilesPtr) || !ConfigFilesPtr)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("No 'files' section found in actor pack config"));
        return false;
    }
    
    // Parse actor pack specific JSON structure
    if (!ParseActorPackJSON(JsonObject, *ConfigFilesPtr))
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("ActorModule: Failed to parse JSON for module %s"), *ModuleID);
        return false;
    }
    
    return true;
}

bool Thespeon::ActorPack::ActorModule::ParseActorPackJSON(const TSharedPtr<FJsonObject>& JsonObject, const TSharedPtr<FJsonObject>& ConfigFiles)
{
    // Find the module entry matching our ModuleID - matching Unity pattern
    const TArray<TSharedPtr<FJsonValue>>* ModulesArray = nullptr;
    if (!JsonObject->TryGetArrayField(TEXT("modules"), ModulesArray))
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("No 'modules' array found in actor pack config"));
        return false;
    }
    
    TSharedPtr<FJsonObject> ModuleObj = nullptr;
    for (const auto& ModuleValue : *ModulesArray)
    {
        TSharedPtr<FJsonObject> TempModuleObj = ModuleValue->AsObject();
        if (TempModuleObj.IsValid())
        {
            FString ModuleBaseID;
            if (TempModuleObj->TryGetStringField(TEXT("actorpack_base_module_id"), ModuleBaseID) && 
                ModuleBaseID == ModuleID)
            {
                ModuleObj = TempModuleObj;
                break;
            }
        }
    }
    
    if (!ModuleObj.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Module %s not found in config file %s"), *ModuleID, *JSONPath);
        return false;
    }
    
    // Load phoneme vocabularies - matching Unity structure
    const TSharedPtr<FJsonObject>* PhonemesTablePtr = nullptr;
    if (ModuleObj->TryGetObjectField(TEXT("phonemes_table"), PhonemesTablePtr) && PhonemesTablePtr)
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
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Phoneme vocabularies are not defined in the module"));
        return false;
    }
    
    // Load model files - try 'files' first (new format), then 'onnxfiles' (old format) for backward compatibility
    const TSharedPtr<FJsonObject>* SentisFilesPtr = nullptr;
    if (!ModuleObj->TryGetObjectField(TEXT("files"), SentisFilesPtr) || !SentisFilesPtr)
    {
        // Fall back to old format
        ModuleObj->TryGetObjectField(TEXT("onnxfiles"), SentisFilesPtr);
    }
    
    if (SentisFilesPtr)
    {
        for (const auto& Pair : (*SentisFilesPtr)->Values)
        {
            FString MD5 = Pair.Value->AsString();
            
            // Add to internal model mappings
            if (!InternalModelMappings.Contains(Pair.Key))
            {
                InternalModelMappings.Add(Pair.Key, MD5);
            }
            
            // Find file entry in config files
            const TSharedPtr<FJsonValue>* FileEntryPtr = ConfigFiles->Values.Find(MD5);
            if (FileEntryPtr && (*FileEntryPtr)->AsObject().IsValid())
            {
                TSharedPtr<FJsonObject> FileEntryObj = (*FileEntryPtr)->AsObject();
                FString FileName;
                if (FileEntryObj->TryGetStringField(TEXT("filename"), FileName))
                {
                    InternalFileMappings.Add(Pair.Key, Thespeon::Core::FModuleFile(FileName, MD5));
                }
            }
        }
    }
    
    // Extract chunk length
    double ChunkLengthValue = 0.0;
    if (ModuleObj->TryGetNumberField(TEXT("decoder_chunk_length"), ChunkLengthValue))
    {
        ChunkLength = static_cast<int32>(ChunkLengthValue);
    }
    
    // Load Actor pack configuration
    if (!LoadActorConfiguration(ModuleObj))
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("ActorModule: Failed to load actor pack configuration for module %s"), *ModuleID);
        return false;
    }
    
    
    return true;
}

bool Thespeon::ActorPack::ActorModule::LoadActorConfiguration(const TSharedPtr<FJsonObject>& ModuleObj)
{
    // Load phonemizer setup for language module mappings - matching Unity structure
    const TSharedPtr<FJsonObject>* PhonemizerSetupPtr = nullptr;
    if (ModuleObj->TryGetObjectField(TEXT("phonemizer_setup"), PhonemizerSetupPtr) && PhonemizerSetupPtr)
    {
        const TSharedPtr<FJsonObject>* ModulesPtr = nullptr;
        if (!(*PhonemizerSetupPtr)->TryGetObjectField(TEXT("modules"), ModulesPtr) || !ModulesPtr)
        {
            // No modules found, continue without phonemizer setup
        }
        else
        {
            for (const auto& Pair : (*ModulesPtr)->Values)
            {
                if (!Pair.Value->AsObject().IsValid())
                    continue;
                
                TSharedPtr<FJsonObject> LangModuleObj = Pair.Value->AsObject();
                
                FString ModuleId;
                if (!LangModuleObj->TryGetStringField(TEXT("module_id"), ModuleId))
                    continue;
                
                // Get first language from languages array
                const TArray<TSharedPtr<FJsonValue>>* LanguagesArray = nullptr;
                if (!LangModuleObj->TryGetArrayField(TEXT("languages"), LanguagesArray) || LanguagesArray->Num() == 0)
                    continue;
                
                TSharedPtr<FJsonObject> LanguageObj = (*LanguagesArray)[0]->AsObject();
                if (!LanguageObj.IsValid())
                    continue;
                
                // Create language key from ModuleLanguage (simplified for now)
                FString Iso639_2;
                if (!LanguageObj->TryGetStringField(TEXT("iso639_2"), Iso639_2))
                    continue;
                
                LanguageModuleIDs.Add(Iso639_2, ModuleId);
            }
        }
    }
    
    // Load language options - matching Unity structure
    const TSharedPtr<FJsonObject>* LanguageOptionsPtr = nullptr;
    if (ModuleObj->TryGetObjectField(TEXT("language_options"), LanguageOptionsPtr) && LanguageOptionsPtr)
    {
        const TArray<TSharedPtr<FJsonValue>>* LanguagesArray = nullptr;
        if ((*LanguageOptionsPtr)->TryGetArrayField(TEXT("languages"), LanguagesArray))
        {
            for (const auto& LangValue : *LanguagesArray)
            {
                if (!LangValue->AsObject().IsValid())
                    continue;
                
                TSharedPtr<FJsonObject> LangObj = LangValue->AsObject();
                
                FString Iso639_2;
                FString Iso639_3;
                FString Glottocode;
                FString Iso3166_1;
                FString Iso3166_2;
                FString CustomDialect;
                double LanguageKey = -1.0;
                
                if (!LangObj->TryGetStringField(TEXT("iso639_2"), Iso639_2))
                    continue;
                    
                if (!LangObj->TryGetNumberField(TEXT("languagekey"), LanguageKey) || LanguageKey == -1.0)
                    continue;
                    
                // Extract additional fields if available
                LangObj->TryGetStringField(TEXT("iso639_3"),Iso639_3);
                LangObj->TryGetStringField(TEXT("glottocode"), Glottocode);
                LangObj->TryGetStringField(TEXT("iso3166_1"), Iso3166_1);
                LangObj->TryGetStringField(TEXT("iso3166_2"), Iso3166_2);
                LangObj->TryGetStringField(TEXT("dialect"), CustomDialect);
                
                // Create ULingotionLanguage from JSON data and use its serialization as key
                FLingotionLanguage ResultLang(Iso639_2, Iso639_3, Glottocode, Iso3166_1, Iso3166_2, CustomDialect);
                
                // Use JSON serialization as key for proper value-based lookup
                LangToLangKey.Add(ResultLang, static_cast<int64>(LanguageKey));

            }
        }
    }
    
    // Load actor key - matching Unity structure (CRITICAL - must succeed)
    const TSharedPtr<FJsonObject>* ActorOptionsPtr = nullptr;
    if (!ModuleObj->TryGetObjectField(TEXT("actor_options"), ActorOptionsPtr) || !ActorOptionsPtr)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Actor options not found in the module configuration"));
        return false;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* ActorsArray = nullptr;
    if (!(*ActorOptionsPtr)->TryGetArrayField(TEXT("actors"), ActorsArray) || ActorsArray->Num() == 0)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Actors array not found or empty in the module configuration"));
        return false;
    }
    
    TSharedPtr<FJsonObject> ActorObj = (*ActorsArray)[0]->AsObject();
    if (!ActorObj.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("First actor object is invalid in the module configuration"));
        return false;
    }
    
    double ActorKeyValue = -1.0;
    if (!ActorObj->TryGetNumberField(TEXT("actorkey"), ActorKeyValue) || ActorKeyValue == -1.0)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Actor key not found in the module configuration"));
        return false;
    }
    
    ActorKey = static_cast<int64>(ActorKeyValue);
    
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("ActorModule %s initialized: ActorKey=%lld, ChunkLength=%d, Languages=%d, Phonemes=%d"), 
        *ModuleID, ActorKey, ChunkLength, LanguageModuleIDs.Num(), PhonemeToEncoderKeys.Num());
    
    return true;
}


//Gets all MD5s of the files in this module
TSet<FString> Thespeon::ActorPack::ActorModule::GetAllFileMD5s() const
{
    TSet<FString> AllMD5s;
    
    // Collect MD5s from internal file mappings
    for (const auto& FilePair : InternalFileMappings)
    {
        AllMD5s.Add(FilePair.Value.MD5);
    }
    
    // Also collect from internal model mappings
    for (const auto& ModelPair : InternalModelMappings)
    {
        AllMD5s.Add(ModelPair.Value);
    }
    
    return AllMD5s;
}

// Encodes phonemes into their corresponding IDs based on the encoder id vocabulary
// Returns -1 for phonemes not found in vocabulary (to be filtered when constructing tensors)
TArray<int64> Thespeon::ActorPack::ActorModule::EncodePhonemes(const FString& Phonemes)
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
        FString SingleChar = Phonemes.Mid(i, 1);
        const int64* EncoderKey = PhonemeToEncoderKeys.Find(SingleChar);
        
        if (EncoderKey)
        {
            EncodedPhonemes.Add(*EncoderKey);
        }
        else
        {
            LINGO_LOG(EVerbosityLevel::Debug, TEXT("Phoneme '%s' not found in vocabulary, skipping"), *SingleChar);
        }
    }
    
    return EncodedPhonemes;
}

//Gets the language key for a given language
int64 Thespeon::ActorPack::ActorModule::GetLangKey(const FLingotionLanguage& Language)
{
    if (Language.IsUndefined())
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("Language parameter is uninitialized"));
        return -1;
    }
    FString LangName = Language.Name;
    const int64* LanguageKey = LangToLangKey.Find(Language);
    if (LanguageKey && *LanguageKey != -1)
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("ActorModule.GetLangKey: Language '%s' has key %lld"), *LangName, *LanguageKey);
        return *LanguageKey;
    }
    FString iso639_2 = Language.ISO639_2;
        LINGO_LOG(EVerbosityLevel::Warning, TEXT("Language key for '%s' not found in actor module config"), *Language.ToJson());
    return -1;
}

//Gets the actor key
int64 Thespeon::ActorPack::ActorModule::GetActorKey() const
{
    if (ActorKey == -1)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("No actorkey found in the actor module"));
    }
    return ActorKey;
}

bool Thespeon::ActorPack::ActorModule::IsIncludedIn(const TSet<FString>& MD5s) const
{
    // Check if all files in this module are present in the provided MD5 set - matching Unity behavior
    for (const auto& FilePair : InternalFileMappings)
    {
        if (!MD5s.Contains(FilePair.Value.MD5))
        {
            return false;
        }
        // --- IGNORE ---
    }
    return true;
}

TArray<FLingotionLanguage> Thespeon::ActorPack::ActorModule::GetSupportedLanguages() const
{
    TArray<FLingotionLanguage> SupportedLanguages;
    LangToLangKey.GetKeys(SupportedLanguages);
    return SupportedLanguages;
}
