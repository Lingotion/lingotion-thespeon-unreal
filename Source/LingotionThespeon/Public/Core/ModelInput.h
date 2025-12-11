// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"        // optional, for safety with macros
#include "../Core/Language.h"
#include "ModelInput.generated.h"


/// @brief Enum for selecting NNE runtime. For later.
UENUM(BlueprintType)
enum class EBackendType : uint8
{
    CPU UMETA(DisplayName = "CPU"),
    GPU UMETA(DisplayName = "GPU")
};

UENUM(BlueprintType)
enum class EThespeonModuleType : uint8
{
    None UMETA(DisplayName = "None"),
    XL   UMETA(DisplayName = "XL"),
    L    UMETA(DisplayName = "L"),
    M    UMETA(DisplayName = "M"),
    S    UMETA(DisplayName = "S"),
    XS   UMETA(DisplayName = "XS")
};

UENUM(BlueprintType)
enum class EEmotion : uint8
{
    None          = 0  UMETA(DisplayName = "None"),
    Ecstasy       = 1  UMETA(DisplayName = "Ecstasy"),
    Admiration    = 2  UMETA(DisplayName = "Admiration"),
    Terror        = 3  UMETA(DisplayName = "Terror"),
    Amazement     = 4  UMETA(DisplayName = "Amazement"),
    Grief         = 5  UMETA(DisplayName = "Grief"),
    Loathing      = 6  UMETA(DisplayName = "Loathing"),
    Rage          = 7  UMETA(DisplayName = "Rage"),
    Vigilance     = 8  UMETA(DisplayName = "Vigilance"),
    Joy           = 9  UMETA(DisplayName = "Joy"),
    Trust         = 10 UMETA(DisplayName = "Trust"),
    Fear          = 11 UMETA(DisplayName = "Fear"),
    Surprise      = 12 UMETA(DisplayName = "Surprise"),
    Sadness       = 13 UMETA(DisplayName = "Sadness"),
    Disgust       = 14 UMETA(DisplayName = "Disgust"),
    Anger         = 15 UMETA(DisplayName = "Anger"),
    Anticipation  = 16 UMETA(DisplayName = "Anticipation"),
    Serenity      = 17 UMETA(DisplayName = "Serenity"),
    Acceptance    = 18 UMETA(DisplayName = "Acceptance"),
    Apprehension  = 19 UMETA(DisplayName = "Apprehension"),
    Distraction   = 20 UMETA(DisplayName = "Distraction"),
    Pensiveness   = 21 UMETA(DisplayName = "Pensiveness"),
    Boredom       = 22 UMETA(DisplayName = "Boredom"),
    Annoyance     = 23 UMETA(DisplayName = "Annoyance"),
    Interest      = 24 UMETA(DisplayName = "Interest"),
    Emotionless   = 25 UMETA(DisplayName = "Emotionless"),
    Contempt      = 26 UMETA(DisplayName = "Contempt"),
    Remorse       = 27 UMETA(DisplayName = "Remorse"),
    Disapproval   = 28 UMETA(DisplayName = "Disapproval"),
    Awe           = 29 UMETA(DisplayName = "Awe"),
    Submission    = 30 UMETA(DisplayName = "Submission"),
    Love          = 31 UMETA(DisplayName = "Love"),
    Optimism      = 32 UMETA(DisplayName = "Optimism"),
    Aggressiveness= 33 UMETA(DisplayName = "Aggressiveness")
};

USTRUCT(BlueprintType)
struct FLingotionInputSegment
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true), Category="Lingotion Thespeon")
    FString Text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lingotion Thespeon")
    EEmotion Emotion = EEmotion::Interest;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lingotion Thespeon")
    FLingotionLanguage Language;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lingotion Thespeon")
    bool bIsCustomPronounced = false;
    FLingotionInputSegment() = default;
    static bool TryParseFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionInputSegment& OutSegment);
};

/**
 * Model input data structure for inference.
 * This is a struct (not UObject) to avoid thread safety issues and simplify copying/passing.
 * Can be used in Blueprints as a data structure.
 */
USTRUCT(BlueprintType)
struct FLingotionModelInput
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lingotion Thespeon")
    TArray<FLingotionInputSegment> Segments;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lingotion Thespeon")
    EThespeonModuleType ModuleType = EThespeonModuleType::L;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lingotion Thespeon")
    FString CharacterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lingotion Thespeon")
    EEmotion DefaultEmotion = EEmotion::Interest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lingotion Thespeon")
    FLingotionLanguage DefaultLanguage;

    FLingotionModelInput() = default;

    template <typename TEnum>
    static bool ParseInputEnum(const FString& Str, TEnum& OutEnum)
    {
        UEnum* Enum = StaticEnum<TEnum>();
        if (!Enum)
            return false;

        int64 Value = Enum->GetValueByNameString(Str);
        if (Value == INDEX_NONE)
            return false;

        OutEnum = static_cast<TEnum>(Value);
        return true;
    }

    // Validates and changes the input in place if needed
    bool ValidateAndPopulate(FLingotionModelInput& Input);
    static bool TryParseInputFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionModelInput& OutModelInput);

    FString ToJson() const
    {
        unimplemented();
        return FString();
    }
};

