// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Engine/InferenceConfig.h"

FInferenceConfig::FInferenceConfig()
    : BackendType(EBackendType::CPU)
    , BufferSeconds(0.5f)
    , ModuleType(EThespeonModuleType::L)
    , FallbackEmotion(EEmotion::Interest)
    , FallbackLanguage(FLingotionLanguage(TEXT("eng")))
    , ThreadPriority(EThreadPriorityWrapper::AboveNormal)
{
	const URuntimeThespeonSettings* Settings = GetDefault<URuntimeThespeonSettings>();
	if (Settings)
	{
		BackendType = SettingToBackendType(Settings->BackendType);
		BufferSeconds = Settings->BufferSeconds;
		ThreadPriority = Settings->ThreadPriority;

		if (Settings->ModuleType != EThespeonModuleType::None)
		{
			ModuleType = Settings->ModuleType;
		}

		if (Settings->Emotion != EEmotion::None)
		{
			FallbackEmotion = Settings->Emotion;
		}

		if (!Settings->Language.IsUndefined())
		{
			FallbackLanguage = Settings->Language;
		}
	}
	else
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Could not get RuntimeThespeonSettings, using hardcoded defaults for InferenceConfig."));
	}
}

EBackendType FInferenceConfig::SettingToBackendType(ESettingBackendType SettingType) const
{
	switch (SettingType)
	{
		case ESettingBackendType::CPU:
			return EBackendType::CPU;
		case ESettingBackendType::GPU:
			return EBackendType::GPU;
		default:
			LINGO_LOG(EVerbosityLevel::Warning, TEXT("Unknown SettingBackendType, defaulting to CPU."));
			return EBackendType::CPU;
	}
}

FString FInferenceConfig::ToString() const
{
	return FString::Printf(
	    TEXT("BackendType: %s, BufferSeconds: %.2f, ModuleType: %s, FallbackEmotion: %s, FallbackLanguage: %s, ThreadPriority: %s"),
	    *UEnum::GetValueAsString(BackendType),
	    BufferSeconds,
	    *UEnum::GetValueAsString(ModuleType),
	    *UEnum::GetValueAsString(FallbackEmotion),
	    *FallbackLanguage.ToJson(),
	    *UEnum::GetValueAsString(ThreadPriority)
	);
}