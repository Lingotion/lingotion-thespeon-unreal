// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "LanguageModule.h"
#include "Core/IO/RuntimeFileLoader.h" // Ensure this header is included for IO::RuntimeFileLoader'
#include "Serialization/JsonReader.h"
#include "Core/LingotionLogger.h"
#include "Serialization/JsonSerializer.h"

Thespeon::LanguagePack::LanguageModule::LanguageModule(const Thespeon::Core::FModuleEntry& ModuleInfo)
    : Thespeon::Core::Module(ModuleInfo)
    , ModuleLanguage()
    , lookupTableSize(0)
{
    // Load configuration from JSON - matching Unity pattern
    if (!ModuleInfo.JsonPath.IsEmpty())
    {
        FString JsonString = Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsString(
            Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeFilePath(ModuleInfo.JsonPath));
        if (!JsonString.IsEmpty())
        {
            InitializeFromJSON(JsonString);
        }
        else
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("LanguageModule: Failed to load JSON from path: %s"), *ModuleInfo.JsonPath);
        }
    }
}

bool Thespeon::LanguagePack::LanguageModule::InitializeFromJSON(const FString& JsonString)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("LanguageModule: Failed to parse JSON for module %s"), *ModuleID);
        return false;
    }
    
    // Validate pack type - matching Unity validation
    FString PackType;
    if (!JsonObject->TryGetStringField(TEXT("type"), PackType) || PackType != TEXT("LANGUAGEPACK"))
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Config file path %s is not a valid languagepack config file"), *JSONPath);
        return false;
    }
    
    // Get config files for MD5 mapping
    const TSharedPtr<FJsonObject>* ConfigFilesPtr = nullptr;
    if (!JsonObject->TryGetObjectField(TEXT("files"), ConfigFilesPtr) || !ConfigFilesPtr)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("No 'files' section found in language pack config"));
        return false;
    }
    
    // Parse language pack specific JSON structure
    ParseLanguagePackJSON(JsonObject, *ConfigFilesPtr);
    
    return true;
}

