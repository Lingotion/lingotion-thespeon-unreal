// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Core/ManifestHandler.h"
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

void UManifestHandler::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("ManifestHandler initialized."));
	ReadManifest();
}

void UManifestHandler::Deinitialize()
{
	Root.Reset();
	Super::Deinitialize();
}

EThespeonModuleType UManifestHandler::FindModuleType(const FString& ModuleTypeString) const
{
	EThespeonModuleType result = EThespeonModuleType::None;
	const EThespeonModuleType* ModuleTypePtr = StringToModuleType.Find(ModuleTypeString);
	if (ModuleTypePtr)
	{
		result = *ModuleTypePtr;
	}
	return result;
}

// Reads the manifest JSON from disk and deserializes it into the Root JSON object.
// The manifest is the central registry of all imported character and language modules.
void UManifestHandler::ReadManifest()
{
	const FString& FilePath = Thespeon::Core::IO::RuntimeFileLoader::GetManifestPath();
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Reading manifest from: %s"), *FilePath);

	FString JsonString = Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsString(FilePath);
	if (!JsonString.IsEmpty())
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

		if (!(FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
		{
			LINGO_LOG(
			    EVerbosityLevel::Error,
			    TEXT(
			        "Failed to deserialize manifest. Manifest may be malformed. Try deleting the LingotionThespeonManifest.json file and recompiling your project."
			    )
			);
		}
	}
}

bool UManifestHandler::IsRootInvalid() const
{
	if (!Root.IsValid())
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Manifest not loaded. Manifest may be malformed. Try deleting the LingotionThespeonManifest.json file and recompiling your project.")
		);
		return true;
	}
	return false;
}

bool UManifestHandler::TryGetCharacterModules(TSharedPtr<FJsonObject>& OutCharacterModulesPtr) const
{
	const TSharedPtr<FJsonObject>* CharacterModulesPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("character_modules"), CharacterModulesPtr) || !CharacterModulesPtr)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT(
		        "'character_modules' not found in manifest. Manifest may be malformed. Try deleting the LingotionThespeonManifest.json file and recompiling your project."
		    )
		);
		return false;
	}
	OutCharacterModulesPtr = *CharacterModulesPtr;
	return true;
}

// Searches the manifest for a character module matching the given character name and quality tier.
// Iterates all character_modules entries, checking each module's "characters" array and "quality" field.
// Returns a populated FModuleEntry on match, or an empty entry if not found.
Thespeon::Core::FModuleEntry UManifestHandler::GetCharacterModuleEntry(const FString& CharacterName, EThespeonModuleType ModuleType) const
{
	if (IsRootInvalid())
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
	// Access character modules
	TSharedPtr<FJsonObject> CharacterModulesPtr;
	if (!TryGetCharacterModules(CharacterModulesPtr))
	{
		return Thespeon::Core::FModuleEntry(); // Empty
	}

	const TSharedPtr<FJsonObject>& CharacterModules = CharacterModulesPtr;

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Found %d character modules."), CharacterModules->Values.Num());

	// Iterate through modules
	for (const auto& ModulePair : CharacterModules->Values)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Checking module: %s"), *ModulePair.Key);
		const FString& ModuleId = ModulePair.Key;
		const TSharedPtr<FJsonObject> ModuleObj = ModulePair.Value->AsObject();
		if (!ModuleObj.IsValid())
		{
			continue;
		}

		// Check if the "characters" array contains CharacterName
		const TArray<TSharedPtr<FJsonValue>>* CharactersArray = nullptr;
		ModuleObj->TryGetArrayField(TEXT("characters"), CharactersArray);

		bool bCharacterMatch = false;
		if (CharactersArray)
		{
			for (const auto& CharacterValue : *CharactersArray)
			{
				if (CharacterValue->AsString() == CharacterName)
				{
					bCharacterMatch = true;
					break;
				}
			}
		}

		// Check "quality" field
		FString Quality;
		ModuleObj->TryGetStringField(TEXT("quality"), Quality);

		// If both match, return this module
		if (bCharacterMatch && Quality == ModuleTypeString)
		{
			FString JsonPath;
			ModuleObj->TryGetStringField(TEXT("jsonpath"), JsonPath);
			return Thespeon::Core::FModuleEntry(ModuleId, JsonPath, ReadVersionObject(ModuleObj));
		}
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug,
		    TEXT("No match for module: %s (CharacterMatch: %s, Quality: %s)"),
		    *ModuleId,
		    bCharacterMatch ? TEXT("true") : TEXT("false"),
		    *Quality
		);
	}

	// If nothing matches, return empty
	LINGO_LOG(
	    EVerbosityLevel::Error,
	    TEXT("No matching character module found for character '%s' of type '%s'. Try importing the desired character."),
	    *CharacterName,
	    *ModuleTypeString
	);
	return Thespeon::Core::FModuleEntry();
}

