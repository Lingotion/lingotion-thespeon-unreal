// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "../../Public/Core/LingotionLogger.h"
#include "UObject/Class.h"


std::atomic<EVerbosityLevel> LingotionLogger::CurrentVerbosityLevel = EVerbosityLevel::Error;

void LingotionLogger::Log(EVerbosityLevel level, const FString& message)
{
    if(level > CurrentVerbosityLevel)
        return;
    static const TCHAR* LevelStrings[] = { TEXT("None"), TEXT("Error"), TEXT("Warning"), TEXT("Info"), TEXT("Debug")};
    
    const TCHAR* levelStr = LevelStrings[static_cast<uint8>(level)];
    switch(level)
    {
        case EVerbosityLevel::Error:
            UE_LOG(LogTemp, Error, TEXT("[%s] %s"), levelStr, *message);
            break;
        case EVerbosityLevel::Warning:
            UE_LOG(LogTemp, Warning, TEXT("[%s] %s"), levelStr, *message);
            break;
        case EVerbosityLevel::Info:
            UE_LOG(LogTemp, Log, TEXT("[%s] %s"), levelStr, *message);
            break;
        case EVerbosityLevel::Debug:
            UE_LOG(LogTemp, Log, TEXT("[%s] %s"), levelStr, *message);
            break;
        default:
            break;
    
    }
}
