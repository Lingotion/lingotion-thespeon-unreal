// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Core/LingotionBlueprintLibrary.h"
#include "Core/IO/RuntimeFileLoader.h"
#include "Utils/WavSaver.h"

bool ULingotionBlueprintLibrary::ParseModelInputFromJson(const FString& FilePath, FLingotionModelInput& OutModelInput)
{
	FString JsonString = Thespeon::Core::IO::RuntimeFileLoader::LoadFileAsString(FilePath);
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		return false;
	}
	return FLingotionModelInput::TryParseInputFromJson(Json, OutModelInput);
}

bool ULingotionBlueprintLibrary::ValidateCharacterModule(
    FLingotionModelInput& ModelInput, EThespeonModuleType FallbackModuleType, FLingotionModelInput& OutModelInput
)
{
	bool bSuccess = ModelInput.ValidateCharacterModule(FallbackModuleType);
	OutModelInput = ModelInput;
	return bSuccess;
}

bool ULingotionBlueprintLibrary::ValidateAndPopulate(
    FLingotionModelInput& ModelInput,
    EThespeonModuleType FallbackModuleType,
    FLingotionLanguage FallbackLanguage,
    EEmotion FallbackEmotion,
    FLingotionModelInput& OutModelInput
)
{
	bool bSuccess = ModelInput.ValidateAndPopulate(FallbackModuleType, FallbackLanguage, FallbackEmotion);
	OutModelInput = ModelInput;
	return bSuccess;
}

bool ULingotionBlueprintLibrary::SaveAudioAsWav(const FString& Filename, const TArray<float>& Samples)
{
	return FWavSaver::SaveWav(Filename, Samples);
}