Thespeon::Core::FModuleEntry UManifestHandler::GetLanguageModuleEntry(const FString& ModuleName) const
{
	if (IsRootInvalid())
	{
		return Thespeon::Core::FModuleEntry();
	}

	// Get the "language_modules" object
	const TSharedPtr<FJsonObject>* LangModulesObjPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("language_modules"), LangModulesObjPtr) || !LangModulesObjPtr)
	{
		LINGO_LOG(
		    EVerbosityLevel::Warning,
		    TEXT(
		        "'language_modules' not found in manifest. Manifest may be malformed. Try deleting the LingotionThespeonManifest.json file and recompiling your project."
		    )
		);
		return Thespeon::Core::FModuleEntry();
	}
	const TSharedPtr<FJsonObject>& LangModules = *LangModulesObjPtr;

	// Find the module by key == ModuleName
	const TSharedPtr<FJsonValue>* ValuePtr = LangModules->Values.Find(ModuleName);
	if (!ValuePtr || !(*ValuePtr).IsValid())
	{
		LINGO_LOG(
		    EVerbosityLevel::Warning,
		    TEXT("A language has not been imported and use of it will not be possible. Check Thespeon Info Window for more information.")
		);
		return Thespeon::Core::FModuleEntry();
	}

	const TSharedPtr<FJsonObject> ModuleObj = (*ValuePtr)->AsObject();
	if (!ModuleObj.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Language module '%s' is not a valid object."), *ModuleName);
		return Thespeon::Core::FModuleEntry();
	}

	// Read jsonpath (empty if missing)
	FString JsonPath;
	ModuleObj->TryGetStringField(TEXT("jsonpath"), JsonPath);

	// Return the entry (ModuleId = key name, JsonPath = field)
	return Thespeon::Core::FModuleEntry(ModuleName, JsonPath, ReadVersionObject(ModuleObj));
}

// Iterates all valid entries in character_modules, invoking Callback for each.
// Handles root validation and character_modules extraction; skips invalid module objects.
void UManifestHandler::IterateCharacterModules(TFunctionRef<void(const FString&, const TSharedPtr<FJsonObject>&)> Callback) const
{
	if (IsRootInvalid())
	{
		return;
	}

	TSharedPtr<FJsonObject> CharacterModules;
	if (!TryGetCharacterModules(CharacterModules))
	{
		return;
	}

	for (const auto& ModulePair : CharacterModules->Values)
	{
		const TSharedPtr<FJsonObject> ModuleObj = ModulePair.Value->AsObject();
		if (!ModuleObj.IsValid())
		{
			continue;
		}
		Callback(ModulePair.Key, ModuleObj);
	}
}

