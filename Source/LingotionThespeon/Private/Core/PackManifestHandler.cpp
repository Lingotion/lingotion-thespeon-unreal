// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Core/PackManifestHandler.h"
#include "CoreMinimal.h"
#include "Misc/FileHelper.h" // FFileHelper::LoadFileToString
#include "Misc/Paths.h"      // FPaths::GetBaseFilename / Combine / ProjectDir / etc.
#include "Policies/CondensedJsonPrintPolicy.h"
#include "JsonObjectConverter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Core/LingotionLogger.h"
#include "Core/IO/RuntimeFileLoader.h"



void UPackManifestHandler::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Initialize the module type mapping
    ModuleTypeToString.Add(EThespeonModuleType::XS, TEXT("ultralow"));
    ModuleTypeToString.Add(EThespeonModuleType::S, TEXT("low"));
    ModuleTypeToString.Add(EThespeonModuleType::M, TEXT("mid"));
    ModuleTypeToString.Add(EThespeonModuleType::L, TEXT("high"));
    ModuleTypeToString.Add(EThespeonModuleType::XL, TEXT("ultrahigh"));

    ModuleTypeToFrontEndString.Add(EThespeonModuleType::XS,TEXT("XS"));
    ModuleTypeToFrontEndString.Add(EThespeonModuleType::S, TEXT("S"));
    ModuleTypeToFrontEndString.Add(EThespeonModuleType::M, TEXT("M"));
    ModuleTypeToFrontEndString.Add(EThespeonModuleType::L, TEXT("L"));
    ModuleTypeToFrontEndString.Add(EThespeonModuleType::XL,TEXT("XL"));

    StringToModuleType.Add(TEXT("ultralow"), EThespeonModuleType::XS);
    StringToModuleType.Add(TEXT("low"), EThespeonModuleType::S);
    StringToModuleType.Add(TEXT("mid"), EThespeonModuleType::M);
    StringToModuleType.Add(TEXT("high"), EThespeonModuleType::L);
    StringToModuleType.Add(TEXT("ultrahigh"), EThespeonModuleType::XL);
    StringToModuleType.Add(TEXT("XS"), EThespeonModuleType::XS);
    StringToModuleType.Add(TEXT("S"), EThespeonModuleType::S);
    StringToModuleType.Add(TEXT("M"), EThespeonModuleType::M);
    StringToModuleType.Add(TEXT("L"), EThespeonModuleType::L);
    StringToModuleType.Add(TEXT("XL"), EThespeonModuleType::XL);
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("PackManifestHandler initialized."));
    
    ReadPackManifest();
}

void UPackManifestHandler::Deinitialize()
{
    Root.Reset();
    Super::Deinitialize();
}

EThespeonModuleType UPackManifestHandler::FindModuleType(const FString& ModuleTypeString) const
{
    EThespeonModuleType result = EThespeonModuleType::None;  
    const EThespeonModuleType* ModuleTypePtr = StringToModuleType.Find(ModuleTypeString);
    if(ModuleTypePtr)
    {
        result = *ModuleTypePtr;
    }
    return result;
}

void UPackManifestHandler::ReadPackManifest()
{
    FString FilePath = Thespeon::Core::IO::RuntimeFileLoader::GetManifestPath();
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("Reading pack manifest from: %s"), *FilePath);
    
    FString JsonString = Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsString(FilePath);
    if (!JsonString.IsEmpty())
    {
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

        if (!(FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to deserialize pack manifest."));
        }
    }
    
}

bool UPackManifestHandler::IsRootInvalid() const
{
    if (!Root.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Pack manifest not loaded."));
        return true;
    }
    return false;
}

bool UPackManifestHandler::TryGetActorpackModules(TSharedPtr<FJsonObject>& OutActorModulesPtr) const
{
    const TSharedPtr<FJsonObject>* ActorPackModulesPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("actorpack_modules"), ActorPackModulesPtr) || !ActorPackModulesPtr)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("'actorpack_modules' not found in manifest."));
        return false;
    }
    OutActorModulesPtr = *ActorPackModulesPtr;
    return true;
}

