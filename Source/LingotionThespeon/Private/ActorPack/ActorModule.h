// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Core/Module.h"
#include "Core/Language.h"
#include "NNEModelData.h"

namespace Thespeon
{
    namespace ActorPack
    {
        /// @brief ActorModule - Character-Specific Functionality
        /// Represents a virtual character with voice synthesis models, language support, and phoneme encoding
        class ActorModule : public Thespeon::Core::Module
        {
        public: 
            // Voice synthesis configuration
            int32 ChunkLength;
            
            // Language support mapping - maps languages to their corresponding LanguageModule IDs
            TMap<FString, FString> LanguageModuleIDs; // map of language code to language module id
            
            // Constructor
            ActorModule(const Thespeon::Core::FModuleEntry& ModuleInfo);
            
            // Module interface implementation
            virtual const FString GetModuleType() const override {return TEXT("actor");}
            virtual TSet<FString> GetAllFileMD5s() const override; // Gets all MD5s of the files in this module
            virtual bool IsIncludedIn(const TSet<FString>& MD5s) const override; // Checks if module is fully included in provided MD5 set
            
            
            // Phoneme encoding - converts text phonemes to neural network input IDs
            TArray<int64> EncodePhonemes(const FString& Phonemes); // Returns encoded phonemes (-1 for not found phonemes)
            
            
            // Actor/Language keys - numerical identifiers for ML models  
            int64 GetLangKey(const FLingotionLanguage& Language); // Gets the language key for a given language
            int64 GetActorKey() const; // Gets the actor key

            TArray<FLingotionLanguage> GetSupportedLanguages() const; // Gets all supported languages for this actor module
        protected:
            // Initialize from JSON data - implements base class pure virtual
            virtual bool InitializeFromJSON(const FString& JsonString) override;
            
        private:
            // Internal model metadata - using language serialization as key for proper value-based lookup
            TMap<FLingotionLanguage, int64> LangToLangKey; // Key: language JSON serialization, Value: language key for model
            int64 ActorKey;
            TMap<FString, int64> PhonemeToEncoderKeys; // dict of what key each phoneme is encoded as for model
            
            // Helper methods
            bool LoadActorConfiguration(const TSharedPtr<FJsonObject>& JsonObject);
            bool ParseActorPackJSON(const TSharedPtr<FJsonObject>& JsonObject, const TSharedPtr<FJsonObject>& ConfigFiles);
        };
    }
}