void Thespeon::LanguagePack::LanguageModule::ParseLanguagePackJSON(const TSharedPtr<FJsonObject>& JsonObject, const TSharedPtr<FJsonObject>& ConfigFiles)
{
    // Find the module entry matching our ModuleID - matching Unity pattern
    const TArray<TSharedPtr<FJsonValue>>* ModulesArray = nullptr;
    if (!JsonObject->TryGetArrayField(TEXT("modules"), ModulesArray))
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("No 'modules' array found in language pack config"));
        return;
    }
    
    TSharedPtr<FJsonObject> ModuleObj = nullptr;
    for (const auto& ModuleValue : *ModulesArray)
    {
        TSharedPtr<FJsonObject> TempModuleObj = ModuleValue->AsObject();
        if (TempModuleObj.IsValid())
        {
            FString ModuleBaseID;
            if (TempModuleObj->TryGetStringField(TEXT("base_module_id"), ModuleBaseID) && 
                ModuleBaseID == ModuleID)
            {
                ModuleObj = TempModuleObj;
                break;
            }
        }
    }
    
    if (!ModuleObj.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Module %s not found in the configuration file %s. Ensure the configuration file is valid and contains the required module"), *ModuleID, *JSONPath);
        return;
    }
    
    // Load files - matching Unity structure
    const TSharedPtr<FJsonObject>* FilesPtr = nullptr;
    if (ModuleObj->TryGetObjectField(TEXT("files"), FilesPtr) && FilesPtr)
    {
        for (const auto& Pair : (*FilesPtr)->Values)
        {
            FString MD5 = Pair.Value->AsString();
            
            // Find file entry in config files
            const TSharedPtr<FJsonValue>* FileEntryPtr = ConfigFiles->Values.Find(MD5);
            if (FileEntryPtr && (*FileEntryPtr)->AsObject().IsValid())
            {
                TSharedPtr<FJsonObject> FileEntryObj = (*FileEntryPtr)->AsObject();
                FString FileName;
                if (FileEntryObj->TryGetStringField(TEXT("filename"), FileName))
                {
                    Thespeon::Core::FModuleFile ModuleFile(FileName, MD5);
                    InternalFileMappings.Add(Pair.Key, ModuleFile);
                    
                    // Check if it's an onnx model file - matching Unity logic
                    if (FPaths::GetExtension(FileName).Equals(TEXT("onnx"), ESearchCase::IgnoreCase))
                    {       
                        InternalModelMappings.Add(Pair.Key, MD5);
                        LINGO_LOG(EVerbosityLevel::Debug, TEXT("LanguageModule::ParseLanguagePackJSON: Added model mapping - Key: %s, MD5: %s"), *Pair.Key, *MD5);
                    }
                    else if (!Pair.Key.Equals(TEXT("LOOKUPTABLE"), ESearchCase::IgnoreCase))
                    {
                        LINGO_LOG(EVerbosityLevel::Error, TEXT("File %s in module %s is not a recognized file type: %s"), *FileName, *ModuleID, *Pair.Key);
                    }
                }
            }
        }
    }
    
    // Load vocabularies
    LoadVocabularies(ModuleObj);
    
    // Load language information - matching Unity structure
    const TArray<TSharedPtr<FJsonValue>>* LanguagesArray = nullptr;
    if (ModuleObj->TryGetArrayField(TEXT("languages"), LanguagesArray) && LanguagesArray->Num() > 0)
    {
        TSharedPtr<FJsonObject> LanguageObj = (*LanguagesArray)[0]->AsObject();
        if (LanguageObj.IsValid())
        {
            // TODO: Create ULingotionLanguage from JSON data
            // ModuleLanguage = CreateULingotionLanguageFromJSON(LanguageObj);
        }
    }
    
    // Load lookup table size
    double LookupTableSizeValue = 0.0;
    if (ModuleObj->TryGetNumberField(TEXT("lookuptable_size"), LookupTableSizeValue))
    {
        lookupTableSize = static_cast<int32>(LookupTableSizeValue);
    }
    
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("LanguageModule %s initialized: Files=%d, Models=%d, Graphemes=%d, Phonemes=%d"), 
        *ModuleID, InternalFileMappings.Num(), InternalModelMappings.Num(), GraphemeToID.Num(), PhonemeToID.Num());
}

void Thespeon::LanguagePack::LanguageModule::LoadVocabularies(const TSharedPtr<FJsonObject>& ModuleObj)
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
                LINGO_LOG(EVerbosityLevel::Debug, TEXT("Unknown vocabulary %s encountered in Language Module. Ignoring"), *VocabName);
            }
        }
    }
    
    if (GraphemeToID.Num() == 0 || PhonemeToID.Num() == 0 || IDToPhoneme.Num() == 0)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Grapheme or phoneme vocabularies are not defined in the module"));
        return;
    }
}


TSet<FString> Thespeon::LanguagePack::LanguageModule::GetAllFileMD5s() const
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

TArray<int64> Thespeon::LanguagePack::LanguageModule::EncodePhonemes(const FString& Phonemes)
{
    TArray<int64> EncodedPhonemes;
    
    // First try to find the entire phonemes string as single entry - matching Unity behavior
    const int64* FullStringKey = PhonemeToID.Find(Phonemes);
    if (FullStringKey)
    {
        EncodedPhonemes.Add(*FullStringKey);
        return EncodedPhonemes;
    }
    
    // If not found as full string, encode character by character
    for (int32 i = 0; i < Phonemes.Len(); ++i)
    {
        FString SingleChar = Phonemes.Mid(i, 1);
        const int64* Key = PhonemeToID.Find(SingleChar);
        
        if (Key)
        {
            EncodedPhonemes.Add(*Key);
        }
        else
        {
            // Use sentinel value (-1) for consistency with ActorModule
            EncodedPhonemes.Add(-1);
            LINGO_LOG(EVerbosityLevel::Debug, TEXT("Phoneme '%s' not found in vocabulary, using sentinel value -1"), *SingleChar);
        }
    }
    
    return EncodedPhonemes;
}

