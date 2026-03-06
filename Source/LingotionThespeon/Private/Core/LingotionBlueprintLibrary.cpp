// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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