Thespeon::Core::FModuleEntry UPackManifestHandler::GetActorPackModuleEntry(const FString& CharacterName, EThespeonModuleType ModuleType) const
{
    if(IsRootInvalid())
    {
        return Thespeon::Core::FModuleEntry(); // Empty if no data
    }

    // Get the string name for the enum value (e.g., "L", "M", "XS")
    const FString* ModuleTypeStringPtr = ModuleTypeToString.Find(ModuleType);
    if (!ModuleTypeStringPtr)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Unknown module type: %s"), *UEnum::GetValueAsString(ModuleType));
        return Thespeon::Core::FModuleEntry();
    }
    FString ModuleTypeString = *ModuleTypeStringPtr;
    // Access "actorpack_modules" object
    TSharedPtr<FJsonObject> ActorPackModulesPtr;
    if (!TryGetActorpackModules(ActorPackModulesPtr))
    {
        return Thespeon::Core::FModuleEntry(); // Empty
    }

    const TSharedPtr<FJsonObject>& ActorPackModules = ActorPackModulesPtr;

    LINGO_LOG(EVerbosityLevel::Debug, TEXT("Found %d actor pack modules."), ActorPackModules->Values.Num());

    // Iterate through modules
    for (const auto& ModulePair : ActorPackModules->Values)
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("Checking module: %s"), *ModulePair.Key);
        const FString& ModuleId = ModulePair.Key;
        const TSharedPtr<FJsonObject> ModuleObj = ModulePair.Value->AsObject();
        if (!ModuleObj.IsValid())
        {
            continue;
        }

        // Check if the "actors" array contains CharacterName
        const TArray<TSharedPtr<FJsonValue>>* ActorsArray = nullptr;
        ModuleObj->TryGetArrayField(TEXT("actors"), ActorsArray);

        bool bActorMatch = false;
        if (ActorsArray)
        {
            for (const auto& ActorValue : *ActorsArray)
            {
                if (ActorValue->AsString() == CharacterName)
                {
                    bActorMatch = true;
                    break;
                }
            }
        }

        // Check "quality" field
        FString Quality;
        ModuleObj->TryGetStringField(TEXT("quality"), Quality);

        // If both match, return this module
        if (bActorMatch && Quality == ModuleTypeString)
        {
            FString JsonPath;
            ModuleObj->TryGetStringField(TEXT("jsonpath"), JsonPath);
            return Thespeon::Core::FModuleEntry(ModuleId, JsonPath, ReadVersionObject(ModuleObj));
        }
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("No match for module: %s (ActorMatch: %s, Quality: %s)"), *ModuleId, bActorMatch ? TEXT("true") : TEXT("false"), *Quality);
    }

    // If nothing matches, return empty
    LINGO_LOG(EVerbosityLevel::Error, TEXT("No matching actor pack module found for character '%s' with quality '%s'."), *CharacterName, *ModuleTypeString);
    return Thespeon::Core::FModuleEntry();
}


Thespeon::Core::FModuleEntry UPackManifestHandler::GetLanguagePackModuleEntry(const FString& ModuleName) const
{
    if (IsRootInvalid())
    {
        return Thespeon::Core::FModuleEntry();
    }

    // Get the "languagepack_modules" object
    const TSharedPtr<FJsonObject>* LangModulesObjPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("languagepack_modules"), LangModulesObjPtr) || !LangModulesObjPtr)
    {
        LINGO_LOG(EVerbosityLevel::Warning, TEXT("'languagepack_modules' not found in manifest."));
        return Thespeon::Core::FModuleEntry();
    }
    const TSharedPtr<FJsonObject>& LangModules = *LangModulesObjPtr;

    // Find the module by key == ModuleName
    const TSharedPtr<FJsonValue>* ValuePtr = LangModules->Values.Find(ModuleName);
    if (!ValuePtr || !(*ValuePtr).IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Warning, TEXT("A language pack has not been imported and use of its language will not be possible. Check Thespeon Info window for more information."));
        return Thespeon::Core::FModuleEntry();
    }

    const TSharedPtr<FJsonObject> ModuleObj = (*ValuePtr)->AsObject();
    if (!ModuleObj.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Warning, TEXT("Language pack module '%s' is not a valid object."), *ModuleName);
        return Thespeon::Core::FModuleEntry();
    }

    // Read jsonpath (empty if missing)
    FString JsonPath;
    ModuleObj->TryGetStringField(TEXT("jsonpath"), JsonPath);

    // Return the entry (ModuleId = key name, JsonPath = field)
    return Thespeon::Core::FModuleEntry(ModuleName, JsonPath, ReadVersionObject(ModuleObj));
}

