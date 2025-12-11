// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Subsystems/EngineSubsystem.h"
#include "Containers/Map.h"
#include "../../Public/Core/ModelInput.h"
#include "Module.h"
#include "Dom/JsonObject.h"
#include "Language.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "PackManifestHandler.generated.h"

// Structs for returning module information to UI
struct FActorModuleInfo
{
    FString ModuleID;
    FString PackName;
    FString JsonPath;
    FString ActorName;
    FString Quality;
};

struct FLanguageModuleInfo
{
    FString ModuleID;
    FString PackName;
    FString JsonPath;
    FString LanguageName;
};

DECLARE_MULTICAST_DELEGATE(FOnDataChanged); // delegate to notify when data changes. Not used yet, but unity proj uses it in editor info window.

UCLASS()
class LINGOTIONTHESPEON_API UPackManifestHandler : public UEngineSubsystem
{
	GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void ReadPackManifest();
    // Static convenience getter
    inline static UPackManifestHandler* Get() { return GEngine ? GEngine->GetEngineSubsystem<UPackManifestHandler>() : nullptr; }
    // Matches string to module type, returning EThespeonModuleType::None if not matching
    UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon")
    EThespeonModuleType FindModuleType(const FString& ModuleTypeString) const;
    // Instance methods for inference to run
    Thespeon::Core::FModuleEntry GetActorPackModuleEntry(const FString& CharacterName, EThespeonModuleType ModuleType) const;
    Thespeon::Core::FModuleEntry GetLanguagePackModuleEntry(const FString& ModuleName) const;
    FString GetPackDirectory(const FString& PackName) const;
    // Creates a list of language object pointers for all languages in actor module
    UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon")
    TArray<FLingotionLanguage> GetAllLanguagesInActorModule(const FString& ModuleName) const;
    // Fetches the names of all available actors
    UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon")
    // Read-only queries (should be const):
    TSet<FString> GetAllAvailableActors() const;

    // Creates a mapping of Module Type to corresponding module id string
    UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon")
    TMap<EThespeonModuleType, FString> GetModuleTypesOfActor(const FString& ActorName) const;

    // Editor UI helper methods - Returns all actor modules for display
    TArray<FActorModuleInfo> GetAllActorModules() const;
    
    // Editor UI helper methods - Returns all language modules for display
    TArray<FLanguageModuleInfo> GetAllLanguageModules() const;

    // OBS when adding methods here, take a second to reflect. The methods listed below not marked with obsolete are more or less safe to implement when needed.
    // but if you feel like something else is needed - check first whether any of the existing methods already do what you want.
    // AVOID DUPLICATE FUNCTIONALITY. If you feel something is missing, bring it up for discussion.
    // See TUNI-330 for general discussion on this class.

    // Stuff used for info window (IW) and or railguards (R) and or PackImporter (PI) in Unity. Obsolete (O)

    // (O) (PI R) Dictionary<string, (string packName, Version version)> GetAllLanguagesAndTheirPacks() 
                    // Do collision detection like its done with actors instead. Use GetLanguagePackModuleEntry which now returns version.

    // (R)              List<ModuleLanguage> GetAllLanguageModuleLanguages() used to get candidates for best match.
                    // In unreal this will return TArray<ULingotionLanguage*> which contains our 6 codes + Name.
    // (IW) (R)         List<string> GetAllActors() used to populate IW and to replace invalid character names in input.
    // (IW) (R)         List<ModuleType> GetAllModuleTypesForActor(string actorName) used to populate IW and to replace invalid module names in input.
    // (IW)             Dictionary<string, ModuleLanguage> GetAllLanguagesForActorAndModuleType(string actorName, ModuleType moduleType)   OBS! List of ULingotionLanguage should suffice. Contains name in english.
    // (IW) (O)            Dictionary<string, ModuleLanguage> GetAllDialectsInModuleLanguage(string actorName, ModuleType type, string iso639_2) 
                    //OBS! List of ULingotionLanguage* should suffice. Contains name in english.
                    // This is pretty much obsolete. Aim to instead use GetAllLanguagesForActorAndModuleType and then filter on the result for those with the right iso code. 
    // (R)              List<ModuleLanguage> GetAllSupportedLanguages(string actorName, ModuleType type)
    // (IW)(Warmup)  Dictionary<string, string> GetAllSupportedLanguageCodes(string actorName, ModuleType type)      
                    // There is overlap functionality between this and Use GetAllSupportedLanguages. The difference is that GetAllSupportedLanguages intersects with what is imported while Codes does not.
                    // Try to coalesce or at least use common helper.
    // (IW)             List<string> GetAllActorPackNames()
    // (IW)           List<string> GetAllLanguagePackNames()
    // (IW)             List<string> GetAllModuleInfoInActorPack(string packName)
    // (IW)             List<string> GetAllModuleInfoInLanguagePack(string packName)
    // (IW)             List<string> GetMissingLanguagePacks()
private:
    // Instance members (no longer static)
    TSharedPtr<FJsonObject> Root;
    TMap<EThespeonModuleType, FString> ModuleTypeToString;
    TMap<EThespeonModuleType, FString> ModuleTypeToFrontEndString;
    TMap<FString, EThespeonModuleType> StringToModuleType;
    
    bool IsRootInvalid() const;
    bool TryGetActorpackModules(TSharedPtr<FJsonObject>& OutActorModulesPtr) const;
    Thespeon::Core::FVersion ReadVersionObject(const TSharedPtr<FJsonObject>& ModuleObj) const;

};