TArray<int64> Thespeon::LanguagePack::LanguageModule::EncodeGraphemes(const FString& Graphemes)
{
    TArray<int64> EncodedGraphemes;
    
    // Encode character by character - matching Unity behavior
    for (int32 i = 0; i < Graphemes.Len(); ++i)
    {
        FString SingleChar = Graphemes.Mid(i, 1);
        const int64* Key = GraphemeToID.Find(SingleChar);
        
        if (Key)
        {
            EncodedGraphemes.Add(*Key);
        }
        else
        {
            LINGO_LOG(EVerbosityLevel::Debug, TEXT("Lookup for grapheme '%s' not found in vocabulary. Character will be filtered out"), *SingleChar);
            // Skip this character - matching Unity behavior of filtering out
        }
    }
    
    return EncodedGraphemes;
}

FString Thespeon::LanguagePack::LanguageModule::DecodePhonemes(const TArray<int64>& PhonemeIDs)
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
            LINGO_LOG(EVerbosityLevel::Debug, TEXT("Lookup for phoneme id '%lld' not found in vocabulary. Character will be filtered out"), PhonemeID);
            // Skip this ID - matching Unity behavior of filtering out
        }
    }
    
    return Result;
}

void Thespeon::LanguagePack::LanguageModule::InsertStringBoundaries(TArray<int64>& WordIDs)
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

TMap<FString, FString> Thespeon::LanguagePack::LanguageModule::GetLookupTable()
{
    TMap<FString, FString> Result;
    
    // Find the lookup table file in internal file mappings
    const Thespeon::Core::FModuleFile* LookupFile = InternalFileMappings.Find(TEXT("lookuptable"));
    if (!LookupFile)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("LanguageModule::GetLookupTable - Lookup table not found in internal file mappings for module %s"), *ModuleID);
        return Result;
    }

    // Construct the full path to the lookup table JSON file
    FString LookupTablePath = Thespeon::Core::IO::RuntimeFileLoader::GetRuntimeFilePath(LookupFile->FileName);
    
    // Load the JSON content
    FString JsonContent = Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsString(LookupTablePath);
    if (JsonContent.IsEmpty())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("LanguageModule::GetLookupTable - Failed to load lookup table from path: %s"), *LookupTablePath);
        return Result;
    }

    // Parse the JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("LanguageModule::GetLookupTable - Failed to parse lookup table JSON for module %s"), *ModuleID);
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
            LINGO_LOG(EVerbosityLevel::Debug, TEXT("LanguageModule::GetLookupTable - Could not convert value for key '%s' to string"), *Pair.Key);
        }
    }
    
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("LanguageModule::GetLookupTable - Successfully loaded %d entries from lookup table for module %s"), 
        Result.Num(), *ModuleID);

    return Result;
}

bool Thespeon::LanguagePack::LanguageModule::IsIncludedIn(const TSet<FString>& MD5s) const
{
    // Check if all files in this module (except lookuptable) are present in the provided MD5 set - matching Unity behavior
    for (const auto& FilePair : InternalFileMappings)
    {
        if (FilePair.Key != TEXT("lookuptable") && !MD5s.Contains(FilePair.Value.MD5))
        {
            return false;
        }
    }
    return true;
}

FString Thespeon::LanguagePack::LanguageModule::GetLookupTableID() const
{
    const Thespeon::Core::FModuleFile* LookupFile = InternalFileMappings.Find(TEXT("lookuptable"));
    if (!LookupFile)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("LanguageModule::GetLookupTableID - Lookup table not found in internal file mappings for module %s"), *ModuleID);
        return FString();
    }
    return LookupFile->MD5;
}
