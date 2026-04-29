// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once
#include "CoreMinimal.h"
#include "JsonObjectConverter.h"
#include "Language.generated.h"

/**
 * Identifies a language and dialect using standard linguistic codes.
 *
 * Multiple code systems are supported (ISO 639, Glottocode, ISO 3166) to allow
 * precise specification of languages and regional dialects. Used as a key in
 * character module language mappings. Can be used as a TMap key (provides GetTypeHash).
 *
 * A default-constructed instance represents an undefined language ("NOLANG").
 */
USTRUCT(BlueprintType)
struct FLingotionLanguage
{
	GENERATED_BODY()

	/** ISO 639-2 three-letter language code (e.g. "eng", "swe"). Primary identifier used for language matching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Language")
	FString ISO639_2;

	/** ISO 639-3 three-letter language code. More specific than ISO 639-2 for distinguishing individual languages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Language")
	FString ISO639_3;

	/** Glottocode identifier for the language family or dialect (e.g. "stan1293" for Standard English). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Language")
	FString Glottocode;

	/** ISO 3166-1 country code (e.g. "US", "GB"). Used to distinguish regional variants of a language. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Language")
	FString ISO3166_1;

	/** ISO 3166-2 subdivision code (e.g. "US-TX"). Identifies sub-national language variants. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Language")
	FString ISO3166_2;

	/** Free-form dialect identifier for variants not covered by standard codes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Language")
	FString CustomDialect;

	/** Human-readable display name, auto-generated from ISO639_2 and ISO3166_1 (e.g. "eng US"). */
	UPROPERTY(BlueprintReadOnly, Category = "Language")
	FString Name;

	/** Sentinel value representing an undefined language. */
	inline static const FString NoLang = TEXT("NOLANG");

	FLingotionLanguage() : ISO639_2(NoLang)
	{
		Name = NoLang;
	};
	FLingotionLanguage(const FLingotionLanguage& Other)
	    : ISO639_2(Other.ISO639_2)
	    , ISO639_3(Other.ISO639_3)
	    , Glottocode(Other.Glottocode)
	    , ISO3166_1(Other.ISO3166_1)
	    , ISO3166_2(Other.ISO3166_2)
	    , CustomDialect(Other.CustomDialect)
	    , Name(Other.Name)
	{
	}

	/**
	 * @brief Constructs a language from individual code components. Only ISO639_2 is required.
	 *
	 * @param InIso639_2 ISO 639-2 three-letter language code (required).
	 * @param InIso639_3 ISO 639-3 code (optional).
	 * @param InGlottocode Glottocode identifier (optional).
	 * @param InIso3166_1 ISO 3166-1 country code (optional).
	 * @param InIso3166_2 ISO 3166-2 subdivision code (optional).
	 * @param InCustomDialect Custom dialect string (optional).
	 */
	FLingotionLanguage(
	    const FString& InIso639_2,
	    const FString& InIso639_3 = TEXT(""),
	    const FString& InGlottocode = TEXT(""),
	    const FString& InIso3166_1 = TEXT(""),
	    const FString& InIso3166_2 = TEXT(""),
	    const FString& InCustomDialect = TEXT("")
	)
	    : ISO639_2(InIso639_2)
	    , ISO639_3(InIso639_3)
	    , Glottocode(InGlottocode)
	    , ISO3166_1(InIso3166_1)
	    , ISO3166_2(InIso3166_2)
	    , CustomDialect(InCustomDialect)
	{
		Name = InIso639_2 + TEXT(" ") + InIso3166_1;
	}

	/** Compares all code fields for equality. Name is excluded from comparison. */
	bool operator==(const FLingotionLanguage& Other) const
	{
		return ISO639_2 == Other.ISO639_2 && ISO639_3 == Other.ISO639_3 && Glottocode == Other.Glottocode && CustomDialect == Other.CustomDialect &&
		       ISO3166_1 == Other.ISO3166_1 && ISO3166_2 == Other.ISO3166_2;
	}

	/**
	 * @brief Checks whether this language is undefined.
	 *
	 * @return true if the language is undefined.
	 */
	bool IsUndefined() const
	{
		return ISO639_2 == NoLang;
	}

	/**
	 * @brief Serializes this language to a JSON string.
	 *
	 * @return A JSON representation of all language code fields.
	 */
	FString ToJson() const;

	/**
	 * @brief Concatenates all code fields into a single string for logging or hashing.
	 *
	 * @return All code fields joined together without separators.
	 */
	FString ToString() const
	{
		return ISO639_2 + ISO639_3 + Glottocode + ISO3166_1 + ISO3166_2 + CustomDialect;
	}

  public:
	/**
	 * @brief Attempts to parse a language from a JSON object.
	 *
	 * @param Json The JSON object containing language code fields.
	 * @param OutLanguage Receives the parsed language on success.
	 * @return true if parsing succeeded.
	 */
	static bool TryParseFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionLanguage& OutLanguage);
};

/** Hash function for FLingotionLanguage, required for use as a TMap key. */
FORCEINLINE uint32 GetTypeHash(const FLingotionLanguage& L)
{
	uint32 H = GetTypeHash(L.ISO639_2);
	H = HashCombineFast(H, GetTypeHash(L.ISO639_3));
	H = HashCombineFast(H, GetTypeHash(L.Glottocode));
	H = HashCombineFast(H, GetTypeHash(L.ISO3166_1));
	H = HashCombineFast(H, GetTypeHash(L.ISO3166_2));
	H = HashCombineFast(H, GetTypeHash(L.CustomDialect));
	return H;
}

namespace Thespeon
{
namespace Core
{
/**
 * @brief Finds the best matching language from a set of candidates.
 * Matches on ISO 639-2 first, then refines using additional codes when available.
 *
 * @param Candidates The available languages to match against.
 * @param Query The desired language to find the best match for.
 * @return The best matching language from Candidates, or an undefined language if no match.
 */
FLingotionLanguage BestLanguageMatch(const TArray<FLingotionLanguage>& Candidates, const FLingotionLanguage& Query);
} // namespace Core
} // namespace Thespeon
