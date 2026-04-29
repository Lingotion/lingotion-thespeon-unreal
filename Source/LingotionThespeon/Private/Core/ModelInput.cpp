// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Core/ModelInput.h"
#include "Core/LingotionLogger.h"
#include "Core/ManifestHandler.h"
#include "Inference/ModuleManager.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Language/TextPreprocessor.h"

bool FLingotionModelInput::ValidateCharacterModule(const EThespeonModuleType FallbackModuleType)
{
	auto* Manifest = UManifestHandler::Get();
	if (!Manifest)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error, TEXT("ManifestHandler instance is null. Make sure game subsystems are initialized before validating input.")
		);
		return false;
	}

	if (!this->SetCharacterNameIfInvalid(Manifest))
	{
		return false;
	}
	if (!this->SetModuleTypeIfInvalid(Manifest, FallbackModuleType))
	{
		return false;
	}
	return true;
}

// FModelInput is now a struct with inline default constructor
// No additional implementation needed
bool FLingotionModelInput::ValidateAndPopulate(
    const EThespeonModuleType FallbackModuleType, const FLingotionLanguage FallbackLanguage, const EEmotion FallbackEmotion
)
{
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Validating FLingotionModelInput:\n%s"), *this->ToJson());
	auto* Manifest = UManifestHandler::Get();
	if (!Manifest)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error, TEXT("ManifestHandler instance is null. Make sure game subsystems are initialized before validating input.")
		);
		return false;
	}
	auto* ModuleManager = UModuleManager::Get();
	if (!ModuleManager)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ModuleManager instance is null. Make sure game subsystems are initialized before validating input."));
		return false;
	}

	if (this->Segments.Num() == 0)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Segments cannot be empty. Please provide at least one segment with text to synthesize."));
		return false;
	}
	if (!ValidateCharacterModule(FallbackModuleType))
	{
		return false;
	}

	TArray<FLingotionLanguage> CandidateLanguages = this->GetCandidateLanguages(Manifest, ModuleManager);
	if (CandidateLanguages.Num() == 0)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Character module has no supported languages. Make sure you import at least one language that the module supports.")
		);
		return false;
	}
	FLingotionLanguage LangToTry = this->DefaultLanguage.IsUndefined() ? FallbackLanguage : this->DefaultLanguage;
	FLingotionLanguage LangToUse = Thespeon::Core::BestLanguageMatch(CandidateLanguages, LangToTry);
	if (LangToUse.IsUndefined())
	{
		LINGO_LOG(
		    EVerbosityLevel::Warning,
		    TEXT("No matching language found in character module, defaulting to first supported language %s"),
		    *CandidateLanguages[0].ToJson()
		);
		LangToUse = CandidateLanguages[0];
	}
	this->DefaultLanguage = LangToUse;
	if (this->DefaultEmotion == EEmotion::None)
	{
		this->DefaultEmotion = FallbackEmotion;
		LINGO_LOG(
		    EVerbosityLevel::Warning, TEXT("DefaultEmotion is None. Using fallback emotion %s."), *UEnum::GetValueAsString(this->DefaultEmotion)
		);
	}

	// Process segments - we need to build a new array since splitting can expand segments
	TArray<FLingotionInputSegment> ProcessedSegments;

	for (int32 Idx = 0; Idx < this->Segments.Num(); ++Idx)
	{
		FLingotionInputSegment& Segment = this->Segments[Idx];
		FString CleanedText = FTextPreprocessor::CleanText(Segment.Text, GetSegmentPosition(Idx, this->Segments.Num()));
		if (CleanedText.IsEmpty())
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Segment text cannot be empty. Please make sure each segment has meaningful text content."));
			return false;
		}
		Segment.Text = CleanedText;

		if (Segment.Emotion == EEmotion::None)
		{
			Segment.Emotion = this->DefaultEmotion;
			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug, TEXT("Segment emotion is None. Using default emotion %s."), *UEnum::GetValueAsString(Segment.Emotion)
			);
		}

		Segment.Language = ResolveSegmentLanguage(Segment, CandidateLanguages);

		// Split segment by numbers - this converts numbers to phonemes in separate CustomPronounced segments
		// This prevents double-phonemization of already converted numbers
		TArray<FLingotionInputSegment> SplitSegments = FTextPreprocessor::SplitSegmentByNumbers(Segment);
		ProcessedSegments.Append(SplitSegments);
	}

	// Replace original segments with processed (potentially expanded) segments
	this->Segments = MoveTemp(ProcessedSegments);

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("ValidateAndPopulate complete. Total segments after number splitting: %d"), this->Segments.Num());

	return true;
}

