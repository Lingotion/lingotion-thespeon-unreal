// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "JsonObjectConverter.h"
#include "Language.generated.h"
 
USTRUCT(BlueprintType)
struct FLingotionLanguage
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ISO639_2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ISO639_3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Glottocode;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ISO3166_1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ISO3166_2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CustomDialect;
    UPROPERTY(BlueprintReadOnly)
    FString Name;

    inline static const FString NoLang = TEXT("NOLANG");

    // we need a default constructor for USTRUCTS, not intended for users.
    FLingotionLanguage(): ISO639_2(NoLang){Name = NoLang;};
    FLingotionLanguage(const FLingotionLanguage& Other)
    : ISO639_2(Other.ISO639_2)
    , ISO639_3(Other.ISO639_3)
    , Glottocode(Other.Glottocode)
    , ISO3166_1(Other.ISO3166_1)
    , ISO3166_2(Other.ISO3166_2)
    , CustomDialect(Other.CustomDialect)
    , Name(Other.Name)
    {}
    FLingotionLanguage(const FString& InIso639_2,
                       const FString& InIso639_3 = TEXT(""),
                       const FString& InGlottocode = TEXT(""),
                       const FString& InIso3166_1 = TEXT(""),
                       const FString& InIso3166_2 = TEXT(""),
                       const FString& InCustomDialect = TEXT(""))
        : ISO639_2(InIso639_2)
        , ISO639_3(InIso639_3)
        , Glottocode(InGlottocode)
        , ISO3166_1(InIso3166_1)
        , ISO3166_2(InIso3166_2)
        , CustomDialect(InCustomDialect)
    { Name = InIso639_2 + TEXT(" ") + InIso3166_1;}

    bool operator==(const FLingotionLanguage& Other) const
    {
        return ISO639_2 == Other.ISO639_2
            && ISO639_3 == Other.ISO639_3
            && Glottocode == Other.Glottocode
            && CustomDialect == Other.CustomDialect
            && ISO3166_1 == Other.ISO3166_1
            && ISO3166_2 == Other.ISO3166_2;
    }

    bool IsUndefined() const
    {
        return ISO639_2 == NoLang;
    }

    FString ToJson() const
    {
        // Serialize struct to JSON string using UE's built-in converter
        FString OutputString;
        FJsonObjectConverter::UStructToJsonObjectString(*this, OutputString);
        
        return OutputString;
    }

    FString ToString() const
    {
        return  ISO639_2 + ISO639_3 + Glottocode + ISO3166_1 + ISO3166_2 + CustomDialect;
    }
public:
    static bool TryParseFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionLanguage& OutLanguage);
};

// Hash function for FLingotionLanguage - required for using it as a TMap key
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
        FLingotionLanguage BestLanguageMatch(
            const TArray<FLingotionLanguage>& Candidates,
            const FLingotionLanguage& Query);
    }
}
