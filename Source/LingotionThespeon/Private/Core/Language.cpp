// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Core/Language.h"
#include "Core/LingotionLogger.h"

// Simple BestMatch for now. Rejects all queries that do not have an iso639_2 code.
FLingotionLanguage Thespeon::Core::BestLanguageMatch(const TArray<FLingotionLanguage>& Candidates, const FLingotionLanguage& Query)
{

	if (Query.IsUndefined())
	{
		return FLingotionLanguage();
	}
	const FLingotionLanguage* Best = nullptr;
	int BestLangScore = 0; // init to -1 if we want to guarantee a match (first or any that matches either lang or region)
	int BestRegionScore = 0;

	for (const FLingotionLanguage& Candidate : Candidates)
	{
		if (Candidate.IsUndefined())
		{
			continue;
		} // defensive

		// Primary score: language
		const int LangScore = Candidate.ISO639_2.Equals(Query.ISO639_2, ESearchCase::IgnoreCase) ? 10 : 0; // to be expanded

		// Secondary score: region (tie-breaker)
		const int RegionScore = Candidate.ISO3166_1.Equals(Query.ISO3166_1, ESearchCase::IgnoreCase) ? 10 : 0; // to be expanded

		const bool BetterLang = (LangScore > BestLangScore);
		const bool TieLangBetterRegion = (LangScore == BestLangScore) && (RegionScore > BestRegionScore);

		if (BetterLang || TieLangBetterRegion)
		{
			Best = &Candidate;
			BestLangScore = LangScore;
			BestRegionScore = RegionScore;
		}
	}
	if (!Best)
	{
		return FLingotionLanguage();
	}
	return *Best;
}

bool FLingotionLanguage::TryParseFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionLanguage& OutLanguage)
{
	if (!Json.IsValid())
	{
		return false;
	}

	if (!Json->TryGetStringField(TEXT("iso639_2"), OutLanguage.ISO639_2))
	{
		return false;
	}
	Json->TryGetStringField(TEXT("iso639_3"), OutLanguage.ISO639_3);
	Json->TryGetStringField(TEXT("glottocode"), OutLanguage.Glottocode);
	Json->TryGetStringField(TEXT("iso3166_1"), OutLanguage.ISO3166_1);
	Json->TryGetStringField(TEXT("iso3166_2"), OutLanguage.ISO3166_2);
	Json->TryGetStringField(TEXT("customDialect"), OutLanguage.CustomDialect);

	OutLanguage.Name = OutLanguage.ISO639_2 + TEXT(" ") + OutLanguage.ISO3166_1;

	return true;
}

FString FLingotionLanguage::ToJson() const
{
	FString OutputString;
	const bool bSuccess = FJsonObjectConverter::UStructToJsonObjectString(*this, OutputString);
	if (!bSuccess)
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Failed to serialize to JSON"));
	}
	return OutputString;
}