TArray<FLingotionLanguage> UPackManifestHandler::GetAllLanguagesInActorModule(const FString& ModuleName) const
{
    
    TArray<FLingotionLanguage> result;
    if (IsRootInvalid())
    {
        return result;
    }

    // Get the "languagepack_modules" object
    const TSharedPtr<FJsonObject>* ActorModulesObjPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("actorpack_modules"), ActorModulesObjPtr) || !ActorModulesObjPtr)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("'actorpack_modules' not found in manifest."));
        return result;
    }
    const TSharedPtr<FJsonObject>& ActorModules = *ActorModulesObjPtr;

    // Find the module by key == ModuleName
    const TSharedPtr<FJsonValue>* ValuePtr = ActorModules->Values.Find(ModuleName);
    if (!ValuePtr || !(*ValuePtr).IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Module %s is not a valid actor module."), *ModuleName);
        return result;
    }

    const TSharedPtr<FJsonObject> ModuleObj = (*ValuePtr)->AsObject();
    if (!ModuleObj.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Actor module '%s' is not a valid object."), *ModuleName);
        return result;
    }

    // Check the "languages" array 
    const TArray<TSharedPtr<FJsonValue>>* LanguagesArray = nullptr;
    ModuleObj->TryGetArrayField(TEXT("languages"), LanguagesArray);

    if (LanguagesArray)
    {
        for (const auto& LanguageValue : *LanguagesArray)
        {
            const TSharedPtr<FJsonObject> LangObj = LanguageValue-> AsObject();
            FString iso639_2;
            FString iso3166_1;
            LangObj->TryGetStringField(TEXT("iso639_2"), iso639_2);
            if(LangObj->TryGetStringField(TEXT("iso3166_1"), iso3166_1))
            {
                FLingotionLanguage Lang(iso639_2, TEXT(""), TEXT(""), iso3166_1);
                result.Add(Lang);
            } else 
            {
                FLingotionLanguage Lang(iso639_2);
                result.Add(Lang);
            }
            
        }
    }

    return result;
}

TSet<FString> UPackManifestHandler::GetAllAvailableActors() const
{
    TSet<FString> result;
    if (IsRootInvalid())
    {
        return result;
    }
    // Access "actorpack_modules" object
    const TSharedPtr<FJsonObject>* ActorPackModulesPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("actorpack_modules"), ActorPackModulesPtr) || !ActorPackModulesPtr)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("'actorpack_modules' not found in manifest."));
        return result;
    }

    const TSharedPtr<FJsonObject>& ActorPackModules = *ActorPackModulesPtr;

    // Iterate through modules
    for (const auto& ModulePair : ActorPackModules->Values)
    {
        const TSharedPtr<FJsonObject> ModuleObj = ModulePair.Value->AsObject();
        if (!ModuleObj.IsValid())
        {
            continue;
        }
        // Check actors array
        const TArray<TSharedPtr<FJsonValue>>* ActorsArray = nullptr;
        
        ModuleObj->TryGetArrayField(TEXT("actors"), ActorsArray);

        if (ActorsArray)
        {
            for (const auto& ActorValue : *ActorsArray)
            {
                result.Emplace(ActorValue->AsString());
            }
        }
    
    }
    return result;
}

TMap<EThespeonModuleType, FString> UPackManifestHandler::GetModuleTypesOfActor(const FString& ActorName) const
{
    TMap<EThespeonModuleType, FString> result;
    if (IsRootInvalid())
    {
        return result;
    }
    // Access "actorpack_modules" object
    const TSharedPtr<FJsonObject>* ActorPackModulesPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("actorpack_modules"), ActorPackModulesPtr) || !ActorPackModulesPtr)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("'actorpack_modules' not found in manifest."));
        return result;
    }

    const TSharedPtr<FJsonObject>& ActorPackModules = *ActorPackModulesPtr;

    // Iterate through modules
    for (const auto& ModulePair : ActorPackModules->Values)
    {
        const TSharedPtr<FJsonObject> ModuleObj = ModulePair.Value->AsObject();
        if (!ModuleObj.IsValid())
        {
            continue;
        }
        // Check actors array
        const TArray<TSharedPtr<FJsonValue>>* ActorsArray = nullptr;
        
        ModuleObj->TryGetArrayField(TEXT("actors"), ActorsArray);

        if (ActorsArray)
        {
            for (const auto& ActorValue : *ActorsArray)
            {
                FString CurrentActorName = ActorValue->AsString();
                // add module name to array corresponding to actor
                if(ActorName == CurrentActorName)
                {
                    FString ModuleTypeString;
                    ModuleObj->TryGetStringField(TEXT("quality"), ModuleTypeString);
                    EThespeonModuleType ModuleType = FindModuleType(ModuleTypeString); 
                    if(ModuleType != EThespeonModuleType::None)
                    {
                        result.Emplace(ModuleType, ModulePair.Key); // Move-construct if possible
                    }
                }
            }
        }
    
    }
    return result;
}