// Extracts all languages from a specific character module's "languages" array.
// Each language entry is parsed into an FLingotionLanguage using iso639_2 and optionally iso3166_1 fields.
TArray<FLingotionLanguage> UManifestHandler::GetAllLanguagesInCharacterModule(const FString& ModuleName) const
{
	TArray<FLingotionLanguage> result;

	IterateCharacterModules(
	    [&](const FString& ModuleId, const TSharedPtr<FJsonObject>& ModuleObj)
	    {
		    if (ModuleId != ModuleName)
		    {
			    return;
		    }

		    const TArray<TSharedPtr<FJsonValue>>* LanguagesArray = nullptr;
		    ModuleObj->TryGetArrayField(TEXT("languages"), LanguagesArray);

		    if (!LanguagesArray)
		    {
			    return;
		    }

		    for (const auto& LanguageValue : *LanguagesArray)
		    {
			    const TSharedPtr<FJsonObject> LangObj = LanguageValue->AsObject();
			    FString iso639_2;
			    FString iso3166_1;
			    LangObj->TryGetStringField(TEXT("iso639_2"), iso639_2);
			    if (LangObj->TryGetStringField(TEXT("iso3166_1"), iso3166_1))
			    {
				    result.Add(FLingotionLanguage(iso639_2, TEXT(""), TEXT(""), iso3166_1));
			    }
			    else
			    {
				    result.Add(FLingotionLanguage(iso639_2));
			    }
		    }
	    }
	);

	return result;
}

// Collects all unique character names across every character_modules entry in the manifest.
TSet<FString> UManifestHandler::GetAllAvailableCharacters() const
{
	TSet<FString> result;

	IterateCharacterModules(
	    [&](const FString& /*ModuleId*/, const TSharedPtr<FJsonObject>& ModuleObj)
	    {
		    const TArray<TSharedPtr<FJsonValue>>* CharactersArray = nullptr;
		    ModuleObj->TryGetArrayField(TEXT("characters"), CharactersArray);

		    if (!CharactersArray)
		    {
			    return;
		    }

		    for (const auto& CharacterValue : *CharactersArray)
		    {
			    result.Emplace(CharacterValue->AsString());
		    }
	    }
	);

	return result;
}

// Returns all available quality tiers (module types) for a given character, mapped to their module IDs.
// Iterates every character_modules entry to find modules whose "characters" array contains CharacterName.
TMap<EThespeonModuleType, FString> UManifestHandler::GetModuleTypesOfCharacter(const FString& CharacterName) const
{
	TMap<EThespeonModuleType, FString> result;

	IterateCharacterModules(
	    [&](const FString& ModuleId, const TSharedPtr<FJsonObject>& ModuleObj)
	    {
		    const TArray<TSharedPtr<FJsonValue>>* CharactersArray = nullptr;
		    ModuleObj->TryGetArrayField(TEXT("characters"), CharactersArray);

		    if (!CharactersArray)
		    {
			    return;
		    }

		    for (const auto& CharacterValue : *CharactersArray)
		    {
			    if (CharacterName == CharacterValue->AsString())
			    {
				    FString ModuleTypeString;
				    ModuleObj->TryGetStringField(TEXT("quality"), ModuleTypeString);
				    EThespeonModuleType ModuleType = FindModuleType(ModuleTypeString);
				    if (ModuleType != EThespeonModuleType::None)
				    {
					    result.Emplace(ModuleType, ModuleId);
				    }
			    }
		    }
	    }
	);

	return result;
}

// Extracts a semantic version (major.minor.patch) from a module's "version" JSON sub-object.
Thespeon::Core::FVersion UManifestHandler::ReadVersionObject(const TSharedPtr<FJsonObject>& ModuleObj) const
{
	int32 Major = 0, Minor = 0, Patch = 0;

	const TSharedPtr<FJsonObject>* VersionObjPtr = nullptr;
	if (ModuleObj.IsValid() && ModuleObj->TryGetObjectField(TEXT("version"), VersionObjPtr) && VersionObjPtr && (*VersionObjPtr).IsValid())
	{
		double Num = 0.0;
		if ((*VersionObjPtr)->TryGetNumberField(TEXT("major"), Num))
		{
			Major = static_cast<int32>(Num);
		}
		if ((*VersionObjPtr)->TryGetNumberField(TEXT("minor"), Num))
		{
			Minor = static_cast<int32>(Num);
		}
		if ((*VersionObjPtr)->TryGetNumberField(TEXT("patch"), Num))
		{
			Patch = static_cast<int32>(Num);
		}
	}

	return Thespeon::Core::FVersion(Major, Minor, Patch);
}

