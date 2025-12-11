// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "LingotionLogger.generated.h"

UENUM(BlueprintType)
enum class EVerbosityLevel : uint8
{
    None UMETA(DisplayName = "None"),
    Error UMETA(DisplayName = "Error"),
    Warning UMETA(DisplayName = "Warning"),
    Info UMETA(DisplayName = "Info"),
    Debug UMETA(DisplayName = "Debug"),
};


class LINGOTIONTHESPEON_API LingotionLogger
{
public:


    static std::atomic<EVerbosityLevel> CurrentVerbosityLevel;
    // Uses varargs and a TCHAR* so as to replace UE_LOG with minimal friction.
    static void Log(EVerbosityLevel level, const FString& message);
};


// Macro for UE_LOG-like function
#define LINGO_LOG(Level, Fmt, ...) \
LingotionLogger::Log(Level, FString::Printf(Fmt, ##__VA_ARGS__))