Thespeon::Core::FVersion UPackManifestHandler::ReadVersionObject(const TSharedPtr<FJsonObject>& ModuleObj) const
{
    int32 Major = 0, Minor = 0, Patch = 0;

    const TSharedPtr<FJsonObject>* VersionObjPtr = nullptr;
    if (ModuleObj.IsValid() &&
        ModuleObj->TryGetObjectField(TEXT("version"), VersionObjPtr) &&
        VersionObjPtr && (*VersionObjPtr).IsValid())
    {
        double Num = 0.0;
        if ((*VersionObjPtr)->TryGetNumberField(TEXT("major"), Num)) Major = static_cast<int32>(Num);
        if ((*VersionObjPtr)->TryGetNumberField(TEXT("minor"), Num)) Minor = static_cast<int32>(Num);
        if ((*VersionObjPtr)->TryGetNumberField(TEXT("patch"), Num)) Patch = static_cast<int32>(Num);
    }

    return Thespeon::Core::FVersion(Major, Minor, Patch);
}

TArray<FActorModuleInfo> UPackManifestHandler::GetAllActorModules() const
{
    TArray<FActorModuleInfo> Result;
    
    if (!Root.IsValid())
    {
        return Result;
    }
    
    const TSharedPtr<FJsonObject>* ActorPackModulesPtr = nullptr;
    if (Root->TryGetObjectField(TEXT("actorpack_modules"), ActorPackModulesPtr) && ActorPackModulesPtr)
    {
        const TSharedPtr<FJsonObject>& ActorPackModules = *ActorPackModulesPtr;
        
        for (const auto& ModulePair : ActorPackModules->Values)
        {
            const TSharedPtr<FJsonObject> ModuleObj = ModulePair.Value->AsObject();
            if (!ModuleObj.IsValid())
                continue;
            
            FActorModuleInfo Info;
            Info.ModuleID = ModulePair.Key;
            ModuleObj->TryGetStringField(TEXT("packname"), Info.PackName);
            ModuleObj->TryGetStringField(TEXT("jsonpath"), Info.JsonPath);
            FString typeString = "";
            ModuleObj->TryGetStringField(TEXT("quality"), typeString);
            Info.Quality = ModuleTypeToFrontEndString[StringToModuleType[typeString]];
            
            // Get actor name (first element of actors array)
            const TArray<TSharedPtr<FJsonValue>>* ActorsArray = nullptr;
            if (ModuleObj->TryGetArrayField(TEXT("actors"), ActorsArray) && ActorsArray && ActorsArray->Num() > 0)
            {
                Info.ActorName = (*ActorsArray)[0]->AsString();
            }
            
            Result.Add(Info);
        }
    }
    
    return Result;
}

TArray<FLanguageModuleInfo> UPackManifestHandler::GetAllLanguageModules() const
{
    TArray<FLanguageModuleInfo> Result;
    
    if (!Root.IsValid())
    {
        return Result;
    }
    
    const TSharedPtr<FJsonObject>* LanguagePackModulesPtr = nullptr;
    if (Root->TryGetObjectField(TEXT("languagepack_modules"), LanguagePackModulesPtr) && LanguagePackModulesPtr)
    {
        const TSharedPtr<FJsonObject>& LanguagePackModules = *LanguagePackModulesPtr;
        
        for (const auto& ModulePair : LanguagePackModules->Values)
        {
            const TSharedPtr<FJsonObject> ModuleObj = ModulePair.Value->AsObject();
            if (!ModuleObj.IsValid())
                continue;
            
            FLanguageModuleInfo Info;
            Info.ModuleID = ModulePair.Key;
            ModuleObj->TryGetStringField(TEXT("packname"), Info.PackName);
            ModuleObj->TryGetStringField(TEXT("jsonpath"), Info.JsonPath);
            
            // Get language name (first element of languages array, nameinenglish field)
            const TArray<TSharedPtr<FJsonValue>>* LanguagesArray = nullptr;
            if (ModuleObj->TryGetArrayField(TEXT("languages"), LanguagesArray) && LanguagesArray && LanguagesArray->Num() > 0)
            {
                const TSharedPtr<FJsonObject> LangObj = (*LanguagesArray)[0]->AsObject();
                if (LangObj.IsValid())
                {
                    LangObj->TryGetStringField(TEXT("nameinenglish"), Info.LanguageName);
                }
            }
            
            Result.Add(Info);
        }
    }
    
    return Result;
}

// Non vital functions below.