bool FLingotionInputSegment::TryParseFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionInputSegment& OutSegment)
{
	if (!Json.IsValid())
	{
		return false;
	}

	return Json->TryGetStringField(TEXT("text"), OutSegment.Text);
}

bool FLingotionModelInput::TryParseInputFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionModelInput& OutModelInput)
{
	if (!Json.IsValid())
	{
		return false;
	}

	bool bOk = true;

	bOk &= Json->TryGetStringField(TEXT("actorName"), OutModelInput.CharacterName);

	FString ModuleStr;
	if (Json->TryGetStringField(TEXT("moduleType"), ModuleStr))
	{
		bOk &= ParseInputEnum<EThespeonModuleType>(ModuleStr, OutModelInput.ModuleType);
	}

	FString EmotionStr;
	if (Json->TryGetStringField(TEXT("defaultEmotion"), EmotionStr))
	{
		bOk &= ParseInputEnum<EEmotion>(EmotionStr, OutModelInput.DefaultEmotion);
	}

	const TSharedPtr<FJsonObject>* LangObj = nullptr;
	if (Json->TryGetObjectField(TEXT("defaultLanguage"), LangObj))
	{
		bOk &= FLingotionLanguage::TryParseFromJson(*LangObj, OutModelInput.DefaultLanguage);
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonSegments = nullptr;
	if (Json->TryGetArrayField(TEXT("segments"), JsonSegments))
	{
		OutModelInput.Segments.Reset();
		for (auto& Val : *JsonSegments)
		{
			if (!Val.IsValid() || !Val->AsObject().IsValid())
			{
				return false;
			}
			FLingotionInputSegment newSegment;
			bOk &= FLingotionInputSegment::TryParseFromJson(Val->AsObject(), newSegment);
			OutModelInput.Segments.Add(newSegment);
		}
	}

	return bOk;
}

FString FLingotionModelInput::ToJson() const
{
	FString OutputString;
	const bool bSuccess = FJsonObjectConverter::UStructToJsonObjectString(*this, OutputString);
	if (!bSuccess)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to serialize input to JSON. Check that all fields are valid and serializable."));
	}
	return OutputString;
}

bool FLingotionModelInput::SetCharacterNameIfInvalid(UManifestHandler* Manifest)
{
	TSet<FString> AvailableCharacters = Manifest->GetAllAvailableCharacters();
	if (this->CharacterName.IsEmpty() || !AvailableCharacters.Contains(this->CharacterName))
	{
		if (AvailableCharacters.Num() == 0)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("No characters found. Make sure to import a character."));
			return false;
		}
		FString OriginalName = this->CharacterName;
		FString FirstCharacter = *AvailableCharacters.CreateConstIterator();
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Requested character '%s' was not found. Defaulting to '%s'."), *OriginalName, *FirstCharacter);
		this->CharacterName = FirstCharacter;
	}
	return true;
}

