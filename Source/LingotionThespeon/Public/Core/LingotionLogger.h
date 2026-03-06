// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Core/RuntimeThespeonSettings.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLingotionThespeon, Log, All);

/**
 * Centralized logging utility for Lingotion Thespeon.
 *
 * Respects the verbosity level configured in URuntimeThespeonSettings.
 * Use the LINGO_LOG and LINGO_LOG_FUNC macros instead of calling methods directly.
 */
class LINGOTIONTHESPEON_API LingotionLogger
{
  public:
	/**
	 * @brief Logs a message at the specified verbosity level.
	 * Messages above the configured verbosity threshold in URuntimeThespeonSettings are suppressed.
	 *
	 * @param level The verbosity level of this message.
	 * @param message The formatted message string to log.
	 */
	static void Log(EVerbosityLevel level, const FString& message);

	/**
	 * @brief Extracts a human-readable function name from a compiler-generated function signature.
	 *
	 * @param prettyFunction The raw __FUNCSIG__ or __PRETTY_FUNCTION__ string.
	 * @return The cleaned-up function name (e.g. "ClassName::MethodName").
	 */
	static FString ParsePrettyFunction(const char* prettyFunction);
};

/** Logs a formatted message at the given verbosity level. Usage: LINGO_LOG(EVerbosityLevel::Info, TEXT("Value: %d"), 42) */
#define LINGO_LOG(Level, Fmt, ...) LingotionLogger::Log(Level, FString::Printf(Fmt, ##__VA_ARGS__))

#if defined(_MSC_VER)
#define CURRENT_FUNCTION __FUNCSIG__
#else
#define CURRENT_FUNCTION __PRETTY_FUNCTION__
#endif

/** Logs a formatted message prefixed with the calling function name. Usage: LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("step %d"), i) */
#define LINGO_LOG_FUNC(Verbosity, Format, ...)                                                                                                       \
	LINGO_LOG(Verbosity, TEXT("[%s] ") Format, *LingotionLogger::ParsePrettyFunction(CURRENT_FUNCTION), ##__VA_ARGS__)
