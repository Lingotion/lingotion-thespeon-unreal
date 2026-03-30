// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
namespace Thespeon
{
namespace Language
{
/**
 * Abstract base class for converting numeric strings to word/phoneme representations.
 *
 * This class defines the contract for language-specific number converters.
 * Each language implementation (English, Swedish, etc.) should extend this class
 * and provide its own ConvertNumber implementation.
 *
 * The number conversion happens BEFORE phonemization in the inference pipeline,
 * converting numeric text like "123" into graphemes that the phonemizer can process.
 *
 * Usage:
 *   TUniquePtr<FNumberConverter> Converter = FNumberConverterFactory::Create(TEXT("eng"));
 *   FString Result = Converter->ConvertNumber(TEXT("42nd"));  // Returns phoneme representation
 */
class FNumberConverter
{
  public:
	virtual ~FNumberConverter() = default;

	/**
	 * Convert a numeric string to its word/phoneme representation.
	 *
	 * @param Input - The numeric string to convert (e.g., "123", "42nd", "3.14")
	 * @return The converted representation (phonemes or words depending on implementation)
	 */
	virtual FString ConvertNumber(const FString& Input) = 0;

  protected:
	/**
	 * Maximum safe integer value for conversion (2^53 - 1).
	 * Numbers beyond this threshold may lose precision when converted to/from double.
	 */
	static constexpr int64 MAX_SAFE_INTEGER = 9007199254740991LL;
};

/**
 * Factory class for creating language-specific NumberConverter instances.
 *
 * Uses ISO 639-2 language codes (3-letter codes like "eng", "swe") to
 * instantiate the appropriate converter via a registry pattern.
 *
 * To add a new language, simply add an entry to GetRegistry().
 */
class FNumberConverterFactory
{
  public:
	FNumberConverterFactory() = delete;
	FNumberConverterFactory(const FNumberConverterFactory&) = delete;
	FNumberConverterFactory& operator=(const FNumberConverterFactory&) = delete;

	using FFactoryFunc = TFunction<TUniquePtr<FNumberConverter>()>;

	/**
	 * Creates a NumberConverter for the specified language.
	 *
	 * @param Iso639_2 - ISO 639-2 language code (e.g., "eng" for English)
	 * @return Unique pointer to the converter, or nullptr if language not supported
	 */
	static TUniquePtr<FNumberConverter> Create(const FString& Iso639_2);

  private:
	/** Template helper to create factory functions for converter types */
	template <typename T> static FFactoryFunc MakeFactory()
	{
		return []() { return MakeUnique<T>(); };
	}

	/** Returns the registry of language codes to factory functions */
	static const TMap<FString, FFactoryFunc>& GetRegistry();
};
} // namespace Language
} // namespace Thespeon