FString UManifestHandler::GetQuality(const FString& QualityString, const FString& ModuleID) const
{
	const EThespeonModuleType* ModuleTypePtr = StringToModuleType.Find(QualityString);
	if (ModuleTypePtr)
	{
		const FString* QualityPtr = ModuleTypeToFrontEndString.Find(*ModuleTypePtr);
		return QualityPtr ? *QualityPtr : TEXT("");
	}

	LINGO_LOG(EVerbosityLevel::Warning, TEXT("Unknown quality string '%s' for module '%s'"), *QualityString, *ModuleID);
	return TEXT("");
}

TArray<FCharacterModuleInfo> UManifestHandler::GetAllCharacterModules() const
{
	TArray<FCharacterModuleInfo> Result;

	IterateCharacterModules(
	    [&](const FString& ModuleId, const TSharedPtr<FJsonObject>& ModuleObj)
	    {
		    FCharacterModuleInfo Info;
		    Info.ModuleID = ModuleId;
		    ModuleObj->TryGetStringField(TEXT("name"), Info.Name);
		    ModuleObj->TryGetStringField(TEXT("jsonpath"), Info.JsonPath);
		    FString typeString = "";
		    ModuleObj->TryGetStringField(TEXT("quality"), typeString);
		    Info.Quality = GetQuality(typeString, Info.ModuleID);

		    // Get character name (first element of characters array)
		    const TArray<TSharedPtr<FJsonValue>>* CharactersArray = nullptr;
		    if (ModuleObj->TryGetArrayField(TEXT("characters"), CharactersArray) && CharactersArray && CharactersArray->Num() > 0)
		    {
			    Info.CharacterName = (*CharactersArray)[0]->AsString();
		    }

		    Result.Add(Info);
	    }
	);

	return Result;
}

TArray<FLanguageModuleInfo> UManifestHandler::GetAllLanguageModules() const
{
	TArray<FLanguageModuleInfo> Result;

	if (!Root.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* LanguageModulesPtr = nullptr;
	if (Root->TryGetObjectField(TEXT("language_modules"), LanguageModulesPtr) && LanguageModulesPtr)
	{
		const TSharedPtr<FJsonObject>& LanguageModules = *LanguageModulesPtr;

		for (const auto& ModulePair : LanguageModules->Values)
		{
			const TSharedPtr<FJsonObject> ModuleObj = ModulePair.Value->AsObject();
			if (!ModuleObj.IsValid())
			{
				continue;
			}

			FLanguageModuleInfo Info;
			Info.ModuleID = ModulePair.Key;
			ModuleObj->TryGetStringField(TEXT("name"), Info.Name);
			ModuleObj->TryGetStringField(TEXT("jsonpath"), Info.JsonPath);

			// Get language name (first element of languages array, nameinenglish field)
			const TArray<TSharedPtr<FJsonValue>>* LanguagesArray = nullptr;
			if (ModuleObj->TryGetArrayField(TEXT("languages"), LanguagesArray) && LanguagesArray && LanguagesArray->Num() > 0)
			{
				const TSharedPtr<FJsonObject> LangObj = (*LanguagesArray)[0]->AsObject();
				if (LangObj.IsValid())
				{
					LangObj->TryGetStringField(TEXT("nameinenglish"), Info.LanguageName);
					LangObj->TryGetStringField(TEXT("iso639_2"), Info.Iso639_2);
				}
			}

			Result.Add(Info);
		}
	}

	return Result;
}

// Non vital functions below.

// Checks whether a file (identified by MD5) is referenced by more than one module in the manifest's
// "file_usage" map. Shared files must not be deleted when a single module is removed.
bool UManifestHandler::IsFileShared(const FString& MD5) const
{
	if (IsRootInvalid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* FileUsagePtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("file_usage"), FileUsagePtr) || !FileUsagePtr)
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* UsersArray = nullptr;
	if (!(*FileUsagePtr)->TryGetArrayField(MD5, UsersArray) || !UsersArray)
	{
		return false;
	}

	uint32 OutUsageCount = UsersArray->Num();
	return OutUsageCount > 1;
}