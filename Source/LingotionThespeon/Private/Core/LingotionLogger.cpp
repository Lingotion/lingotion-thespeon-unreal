// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Core/LingotionLogger.h"

DEFINE_LOG_CATEGORY(LogLingotionThespeon);

FString LingotionLogger::ParsePrettyFunction(const char* prettyFunction)
{
	FString fullSignature(prettyFunction);

	int32 parenIndex = fullSignature.Find(TEXT("("), ESearchCase::IgnoreCase, ESearchDir::FromStart);
	if (parenIndex == INDEX_NONE)
	{
		return fullSignature;
	}

	FString beforeParams = fullSignature.Left(parenIndex).TrimEnd();

	int32 lastSpaceIndex = beforeParams.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (lastSpaceIndex != INDEX_NONE)
	{
		return beforeParams.RightChop(lastSpaceIndex + 1);
	}

	return beforeParams;
}

void LingotionLogger::Log(EVerbosityLevel level, const FString& message)
{
	const URuntimeThespeonSettings* Settings = GetDefault<URuntimeThespeonSettings>();
	EVerbosityLevel CurrentLevel = EVerbosityLevel::Error;
	if (!Settings)
	{
		UE_LOG(LogLingotionThespeon, Warning, TEXT("[Error] Could not get RuntimeThespeonSettings, defaulting to Error level."));
	}
	else
	{
		CurrentLevel = Settings->VerbosityLevel;
	}
	if (level > CurrentLevel)
	{
		return;
	}
	static const TCHAR* LevelStrings[] = {TEXT("None"), TEXT("Error"), TEXT("Warning"), TEXT("Info"), TEXT("Debug")};

	const TCHAR* levelStr = LevelStrings[static_cast<uint8>(level)];
	switch (level)
	{
		case EVerbosityLevel::Error:
			UE_LOG(LogLingotionThespeon, Error, TEXT("[%s] %s"), levelStr, *message);
			break;
		case EVerbosityLevel::Warning:
			UE_LOG(LogLingotionThespeon, Warning, TEXT("[%s] %s"), levelStr, *message);
			break;
		case EVerbosityLevel::Info:
			UE_LOG(LogLingotionThespeon, Log, TEXT("[%s] %s"), levelStr, *message);
			break;
		case EVerbosityLevel::Debug:
			UE_LOG(LogLingotionThespeon, Log, TEXT("[%s] %s"), levelStr, *message);
			break;
		default:
			break;
	}
}