bool FLingotionModelInput::SetModuleTypeIfInvalid(UManifestHandler* Manifest, EThespeonModuleType FallbackModuleType)
{
	TMap<EThespeonModuleType, FString> Result = Manifest->GetModuleTypesOfCharacter(this->CharacterName);
	TArray<EThespeonModuleType> AvailableModuleTypes;
	Result.GenerateKeyArray(AvailableModuleTypes);
	if (this->ModuleType == EThespeonModuleType::None || !AvailableModuleTypes.Contains(this->ModuleType))
	{
		if (AvailableModuleTypes.Num() == 0)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("No module types found for character %s. Make sure to import a character."), *this->CharacterName);
			return false;
		}
		EThespeonModuleType OriginalType = this->ModuleType;
		this->ModuleType = FallbackModuleType != EThespeonModuleType::None && AvailableModuleTypes.Contains(FallbackModuleType)
		                       ? FallbackModuleType
		                       : AvailableModuleTypes[0];
		LINGO_LOG(
		    EVerbosityLevel::Warning,
		    TEXT("Requested module type '%s' is not available for character '%s'. Defaulting to '%s'."),
		    *UEnum::GetValueAsString(OriginalType),
		    *this->CharacterName,
		    *UEnum::GetValueAsString(this->ModuleType)
		);
	}
	return true;
}

FLingotionLanguage
FLingotionModelInput::ResolveSegmentLanguage(const FLingotionInputSegment& Segment, const TArray<FLingotionLanguage>& CandidateLanguages) const
{
	if (Segment.Language.IsUndefined())
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Segment language is undefined. Using default language %s."), *this->DefaultLanguage.ToJson());
		return this->DefaultLanguage;
	}

	FLingotionLanguage BestMatch = Thespeon::Core::BestLanguageMatch(CandidateLanguages, Segment.Language);
	if (BestMatch.IsUndefined())
	{
		LINGO_LOG(
		    EVerbosityLevel::Warning,
		    TEXT("No matching language found in character module for segment language %s, defaulting to default language %s"),
		    *Segment.Language.ToJson(),
		    *this->DefaultLanguage.ToJson()
		);
		return this->DefaultLanguage;
	}

	return BestMatch;
}

TArray<FLingotionLanguage> FLingotionModelInput::GetCandidateLanguages(UManifestHandler* Manifest, UModuleManager* ModuleManager)
{
	Thespeon::Core::FModuleEntry Entry = Manifest->GetCharacterModuleEntry(this->CharacterName, this->ModuleType);
	// TSharedPtr keeps module alive for this scope; raw pointer used for downstream API compatibility
	TSharedPtr<Thespeon::Character::CharacterModule, ESPMode::ThreadSafe> CharacterModulePtr =
	    ModuleManager->GetModule<Thespeon::Character::CharacterModule>(Entry, false);
	Thespeon::Character::CharacterModule* CharacterModule = CharacterModulePtr.Get();

	TArray<FLingotionLanguage> CandidateLanguages = CharacterModule->GetSupportedLanguages();
	TArray<FLanguageModuleInfo> ImportedLanguages = Manifest->GetAllLanguageModules();
	TArray<FLingotionLanguage> LanguagesToRemove;
	for (FLingotionLanguage& Lang : CandidateLanguages)
	{
		// if Langs iso639_2 is not in ImportedLanguages, remove it from CandidateLanguages
		bool bFound = ImportedLanguages.ContainsByPredicate([&Lang](const FLanguageModuleInfo& Info) { return Lang.ISO639_2 == Info.Iso639_2; });
		if (!bFound)
		{
			LanguagesToRemove.Add(Lang);
		}
	}
	if (LanguagesToRemove.Num() > 0)
	{
		LINGO_LOG(
		    EVerbosityLevel::Warning,
		    TEXT("These supported languages have not been imported. This is okay but use of these languages will not be possible: \n%s"),
		    *FString::JoinBy(LanguagesToRemove, TEXT("\n"), [](const FLingotionLanguage& Lang) { return Lang.ToJson(); })
		);
	}
	for (FLingotionLanguage& LangToRemove : LanguagesToRemove)
	{
		CandidateLanguages.Remove(LangToRemove);
	}
	return CandidateLanguages;
}