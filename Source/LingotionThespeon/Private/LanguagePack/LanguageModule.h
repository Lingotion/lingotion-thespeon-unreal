// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
// #include "NNE.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "NNEModelData.h"
#include "Core/Language.h"
#include "Core/Module.h"


namespace Thespeon
{
    namespace LanguagePack
    {
        /// @brief LanguageModule - Language-Specific Processing
        /// Handles language-specific tasks including text processing models, vocabularies, and lookup tables
        class LanguageModule : public Thespeon::Core::Module
        {
        public: 
            // Language metadata - matching Unity field name
            const FLingotionLanguage ModuleLanguage;
            
            // Constructor
            LanguageModule(const Thespeon::Core::FModuleEntry& ModuleInfo);
            
            // Module interface implementation
            virtual const FString GetModuleType() const override {return TEXT("language");}
            virtual TSet<FString> GetAllFileMD5s() const override; // Gets all MD5s of the files in this module
            virtual bool IsIncludedIn(const TSet<FString>& MD5s) const override; // Checks if module is fully included in provided MD5 set
            

            
            // Text processing functionality - matching Unity signatures
            TArray<int64> EncodePhonemes(const FString& Phonemes); // Encodes phonemes into their corresponding IDs (-1 for not found)
            TArray<int64> EncodeGraphemes(const FString& Graphemes); // Encodes graphemes into their corresponding IDs based on the phonemizer vocabulary
            FString DecodePhonemes(const TArray<int64>& PhonemeIDs); // Decodes phoneme IDs back into their string representation for debugging
            
            void InsertStringBoundaries(TArray<int64>& WordIDs); // Inserts SOS/EOS tokens - matching Unity method
            
            // Lookup tables for text normalization and pronunciation
            TMap<FString, FString> GetLookupTable(); // Gets lookup table for text normalization from internal file mappings
            FString GetLookupTableID() const;  // Gets the MD5 of the lookup table file - matching Unity method
        protected:
            // Initialize from JSON data - implements base class pure virtual
            virtual bool InitializeFromJSON(const FString& JsonString) override;
            
        private:
            // Lookup table size for initialization
            int32 lookupTableSize;
            
            // Vocabularies for encoding/decoding - using proper Unreal naming
            TMap<FString, int64> GraphemeToID; 
            TMap<FString, int64> PhonemeToID;
            TMap<int64, FString> IDToPhoneme; // Reverse mapping for decoding
            
            // Helper methods
            void ParseLanguagePackJSON(const TSharedPtr<FJsonObject>& JsonObject, const TSharedPtr<FJsonObject>& ConfigFiles);
            void LoadVocabularies(const TSharedPtr<FJsonObject>& ModuleObj);
        };
    }
}
